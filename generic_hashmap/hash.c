#include "hash.h"
#include <string.h>

#define FVN_OFFSET_BASIS ((uint64_t)0xcbf29ce484222325)
#define FVN_PRIME        ((uint64_t)0x00000100000001b3)

uint64_t fnv1a(const void *data, size_t size) {
    uint64_t hash = FVN_OFFSET_BASIS;
    const uint8_t *const aux = data;

    for (size_t i = 0; i < size; i++) {
        hash ^= aux[i];
        hash *= FVN_PRIME;
    }

    return hash;
}

uint64_t hash_string(const char *data) {
    return fnv1a(data, strlen(data));
}
