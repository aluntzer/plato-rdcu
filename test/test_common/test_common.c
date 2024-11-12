#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "pcg_basic.h"
#include "test_common.h"

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
#  include "../fuzz/fuzz_helpers.h"
#  define TEST_ASSERT(cond) FUZZ_ASSERT(cond)
#else
#  include <unity.h>
#endif

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


uint32_t cmp_rand_nbits(unsigned int n_bits)
{
	TEST_ASSERT(n_bits > 0);
	TEST_ASSERT(n_bits <= 32);

	return cmp_rand32() >> (32 - n_bits);
}
