#include "generic_hashmap.h"
#include "hash.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// REMOVE
// REMOVE
// REMOVE
// REMOVE
// REMOVE
#include <stdio.h>
// REMOVE
// REMOVE
// REMOVE
// REMOVE
// REMOVE

typedef enum ghm_index_state {
    GHM_INDEX_OCCUPIED = 1,
    GHM_INDEX_FREE = 0,
    GHM_INDEX_DELETED = -1
} ghm_index_state;

typedef struct ghm_index_s {
    size_t value;
    ghm_index_state state;
} ghm_index_t;

typedef struct ghm_entry_s {
    uint64_t hash;
    uint8_t data[];
} ghm_entry_t;

struct ghm_s {
    g_functions_t key_fn;
    g_functions_t value_fn;

    ghm_index_t *indices;
    ghm_entry_t *entries;

    size_t nentries;
    size_t used; // ghm_size returns used
    size_t capacity;

    size_t key_size;
    size_t value_size;
    size_t value_offset;
    size_t entry_size;
};

#define GHM_DEFAULT_CAPACITY ((size_t)0x10)
#define GHM_MAX_LOAD(n)      (((n) << 1) / 3)

#define GHM_OVERLOADED(hm) ((hm)->nentries > GHM_MAX_LOAD((hm)->capacity))

#define GHM_ENTRY_PTR_FROM_INDEX(hm, index)                                    \
    ((ghm_entry_t *)(((uint8_t *)((hm)->entries))                              \
                     + ((hm)->indices[(index)].value * (hm)->entry_size)))

#define GHM_ENTRY_PTR(hm, entry_index)                                         \
    ((ghm_entry_t *)(((uint8_t *)((hm)->entries))                              \
                     + ((entry_index) * (hm)->entry_size)))

static inline size_t ghm_get_value_offset(size_t key_size, size_t value_size) {
    size_t n = 0;
    if (value_size < alignof(max_align_t)) {
        n = value_size;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        n |= n >> 32;
        n >>= 1;
    } else
        n = alignof(max_align_t) - 1;

    return key_size + (key_size & n);
}

static inline size_t ghm_get_entry_size(size_t value_size,
                                        size_t value_offset) {
    const size_t aux = sizeof(ghm_entry_t) + value_size + value_offset;
    return aux + (aux & (alignof(ghm_entry_t) - 1));
}

static inline void *ghm_key(const ghm_t *hm, ghm_entry_t *entry) {
    (void)hm;
    return entry->data;
}

static inline void *ghm_value(const ghm_t *hm, ghm_entry_t *entry) {
    return entry->data + hm->value_offset;
}

ghm_t *ghm_create(size_t key_size,
                  size_t value_size,
                  g_functions_t key_fn,
                  g_functions_t value_fn) {
    if (key_size == 0 || value_size == 0 || key_fn.cmp == nullptr)
        return nullptr;

    ghm_t *const hm = malloc(sizeof(ghm_t));
    if (hm == nullptr)
        goto CLEANUP;

    hm->indices = calloc(GHM_DEFAULT_CAPACITY, sizeof(ghm_index_t));
    if (hm->indices == nullptr)
        goto CLEANUP;

    const size_t value_offset = ghm_get_value_offset(key_size, value_size);
    const size_t entry_size = ghm_get_entry_size(value_size, value_offset);
    hm->entries = calloc(GHM_DEFAULT_CAPACITY, entry_size);
    if (hm->entries == nullptr)
        goto CLEANUP;

    hm->nentries = 0;
    hm->used = 0;
    hm->capacity = GHM_DEFAULT_CAPACITY;

    hm->key_size = key_size;
    hm->value_size = value_size;
    hm->value_offset = value_offset;
    hm->entry_size = entry_size;

    hm->key_fn = key_fn;
    hm->value_fn = value_fn;

    // printf("Key size: %lu\n", key_size);
    // printf("Value size: %lu\n", value_size);
    // printf("Value offset: %lu\n", value_offset);
    // printf("Entry size: %lu\n", entry_size);

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

    const g_free_f key_free = hm->key_fn.free;
    const g_free_f value_free = hm->value_fn.free;

    if (hm->used > 0 && (key_free != nullptr || value_free != nullptr)) {
        for (size_t i = 0; i < hm->capacity; i++) {
            const ghm_index_t entry_index = hm->indices[i];
            if (entry_index.state == GHM_INDEX_OCCUPIED) {
                ghm_entry_t *const entry = GHM_ENTRY_PTR(hm, entry_index.value);
                if (key_free != nullptr)
                    key_free(*((void **)ghm_key(hm, entry)));
                if (value_free != nullptr)
                    value_free(*((void **)ghm_value(hm, entry)));
            }
        }
    }

    free(hm->indices);
    free(hm->entries);
    free(hm);
}

// RETURN used
size_t ghm_size(const ghm_t *hm) {
    return hm->used;
}

typedef struct ghm_hash_to_index_result {
    size_t index;
    int ret_code;
} ghm_hash_to_index_result;

typedef enum ghm_hash_to_index_op {
    GHM_HTI_INSERT_VALUE,
    GHM_HTI_GET_VALUE
} ghm_hash_to_index_op;

// Quadratic probing with trangular numbers
static ghm_hash_to_index_result ghm_hash_to_index(const ghm_t *hm,
                                                  uint64_t hash,
                                                  ghm_hash_to_index_op op) {
    const size_t mask = hm->capacity - 1;
    ghm_hash_to_index_result res = {0};
    uint64_t probe = hash;
    // printf("Hash to index\nHash: %016lx\n", hash);
    for (size_t i = 0; i < hm->capacity; i++) {
        probe += i;
        res.index = probe & mask;
        const ghm_index_t entry_index = hm->indices[res.index];
        if (entry_index.state == GHM_INDEX_OCCUPIED) {
            if (GHM_ENTRY_PTR(hm, entry_index.value)->hash == hash) {
                // printf("Occupied\nHash: %016lx\nIndex: %lu\n", hash,
                // res.index);
                res.ret_code = G_EDUP;
                return res;
            }
        } else if (op == GHM_HTI_INSERT_VALUE) {
            // printf("Insert\nHash: %016lx\nIndex: %lu\n", hash, res.index);
            return res;
        }
    }

    // printf("Not found\nHash: %016lx\nIndex: %lu\n", hash, res.index);
    res.ret_code = G_ENOTFOUND;
    return res;
}

static int ghm_expand(ghm_t *hm) {
    const size_t old_capacity = hm->capacity;
    const size_t new_capacity = hm->capacity * 2;

    ghm_entry_t *const new_entries = calloc(new_capacity, hm->entry_size);
    if (new_entries == nullptr)
        goto CLEANUP;

    ghm_index_t *const new_indices = calloc(new_capacity, sizeof(ghm_index_t));
    if (new_indices == nullptr)
        goto CLEANUP;

    ghm_index_t *const old_indices = hm->indices;
    ghm_entry_t *const old_entries = hm->entries;

    hm->indices = new_indices;
    hm->entries = new_entries;
    hm->capacity = new_capacity;

    size_t entry_index = 0;
    // printf("Expand\n");
    for (size_t i = 0; i < old_capacity; i++) {
        if (old_indices[i].state == GHM_INDEX_OCCUPIED) {
            const ghm_entry_t *const
                entry = (ghm_entry_t *)(((uint8_t *)old_entries)
                                        + (old_indices[i].value
                                           * hm->entry_size));
            ghm_entry_t *const new_entry = GHM_ENTRY_PTR(hm, entry_index);

            memcpy(new_entry, entry, hm->entry_size);

            const ghm_hash_to_index_result res = ghm_hash_to_index(
                hm,
                new_entry->hash,
                GHM_HTI_INSERT_VALUE);
            new_indices[res.index].value = entry_index;
            new_indices[res.index].state = GHM_INDEX_OCCUPIED;

            // printf("Entry index: %lu\nHash: %016lx\n\n",
            //        entry_index,
            //        new_entry->hash);

            entry_index++;
        }
    }

    hm->nentries = entry_index;
    hm->used = entry_index;

    free(old_indices);
    free(old_entries);
    return G_OK;

CLEANUP:
    if (new_entries != nullptr)
        free(new_entries);

    return G_ENOMEM;
}

static void ghm_append(ghm_t *hm,
                       const void *key,
                       const void *value,
                       uint64_t hash,
                       size_t index) {
    hm->indices[index].value = hm->nentries;
    hm->indices[index].state = GHM_INDEX_OCCUPIED;

    ghm_entry_t *const new = GHM_ENTRY_PTR(hm, hm->nentries);

    new->hash = hash;

    memcpy(ghm_key(hm, new), key, hm->key_size);
    memcpy(ghm_value(hm, new), value, hm->value_size);

    hm->nentries++;
    hm->used++;

    // printf("Append\nHash: %016lx\n\n", new->hash);
}

static void ghm_set(ghm_t *hm, const void *value, size_t index) {
    void *const aux = ghm_value(hm, GHM_ENTRY_PTR_FROM_INDEX(hm, index));

    if (hm->value_fn.free != nullptr)
        hm->value_fn.free(*((void **)aux));

    memcpy(aux, value, hm->value_size);
}

static inline uint64_t ghm_hash(const ghm_t *hm, const void *key) {
    return hm->key_fn.hash == nullptr ? fnv1a(key, hm->key_size)
                                      : hm->key_fn.hash(key);
}

int ghm_insert(ghm_t *hm, const void *key, const void *value) {
    if (hm == nullptr || key == nullptr || value == nullptr)
        return G_EINVAL;

    if (GHM_OVERLOADED(hm) && ghm_expand(hm) == G_ENOMEM)
        return G_ENOMEM;

    const uint64_t hash = ghm_hash(hm, key);
    const ghm_hash_to_index_result res = ghm_hash_to_index(
        hm,
        hash,
        GHM_HTI_INSERT_VALUE);

    assert(res.ret_code != G_ENOTFOUND && "ghm_insert: G_ENOTFOUND");

    if (res.ret_code == G_EDUP)
        ghm_set(hm, value, res.index);
    else
        ghm_append(hm, key, value, hash, res.index);

    return G_OK;
}

int ghm_delete(ghm_t *hm, const void *key) {
    if (hm == nullptr || key == nullptr)
        return G_EINVAL;

    const uint64_t hash = ghm_hash(hm, key);
    const ghm_hash_to_index_result res = ghm_hash_to_index(hm,
                                                           hash,
                                                           GHM_HTI_GET_VALUE);
    if (res.ret_code == G_ENOTFOUND)
        return G_ENOTFOUND;

    ghm_entry_t *const del = GHM_ENTRY_PTR_FROM_INDEX(hm, res.index);

    if (hm->key_fn.free != nullptr)
        hm->key_fn.free(*(void **)ghm_key(hm, del));

    if (hm->value_fn.free != nullptr)
        hm->value_fn.free(*(void **)ghm_value(hm, del));

    memset(del, 0, hm->entry_size);

    hm->indices[res.index].state = GHM_INDEX_DELETED;
    hm->used--;

    return G_OK;
}

int ghm_get(const ghm_t *hm, const void *key, void *out) {
    if (hm == nullptr || key == nullptr)
        return G_EINVAL;

    const uint64_t hash = ghm_hash(hm, key);
    const ghm_hash_to_index_result res = ghm_hash_to_index(hm,
                                                           hash,
                                                           GHM_HTI_GET_VALUE);
    if (res.ret_code == G_ENOTFOUND)
        return G_ENOTFOUND;

    memcpy(out,
           ghm_value(hm, GHM_ENTRY_PTR_FROM_INDEX(hm, res.index)),
           hm->value_size);

    return G_OK;
}
