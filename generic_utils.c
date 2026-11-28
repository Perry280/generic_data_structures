#include "generic_utils.h"
#include <string.h>

int cmp_int(const void *a, const void *b) {
    const int x = *((const int *)a);
    const int y = *((const int *)b);
    return (x > y) - (x < y);
}

int cmp_string(const void *a, const void *b) {
    return strcmp(*((const char **)a), *((const char **)b));
}
