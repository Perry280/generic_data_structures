#ifndef GENERIC_TYPES_H
#define GENERIC_TYPES_H

#define G_OK        0  /**< Success. */
#define G_ENOMEM    -1 /**< Memory allocation failed. */
#define G_EINVAL    -2 /**< Invalid argument (e.g. a `nullptr` pointer). */
#define G_ERANGE    -3 /**< Index out of range. */
#define G_ENOTFOUND -4 /**< Value not found. */
#define G_EEMPTY    -5 /**< Operation on an empty struct. */
#define G_EITER     -6 /**< Iterator has no next element. */
#define G_EDUP -7 /**< Attempting to insert a duplicate element in struct. */

/**
 * @brief
 * Compare function.
 * The function must return an integer less than, equal to, or greater than zero
 * if the first argument is found, respectively, to be less than, to match, or
 * be greater than the second argument.
 */
typedef int (*g_cmp_f)(const void *, const void *);

/**
 * @brief
 * Data specific free function.
 * Function that can be used to free the data field in a struct.
 */
typedef void (*g_free_f)(void *);

#endif
