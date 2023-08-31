#include <assert.h>

#include "pcg_basic.h"


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
	assert(min < max);

	return min + pcg32_boundedrand(max-min+1);
}


uint32_t cmp_rand_nbits(unsigned int nbits)
{
	assert(nbits > 0);

	return cmp_rand32() >> (32 - nbits);
}
