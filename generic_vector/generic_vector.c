#include "generic_vector.h"
#include <stdlib.h>
#include <string.h>

#define GVEC_DEFAULT_CAPACITY ((size_t)128)
#define GVEC_MAX_CAPACITY(data_size)                                           \
    ((size_t)(((size_t)PTRDIFF_MAX) / (data_size)))

struct gvec_s {
    g_functions_t fn;

    uint8_t *array;

    size_t data_size;
    size_t size;
    size_t capacity;
};

struct gvec_iter_s {
    const gvec_t *vec;
    size_t index;
};

#define GVEC_PTR_AT(vec, index) ((vec)->array + ((index) * (vec)->data_size))
#define GVEC_PTR_LAST(vec)      GVEC_PTR_AT(vec, (vec)->size - 1)

gvec_t *gvec_create(size_t data_size, g_functions_t fn) {
    return gvec_create_capacity(data_size, fn, GVEC_DEFAULT_CAPACITY);
}

gvec_t *gvec_create_capacity(size_t data_size,
                             g_functions_t fn,
                             size_t capacity) {
    if (data_size == 0 || fn.cmp == nullptr
        || capacity > GVEC_MAX_CAPACITY(data_size))
        return nullptr;

    gvec_t *const vec = malloc(sizeof(gvec_t));
    if (vec == nullptr)
        goto CLEANUP;

    if (capacity > 0) {
        vec->array = malloc(capacity * data_size);
        if (vec->array == nullptr)
            goto CLEANUP;
    } else
        vec->array = nullptr;

    vec->size = 0;
    vec->data_size = data_size;
    vec->fn = fn;
    vec->capacity = capacity;

    return vec;

CLEANUP:
    if (vec != nullptr)
        free(vec);
    return nullptr;
}

static void gvec_free_array(gvec_t *vec) {
    if (vec->fn.free != nullptr) {
        const g_free_f free_data = vec->fn.free;
        for (size_t i = 0; i < vec->size; i++)
            free_data(GVEC_PTR_AT(vec, i));
    }

    free(vec->array);
}

void gvec_free(gvec_t *vec) {
    if (vec == nullptr)
        return;

    if (vec->array != nullptr)
        gvec_free_array(vec);
    free(vec);
}

const void *gvec_array(const gvec_t *vec) {
    return (const void *)vec->array;
}

size_t gvec_size(const gvec_t *vec) {
    return vec->size;
}

size_t gvec_capacity(const gvec_t *vec) {
    return vec->capacity;
}

const void *gvec_ptr_at(const gvec_t *vec, size_t index) {
    return (const void *)GVEC_PTR_AT(vec, index);
}

const void *gvec_ptr_last(const gvec_t *vec) {
    return (const void *)GVEC_PTR_LAST(vec);
}

int gvec_set(gvec_t *vec, size_t index, const void *data) {
    if (vec == nullptr || data == nullptr)
        return G_EINVAL;

    if (index >= vec->size)
        return G_ERANGE;

    if (vec->fn.free != nullptr)
        vec->fn.free(GVEC_PTR_AT(vec, index));

    memcpy(GVEC_PTR_AT(vec, index), data, vec->data_size);
    return G_OK;
}

int gvec_get(const gvec_t *vec, size_t index, void *out) {
    if (vec == nullptr || out == nullptr)
        return G_EINVAL;

    if (index >= vec->size)
        return G_ERANGE;

    memcpy(out, GVEC_PTR_AT(vec, index), vec->data_size);
    return G_OK;
}

int gvec_push_back(gvec_t *vec, const void *data) {
    if (vec == nullptr || data == nullptr)
        return G_EINVAL;

    if (vec->size == vec->capacity) {
        const size_t vec_cap = vec->capacity;
        const size_t max = GVEC_MAX_CAPACITY(vec->data_size);
        if (vec_cap >= max)
            return G_ENOMEM;

        const size_t new_capacity = vec->array == nullptr
                                        ? GVEC_DEFAULT_CAPACITY
                                        : ((vec_cap > max / 2) ? max
                                                               : vec_cap * 2);

        const int rc = gvec_reserve(vec, new_capacity);
        if (rc != G_OK)
            return rc;
    }

    memcpy(GVEC_PTR_AT(vec, vec->size), data, vec->data_size);
    vec->size++;

    return G_OK;
}

int gvec_pop_back(gvec_t *vec, void *out) {
    if (vec == nullptr)
        return G_EINVAL;

    if (vec->size == 0)
        return G_EEMPTY;

    if (out != nullptr)
        memcpy(out, GVEC_PTR_LAST(vec), vec->data_size);
    else if (vec->fn.free != nullptr)
        vec->fn.free(GVEC_PTR_LAST(vec));

    vec->size--;
    return G_OK;
}

int gvec_reserve(gvec_t *vec, size_t new_capacity) {
    if (vec == nullptr || new_capacity == 0)
        return G_EINVAL;

    if (new_capacity <= vec->capacity)
        return G_OK;

    if (new_capacity > GVEC_MAX_CAPACITY(vec->data_size))
        return G_ENOMEM;

    const size_t mem_size = new_capacity * vec->data_size;
    void *const new_array = vec->array != nullptr
                                ? realloc(vec->array, mem_size)
                                : malloc(mem_size);

    if (new_array == nullptr)
        return G_ENOMEM;

    vec->array = new_array;
    vec->capacity = new_capacity;
    return G_OK;
}

int gvec_shrink_to_fit(gvec_t *vec) {
    if (vec == nullptr)
        return G_EINVAL;

    if (vec->size == vec->capacity || vec->array == nullptr)
        return G_OK;

    if (vec->size == 0) {
        gvec_free_array(vec);
        vec->array = nullptr;
        vec->capacity = 0;
        return G_OK;
    }

    void *const new_array = realloc(vec->array, vec->size * vec->data_size);

    if (new_array == nullptr)
        return G_ENOMEM;

    vec->array = new_array;
    vec->capacity = vec->size;
    return G_OK;
}

gvec_iter_t *gvec_iter_create(const gvec_t *vec) {
    if (vec == nullptr)
        return nullptr;

    gvec_iter_t *const iter = malloc(sizeof(gvec_iter_t));
    if (iter == nullptr)
        return nullptr;

    iter->vec = vec;
    iter->index = 0;

    return iter;
}

void gvec_iter_free(gvec_iter_t *iter) {
    if (iter == nullptr)
        return;

    free(iter);
}

bool gvec_iter_has_next(const gvec_iter_t *iter) {
    if (iter == nullptr)
        return false;

    return iter->index < iter->vec->size;
}

int gvec_iter_next(gvec_iter_t *iter, void *out) {
    if (iter == nullptr || out == nullptr)
        return G_EINVAL;

    if (iter->index >= iter->vec->size)
        return G_EITER;

    memcpy(out, GVEC_PTR_AT(iter->vec, iter->index), iter->vec->data_size);
    iter->index++;

    return G_OK;
}
