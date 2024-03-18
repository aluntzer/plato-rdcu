#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "pcg_basic.h"
#include "test_common.h"

#include <unity.h>

void cmp_rand_seed(uint64_t seed)
{
	pcg32_srandom(seed, 0);
}


uint32_t cmp_rand32(void)
{
	return pcg32_random();
}


/**
 * @brief generate a random number
 *
 * @param min minimum value (inclusive)
 * @param max maximum value (inclusive)
 *
 * @returns "random" numbers in the range [M, N]
 *
 */

uint32_t cmp_rand_between(uint32_t min, uint32_t max)
{
	TEST_ASSERT(min < max);

	return min + pcg32_boundedrand(max-min+1);
}


uint32_t cmp_rand_nbits(unsigned int nbits)
{
	TEST_ASSERT(nbits > 0);

	return cmp_rand32() >> (32 - nbits);
}


/**
 * @brief allocates memory safely for tests
 *
 * @param size The size of memory to allocate.
 *
 * @returns a pointer to the allocated memory, or NULL if allocation fails
 */

void* TEST_malloc(size_t size)
{
    if (size > 0) {
        void* const mem = malloc(size);
        TEST_ASSERT(mem);
        return mem;
    }
    return NULL;
}
