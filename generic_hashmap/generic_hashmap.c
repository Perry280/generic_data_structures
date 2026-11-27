#include "generic_hashmap.h"
#include "hash.h"
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
    bool deleted;
    alignas(max_align_t) uint8_t data[];
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
#define GHM_MAX_CAPACITY(entry_size)                                           \
    ((size_t)(((size_t)PTRDIFF_MAX) / (entry_size)))

#define GHM_MAX_LOAD(capacity) (((capacity) << 1) / 3)
#define GHM_OVERLOADED(hm)     ((hm)->nentries > GHM_MAX_LOAD((hm)->capacity))
#define GHM_EXPAND_THRESHOLD(nentries) ((nentries) >> 1)

#define GHM_ENTRY_PTR(hm, entry_index)                                         \
    ((ghm_entry_t *)(((uint8_t *)((hm)->entries))                              \
                     + ((entry_index) * (hm)->entry_size)))

#define GHM_ENTRY_PTR_FROM_INDEX(hm, index)                                    \
    GHM_ENTRY_PTR((hm), (hm)->indices[(index)].value)

static inline size_t ghm_get_value_offset(size_t key_size, size_t value_size) {
    size_t alignment = 0;
    if (value_size < alignof(max_align_t)) {
        alignment = value_size;
        alignment |= alignment >> 1;
        alignment |= alignment >> 2;
        alignment |= alignment >> 4;
        alignment |= alignment >> 8;
        alignment |= alignment >> 16;
#if SIZE_MAX > UINT32_MAX
        alignment |= alignment >> 32;
#endif
        alignment >>= 1;
    } else
        alignment = alignof(max_align_t) - 1;

    return (key_size + alignment) & ~alignment; // round up
}

static inline size_t ghm_get_entry_size(size_t value_size,
                                        size_t value_offset) {
    const size_t size = sizeof(ghm_entry_t) + value_size + value_offset;
    const size_t alignment = alignof(ghm_entry_t) - 1;
    return (size + alignment) & ~alignment; // round up
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
    if (key_size == 0 || value_size == 0 || key_fn.cmp == nullptr
        || (key_fn.free != nullptr
            && (key_fn.hash == nullptr || key_size != sizeof(void *)))
        || (value_fn.free != nullptr && value_size != sizeof(void *)))
        return nullptr;

    ghm_t *const hm = malloc(sizeof(ghm_t));
    if (hm == nullptr)
        goto CLEANUP;

    hm->indices = calloc(GHM_DEFAULT_CAPACITY, sizeof(ghm_index_t));
    if (hm->indices == nullptr)
        goto CLEANUP;

    hm->value_offset = ghm_get_value_offset(key_size, value_size);
    hm->entry_size = ghm_get_entry_size(value_size, hm->value_offset);
    hm->entries = calloc(GHM_DEFAULT_CAPACITY, hm->entry_size);
    if (hm->entries == nullptr)
        goto CLEANUP;

    hm->nentries = 0;
    hm->used = 0;
    hm->capacity = GHM_DEFAULT_CAPACITY;

    hm->key_size = key_size;
    hm->value_size = value_size;

    hm->key_fn = key_fn;
    hm->value_fn = value_fn;

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

            if (entry_index.state != GHM_INDEX_OCCUPIED)
                continue;

            ghm_entry_t *const entry = GHM_ENTRY_PTR(hm, entry_index.value);
            if (key_free != nullptr)
                key_free(*((void **)ghm_key(hm, entry)));
            if (value_free != nullptr)
                value_free(*((void **)ghm_value(hm, entry)));
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

typedef struct ghm_lookup_result {
    size_t index;
    int ret_code;
} ghm_lookup_result;

typedef enum ghm_lookup_op {
    GHM_LOOKUP_INSERT,
    GHM_LOOKUP_GET
} ghm_lookup_op;

/* Quadratic probing with trangular numbers
 * There always exists at least one free spot,
 * so it can never return G_EINVAL as long as the hash map
 * has only been manipulated with the library functions.
 *
 * If `op == GHM_LOOKUP_INSERT`, return value is either `G_OK` or `G_EDUP`.
 * If `op == GHM_LOOKUP_GET`, return value is either `G_EDUP` or `G_ENOTFOUND`.
 */
static ghm_lookup_result ghm_lookup(const ghm_t *hm,
                                    uint64_t hash,
                                    const void *key,
                                    ghm_lookup_op op) {
    const size_t mask = hm->capacity - 1;
    ghm_lookup_result res = {0};
    uint64_t probe = hash;

    bool tombstone_found = false;
    size_t first_tombstone_found = 0;

    for (size_t i = 0; i < hm->capacity; i++) {
        probe += i;
        res.index = probe & mask;
        const ghm_index_t entry_index = hm->indices[res.index];

        switch (entry_index.state) {
            case GHM_INDEX_OCCUPIED:
                ghm_entry_t *const entry = GHM_ENTRY_PTR(hm, entry_index.value);

                if (entry->hash == hash
                    && hm->key_fn.cmp(ghm_key(hm, entry), key) == 0) {
                    res.ret_code = G_EDUP;
                    return res;
                }
                break;
            case GHM_INDEX_FREE:
                if (op == GHM_LOOKUP_GET)
                    res.ret_code = G_ENOTFOUND;
                else if (tombstone_found)
                    res.index = first_tombstone_found;
                return res;
            case GHM_INDEX_DELETED:
                if (op == GHM_LOOKUP_INSERT && !tombstone_found) {
                    tombstone_found = true;
                    first_tombstone_found = res.index;
                }
                break;
        }
    }

    res.ret_code = G_EINVAL;
    return res;
}

static int ghm_rehash(ghm_t *hm) {
    const size_t new_capacity = hm->used >= GHM_EXPAND_THRESHOLD(hm->nentries)
                                    ? hm->capacity * 2
                                    : hm->capacity;

    if (new_capacity > GHM_MAX_CAPACITY(hm->entry_size))
        return G_ENOMEM;

    ghm_entry_t *const new_entries = calloc(new_capacity, hm->entry_size);
    if (new_entries == nullptr)
        goto CLEANUP;

    ghm_index_t *const new_indices = calloc(new_capacity, sizeof(ghm_index_t));
    if (new_indices == nullptr)
        goto CLEANUP;

    const size_t old_nentries = hm->nentries;
    ghm_index_t *const old_indices = hm->indices;
    ghm_entry_t *const old_entries = hm->entries;

    hm->indices = new_indices;
    hm->entries = new_entries;
    hm->capacity = new_capacity;

    size_t new_nentries = 0;
    for (size_t i = 0; i < old_nentries; i++) {
        const ghm_entry_t *const
            old_entry = (ghm_entry_t *)(((uint8_t *)old_entries)
                                        + (i * hm->entry_size));
        if (old_entry->deleted)
            continue;

        ghm_entry_t *const new_entry = GHM_ENTRY_PTR(hm, new_nentries);
        memcpy(new_entry, old_entry, hm->entry_size);

        const ghm_lookup_result res = ghm_lookup(hm,
                                                 new_entry->hash,
                                                 ghm_key(hm, new_entry),
                                                 GHM_LOOKUP_INSERT);
        new_indices[res.index].value = new_nentries;
        new_indices[res.index].state = GHM_INDEX_OCCUPIED;

        new_nentries++;
    }

    hm->nentries = new_nentries;
    hm->used = new_nentries;

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

    const uint64_t hash = ghm_hash(hm, key);
    ghm_lookup_result res = ghm_lookup(hm, hash, key, GHM_LOOKUP_INSERT);

    // res.ret_code cannot be G_ENOTFOUND here

    if (res.ret_code == G_EINVAL)
        return G_EINVAL;

    if (res.ret_code == G_OK) {
        if (GHM_OVERLOADED(hm)) {
            if (ghm_rehash(hm) == G_ENOMEM)
                return G_ENOMEM;
            res = ghm_lookup(hm, hash, key, GHM_LOOKUP_INSERT);
            if (res.ret_code == G_EINVAL)
                return G_EINVAL;
        }
        ghm_append(hm, key, value, hash, res.index);
    } else
        ghm_set(hm, value, res.index);

    return res.ret_code;
}

int ghm_delete(ghm_t *hm, const void *key) {
    if (hm == nullptr || key == nullptr)
        return G_EINVAL;

    const ghm_lookup_result res = ghm_lookup(hm,
                                             ghm_hash(hm, key),
                                             key,
                                             GHM_LOOKUP_GET);
    if (res.ret_code == G_EINVAL || res.ret_code == G_ENOTFOUND)
        return res.ret_code;

    ghm_entry_t *const del = GHM_ENTRY_PTR_FROM_INDEX(hm, res.index);

    if (hm->key_fn.free != nullptr
        && *((void **)ghm_key(hm, del)) != *((const void **)key))
        hm->key_fn.free(*((void **)ghm_key(hm, del)));

    if (hm->value_fn.free != nullptr)
        hm->value_fn.free(*((void **)ghm_value(hm, del)));

    memset(del, 0, hm->entry_size);
    del->deleted = true;

    hm->indices[res.index].state = GHM_INDEX_DELETED;
    hm->used--;

    return G_OK;
}

int ghm_get(const ghm_t *hm, const void *key, void *out) {
    if (hm == nullptr || key == nullptr || out == nullptr)
        return G_EINVAL;

    const ghm_lookup_result res = ghm_lookup(hm,
                                             ghm_hash(hm, key),
                                             key,
                                             GHM_LOOKUP_GET);

    if (res.ret_code == G_EINVAL || res.ret_code == G_ENOTFOUND)
        return res.ret_code;

    memcpy(out,
           ghm_value(hm, GHM_ENTRY_PTR_FROM_INDEX(hm, res.index)),
           hm->value_size);

    return G_OK;
}
