/**
 * @headerfile singly_linked_list_int.h
 * @brief Singly linked list of `int`.
 *
 * The library is not thread-safe.
 */
#ifndef GENERIC_SINGLY_LINKED_LIST_H
#define GENERIC_SINGLY_LINKED_LIST_H

#include "../generic_types.h"
#include <stddef.h>

/** @name Return codes
 *  @{ */
#define GSLL_OK     G_OK     /**< Success. */
#define GSLL_ENOMEM G_ENOMEM /**< Memory allocation failed. */
#define GSLL_EINVAL                                                            \
    G_EINVAL /**< Invalid argument (e.g. a `nullptr` pointer). */
#define GSLL_ERANGE    G_ERANGE    /**< Index out of range. */
#define GSLL_ENOTFOUND G_ENOTFOUND /**< Value not found. */
#define GSLL_EEMPTY    G_EEMPTY    /**< Operation on an empty list. */
/** @} */

/** @brief A node of the linked list. */
typedef struct gsll_node_s {
    struct gsll_node_s *next; /**< Next node, or `nullptr` if last. */
    void *data;               /**< Pointer to value stored in the node. */
} gsll_node_t;

/**
 * @brief
 * Used in `gsll_foreach` and called on every node.
 * Second argument can be 'nullptr'.
 */
typedef void (*gsll_foreach_f)(gsll_node_t *, void *);

/** @brief Singly linked list of integers. */
typedef struct gsll_list_s gsll_list_t;

/**
 * @brief Create an empty linked list.
 *
 * @param data_size
 * Size of the data stored in each node of the list.
 * It must be a positive integer.
 *
 * @param cmp Compare function. It must not be `nullptr`.
 *
 * @param free_node Data specific free function. It is called in `gsll_free`.
 *
 * @return The pointer to the newly created list,
 * or `nullptr` if `malloc` failed or `data_size == 0` of `cmp == nullptr`.
 *
 * @details
 * The caller owns the returned list
 * and must release it with `gsll_free` when done.
 * If `free_node == nullptr`, `free` field of the list is set to `nullptr`,
 * and `free` function of `stdlib.h` is used to free data.
 */
gsll_list_t *gsll_create(size_t data_size, g_cmp_f cmp, g_free_f free_node);

/**
 * @brief Free a linked list and its content.
 *
 * @param ll Pointer to the linked list.
 *
 * @details
 * If `ll` is `nullptr` the function is no-op.
 * After the function terminates, `ll` is invalid.
 */
void gsll_free(gsll_list_t *ll);

/**
 * @brief Return the number of elements in the linked list.
 *
 * @param ll Pointer to the linked list.
 *
 * @return The size of the linked list.
 *
 * @details
 * The function does not check if `ll == nullptr`.
 */
size_t gsll_size(const gsll_list_t *ll);

/**
 * @brief Put a node with `data` at the beginning of the list.
 *
 * @param ll Pointer to the linked list.
 * @param data Pointer to data to insert.
 *
 * @retval GSLL_OK Success.
 * @retval GSLL_EINVAL If `ll` or `data` are `nullptr`.
 * @retval GSLL_ENOMEM If `malloc` failed.
 *
 * @details
 * On failure, `ll` is not modified.
 * The value pointed by `data` gets copied to the new node through shallow copy.
 */
int gsll_push_front(gsll_list_t *ll, const void *data);

/**
 * @brief Put a node with `data` at the end of the list.
 *
 * @param ll Pointer to the linked list.
 * @param data Pointer to data to insert.
 *
 * @retval GSLL_OK Success.
 * @retval GSLL_EINVAL If `ll` or `data` are `nullptr`.
 * @retval GSLL_ENOMEM If `malloc` failed.
 *
 * @details
 * On failure, `ll` is not modified.
 * The value pointed by `data` gets copied to the new node through shallow copy.
 */
int gsll_push_back(gsll_list_t *ll, const void *data);

/**
 * @brief Remove a node from the beginning of the list.
 *
 * @param ll Pointer to the linked list.
 * @param out
 * On success, if not `nullptr`, will contain the content of the removed node.
 *
 * @retval GSLL_OK Success.
 * @retval GSLL_EINVAL If `ll` is `nullptr`.
 * @retval GSLL_EEMPTY If `ll` is an empty list.
 *
 * @details
 * On failure, `ll` is not modified.
 * On failure, if `out` is not `nullptr`, `out` is not modified.
 */
int gsll_pop_front(gsll_list_t *ll, void *out);

/**
 * @brief Get the data at index `index`.
 *
 * @param ll Pointer to the linked list.
 * @param index Index of the node to look for. It must be in `[0, size - 1]`.
 * @param out On success, contains the data at index `index`.
 *
 * @retval GSLL_OK Success.
 * @retval GSLL_EINVAL If `ll` or `out` are `nullptr`.
 * @retval GSLL_EEMPTY If `ll` is an empty list.
 * @retval GSLL_ERANGE If `index >= size`.
 *
 * @details
 * On failure, `out` is not modified.
 * If `index == size - 1` the function runs in `O(1)`.
 */
int gsll_get(const gsll_list_t *ll, size_t index, void *out);

/**
 * @brief Insert a node at index `index`.
 *
 * @param ll Pointer to the linked list.
 * @param index
 * Position where the new node will be inserted. It must be in `[0, size]`.
 * @param data Pointer to data to insert.
 *
 * @retval GSLL_OK Success.
 * @retval GSLL_EINVAL If `ll` or `data` are `nullptr`.
 * @retval GSLL_ERANGE If `index > size`.
 * @retval GSLL_ENOMEM If `malloc` fails.
 *
 * @details
 * On failure, `ll` is not modified.
 * If `index == 0` the function is equivalent to `ll_int_push(ll, data)`.
 * If `index == size` the function runs in `O(1)`
 * and is equivalent to `ll_int_push_back(ll, data)`.
 * The value pointed by `data` gets copied to the new node through shallow copy.
 */
int gsll_insert_at(gsll_list_t *ll, size_t index, const void *data);

/**
 * @brief Remove a node at index `index`.
 *
 * @param ll Pointer to the linked list.
 * @param index Index of the node to remove. It must be in `[0, size - 1]`.
 *
 * @retval GSLL_OK Success.
 * @retval GSLL_EINVAL If `ll` is `nullptr`.
 * @retval GSLL_ERANGE If `index >= size`.
 *
 * @details
 * On failure, `ll` is not modified.
 * If `index == 0` the function is equivalent to `ll_int_pop(ll, nullptr)`.
 */
int gsll_delete_at(gsll_list_t *ll, size_t index);

/**
 * @brief Find the index of the first node containing `data`.
 *
 * @param ll Pointer to the linked list.
 * @param data Pointer to data to look for.
 * @param out
 * If not `nullptr`, on success, contains the index of the first match.
 *
 * @retval GSLL_OK Success.
 * @retval GSLL_EINVAL If `ll` or `data` are `nullptr`.
 * @retval GSLL_EEMPTY If `ll` is an empty list.
 * @retval GSLL_ENOTFOUND If `data` is not in the list.
 *
 * @details
 * On failure `out` is not modified.
 */
int gsll_search(const gsll_list_t *ll, const void *data, size_t *out);

/**
 * @brief Reverse the linked list.
 *
 * @param ll Pointer to the linked list.
 *
 * @retval GSLL_OK Success.
 * @retval GSLL_EINVAL If `ll` is `nullptr`.
 * @retval GSLL_EEMPTY If `ll` is an empty list.
 *
 * @details
 * On failure `ll` is not modified.
 */
int gsll_reverse(gsll_list_t *ll);

/**
 * @brief Print the linked list size and the data in the list.
 *
 * @param ll Pointer to the linked list.
 * @param f Pointer to the function to be applied to each node.
 * @param args Arguments passed to f. Can be `nullptr`.
 *
 * @retval GSLL_EINVAL If `ll` or `f` are `nullptr`.
 * @retval GSLL_EEMPTY If `ll` is an empty list.
 *
 * @details
 * On failure, `ll` is not modified.
 */
int gsll_foreach(gsll_list_t *ll, gsll_foreach_f f, void *args);

#endif
