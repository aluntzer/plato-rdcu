#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdint.h>
#include <stddef.h>

void cmp_rand_seed(uint64_t seed);

uint32_t cmp_rand32(void);

uint32_t cmp_rand_between(uint32_t min, uint32_t max);

uint32_t cmp_rand_nbits(unsigned int n_bits);

void* TEST_malloc(size_t size);

#endif /* TEST_COMMON_H */
