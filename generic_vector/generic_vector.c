#include "generic_vector.h"
#include <bits/posix1_lim.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define GVEC_DEFAULT_CAPACITY (size_t)128

#define GVEC_MEM_SIZE_ALLOC(capacity, data_size)                               \
    (capacity) <= (size_t)(SIZE_MAX / (data_size)) ? (capacity) * (data_size)  \
                                                   : SIZE_MAX

struct gvec_iter_s {
    const gvec_t *vec;
    size_t index;
};

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
    if (data_size == 0 || cmp == NULL)
        return NULL;

    gvec_t *const vec = malloc(sizeof(gvec_t));
    if (vec == NULL)
        return NULL;

    if (capacity > 0) {
        vec->array = malloc(GVEC_MEM_SIZE_ALLOC(capacity, data_size));
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

void gvec_free(gvec_t *vec) {
    if (vec == NULL)
        return;

    if (vec->array != NULL) {
        if (vec->free != NULL) {
            const g_free_f fn = vec->free;
            for (size_t i = 0; i < vec->size; i++)
                fn(GVEC_PTR_AT(vec, i));
        }
        free(vec->array);
    }

    free(vec);
}

int gvec_set(gvec_t *vec, size_t index, const void *data) {
    if (vec == NULL || data == NULL)
        return GVEC_EINVAL;

    if (index >= vec->size)
        return GVEC_ERANGE;

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
        size_t new_capacity =
            vec->array != NULL ? vec->capacity * 2 : GVEC_DEFAULT_CAPACITY;
        if (gvec_reserve(vec, new_capacity) == GVEC_ENOMEM)
            return GVEC_ENOMEM;
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

    const size_t mem_size = GVEC_MEM_SIZE_ALLOC(new_capacity, vec->data_size);
    void *const new_array =
        vec->array != NULL ? realloc(vec->array, mem_size) : malloc(mem_size);

    if (new_array == NULL)
        return GVEC_ENOMEM;

    vec->array = new_array;
    vec->capacity = new_capacity;
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
