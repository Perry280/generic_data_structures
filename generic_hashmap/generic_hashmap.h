#ifndef GENERIC_HASHMAP_H
#define GENERIC_HASHMAP_H

#include "../generic_types.h"
#include <stddef.h>
#include <stdint.h>

typedef struct ghm_s ghm_t;

ghm_t *ghm_create(size_t key_size,
                  size_t elem_size,
                  g_fun_t key_fun,
                  g_fun_t elem_fun);

void ghm_free(ghm_t *hm);

// Hello
size_t ghm_size(const ghm_t *hm);

int ghm_insert(ghm_t *hm, const void *key, const void *elem);
int ghm_delete(ghm_t *hm, const void *key);
int ghm_get(const ghm_t *hm, const void *key, void *out);

#endif
