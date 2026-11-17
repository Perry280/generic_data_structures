#ifndef GENERIC_HASHMAP_H
#define GENERIC_HASHMAP_H

#include "../generic_types.h"
#include <stddef.h>
#include <stdint.h>

#define GHM_OK        G_OK     /**< Success. */
#define GHM_ENOMEM    G_ENOMEM /**< Memory allocation failed. */
#define GHM_EINVAL    G_EINVAL /**< Invalid argument (e.g. a `NULL` pointer). */
#define GHM_ENOTFOUND G_ENOTFOUND /**< Value not found. */
#define GHM_EEMPTY    G_EEMPTY    /**< Operation on an empty struct. */

typedef struct ghm_s ghm_t;

typedef uint64_t (*ghm_hash_f)(const void *);

ghm_t *ghm_create(size_t key_size,
                  size_t elem_size,
                  g_cmp_f key_cmp,
                  ghm_hash_f key_hash,
                  g_free_f key_free,
                  g_free_f elem_free);

void ghm_free(ghm_t *hm);

int ghm_insert(ghm_t *hm, const void *key, const void *elem);

#endif
