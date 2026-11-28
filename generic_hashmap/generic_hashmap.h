/**
 * @file generic_hashmap.h
 * @brief Generic hash map with open addressing and insertion-ordered storage.
 *
 * Keys and values are stored by value, as raw bytes of a fixed size chosen when
 * the map is created. Every key and value argument of the API is therefore a
 * *pointer to* the object being stored, never the object itself.
 *
 * ### Pointer keys and values
 * If `free` is set in the `g_functions_t` of the key (or of the value), that
 * key (or value) is assumed to be a pointer. In that case:
 *  - its size must be exactly `sizeof(void *)`;
 *  - the argument passed to the API is a pointer to that pointer (a `char **`
 *    for a string key, for example);
 *  - the map owns the pointed-to object once it is stored, and releases it with
 *    `free` (see `ghm_insert()`, `ghm_delete()` and `ghm_free()`).
 *
 * ### Thread safety
 * The map has no internal synchronization. Any concurrent access that includes
 * a writer needs external locking.
 */

#ifndef GENERIC_HASHMAP_H
#define GENERIC_HASHMAP_H

#include "../generic_types.h"
#include <stddef.h>
#include <stdint.h>

/** @brief Opaque hash map type. */
typedef struct ghm_s ghm_t;

/**
 * @brief
 * Creates an empty hash map.
 *
 * @param key_size   Size in bytes of a key. Must be non-zero. Must be
 *                   `sizeof(void *)` if `key_fn.free` is set.
 * @param value_size Size in bytes of a value. Must be non-zero. Must be
 *                   `sizeof(void *)` if `value_fn.free` is set.
 * @param key_fn     Functions used to manage keys. In every call, the
 *                   arguments are pointers to the key as it is passed to the
 *                   API (a pointer to a pointer for pointer keys).
 *                   - `cmp`: **required**. Called as `cmp(stored_key,
 *                     lookup_key)`, and only when the two hashes are equal.
 *                     Returns `0` when the keys are equal.
 *                   - `hash`: optional for plain-data keys, **required for
 *                     pointer keys**. Keys that compare equal must hash
 *                     equal. See the warning below.
 *                   - `free`: optional. If set, keys are assumed to be
 *                     pointers.
 * @param value_fn   Functions used to manage values. Only `free` is used.
 *                   `cmp` and `hash` are ignored. If `free` is set, values
 *                   are assumed to be pointers.
 *
 * @return A new map, to be released with `ghm_free()`, or `nullptr` if:
 *         - `key_size` or `value_size` is `0`;
 *         - `key_fn.cmp` is `nullptr`;
 *         - `key_fn.free` is set and `key_size != sizeof(void *)`;
 *         - `key_fn.free` is set and `key_fn.hash` is `nullptr`;
 *         - `value_fn.free` is set and `value_size != sizeof(void *)`;
 *         - memory allocation fails.
 *
 * @details
 * @warning
 * If the key is a pointer (a `char *` string, for example), a custom hash
 * function **must** be provided, such as `hash_string` from `hash.h`. When
 * `key_fn.hash` is `nullptr` the map hashes the `key_size` raw bytes of the
 * key, which for a pointer key means the pointer's *address* and not the data
 * it points to. Two equal strings at different addresses would then hash
 * differently and lookups would fail even though `cmp` reports them equal.
 * The map can only detect a pointer key when `key_fn.free` is set. For a
 * non-owning pointer key (no `free`), providing `hash` is the caller's
 * responsibility.
 *
 * @note
 * Values are never hashed, so a value that is a pointer needs no hash
 * function.
 *
 * Example with owned string keys and `int` values:
 * @code
 * static int cmp_string(const void *a, const void *b) {
 *     return strcmp(*(const char *const *)a, *(const char *const *)b);
 * }
 *
 * g_functions_t key_fn = {.cmp = cmp_string,
 *                         .free = free,
 *                         .hash = hash_string};
 * g_functions_t value_fn = {};
 *
 * ghm_t *hm = ghm_create(sizeof(char *), sizeof(int), key_fn, value_fn);
 * @endcode
 */
ghm_t *ghm_create(size_t key_size,
                  size_t value_size,
                  g_functions_t key_fn,
                  g_functions_t value_fn);

/**
 * @brief
 * Destroys a hash map and releases its memory.
 *
 * For every entry still in the map, `key_fn.free` is called on the stored key
 * pointer and `value_fn.free` on the stored value pointer, if they are set.
 *
 * @param hm The map to destroy. If `nullptr`, nothing is done.
 *
 * @details
 * Pointer values previously obtained through ghm_get() from a map with a
 * `value_fn.free` function dangle after this call.
 */
void ghm_free(ghm_t *hm);

/**
 * @brief
 * Returns the number of entries in the map.
 *
 * @param hm The map. Must not be `nullptr`.
 * @return The number of key-value pairs currently stored.
 */
size_t ghm_size(const ghm_t *hm);

/**
 * @brief
 * Inserts a key-value pair, or replaces the value of an existing key.
 *
 * `key_size` bytes are copied from `key` and `value_size` bytes from `value`.
 * Both arguments are pointers to the objects to store, so for pointer keys and
 * values they are pointers to the pointer.
 *
 * If the key is not in the map, the pair is added and the map takes ownership
 * of the key and the value (when their `free` functions are set).
 *
 * If the key is already in the map, only the value is replaced:
 *  - the old value is released with `value_fn.free`, if set;
 *  - the new value is copied in and owned by the map;
 *  - the map keeps the **old** key. The `key` passed in is not stored, so it
 *    stays the caller's responsibility.
 *
 * @param hm    The map. Must not be `nullptr`.
 * @param key   Pointer to the key. Must not be `nullptr`.
 * @param value Pointer to the value. Must not be `nullptr`.
 *
 * @retval G_OK      The key was new and the pair was added. The map owns the
 *                   key and the value.
 * @retval G_EDUP    The key already existed and its value was replaced. This
 *                   is **not** a failure. The map owns the new value, but the
 *                   `key` passed in was not stored and still belongs to the
 *                   caller.
 * @retval G_EINVAL  `hm`, `key` or `value` is `nullptr`, or the map's internal
 *                   state is inconsistent. Nothing was stored.
 * @retval G_ENOMEM  The map needed to grow and allocation failed. Nothing was
 *                   stored.
 *
 * @details
 * For owned (`free`-managed) pointers, the caller must therefore release:
 *  - the key, whenever the result is anything other than `G_OK`;
 *  - the value, only when the result is `G_EINVAL` or `G_ENOMEM`.
 *
 * @code
 * char *k = strdup("one");
 * int v = 1;
 * if (ghm_insert(hm, &k, &v) != G_OK)
 *     free(k); // not stored: key already present, or an error
 * @endcode
 *
 * @warning
 * When replacing a pointer value, do not pass the pointer that is already
 * stored in the map. The old value is released before the new one is copied,
 * so the map would end up storing a dangling pointer.
 */
int ghm_insert(ghm_t *hm, const void *key, const void *value);

/**
 * @brief
 * Removes the entry with the given key.
 *
 * If set, `key_fn.free` is called on the stored key pointer and
 * `value_fn.free` on the stored value pointer. The `key` argument is only used
 * for the lookup and is not released by the call.
 *
 * @param hm  The map. Must not be `nullptr`.
 * @param key Pointer to the key to remove. Must not be `nullptr`.
 *
 * @retval G_OK        The entry was removed.
 * @retval G_ENOTFOUND No entry with that key exists.
 * @retval G_EINVAL    `hm` or `key` is `nullptr`, or the map's internal state
 *                     is inconsistent.
 *
 * @details
 * The stored key is released except when the pointer passed as `key` is the
 * same as the stored pointer.
 */
int ghm_delete(ghm_t *hm, const void *key);

/**
 * @brief
 * Looks up a key and copies its value to `out`.
 *
 * Exactly `value_size` bytes are copied to `out`. For pointer values this
 * copies the pointer, not the object it points to. The map keeps ownership of
 * that object, so the pointer is valid only until the entry is deleted or
 * replaced, or the map is freed.
 *
 * @param hm  The map. Must not be `nullptr`.
 * @param key Pointer to the key to look up. Must not be `nullptr`.
 * @param out Destination buffer of at least `value_size` bytes. Must not be
 *            `nullptr`. It is left untouched on failure.
 *
 * @retval G_OK        The value was copied to `out`.
 * @retval G_ENOTFOUND No entry with that key exists.
 * @retval G_EINVAL    `hm`, `key` or `out` is `nullptr`, or the map's internal
 *                     state is inconsistent.
 */
int ghm_get(const ghm_t *hm, const void *key, void *out);

#endif
