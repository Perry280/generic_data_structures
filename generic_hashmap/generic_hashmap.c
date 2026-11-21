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
    uint64_t hash;
    void *key;
    void *elem;
    alignas(max_align_t) uint8_t data[];
} ghm_entry_t;

struct ghm_s {
    g_fun_t key_fun;
    g_fun_t elem_fun;

    ghm_index_t *indices;
    ghm_entry_t *entries;

    size_t nentries;
    size_t used; // ghm_size returns used
    size_t deleted;
    size_t capacity;

    size_t key_size;
    size_t elem_size;
    size_t entry_size;
};

#define GHM_DEFAULT_CAPACITY ((size_t)0x10)
#define GHM_MAX_LOAD(n)      (((n) << 1) / 3)

#define GHM_OVERLOADED(hm)                                                     \
    ((((hm)->nentries) / ((hm)->capacity)) > GHM_MAX_LOAD((hm)->capacity))

#define GHM_GET_ENTRY_LONG(hm, index)                                          \
    ((ghm_entry_t *)(((uint8_t *)((hm)->entries))                              \
                     + ((hm)->indices[(index)].value * (hm)->entry_size)))

#define GHM_GET_ENTRY_SHORT(hm, entry_index)                                   \
    ((ghm_entry_t *)(((uint8_t *)((hm)->entries))                              \
                     + ((entry_index) * (hm)->entry_size)))

ghm_t *ghm_create(size_t key_size,
                  size_t elem_size,
                  g_fun_t key_fun,
                  g_fun_t elem_fun) {
    if (key_size == 0 || elem_size == 0 || key_fun.cmp == nullptr)
        return nullptr;

    ghm_t *const hm = malloc(sizeof(ghm_t));
    if (hm == nullptr)
        goto CLEANUP;

    hm->indices = calloc(GHM_DEFAULT_CAPACITY, sizeof(ghm_index_t));
    if (hm->indices == nullptr)
        goto CLEANUP;

    const size_t entry_size = sizeof(ghm_entry_t) + key_size + elem_size;
    hm->entries = calloc(GHM_DEFAULT_CAPACITY, entry_size);
    if (hm->entries == nullptr)
        goto CLEANUP;

    hm->nentries = 0;
    hm->used = 0;
    hm->deleted = 0;
    hm->capacity = GHM_DEFAULT_CAPACITY;

    hm->key_size = key_size;
    hm->elem_size = elem_size;
    hm->entry_size = entry_size;

    hm->key_fun = key_fun;
    hm->elem_fun = elem_fun;

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

    const g_free_f key_free = hm->key_fun.free;
    const g_free_f elem_free = hm->elem_fun.free;

    if (hm->used > 0 && (key_free != nullptr || elem_free != nullptr)) {
        for (size_t i = 0; i < hm->capacity; i++) {
            const ghm_index_t entry_index = hm->indices[i];
            if (entry_index.state == GHM_INDEX_OCCUPIED) {
                const ghm_entry_t entry = hm->entries[entry_index.value];
                if (key_free != nullptr)
                    key_free(entry.key);
                if (elem_free != nullptr)
                    elem_free(entry.elem);
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
            if (GHM_GET_ENTRY_SHORT(hm, entry_index.value)->hash == hash) {
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
    for (size_t i = 0; i < old_capacity; i++) {
        const ghm_entry_t entry = *(ghm_entry_t *)(((uint8_t *)old_entries)
                                                   + (i * hm->entry_size));
        if (entry.key != nullptr) {
            *GHM_GET_ENTRY_SHORT(hm, entry_index) = entry;

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

static void ghm_append(ghm_t *hm,
                       const void *key,
                       const void *elem,
                       uint64_t hash,
                       size_t index) {
    hm->indices[index].value = hm->nentries;
    hm->indices[index].state = GHM_INDEX_OCCUPIED;

    ghm_entry_t *const new = GHM_GET_ENTRY_LONG(hm, hm->nentries);

    new->hash = hash;
    new->key = new->data;
    new->elem = new->data + hm->key_size;

    memcpy(new->key, key, hm->key_size);
    memcpy(new->elem, elem, hm->elem_size);

    hm->nentries++;
    hm->used++;
}

static void ghm_set(ghm_t *hm, const void *elem, size_t index) {
    void *const aux = GHM_GET_ENTRY_LONG(hm, index)->elem;

    if (hm->elem_fun.free != nullptr)
        hm->elem_fun.free(aux);

    memcpy(aux, elem, hm->elem_size);
}

static uint64_t ghm_hash(const ghm_t *hm, const void *key) {
    return hm->key_fun.hash == nullptr ? fnv1a(key, hm->key_size)
                                       : hm->key_fun.hash(key);
}

int ghm_insert(ghm_t *hm, const void *key, const void *elem) {
    if (hm == nullptr || key == nullptr || elem == nullptr)
        return G_EINVAL;

    if (GHM_OVERLOADED(hm) && ghm_expand(hm) == G_ENOMEM)
        return G_ENOMEM;

    const uint64_t hash = ghm_hash(hm, key);
    const ghm_hash_to_index_result res = ghm_hash_to_index(hm,
                                                           hash,
                                                           GHM_HTI_INSERT_ELEM);
    assert(res.ret_code != G_ENOTFOUND && "ghm_insert: G_ENOTFOUND");

    if (res.ret_code == G_EDUP)
        ghm_set(hm, elem, res.index);
    else
        ghm_append(hm, key, elem, hash, res.index);

    return G_OK;
}

int ghm_delete(ghm_t *hm, const void *key) {
    if (hm == nullptr || key == nullptr)
        return G_EINVAL;

    const uint64_t hash = ghm_hash(hm, key);
    const ghm_hash_to_index_result res = ghm_hash_to_index(hm,
                                                           hash,
                                                           GHM_HTI_GET_ELEM);
    if (res.ret_code == G_ENOTFOUND)
        return G_ENOTFOUND;

    ghm_entry_t *const del = GHM_GET_ENTRY_LONG(hm, res.index);

    if (hm->key_fun.free != nullptr)
        hm->key_fun.free(del->key);

    if (hm->elem_fun.free != nullptr)
        hm->elem_fun.free(del->elem);

    memset(del, 0, hm->entry_size);

    hm->indices[res.index].state = GHM_INDEX_DELETED;

    hm->deleted++;
    hm->used--;

    return G_OK;
}

int ghm_get(const ghm_t *hm, const void *key, void *out) {
    if (hm == nullptr || key == nullptr)
        return G_EINVAL;

    const uint64_t hash = ghm_hash(hm, key);
    const ghm_hash_to_index_result res = ghm_hash_to_index(hm,
                                                           hash,
                                                           GHM_HTI_GET_ELEM);
    if (res.ret_code == G_ENOTFOUND)
        return G_ENOTFOUND;

    memcpy(out, GHM_GET_ENTRY_LONG(hm, res.index)->elem, hm->elem_size);

    return G_OK;
}
