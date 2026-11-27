#ifndef GENERIC_HASHMAP_H
#define GENERIC_HASHMAP_H

#include "../generic_types.h"
#include <stddef.h>

typedef struct ghm_s ghm_t;

ghm_t *ghm_create(size_t key_size,
                  size_t value_size,
                  g_functions_t key_fn,
                  g_functions_t value_fn);

void ghm_free(ghm_t *hm);

size_t ghm_size(const ghm_t *hm);

int ghm_insert(ghm_t *hm, const void *key, const void *value);
int ghm_delete(ghm_t *hm, const void *key);
int ghm_get(const ghm_t *hm, const void *key, void *out);

#endif
