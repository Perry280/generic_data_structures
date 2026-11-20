#include "generic_hashmap.h"
#include "hash.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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
    void *key;
    void *elem;
    uint64_t hash;
} ghm_entry_t;

struct ghm_s {
    ghm_index_t *indices;
    ghm_entry_t *entries;

    size_t nentries;
    size_t used; // ghm_size returns used
    size_t deleted;
    size_t capacity;

    g_cmp_f key_cmp;
    ghm_hash_f key_hash;
    g_free_f key_free;
    g_free_f elem_free;

    size_t key_size;
    size_t elem_size;
};

#define GHM_DEFAULT_CAPACITY ((size_t)0x10)
#define GHM_MAX_LOAD(n)      (((n) << 1) / 3)
#define GHM_OVERLOADED(hm)                                                     \
    (((hm)->nentries) / ((hm)->capacity)) > GHM_MAX_LOAD((hm)->capacity)
#define GHM_GET_ENTRY(hm, index) ((hm)->entries[(hm)->indices[(index)].value])

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

    hm->indices = calloc(GHM_DEFAULT_CAPACITY, sizeof(ghm_index_t));
    if (hm->indices == nullptr)
        goto CLEANUP;

    hm->entries = calloc(GHM_DEFAULT_CAPACITY, sizeof(ghm_entry_t));
    if (hm->entries == nullptr)
        goto CLEANUP;

    hm->nentries = 0;
    hm->used = 0;
    hm->deleted = 0;
    hm->capacity = GHM_DEFAULT_CAPACITY;

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

    for (size_t i = 0; i < hm->capacity; i++) {
        const ghm_index_t entry_index = hm->indices[i];
        if (entry_index.state == GHM_INDEX_OCCUPIED) {
            const ghm_entry_t entry = hm->entries[entry_index.value];
            key_free(entry.key);
            elem_free(entry.elem);
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
    GHM_HTI_INSERT_ELEM,
    GHM_HTI_GET_ELEM
} ghm_hash_to_index_op;

// Quadratic probing with trangular numbers
static ghm_hash_to_index_result ghm_hash_to_index(const ghm_t *hm,
                                                  uint64_t hash,
                                                  ghm_hash_to_index_op op) {
    const size_t mask = hm->capacity - 1;
    ghm_hash_to_index_result res = {0};
    uint64_t probe = hash;
    size_t i = 0;
    for (; i < hm->capacity; i++) {
        probe += i;
        res.index = probe & mask;
        const ghm_index_t entry_index = hm->indices[res.index];
        if (entry_index.state == GHM_INDEX_OCCUPIED) {
            if (hm->entries[entry_index.value].hash == hash) {
                res.ret_code = G_EDUP;
                return res;
            }
        } else if (op == GHM_HTI_INSERT_ELEM)
            return res;
    }

    res.ret_code = G_ENOTFOUND;
    return res;
}

static int ghm_expand(ghm_t *hm) {
    const size_t old_size = hm->capacity;
    const size_t new_size = hm->capacity * 2;

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
    hm->capacity = new_size;

    size_t entry_index = 0;
    for (size_t i = 0; i < old_size; i++) {
        const ghm_entry_t entry = old_entries[i];
        if (entry.key != nullptr) {
            new_entries[entry_index] = entry;

            const ghm_hash_to_index_result res = ghm_hash_to_index(
                hm,
                entry.hash,
                GHM_HTI_INSERT_ELEM);
            new_indices[res.index].value = entry_index;
            new_indices[res.index].state = GHM_INDEX_OCCUPIED;

            entry_index++;
        }
    }

    hm->nentries = entry_index;
    hm->used = entry_index;
    hm->deleted = 0;

    free(old_indices);
    free(old_entries);
    return G_OK;

CLEANUP:
    if (new_entries != nullptr)
        free(new_entries);

    return G_ENOMEM;
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

    hm->entries[hm->nentries] = new;
    hm->nentries++;
    hm->used++;

    return G_OK;

CLEANUP:
    if (new.key != nullptr)
        free(new.key);
    return G_ENOMEM;
}

static int ghm_set(ghm_t *hm, const void *elem, size_t index) {
    void *const old = GHM_GET_ENTRY(hm, index).elem;

    void *const new = malloc(hm->elem_size);
    if (new == nullptr)
        return G_ENOMEM;

    if (hm->elem_free == nullptr)
        free(old);
    else
        hm->elem_free(old);

    memcpy(new, elem, hm->elem_size);
    GHM_GET_ENTRY(hm, index).elem = new;

    return G_OK;
}

int ghm_insert(ghm_t *hm, const void *key, const void *elem) {
    if (hm == nullptr || key == nullptr || elem == nullptr)
        return G_EINVAL;

    if (GHM_OVERLOADED(hm) && ghm_expand(hm) == G_ENOMEM)
        return G_ENOMEM;

    const uint64_t hash = hm->key_hash == nullptr ? fnv1a(key, hm->key_size)
                                                  : hm->key_hash(key);

    const ghm_hash_to_index_result res = ghm_hash_to_index(hm,
                                                           hash,
                                                           GHM_HTI_INSERT_ELEM);
    assert(res.ret_code != G_ENOTFOUND && "ghm_insert: G_ENOTFOUND");

    if (res.ret_code == G_EDUP) {
        if (ghm_set(hm, elem, res.index) == G_ENOMEM)
            return G_ENOMEM;
    } else if (ghm_append(hm, key, elem, hash, res.index) == G_ENOMEM)
        return G_ENOMEM;

    return G_OK;
}

int ghm_delete(ghm_t *hm, const void *key) {
    if (hm == nullptr || key == nullptr)
        return G_EINVAL;

    const uint64_t hash = hm->key_hash == nullptr ? fnv1a(key, hm->key_size)
                                                  : hm->key_hash(key);

    const ghm_hash_to_index_result res = ghm_hash_to_index(hm,
                                                           hash,
                                                           GHM_HTI_GET_ELEM);
    if (res.ret_code == G_ENOTFOUND)
        return G_ENOTFOUND;

    const size_t entry_index = hm->indices[res.index].value;
    const ghm_entry_t del = hm->entries[entry_index];

    if (hm->key_free == nullptr)
        free(del.key);
    else
        hm->key_free(del.key);

    if (hm->elem_free == nullptr)
        free(del.elem);
    else
        hm->elem_free(del.elem);

    hm->entries[entry_index].key = nullptr;
    hm->entries[entry_index].elem = nullptr;
    hm->entries[entry_index].hash = 0;

    hm->indices[res.index].state = GHM_INDEX_DELETED;

    hm->deleted++;
    hm->used--;

    return G_OK;
}

int ghm_get(const ghm_t *hm, const void *key, void *out) {
    if (hm == nullptr || key == nullptr)
        return G_EINVAL;

    const uint64_t hash = hm->key_hash == nullptr ? fnv1a(key, hm->key_size)
                                                  : hm->key_hash(key);

    const ghm_hash_to_index_result res = ghm_hash_to_index(hm,
                                                           hash,
                                                           GHM_HTI_GET_ELEM);
    if (res.ret_code == G_ENOTFOUND)
        return G_ENOTFOUND;

    memcpy(out, GHM_GET_ENTRY(hm, res.index).elem, hm->elem_size);

    return G_OK;
}
