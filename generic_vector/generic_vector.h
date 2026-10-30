#ifndef GENERIC_VECTOR_H
#define GENERIC_VECTOR_H

#include "../generic_types.h"
#include <stddef.h>
#include <stdint.h>

#define GVEC_OK     G_OK     /**< Success. */
#define GVEC_ENOMEM G_ENOMEM /**< Memory allocation failed. */
#define GVEC_EINVAL G_EINVAL /**< Invalid argument (e.g. a `NULL` pointer). */
#define GVEC_ERANGE G_ERANGE /**< Index out of range. */
#define GVEC_ENOTFOUND G_ENOTFOUND /**< Value not found. */
#define GVEC_EEMPTY    G_EEMPTY    /**< Operation on an empty vector. */
#define GVEC_EITER     G_EITER     /**< Iterator has no next element. */

typedef struct gvec_s {
    void *array;

    g_cmp_f cmp;
    g_free_f free;

    size_t data_size;
    size_t size;
    size_t capacity;
} gvec_t;

typedef struct gvec_iter_s gvec_iter_t;

#define GVEC_PTR_AT(vec, index)                                                \
    ((void *)(((uint8_t *)((vec)->array)) + ((index) * (vec)->data_size)))

#define GVEC_PTR_LAST(vec) GVEC_PTR_AT(vec, (vec)->size - 1)

gvec_t *gvec_create(size_t data_size, g_cmp_f cmp, g_free_f free_array);
gvec_t *gvec_create_capacity(size_t data_size,
                             g_cmp_f cmp,
                             g_free_f free_array,
                             size_t capacity);
void gvec_free(gvec_t *vec);

int gvec_set(gvec_t *vec, size_t index, const void *data);
int gvec_at(const gvec_t *vec, size_t index, void *out);

int gvec_push_back(gvec_t *vec, const void *data);
int gvec_pop_back(gvec_t *vec, void *out);

int gvec_reserve(gvec_t *vec, size_t new_capacity);

gvec_iter_t *gvec_iter_create(const gvec_t *vec);
void gvec_iter_free(gvec_iter_t *iter);

int gvec_iter_has_next(const gvec_iter_t *iter);
int gvec_iter_next(gvec_iter_t *iter, void *out);

#endif
