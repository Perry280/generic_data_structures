#include "generic_hashmap.h"
#include "hash.h"
#include <stdint.h>
#include <stdlib.h>

#define GHM_DEFAULT_SIZE ((size_t)0x10)
#define GHM_MAX_LOAD(n)  (((n) << 1) / 3)
#define GHM_OVERLOADED(hm)                                                     \
    (((hm)->nentries) / ((hm)->size)) > GHM_MAX_LOAD((hm)->size)

enum ghm_index_state {
    GHM_INDEX_OCCUPIED = 1,
    GHM_INDEX_FREE = 0,
    GHM_INDEX_DELETED = -1
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
        if (index.state == GHM_INDEX_OCCUPIED) {
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
    uint64_t probe = hash;
    for (size_t i = 0; i < hm->size; i++) {
        probe += i;
        index = probe & mask;
        if (hm->indices[index].state != GHM_INDEX_OCCUPIED)
            break;
    }

    return index;
}

static int ghm_expand(ghm_t *hm) {
    const size_t old_size = hm->size;
    const size_t new_size = hm->size * 2;

    ghm_entry_t *const new_entries = calloc(new_size, sizeof(ghm_entry_t));
    if (new_entries == nullptr)
        goto CLEANUP;

    ghm_index_t *const new_indices = calloc(new_size, sizeof(ghm_index_t));
    if (new_indices == nullptr)
        goto CLEANUP;

    ghm_index_t *const old_indices = hm->indices;
    ghm_entry_t *const old_entries = hm->entries;

    hm->indices = new_indices;
    hm->entries = new_entries;
    hm->size = new_size;

    size_t entry_index = 0;
    for (size_t i = 0; i < old_size; i++) {
        const ghm_entry_t entry = old_entries[i];
        if (entry.key != nullptr) {
            new_entries[entry_index] = entry;

            const size_t index = ghm_get_index(hm, entry.hash);
            new_indices[index].value = entry_index;
            new_indices[index].state = GHM_INDEX_OCCUPIED;

            entry_index++;
        }
    }

    free(old_indices);
    free(old_entries);
    return GHM_OK;

CLEANUP:
    if (new_entries != nullptr)
        free(new_entries);

    return GHM_ENOMEM;
}

static int ghm_append(ghm_t *hm,
                      const void *key,
                      const void *elem,
                      uint64_t hash,
                      size_t index) {
    ghm_entry_t new = {0};
    new.key = malloc(hm->key_size);
    if (new.key == nullptr)
        goto CLEANUP;

    new.elem = malloc(hm->elem_size);
    if (new.elem == nullptr)
        goto CLEANUP;

    hm->indices[index].value = hm->nentries;
    hm->indices[index].state = GHM_INDEX_OCCUPIED;

    new.hash = hash;
    memcpy(new.key, key, hm->key_size);
    memcpy(new.elem, elem, hm->elem_size);

    hm->entries[index] = new;

    return GHM_OK;

CLEANUP:
    if (new.key != nullptr) {
        if (hm->key_free == nullptr)
            free(new.key);
        else
            hm->key_free(new.key);
    }
    return GHM_ENOMEM;
}

// TODO: check for duplicate keys on insert
int ghm_insert(ghm_t *hm, const void *key, const void *elem) {
    if (hm == nullptr || key == nullptr || elem == nullptr)
        return GHM_EINVAL;

    if (GHM_OVERLOADED(hm)) {
        if (ghm_expand(hm) == GHM_ENOMEM)
            return GHM_ENOMEM;
    }

    const uint64_t hash = hm->key_hash != nullptr ? hm->key_hash(key)
                                                  : fnv1a(key, hm->key_size);

    if (ghm_append(hm, key, elem, hash, ghm_get_index(hm, hash)) == GHM_ENOMEM)
        return GHM_ENOMEM;

    return GHM_OK;
}
