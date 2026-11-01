#include "generic_vector.h"
#include <stdlib.h>
#include <string.h>

#define GVEC_DEFAULT_CAPACITY        ((size_t)128)
#define GVEC_MAX_CAPACITY(data_size) ((size_t)PTRDIFF_MAX / (data_size))

struct gvec_s {
    void *array;

    g_cmp_f cmp;
    g_free_f free;

    size_t data_size;
    size_t size;
    size_t capacity;
};

struct gvec_iter_s {
    const gvec_t *vec;
    size_t index;
};

#define GVEC_PTR_AT(vec, index)                                                \
    ((void *)(((uint8_t *)((vec)->array)) + ((index) * (vec)->data_size)))
#define GVEC_PTR_LAST(vec) GVEC_PTR_AT(vec, (vec)->size - 1)

gvec_t *gvec_create(size_t data_size, g_cmp_f cmp, g_free_f free_array) {
    return gvec_create_capacity(data_size,
                                cmp,
                                free_array,
                                GVEC_DEFAULT_CAPACITY);
}

gvec_t *gvec_create_capacity(size_t data_size,
                             g_cmp_f cmp,
                             g_free_f free_array,
                             size_t capacity) {
    if (data_size == 0 || cmp == NULL
        || capacity > GVEC_MAX_CAPACITY(data_size))
        return NULL;

    gvec_t *const vec = malloc(sizeof(gvec_t));
    if (vec == NULL)
        return NULL;

    if (capacity > 0) {
        vec->array = malloc(capacity * data_size);
        if (vec->array == NULL) {
            free(vec);
            return NULL;
        }
    } else
        vec->array = NULL;

    vec->size = 0;
    vec->data_size = data_size;
    vec->cmp = cmp;
    vec->free = free_array;
    vec->capacity = capacity;

    return vec;
}

static void gvec_free_array(gvec_t *vec) {
    if (vec->array == NULL)
        return;

    if (vec->free != NULL) {
        const g_free_f fn = vec->free;
        for (size_t i = 0; i < vec->size; i++)
            fn(GVEC_PTR_AT(vec, i));
    }
    free(vec->array);
}

void gvec_free(gvec_t *vec) {
    if (vec == NULL)
        return;

    gvec_free_array(vec);
    free(vec);
}

const void *gvec_array(const gvec_t *vec) {
    return vec->array;
}

size_t gvec_size(const gvec_t *vec) {
    return vec->size;
}

size_t gvec_capacity(const gvec_t *vec) {
    return vec->capacity;
}

void *gvec_ptr_at(const gvec_t *vec, size_t index) {
    return GVEC_PTR_AT(vec, index);
}

void *gvec_ptr_last(const gvec_t *vec) {
    return GVEC_PTR_LAST(vec);
}

int gvec_set(gvec_t *vec, size_t index, const void *data) {
    if (vec == NULL || data == NULL)
        return GVEC_EINVAL;

    if (index >= vec->size)
        return GVEC_ERANGE;

    if (vec->free != NULL)
        vec->free(GVEC_PTR_AT(vec, index));

    memcpy(GVEC_PTR_AT(vec, index), data, vec->data_size);
    return GVEC_OK;
}

int gvec_at(const gvec_t *vec, size_t index, void *out) {
    if (vec == NULL || out == NULL)
        return GVEC_EINVAL;

    if (index >= vec->size)
        return GVEC_ERANGE;

    memcpy(out, GVEC_PTR_AT(vec, index), vec->data_size);
    return GVEC_OK;
}

int gvec_push_back(gvec_t *vec, const void *data) {
    if (vec == NULL || data == NULL)
        return GVEC_EINVAL;

    if (vec->size == vec->capacity) {
        const size_t vec_cap = vec->capacity;
        const size_t max = GVEC_MAX_CAPACITY(vec->data_size);
        if (vec_cap >= max)
            return GVEC_ENOMEM;

        const size_t new_capacity =
            vec->array == NULL ? GVEC_DEFAULT_CAPACITY
                               : ((vec_cap > max / 2) ? max : vec_cap * 2);

        const int rc = gvec_reserve(vec, new_capacity);
        if (rc != GVEC_OK)
            return rc;
    }

    memcpy(GVEC_PTR_AT(vec, vec->size), data, vec->data_size);
    vec->size++;

    return GVEC_OK;
}

int gvec_pop_back(gvec_t *vec, void *out) {
    if (vec == NULL)
        return GVEC_EINVAL;

    if (vec->size == 0)
        return GVEC_EEMPTY;

    if (out != NULL)
        memcpy(out, GVEC_PTR_LAST(vec), vec->data_size);
    else if (vec->free != NULL)
        vec->free(GVEC_PTR_LAST(vec));

    vec->size--;
    return GVEC_OK;
}

int gvec_reserve(gvec_t *vec, size_t new_capacity) {
    if (vec == NULL || new_capacity == 0)
        return GVEC_EINVAL;

    if (new_capacity <= vec->capacity)
        return GVEC_OK;

    if (new_capacity > GVEC_MAX_CAPACITY(vec->data_size))
        return GVEC_ENOMEM;

    const size_t mem_size = new_capacity * vec->data_size;
    void *const new_array =
        vec->array != NULL ? realloc(vec->array, mem_size) : malloc(mem_size);

    if (new_array == NULL)
        return GVEC_ENOMEM;

    vec->array = new_array;
    vec->capacity = new_capacity;
    return GVEC_OK;
}

int gvec_shrink_to_fit(gvec_t *vec) {
    if (vec == NULL)
        return GVEC_EINVAL;

    if (vec->size == vec->capacity || vec->array == NULL)
        return GVEC_OK;

    if (vec->size == 0) {
        free(vec->array);
        vec->array = NULL;
        vec->capacity = 0;
        return GVEC_OK;
    }

    void *const new_array = realloc(vec->array, vec->size * vec->data_size);

    if (new_array == NULL)
        return GVEC_ENOMEM;

    vec->array = new_array;
    vec->capacity = vec->size;
    return GVEC_OK;
}

gvec_iter_t *gvec_iter_create(const gvec_t *vec) {
    if (vec == NULL)
        return NULL;

    gvec_iter_t *const iter = malloc(sizeof(gvec_iter_t));
    if (iter == NULL)
        return NULL;

    iter->vec = vec;
    iter->index = 0;

    return iter;
}

void gvec_iter_free(gvec_iter_t *iter) {
    if (iter == NULL)
        return;

    free(iter);
}

int gvec_iter_has_next(const gvec_iter_t *iter) {
    if (iter == NULL)
        return 0;

    return iter->index < iter->vec->size;
}

int gvec_iter_next(gvec_iter_t *iter, void *out) {
    if (iter == NULL || out == NULL)
        return GVEC_EINVAL;

    if (iter->index >= iter->vec->size)
        return GVEC_EITER;

    memcpy(out, GVEC_PTR_AT(iter->vec, iter->index), iter->vec->data_size);
    iter->index++;

    return GVEC_OK;
}
