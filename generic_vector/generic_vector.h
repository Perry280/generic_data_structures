/** @file generic_vector.h
 * @brief Type-agnostic dynamic array (vector) with an index-based iterator.
 *
 * @details
 * Elements are stored contiguously and are handled as raw bytes of a fixed
 * size chosen at creation time. All insertion and retrieval operations copy
 * `data_size` bytes, so the vector owns a copy of each element.
 *
 * An optional destructor callback (@ref g_free_f) can be supplied to release
 * resources held *inside* each element (e.g. a heap-allocated string stored
 * in a struct). The callback receives a pointer to the element slot inside
 * the vector and must not free the slot itself.
 */

#ifndef GENERIC_VECTOR_H
#define GENERIC_VECTOR_H

#include "../generic_types.h"
#include <stddef.h>

/**
 * @brief Opaque handle to a generic vector.
 *
 * @details
 * Created with @ref gvec_create or @ref gvec_create_capacity and destroyed
 * with @ref gvec_free.
 */
typedef struct gvec_s gvec_t;

/**
 * @brief Opaque handle to a forward iterator over a @ref gvec_t.
 *
 * @details
 * Created with @ref gvec_iter_create and destroyed with @ref gvec_iter_free.
 */
typedef struct gvec_iter_s gvec_iter_t;

/**
 * @brief Creates an empty vector with the default initial capacity.
 *
 * @param data_size Size in bytes of each element. Must be greater than 0.
 * @param cmp Comparison function for elements. Must not be `nullptr`.
 * @param fn
 * Struct of pointers to function:
 * - cmp: compare two elements. Must not be `nullptr`.
 * - free: data specific free function. Can be `nullptr`.
 * - hash: unused.
 *
 * @return
 * Pointer to the new vector, or `nullptr` if `data_size` is 0, `cmp` is
 * `nullptr` or memory allocation fails.
 *
 * @details
 * Equivalent to calling @ref gvec_create_capacity with a capacity of 128
 * elements. The returned vector must be released with @ref gvec_free.
 */
gvec_t *gvec_create(size_t data_size, g_functions_t fn);

/**
 * @brief Creates an empty vector with a given initial capacity.
 *
 * @param data_size Size in bytes of each element. Must be greater than 0.
 * @param fn
 * Struct of pointers to function:
 * - cmp: compare two elements. Must not be `nullptr`.
 * - free: data specific free function. Can be `nullptr`.
 * - hash: unused.
 * @param free_array
 * Destructor called on each element when it is discarded.
 * be `nullptr` if elements need no cleanup.
 * @param capacity Number of elements to allocate space for. May be 0.
 *
 * @return
 * Pointer to the new vector, or `nullptr` if `data_size` is 0,
 * `cmp` is `nullptr` or the requested byte size exceeds `PTRDIFF_MAX`
 * or memory allocation fails.
 *
 * @details
 * If `capacity` is 0 no storage is allocated until the first insertion, at
 * which point the default capacity (128 elements) is used.
 * The returned vector must be released with @ref gvec_free.
 */
gvec_t *gvec_create_capacity(size_t data_size,
                             g_functions_t fun,
                             size_t capacity);

/**
 * @brief Destroys a vector and releases all its memory.
 *
 * @param vec Vector to destroy. If `nullptr`, the function does nothing.
 *
 * @details
 * If a destructor was provided at creation, it is called on every stored
 * element (from index 0 to `size - 1`) before the internal array is freed.
 * The vector handle must not be used after this call.
 */
void gvec_free(gvec_t *vec);

/**
 * @brief Gets read-only access to the underlying contiguous storage.
 *
 * @param vec Vector to query. Must not be `nullptr`.
 *
 * @return
 * Pointer to the first element, or `nullptr` if no storage has been
 * allocated yet (capacity 0).
 *
 * @details
 * The pointer is invalidated by any operation that may reallocate the
 * storage (@ref gvec_push_back, @ref gvec_reserve) and by @ref gvec_free.
 * Only the first @ref gvec_size elements are initialized.
 */
const void *gvec_array(const gvec_t *vec);

/**
 * @brief Gets the number of elements currently stored.
 *
 * @param vec Vector to query. Must not be `nullptr`.
 *
 * @return Number of elements in the vector.
 */
size_t gvec_size(const gvec_t *vec);

/**
 * @brief Gets the number of elements the vector can hold without growing.
 *
 * @param vec Vector to query. Must not be `nullptr`.
 *
 * @return Current capacity, in elements.
 */
size_t gvec_capacity(const gvec_t *vec);

/**
 * @brief Gets a pointer to the element at a given index.
 *
 * @param vec Pointer to the vector.
 * @param index Zero-based index of the element.
 *
 * @return `void *` pointing to the first byte of the element.
 *
 * @details
 * No bounds checking is performed and the arguments may be evaluated more
 * than once, so they must not have side effects.
 */
const void *gvec_ptr_at(const gvec_t *vec, size_t index);

/**
 * @brief Gets a pointer to the last element of the vector.
 *
 * @param vec Pointer to the vector.
 *
 * @return `void *` pointing to the first byte of the last element.
 *
 * @details
 * The vector must not be empty: on an empty vector `size - 1` underflows and
 * the resulting pointer is invalid. See @ref gvec_ptr_at for the remaining
 * caveats.
 */
const void *gvec_ptr_last(const gvec_t *vec);

/**
 * @brief Overwrites the element at a given index with a copy of `data`.
 *
 * @param vec Vector to modify.
 * @param index Zero-based index of the element to replace.
 * @param data Pointer to the new value; `data_size` bytes are copied from it.
 *
 * @retval G_OK The element was replaced.
 * @retval G_EINVAL `vec` or `data` is `nullptr`.
 * @retval G_ERANGE `index` is greater than or equal to the vector size.
 *
 * @details
 * The destructor is called on the previous value.
 */
int gvec_set(gvec_t *vec, size_t index, const void *data);

/**
 * @brief Copies the element at a given index into a caller-provided buffer.
 *
 * @param vec Vector to read from.
 * @param index Zero-based index of the element to read.
 * @param out Destination buffer of at least `data_size` bytes.
 *
 * @retval G_OK The element was copied into `out`.
 * @retval G_EINVAL `vec` or `out` is `nullptr`.
 * @retval G_ERANGE `index` is greater than or equal to the vector size.
 *
 * @details
 * Only a shallow copy is made: if the element contains pointers, the copy
 * shares the pointed-to data with the element stored in the vector.
 */
int gvec_at(const gvec_t *vec, size_t index, void *out);

/**
 * @brief Appends a copy of `data` to the end of the vector.
 *
 * @param vec Vector to append to.
 * @param data Pointer to the value; `data_size` bytes are copied from it.
 *
 * @retval G_OK The element was appended.
 * @retval G_EINVAL `vec` or `data` is `nullptr`.
 * @retval G_ENOMEM
 * The storage needed to grow could not be allocated; the
 * vector is left unchanged.
 *
 * @details
 * When the vector is full its capacity is doubled (or set to the default
 * capacity if no storage was allocated yet). Growing may move the storage, so
 * any pointer previously obtained from @ref gvec_array is invalidated.
 */
int gvec_push_back(gvec_t *vec, const void *data);

/**
 * @brief Removes the last element of the vector.
 *
 * @param vec Vector to remove from.
 * @param out
 * Destination buffer of at least `data_size` bytes that receives
 * the removed element, or `nullptr` to discard it.
 *
 * @retval G_OK The last element was removed.
 * @retval G_EINVAL `vec` is `nullptr`.
 * @retval G_EEMPTY The vector has no elements.
 *
 * @details
 * If `out` is not `nullptr` the element is copied into it and ownership passes
 * to the caller; the destructor is not called. If `out` is `nullptr` and a
 * destructor was provided at creation, it is called on the removed element.
 * The capacity is not reduced.
 */
int gvec_pop_back(gvec_t *vec, void *out);

/**
 * @brief Ensures the vector can hold at least `new_capacity` elements.
 *
 * @param vec Vector to resize.
 * @param new_capacity
 * Minimum capacity required, in elements. Must be greater than `0`.
 *
 * @retval G_OK
 * The capacity is now at least `new_capacity` (including
 * the case where no change was needed).
 * @retval G_EINVAL `vec` is `nullptr` or `new_capacity` is 0.
 * @retval G_ENOMEM
 * The storage could not be allocated; the vector is left unchanged.
 *
 * @details
 * The capacity never shrinks: if `new_capacity` is less than or equal to the
 * current capacity the call does nothing. Growing may move the storage, so
 * any pointer previously obtained from @ref gvec_array is invalidated.
 */
int gvec_reserve(gvec_t *vec, size_t new_capacity);

/**
 * @brief Reduces the capacity of the vector to match its size.
 *
 * @param vec Vector to shrink.
 *
 * @retval G_OK
 * The capacity now equals the size (including the case
 * where no change was needed).
 * @retval G_EINVAL `vec` is `nullptr`.
 * @retval G_ENOMEM
 * The storage could not be reallocated;
 * the vector is left unchanged and remains valid.
 *
 * @details
 * If the vector has no allocated storage or its capacity already equals its
 * size, the call does nothing.
 * If the vector is empty, its storage is
 * released entirely, leaving a capacity of 0 and a `nullptr` array; the next
 * insertion will allocate the default capacity again. Otherwise the storage
 * is reallocated to hold exactly @ref gvec_size elements.
 *
 * Shrinking may move the storage, so any pointer previously obtained from
 * @ref gvec_array is invalidated.
 */
int gvec_shrink_to_fit(gvec_t *vec);

/**
 * @brief Creates a forward iterator positioned at the first element.
 *
 * @param vec Vector to iterate over.
 *
 * @return
 * Pointer to the new iterator, or `nullptr` if `vec` is `nullptr`
 * or memory allocation fails.
 *
 * @details
 * The iterator keeps a reference to the vector (it does not copy it), so the
 * vector must outlive the iterator.
 * Elements are read by index, which means the iterator observes elements
 * appended after its creation.
 * The iterator must be released with @ref gvec_iter_free.
 */
gvec_iter_t *gvec_iter_create(const gvec_t *vec);

/**
 * @brief Destroys an iterator.
 *
 * @param iter Iterator to destroy. If `nullptr`, the function does nothing.
 *
 * @details
 * Only the iterator is released; the vector it refers to is not affected.
 */
void gvec_iter_free(gvec_iter_t *iter);

/**
 * @brief Checks whether the iterator has more elements to visit.
 *
 * @param iter Iterator to query.
 *
 * @retval nonzero The next call to @ref gvec_iter_next will yield an element.
 * @retval 0 There are no more elements, or `iter` is `nullptr`.
 */
int gvec_iter_has_next(const gvec_iter_t *iter);

/**
 * @brief Copies the next element into a buffer and advances the iterator.
 *
 * @param iter Iterator to advance.
 * @param out  Destination buffer of at least `data_size` bytes.
 *
 * @retval G_OK The element was copied into `out` and the iterator advanced.
 * @retval G_EINVAL `iter` or `out` is `nullptr`.
 * @retval G_EITER The iterator has no more elements.
 *
 * @details
 * As with @ref gvec_at, only a shallow copy is made.
 * The iterator is not advanced when an error is returned.
 */
int gvec_iter_next(gvec_iter_t *iter, void *out);

#endif
