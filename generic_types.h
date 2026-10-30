#ifndef GENERIC_TYPES_H
#define GENERIC_TYPES_H

#define G_OK        0  /**< Success. */
#define G_ENOMEM    -1 /**< Memory allocation failed. */
#define G_EINVAL    -2 /**< Invalid argument (e.g. a `NULL` pointer). */
#define G_ERANGE    -3 /**< Index out of range. */
#define G_ENOTFOUND -4 /**< Value not found. */
#define G_EEMPTY    -5 /**< Operation on an empty struct. */
#define G_EITER     -6 /**< Iterator has no next element. */

/**
 * @brief
 * Compare function.
 * Must return `0` if arguments are equal, or non-zero value otherwise.
 */
typedef int (*g_cmp_f)(const void *, const void *);

/**
 * @brief
 * Data specific free function.
 * Function that can be used to free the data field in a struct.
 */
typedef void (*g_free_f)(void *);

#endif
