#include "generic_hashmap.h"
#include "hash.h"
#include <stdint.h>
#include <stdlib.h>

#define GHM_DEFAULT_SIZE ((size_t)0x10)

enum ghm_index_state {
    GHM_OCCUPIED = 1,
    GHM_FREE = 0,
    GHM_DELETED = -1
};

typedef struct ghm_index_s {
    size_t value;
    enum ghm_index_state state;
} ghm_index_t;

typedef struct ghm_entry_s {
    void *key;
    void *elem;
    uint64_t hash;
} ghm_entry_t;

struct ghm_s {
    ghm_index_t *indices;
    ghm_entry_t *entries;

    size_t nentries;
    size_t used;
    size_t deleted;
    size_t log2_size;
    size_t size;

    g_cmp_f key_cmp;
    ghm_hash_f key_hash;
    g_free_f key_free;
    g_free_f elem_free;

    size_t key_size;
    size_t elem_size;
};

ghm_t *ghm_create(size_t key_size,
                  size_t elem_size,
                  g_cmp_f key_cmp,
                  ghm_hash_f key_hash,
                  g_free_f key_free,
                  g_free_f elem_free) {
    if (key_size == 0 || elem_size == 0 || key_cmp == nullptr)
        return nullptr;

    ghm_t *const hm = malloc(sizeof(ghm_t));
    if (hm == nullptr)
        goto CLEANUP;

    hm->indices = calloc(GHM_DEFAULT_SIZE, sizeof(ghm_index_t));
    if (hm->indices == nullptr)
        goto CLEANUP;

    hm->entries = calloc(GHM_DEFAULT_SIZE, sizeof(ghm_entry_t));
    if (hm->entries == nullptr)
        goto CLEANUP;

    hm->nentries = 0;
    hm->used = 0;
    hm->deleted = 0;
    hm->size = GHM_DEFAULT_SIZE;

    hm->key_size = key_size;
    hm->elem_size = elem_size;

    hm->key_cmp = key_cmp;
    hm->key_hash = key_hash;
    hm->key_free = key_free;
    hm->elem_free = elem_free;

    return hm;

CLEANUP:
    if (hm != nullptr) {
        if (hm->indices != nullptr)
            free(hm->indices);
        free(hm);
    }
    return nullptr;
}

void ghm_free(ghm_t *hm) {
    if (hm == nullptr)
        return;

    const g_free_f key_free = hm->key_free != nullptr ? hm->key_free : free;
    const g_free_f elem_free = hm->elem_free != nullptr ? hm->elem_free : free;

    for (size_t i = 0; i < hm->size; i++) {
        const ghm_index_t index = hm->indices[i];
        if (index.state == GHM_OCCUPIED) {
            const ghm_entry_t entry = hm->entries[index.value];
            key_free(entry.key);
            elem_free(entry.elem);
        }
    }

    free(hm->indices);
    free(hm->entries);
    free(hm);
}

static size_t ghm_get_index(const ghm_t *hm, uint64_t hash) {
    const size_t mask = hm->size - 1;
    size_t index = 0;
    for (size_t i = 0; i < hm->size; i++) {
        index = (hash + i) & mask;
        if (hm->indices[index].state == GHM_FREE)
            break;
    }

    return index;
}

int ghm_insert(ghm_t *hm, const void *key, const void *elem) {
    if (hm == nullptr || key == nullptr || elem == nullptr)
        return GHM_EINVAL;

    const uint64_t hash =
        hm->key_hash != nullptr ? hm->key_hash(key) : fnv1a(key, hm->key_size);
    const size_t index = ghm_get_index(hm, hash);

    return GHM_OK;
}
