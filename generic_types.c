#include "generic_types.h"
#include <string.h>

int cmp_int(const void *a, const void *b) {
    return *((const int *)a) - *((const int *)b);
}

int cmp_string(const void *a, const void *b) {
    return strcmp(*((const char **)a), *((const char **)b));
}
