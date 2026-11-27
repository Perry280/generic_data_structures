#ifndef HASH_H
#define HASH_H

#include <stddef.h>
#include <stdint.h>

uint64_t fnv1a(const void *data, size_t size);
uint64_t hash_string(const void *data);

#endif
