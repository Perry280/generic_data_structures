#ifndef GENERIC_HASHMAP_H
#define GENERIC_HASHMAP_H

#include "../generic_types.h"
#include <stddef.h>
#include <stdint.h>

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
