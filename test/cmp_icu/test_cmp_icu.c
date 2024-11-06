/**
 * @file   test_cmp_icu.c
 * @author Dominik Loidolt (dominik.loidolt@univie.ac.at)
 * @date   2022
 *
 * @copyright GPLv2
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * @brief software compression tests
 */


#include <stdlib.h>
#include <leon_inttypes.h>

#if defined __has_include
#  if __has_include(<time.h>)
#    include <time.h>
#    include <unistd.h>
#    define HAS_TIME_H 1
#  endif
#endif

#include "unity.h"
#include "../test_common/test_common.h"

#include <cmp_icu.h>
#include <cmp_data_types.h>
#include <cmp_rdcu_cfg.h>
#include "../../lib/icu_compress/cmp_icu.c" /* this is a hack to test static functions */


#define MAX_CMP_MODE CMP_MODE_DIFF_MULTI


/**
 * @brief  Seeds the pseudo-random number generator used by rand()
 */

void setUp(void)
{
	uint64_t seed;
	static int n;

#if HAS_TIME_H
	seed = (uint64_t)(time(NULL) ^ getpid()  ^ (intptr_t)&setUp);
#else
	seed = 1;
#endif

	if (!n) {
		uint32_t seed_up = seed >> 32;
		uint32_t seed_down = seed & 0xFFFFFFFF;

		n = 1;
		cmp_rand_seed(seed);
		printf("seed: 0x%08"PRIx32"%08"PRIx32"\n", seed_up, seed_down);
	}
}


/**
 * @test map_to_pos
 */

void test_map_to_pos(void)
{
	uint32_t value_to_map;
	uint32_t max_data_bits;
	uint32_t mapped_value;

	/* test mapping 32 bits values */
	max_data_bits = 32;

	value_to_map = 0;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_INT(0, mapped_value);

	value_to_map = -1U;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_INT(1, mapped_value);

	value_to_map = 1;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_INT(2, mapped_value);

	value_to_map = 42;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_INT(84, mapped_value);

	value_to_map = INT32_MAX;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_HEX(UINT32_MAX-1, mapped_value);

	value_to_map = (uint32_t)INT32_MIN;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_HEX(UINT32_MAX, mapped_value);

	/* test mapping 16 bits values */
	max_data_bits = 16;

	value_to_map = -1U;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_INT(1, mapped_value);

	/* test mapping 6 bits values */
	max_data_bits = 6;

	value_to_map = 0;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_INT(0, mapped_value);

	value_to_map = -1U;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_INT(1, mapped_value);

	value_to_map = UINT32_MAX;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_INT(1, mapped_value);

	value_to_map = -1U & 0x3FU;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_INT(1, mapped_value);

	value_to_map = 63;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_INT(1, mapped_value);

	value_to_map = 1;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_INT(2, mapped_value);

	value_to_map = 31;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_INT(62, mapped_value);

	value_to_map = -33U; /* aka 31 */
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_INT(62, mapped_value);

	value_to_map = -32U;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_INT(63, mapped_value);

	value_to_map = 32;
	mapped_value = map_to_pos(value_to_map, max_data_bits);
	TEST_ASSERT_EQUAL_INT(63, mapped_value);
}


/**
 * @test put_n_bits32
 */

#define SDP_PB_N 3


static void init_PB32_arrays(uint32_t *z, uint32_t *o)
{
	uint32_t i;

	/* init testarray with all 0 and all 1 */
	for (i = 0; i < SDP_PB_N; i++) {
		z[i] = 0;
		o[i] = 0xffffffff;
	}
}


void test_put_n_bits32(void)
{
	uint32_t v, n;
	uint32_t o;
	uint32_t rval; /* return value */
	uint32_t testarray0[SDP_PB_N];
	uint32_t testarray1[SDP_PB_N];
	const uint32_t l = sizeof(testarray0) * CHAR_BIT;

	/* hereafter, the value is v,
	 * the number of bits to write is n,
	 * the offset of the bit is o,
	 * the max length the bitstream in bits is l
	 */

	init_PB32_arrays(testarray0, testarray1);
	TEST_ASSERT(testarray0[0] == 0);
	TEST_ASSERT(testarray0[1] == 0);
	TEST_ASSERT(testarray0[2] == 0);
	TEST_ASSERT(testarray1[0] == 0xffffffff);
	TEST_ASSERT(testarray1[1] == 0xffffffff);
	TEST_ASSERT(testarray1[2] == 0xffffffff);

	/*** n=0 ***/

	/* do not write, left border */
	v = 0; n = 0; o = 0;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(0, rval);
	TEST_ASSERT(testarray0[0] == 0);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(0, rval);
	TEST_ASSERT(testarray1[0] == 0xffffffff);

	rval = put_n_bits32(v, n, o, NULL, l);
	TEST_ASSERT_EQUAL_INT(0, rval);

	v = 0xffffffff; n = 0; o = 0;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(0, rval);
	TEST_ASSERT(testarray0[0] == 0);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(0, rval);
	TEST_ASSERT(testarray1[0] == 0xffffffff);

	rval = put_n_bits32(v, n, o, NULL, l);
	TEST_ASSERT_EQUAL_INT(0, rval);

	/* do not write, right border */
	v = 0; n = 0; o = l;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(l, rval);
	TEST_ASSERT(testarray0[0] == 0);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(l, rval);
	TEST_ASSERT(testarray1[0] == 0xffffffff);

	rval = put_n_bits32(v, n, o, NULL, l);
	TEST_ASSERT_EQUAL_INT(l, rval);

	/* test value = 0xffffffff; N = 0 */
	v = 0xffffffff; n = 0; o = l;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(l, rval);
	TEST_ASSERT(testarray0[0] == 0);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(l, rval);
	TEST_ASSERT(testarray1[0] == 0xffffffff);

	rval = put_n_bits32(v, n, o, NULL, l);
	TEST_ASSERT_EQUAL_INT(l, rval);

	/*** n=1 ***/

	/* left border, write 0 */
	v = 0; n = 1; o = 0;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(1, rval);
	TEST_ASSERT(testarray0[0] == 0);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(1, rval);
	TEST_ASSERT(testarray1[0] == cpu_to_be32(0x7fffffff));

	/* left border, write 1 */
	v = 1; n = 1; o = 0;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(1, rval);
	TEST_ASSERT(testarray0[0] == cpu_to_be32(0x80000000));

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(1, rval);
	TEST_ASSERT(testarray1[0] == 0xffffffff);

	/* left border, write 32 */
	v = 0xf0f0abcd; n = 32; o = 0;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(rval, 32);
	TEST_ASSERT(testarray0[0] == cpu_to_be32(0xf0f0abcd));
	TEST_ASSERT(testarray0[1] == 0);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(rval, 32);
	TEST_ASSERT(testarray1[0] == cpu_to_be32(0xf0f0abcd));
	TEST_ASSERT(testarray1[1] == 0xffffffff);
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	/* middle, write 2 bits */
	v = 3; n = 2; o = 29;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(rval, 31);
	TEST_ASSERT(testarray0[0] == cpu_to_be32(0x6));

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT(testarray1[0] == 0xffffffff);
	TEST_ASSERT_EQUAL_INT(rval, 31);
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	/*** n=5, unsegmented ***/

	/* left border, write 0 */
	v = 0; n = 5; o = 0;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(rval, 5);
	TEST_ASSERT(testarray0[0] == 0);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT(testarray1[0] == cpu_to_be32(0x07ffffff));
	TEST_ASSERT_EQUAL_INT(rval, 5);
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	/* left border, write 11111 */
	v = 0x1f; n = 5; o = 0;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(rval, 5);
	TEST_ASSERT(testarray0[0] == cpu_to_be32(0xf8000000));

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(rval, 5);
	TEST_ASSERT(testarray1[0] == 0xffffffff);
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	/* middle, write 0 */
	v = 0; n = 5; o = 7;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(rval, 12);
	TEST_ASSERT(testarray0[0] == 0);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(rval, 12);
	TEST_ASSERT(testarray1[0] == cpu_to_be32(0xfe0fffff));
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	/* middle, write 11111 */
	v = 0x1f; n = 5; o = 7;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(rval, 12);
	TEST_ASSERT(testarray0[0] == cpu_to_be32(0x01f00000));

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(rval, 12);
	TEST_ASSERT(testarray1[0] == 0xffffffff);
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	/* right, write 0 */
	v = 0; n = 5; o = 91;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(rval, 96);
	TEST_ASSERT(testarray0[0] == 0);
	TEST_ASSERT(testarray0[1] == 0);
	TEST_ASSERT(testarray0[0] == 0);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(rval, 96);
	TEST_ASSERT(testarray1[0] == 0xffffffff);
	TEST_ASSERT(testarray1[1] == 0xffffffff);
	TEST_ASSERT(testarray1[2] == cpu_to_be32(0xffffffe0));
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	/* right, write 11111 */
	v = 0x1f; n = 5; o = 91;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(rval, 96);
	TEST_ASSERT(testarray0[0] == 0);
	TEST_ASSERT(testarray0[1] == 0);
	TEST_ASSERT(testarray0[2] == cpu_to_be32(0x0000001f));

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(rval, 96);
	TEST_ASSERT(testarray1[0] == 0xffffffff);
	TEST_ASSERT(testarray1[1] == 0xffffffff);
	TEST_ASSERT(testarray1[2] == 0xffffffff);
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	/* 32 bit, write 0 */
	v = 0; n = 32; o = 0;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(rval, 32);
	TEST_ASSERT(testarray0[0] == 0x00000000);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(rval, 32);
	TEST_ASSERT(testarray1[0] == 0x00000000);
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	/* 32 bit, write -1 */
	v = 0xffffffff; n = 32; o = 0;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(rval, 32);
	TEST_ASSERT(testarray0[0] == 0xffffffff);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(rval, 32);
	TEST_ASSERT(testarray1[0] == 0xffffffff);
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	/* SEGMENTED cases */
	/* 5 bit, write 0 */
	v = 0; n = 5; o = 62;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(rval, 67);
	TEST_ASSERT(testarray0[0] == 0);
	TEST_ASSERT(testarray0[1] == 0);
	TEST_ASSERT(testarray0[2] == 0);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(rval, 67);
	TEST_ASSERT(testarray1[0] == 0xffffffff);
	TEST_ASSERT(testarray1[1] == cpu_to_be32(0xfffffffc));
	TEST_ASSERT(testarray1[2] == cpu_to_be32(0x1fffffff));
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	/* 5 bit, write 1f */
	v = 0x1f; n = 5; o = 62;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(rval, 67);
	TEST_ASSERT(testarray0[0] == 0);
	TEST_ASSERT(testarray0[1] == cpu_to_be32(3));
	TEST_ASSERT(testarray0[2] == cpu_to_be32(0xe0000000));

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(rval, 67);
	TEST_ASSERT(testarray1[0] == 0xffffffff);
	TEST_ASSERT(testarray1[1] == 0xffffffff);
	TEST_ASSERT(testarray1[2] == 0xffffffff);
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	/* 32 bit, write 0 */
	v = 0; n = 32; o = 1;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(rval, 33);
	TEST_ASSERT(testarray0[0] == 0x00000000);
	TEST_ASSERT(testarray0[1] == 0x00000000);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(rval, 33);
	TEST_ASSERT(testarray1[0] == cpu_to_be32(0x80000000));
	TEST_ASSERT(testarray1[1] == cpu_to_be32(0x7fffffff));
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	/* 32 bit, write -1 */
	v = 0xffffffff; n = 32; o = 1;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(rval, 33);
	TEST_ASSERT(testarray0[0] == cpu_to_be32(0x7fffffff));
	TEST_ASSERT(testarray0[1] == cpu_to_be32(0x80000000));

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(rval, 33);
	TEST_ASSERT(testarray1[0] == 0xffffffff);
	TEST_ASSERT(testarray1[1] == 0xffffffff);
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	/* test NULL buffer */
	v = 0; n = 0; o = 0;
	rval = put_n_bits32(v, n, o, NULL, l);
	TEST_ASSERT_EQUAL_INT(rval, 0);

	v = 0; n = 1; o = 0;
	rval = put_n_bits32(v, n, o, NULL, l);
	TEST_ASSERT_EQUAL_INT(rval, 1);

	v = 0; n = 5; o = 31;
	rval = put_n_bits32(v, n, o, NULL, l);
	TEST_ASSERT_EQUAL_INT(rval, 36);

	v = 0; n = 2; o = 95;
	rval = put_n_bits32(v, n, o, NULL, l);
	TEST_ASSERT_EQUAL_INT(rval, 97); /* rval can be longer than l */

	/* value larger than n allows */
	v = 0x7f; n = 6; o = 10;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(16, rval);
	TEST_ASSERT(testarray0[0] == cpu_to_be32(0x003f0000));
	TEST_ASSERT(testarray0[1] == 0);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(16, rval);
	TEST_ASSERT(testarray1[0] == 0xffffffff);
	TEST_ASSERT(testarray1[1] == 0xffffffff);

	rval = put_n_bits32(v, n, o, NULL, l);
	TEST_ASSERT_EQUAL_INT(16, rval);
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	v = 0xffffffff; n = 6; o = 10;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(16, rval);
	TEST_ASSERT(testarray0[0] == cpu_to_be32(0x003f0000));
	TEST_ASSERT(testarray0[1] == 0);

	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_EQUAL_INT(16, rval);
	TEST_ASSERT(testarray1[0] == 0xffffffff);
	TEST_ASSERT(testarray1[1] == 0xffffffff);

	rval = put_n_bits32(v, n, o, NULL, l);
	TEST_ASSERT_EQUAL_INT(16, rval);
	/* re-init input arrays after clobbering */
	init_PB32_arrays(testarray0, testarray1);

	/* error cases */
	/* n too large */
	v = 0x0; n = 33; o = 1;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_TRUE(cmp_is_error(rval));
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_INT_DECODER, cmp_get_error_code(rval));
	TEST_ASSERT(testarray0[0] == 0);
	TEST_ASSERT(testarray0[1] == 0);

	rval = put_n_bits32(v, n, o, NULL, l);
	TEST_ASSERT_TRUE(cmp_is_error(rval));
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_INT_DECODER, cmp_get_error_code(rval));

	/* try to put too much in the bitstream */
	v = 0x1; n = 1; o = 96;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_TRUE(cmp_is_error(rval));
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUFFER, cmp_get_error_code(rval));
	TEST_ASSERT(testarray0[0] == 0);
	TEST_ASSERT(testarray0[1] == 0);
	TEST_ASSERT(testarray0[2] == 0);

	/* this should work (if bitstream=NULL no length check) */
	rval = put_n_bits32(v, n, o, NULL, l);
	TEST_ASSERT_EQUAL_INT(97, rval);

	/* offset lager than max_stream_len(l) */
	v = 0x0; n = 32; o = INT32_MAX;
	rval = put_n_bits32(v, n, o, testarray1, l);
	TEST_ASSERT_TRUE(cmp_is_error(rval));
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUFFER, cmp_get_error_code(rval));
	TEST_ASSERT(testarray1[0] == 0xffffffff);
	TEST_ASSERT(testarray1[1] == 0xffffffff);
	TEST_ASSERT(testarray1[2] == 0xffffffff);

	rval = put_n_bits32(v, n, o, NULL, l);
	TEST_ASSERT_FALSE(cmp_is_error(rval));

	/* negative offset */
#if 0
	v = 0x0; n = 0; o = -1;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(-1, rval);
	TEST_ASSERT(testarray0[0] == 0);
	TEST_ASSERT(testarray0[1] == 0);

	rval = put_n_bits32(v, n, o, NULL, l);
	TEST_ASSERT_EQUAL_INT(-1, rval);

	v = 0x0; n = 0; o = -2;
	rval = put_n_bits32(v, n, o, testarray0, l);
	TEST_ASSERT_EQUAL_INT(-1, rval);
	TEST_ASSERT(testarray0[0] == 0);
	TEST_ASSERT(testarray0[1] == 0);

	rval = put_n_bits32(v, n, o, NULL, l);
	TEST_ASSERT_EQUAL_INT(-1, rval);
#endif
}


/**
 * @test rice_encoder
 */

void test_rice_encoder(void)
{
	uint32_t value, g_par, log2_g_par, cw, cw_len;
	const uint32_t MAX_GOLOMB_PAR = 0x80000000;

	/* test minimum Golomb parameter */
	value = 0; log2_g_par = ilog_2(MIN_NON_IMA_GOLOMB_PAR); g_par = 1U << log2_g_par; cw = ~0U;
	cw_len = rice_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(1, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x0, cw);

	value = 31;
	cw_len = rice_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFE, cw);

	/* test some arbitrary values */
	value = 0; log2_g_par = 4; g_par = 1U << log2_g_par; cw = ~0U;
	cw_len = rice_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(5, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x0, cw);

	value = 1;
	cw_len = rice_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(5, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x1, cw);

	value = 42;
	cw_len = rice_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(7, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x6a, cw);

	value = 446;
	cw_len = rice_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFEE, cw);

	value = 447;
	cw_len = rice_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFEF, cw);

	/* test maximum Golomb parameter for rice_encoder */
	value = 0; log2_g_par = ilog_2(MAX_GOLOMB_PAR); g_par = 1U << log2_g_par; cw = ~0U;
	cw_len = rice_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x0, cw);

	value = 1;
	cw_len = rice_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x1, cw);

	value = 0x7FFFFFFE;
	cw_len = rice_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x7FFFFFFE, cw);

	value = 0x7FFFFFFF;
	cw_len = rice_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x7FFFFFFF, cw);
}


/**
 * @test golomb_encoder
 */

void test_golomb_encoder(void)
{
	uint32_t value, g_par, log2_g_par, cw, cw_len;
	const uint32_t MAX_GOLOMB_PAR = 0x80000000;

	/* test minimum Golomb parameter */
	value = 0; g_par = MIN_NON_IMA_GOLOMB_PAR; log2_g_par = ilog_2(g_par); cw = ~0U;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(1, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x0, cw);

	value = 31;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFE, cw);

	/* error case: value larger than allowed */
	value = 32; g_par = 1; log2_g_par = ilog_2(g_par); cw = ~0U;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_GREATER_THAN_UINT(32, cw_len);

	/* error case: value larger than allowed */
	value = 33; g_par = 1; log2_g_par = ilog_2(g_par); cw = ~0U;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_GREATER_THAN_UINT(32, cw_len);

#if 0
	/* error case: value larger than allowed overflow in returned len */
	value = UINT32_MAX; g_par = 1; log2_g_par = ilog_2(g_par); cw = ~0U;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_GREATER_THAN_UINT(32, cw_len);
#endif

	/* test some arbitrary values with g_par = 16 */
	value = 0; g_par = 16; log2_g_par = ilog_2(g_par); cw = ~0U;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(5, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x0, cw);

	value = 1;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(5, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x1, cw);

	value = 42;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(7, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x6a, cw);

	value = 446;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFEE, cw);

	value = 447;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFEF, cw);


	/* test some arbitrary values with g_par = 3 */
	value = 0; g_par = 3; log2_g_par = ilog_2(g_par); cw = ~0U;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(2, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x0, cw);

	value = 1;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(3, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x2, cw);

	value = 42;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(16, cw_len);
	TEST_ASSERT_EQUAL_HEX(0xFFFC, cw);

	value = 44;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(17, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x1FFFB, cw);

	value = 88;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFA, cw);

	value = 89;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFB, cw);

	/* test some arbitrary values with g_par = 0x7FFFFFFF */
	value = 0; g_par = 0x7FFFFFFF; log2_g_par = ilog_2(g_par); cw = ~0U;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(31, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x0, cw);

	value = 1;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x2, cw);

	value = 0x7FFFFFFE;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x7FFFFFFF, cw);

	value = 0x7FFFFFFF;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x80000000, cw);

	/* test maximum Golomb parameter for golomb_encoder */
	value = 0; g_par = MAX_GOLOMB_PAR; log2_g_par = ilog_2(g_par); cw = ~0U;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x0, cw);

	value = 1; g_par = MAX_GOLOMB_PAR; log2_g_par = ilog_2(g_par); cw = ~0U;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x1, cw);

	value = 0x7FFFFFFE;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x7FFFFFFE, cw);

	value = 0x7FFFFFFF;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x7FFFFFFF, cw);

	value = 0; g_par = 0xFFFFFFFF; log2_g_par = ilog_2(g_par); cw = ~0U;
	cw_len = golomb_encoder(value, g_par, log2_g_par, &cw);
	TEST_ASSERT_EQUAL_INT(32, cw_len);
	TEST_ASSERT_EQUAL_HEX(0x0, cw);
}


/**
 * @test encode_value_zero
 */

void test_encode_value_zero(void)
{
	uint32_t data, model;
	uint32_t stream_len;
	struct encoder_setup setup = {0};
	uint32_t bitstream[3] = {0};

	/* setup the setup */
	setup.encoder_par1 = 1;
	setup.encoder_par2 = ilog_2(setup.encoder_par1);
	setup.spillover_par = 32;
	setup.max_data_bits = 32;
	setup.generate_cw_f = rice_encoder;
	setup.bitstream_adr = bitstream;
	setup.max_stream_len = sizeof(bitstream) * CHAR_BIT;

	stream_len = 0;

	data = 0; model = 0;
	stream_len = encode_value_zero(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(2, stream_len);
	TEST_ASSERT_EQUAL_HEX(0x80000000, be32_to_cpu(bitstream[0]));
	TEST_ASSERT_EQUAL_HEX(0x00000000, be32_to_cpu(bitstream[1]));
	TEST_ASSERT_EQUAL_HEX(0x00000000, be32_to_cpu(bitstream[2]));

	data = 5; model = 0;
	stream_len = encode_value_zero(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(14, stream_len);
	TEST_ASSERT_EQUAL_HEX(0xBFF80000, be32_to_cpu(bitstream[0]));
	TEST_ASSERT_EQUAL_HEX(0x00000000, be32_to_cpu(bitstream[1]));
	TEST_ASSERT_EQUAL_HEX(0x00000000, be32_to_cpu(bitstream[2]));

	data = 2; model = 7;
	stream_len = encode_value_zero(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(25, stream_len);
	TEST_ASSERT_EQUAL_HEX(0xBFFBFF00, be32_to_cpu(bitstream[0]));
	TEST_ASSERT_EQUAL_HEX(0x00000000, be32_to_cpu(bitstream[1]));
	TEST_ASSERT_EQUAL_HEX(0x00000000, be32_to_cpu(bitstream[2]));

	/* zero escape mechanism */
	data = 100; model = 42;
	/* (100-42)*2+1=117 -> cw 0 + 0x0000_0000_0000_0075 */
	stream_len = encode_value_zero(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(58, stream_len);
	TEST_ASSERT_EQUAL_HEX(0xBFFBFF00, be32_to_cpu(bitstream[0]));
	TEST_ASSERT_EQUAL_HEX(0x00001D40, be32_to_cpu(bitstream[1]));
	TEST_ASSERT_EQUAL_HEX(0x00000000, be32_to_cpu(bitstream[2]));

	/* test overflow */
	data = (uint32_t)INT32_MIN; model = 0;
	/* (INT32_MIN)*-2-1+1=0(overflow) -> cw 0 + 0x0000_0000_0000_0000 */
	stream_len = encode_value_zero(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(91, stream_len);
	TEST_ASSERT_EQUAL_HEX(0xBFFBFF00, be32_to_cpu(bitstream[0]));
	TEST_ASSERT_EQUAL_HEX(0x00001D40, be32_to_cpu(bitstream[1]));
	TEST_ASSERT_EQUAL_HEX(0x00000000, be32_to_cpu(bitstream[2]));

	/* small buffer error */
	data = 23; model = 26;
	stream_len = encode_value_zero(data, model, stream_len, &setup);
	TEST_ASSERT_TRUE(cmp_is_error(stream_len));
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUFFER, cmp_get_error_code(stream_len));

	/* reset bitstream to all bits set */
	bitstream[0] = ~0U;
	bitstream[1] = ~0U;
	bitstream[2] = ~0U;
	stream_len = 0;

	/* we use now values with maximum 6 bits */
	setup.max_data_bits = 6;

	/* lowest value before zero encoding */
	data = 53; model = 38;
	stream_len = encode_value_zero(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(32, stream_len);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFE, be32_to_cpu(bitstream[0]));
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFF, be32_to_cpu(bitstream[1]));
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFF, be32_to_cpu(bitstream[2]));

	/* lowest value with zero encoding */
	data = 0; model = 16;
	stream_len = encode_value_zero(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(39, stream_len);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFE, be32_to_cpu(bitstream[0]));
	TEST_ASSERT_EQUAL_HEX(0x41FFFFFF, be32_to_cpu(bitstream[1]));
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFF, be32_to_cpu(bitstream[2]));

	/* maximum positive value to encode */
	data = 31; model = 0;
	stream_len = encode_value_zero(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(46, stream_len);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFE, be32_to_cpu(bitstream[0]));
	TEST_ASSERT_EQUAL_HEX(0x40FFFFFF, be32_to_cpu(bitstream[1]));
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFF, be32_to_cpu(bitstream[2]));

	/* maximum negative value to encode */
	data = 0; model = 32;
	stream_len = encode_value_zero(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(53, stream_len);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFE, be32_to_cpu(bitstream[0]));
	TEST_ASSERT_EQUAL_HEX(0x40FC07FF, be32_to_cpu(bitstream[1]));
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFF, be32_to_cpu(bitstream[2]));

	/* small buffer error when creating the zero escape symbol*/
	bitstream[0] = 0;
	bitstream[1] = 0;
	bitstream[2] = 0;
	stream_len = 32;
	setup.max_stream_len = 32;
	data = 31; model = 0;
	stream_len = encode_value_zero(data, model, stream_len, &setup);
	TEST_ASSERT_TRUE(cmp_is_error(stream_len));
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUFFER, cmp_get_error_code(stream_len));
	TEST_ASSERT_EQUAL_HEX(0, be32_to_cpu(bitstream[0]));
	TEST_ASSERT_EQUAL_HEX(0, be32_to_cpu(bitstream[1]));
	TEST_ASSERT_EQUAL_HEX(0, be32_to_cpu(bitstream[2]));
}


/**
 * @test encode_value_multi
 */

void test_encode_value_multi(void)
{
	uint32_t data, model;
	uint32_t stream_len;
	struct encoder_setup setup = {0};
	uint32_t bitstream[4] = {0};

	/* setup the setup */
	setup.encoder_par1 = 1;
	setup.encoder_par2 = ilog_2(setup.encoder_par1);
	setup.spillover_par = 16;
	setup.max_data_bits = 32;
	setup.generate_cw_f = golomb_encoder;
	setup.bitstream_adr = bitstream;
	setup.max_stream_len = sizeof(bitstream) * CHAR_BIT;

	stream_len = 0;

	data = 0; model = 0;
	stream_len = encode_value_multi(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(1, stream_len);
	TEST_ASSERT_EQUAL_HEX(0x00000000, be32_to_cpu(bitstream[0]));
	TEST_ASSERT_EQUAL_HEX(0x00000000, be32_to_cpu(bitstream[1]));
	TEST_ASSERT_EQUAL_HEX(0x00000000, be32_to_cpu(bitstream[2]));
	TEST_ASSERT_EQUAL_HEX(0x00000000, be32_to_cpu(bitstream[3]));

	data = 0; model = 1;
	stream_len = encode_value_multi(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(3, stream_len);
	TEST_ASSERT_EQUAL_HEX(0x40000000, be32_to_cpu(bitstream[0]));
	TEST_ASSERT_EQUAL_HEX(0x00000000, be32_to_cpu(bitstream[1]));
	TEST_ASSERT_EQUAL_HEX(0x00000000, be32_to_cpu(bitstream[2]));
	TEST_ASSERT_EQUAL_HEX(0x00000000, be32_to_cpu(bitstream[3]));

	data = 1+23; model = 0+23;
	stream_len = encode_value_multi(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(6, stream_len);
	TEST_ASSERT_EQUAL_HEX(0x58000000, be32_to_cpu(bitstream[0]));
	TEST_ASSERT_EQUAL_HEX(0x00000000, be32_to_cpu(bitstream[1]));
	TEST_ASSERT_EQUAL_HEX(0x00000000, be32_to_cpu(bitstream[2]));
	TEST_ASSERT_EQUAL_HEX(0x00000000, be32_to_cpu(bitstream[3]));

	/* highest value without multi outlier encoding */
	data = 0+42; model = 8+42;
	stream_len = encode_value_multi(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(22, stream_len);
	TEST_ASSERT_EQUAL_HEX(0x5BFFF800, be32_to_cpu(bitstream[0]));
	TEST_ASSERT_EQUAL_HEX(0x00000000, be32_to_cpu(bitstream[1]));
	TEST_ASSERT_EQUAL_HEX(0x00000000, be32_to_cpu(bitstream[2]));
	TEST_ASSERT_EQUAL_HEX(0x00000000, be32_to_cpu(bitstream[3]));

	/* lowest value with multi outlier encoding */
	data = 8+42; model = 0+42;
	stream_len = encode_value_multi(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(41, stream_len);
	TEST_ASSERT_EQUAL_HEX(0x5BFFFBFF, be32_to_cpu(bitstream[0]));
	TEST_ASSERT_EQUAL_HEX(0xFC000000, be32_to_cpu(bitstream[1]));
	TEST_ASSERT_EQUAL_HEX(0x00000000, be32_to_cpu(bitstream[2]));
	TEST_ASSERT_EQUAL_HEX(0x00000000, be32_to_cpu(bitstream[3]));

	/* highest value with multi outlier encoding */
	data = (uint32_t)INT32_MIN; model = 0;
	stream_len = encode_value_multi(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(105, stream_len);
	TEST_ASSERT_EQUAL_HEX(0x5BFFFBFF, be32_to_cpu(bitstream[0]));
	TEST_ASSERT_EQUAL_HEX(0xFC7FFFFF, be32_to_cpu(bitstream[1]));
	TEST_ASSERT_EQUAL_HEX(0xFF7FFFFF, be32_to_cpu(bitstream[2]));
	TEST_ASSERT_EQUAL_HEX(0xF7800000, be32_to_cpu(bitstream[3]));

	/* small buffer error */
	data = 0; model = 38;
	stream_len = encode_value_multi(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUFFER, cmp_get_error_code(stream_len));

	/* small buffer error when creating the multi escape symbol*/
	bitstream[0] = 0;
	bitstream[1] = 0;
	setup.max_stream_len = 32;

	stream_len = 32;
	data = 31; model = 0;
	stream_len = encode_value_multi(data, model, stream_len, &setup);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUFFER, cmp_get_error_code(stream_len));
	TEST_ASSERT_EQUAL_HEX(0, bitstream[0]);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[1]);
}


/**
 * @brief put the value unencoded with setup->cmp_par_1 bits without any changes
 *      in the bitstream
 *
 * @param value		value to put unchanged in the bitstream
 *			(setup->cmp_par_1 how many bits of the value are used)
 * @param unused	this parameter is ignored
 * @param stream_len	length of the bitstream in bits
 * @param setup		pointer to the encoder setup
 *
 * @returns the bit length of the bitstream with the added unencoded value on
 *	success; negative on error
 */

static uint32_t encode_value_none(uint32_t value, uint32_t unused, uint32_t stream_len,
				  const struct encoder_setup *setup)
{
	(void)(unused);

	return put_n_bits32(value, setup->encoder_par1, stream_len,
			    setup->bitstream_adr, setup->max_stream_len);
}


/**
 * @test encode_value
 */

void test_encode_value(void)
{
	struct encoder_setup setup = {0};
	uint32_t bitstream[4] = {0};
	uint32_t data, model;
	uint32_t cmp_size;

	setup.encode_method_f = encode_value_none;
	setup.bitstream_adr = bitstream;
	setup.max_stream_len = 128;
	cmp_size = 0;

	/* test 32 bit input */
	setup.encoder_par1 = 32;
	setup.max_data_bits = 32;
	setup.lossy_par = 0;

	data = 0; model = 0;
	cmp_size = encode_value(data, model, cmp_size, &setup);
	TEST_ASSERT_EQUAL_UINT(32, cmp_size);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[0]);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[1]);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[2]);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[3]);

	data = UINT32_MAX; model = 0;
	cmp_size = encode_value(data, model, cmp_size, &setup);
	TEST_ASSERT_EQUAL_UINT(64, cmp_size);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[0]);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFF, bitstream[1]);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[2]);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[3]);

	/* test rounding */
	setup.lossy_par = 1;
	data = UINT32_MAX; model = 0;
	cmp_size = encode_value(data, model, cmp_size, &setup);
	TEST_ASSERT_EQUAL_UINT(96, cmp_size);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[0]);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFF, be32_to_cpu(bitstream[1]));
	TEST_ASSERT_EQUAL_HEX(0x7FFFFFFF, be32_to_cpu(bitstream[2]));
	TEST_ASSERT_EQUAL_HEX(0, bitstream[3]);

	setup.lossy_par = 2;
	data = 0x3; model = 0;
	cmp_size = encode_value(data, model, cmp_size, &setup);
	TEST_ASSERT_EQUAL_UINT(128, cmp_size);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[0]);
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFF, bitstream[1]);
	TEST_ASSERT_EQUAL_HEX(0x7FFFFFFF, be32_to_cpu(bitstream[2]));
	TEST_ASSERT_EQUAL_HEX(0x00000000, bitstream[3]);

	/* small buffer error bitstream can not hold more data*/
	cmp_size = encode_value(data, model, cmp_size, &setup);
	TEST_ASSERT_EQUAL_UINT(CMP_ERROR_SMALL_BUFFER, cmp_get_error_code(cmp_size));

	/* reset bitstream */
	bitstream[0] = 0;
	bitstream[1] = 0;
	bitstream[2] = 0;
	bitstream[3] = 0;
	cmp_size = 0;

	/* test 31 bit input */
	setup.encoder_par1 = 31;
	setup.max_data_bits = 31;
	setup.lossy_par = 0;

	data = 0; model = 0;
	cmp_size = encode_value(data, model, cmp_size, &setup);
	TEST_ASSERT_EQUAL_UINT(31, cmp_size);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[0]);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[1]);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[2]);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[3]);

	data = 0x7FFFFFFF; model = 0;
	cmp_size = encode_value(data, model, cmp_size, &setup);
	TEST_ASSERT_EQUAL_UINT(62, cmp_size);
	TEST_ASSERT_EQUAL_HEX(0x00000001, be32_to_cpu(bitstream[0]));
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFC, be32_to_cpu(bitstream[1]));
	TEST_ASSERT_EQUAL_HEX(0, bitstream[2]);
	TEST_ASSERT_EQUAL_HEX(0, bitstream[3]);

	/* round = 1 */
	setup.lossy_par = 1;
	data = UINT32_MAX; model = UINT32_MAX;
	cmp_size = encode_value(data, model, cmp_size, &setup);
	TEST_ASSERT_EQUAL_UINT(93, cmp_size);
	TEST_ASSERT_EQUAL_HEX(0x00000001, be32_to_cpu(bitstream[0]));
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFFF, be32_to_cpu(bitstream[1]));
	TEST_ASSERT_EQUAL_HEX(0xFFFFFFF8, be32_to_cpu(bitstream[2]));
	TEST_ASSERT_EQUAL_HEX(0, bitstream[3]);

	/* data are bigger than max_data_bits */
	setup.lossy_par = 0;
	data = UINT32_MAX; model = 0;
	cmp_size = encode_value(data, model, cmp_size, &setup);
	TEST_ASSERT_EQUAL_UINT(CMP_ERROR_DATA_VALUE_TOO_LARGE, cmp_get_error_code(cmp_size));

	/* model are bigger than max_data_bits */
	setup.lossy_par = 0;
	cmp_size = 93;
	data = 0; model = UINT32_MAX;
	cmp_size = encode_value(data, model, cmp_size, &setup);
	TEST_ASSERT_EQUAL_UINT(CMP_ERROR_DATA_VALUE_TOO_LARGE, cmp_get_error_code(cmp_size));
}


/**
 * @test compress_imagette
 */

void test_compress_imagette_diff(void)
{
	uint16_t data[] = {0xFFFF, 1, 0, 42, 0x8000, 0x7FFF, 0xFFFF};
	uint32_t output_buf[3] = {0xFFFF, 0xFFFF, 0xFFFF};
	struct rdcu_cfg rcfg = {0};
	int error;
	uint32_t cmp_size;
	struct cmp_info info;

	uint32_t golomb_par = 1;
	uint32_t spill = 8;
	uint32_t samples = 7;

	error = rdcu_cfg_create(&rcfg, CMP_MODE_DIFF_ZERO, 8, CMP_LOSSLESS);
	TEST_ASSERT_FALSE(error);
	rcfg.input_buf = data;
	rcfg.samples = samples;
	rcfg.icu_output_buf = output_buf;
	rcfg.buffer_length = samples;
	rcfg.golomb_par = golomb_par;
	rcfg.spill = spill;


	cmp_size = compress_like_rdcu(&rcfg, NULL);
	TEST_ASSERT_EQUAL_INT(66, cmp_size);
	TEST_ASSERT_EQUAL_HEX(0xDF6002AB, be32_to_cpu(output_buf[0]));
	TEST_ASSERT_EQUAL_HEX(0xFEB70000, be32_to_cpu(output_buf[1]));
	TEST_ASSERT_EQUAL_HEX(0x00000000, be32_to_cpu(output_buf[2]));

	rcfg.ap1_golomb_par = 2;
	rcfg.ap1_spill = 1000;
	rcfg.ap2_golomb_par = 1;
	rcfg.ap2_spill = 0;
	cmp_size = compress_like_rdcu(&rcfg, &info);
	TEST_ASSERT_EQUAL_INT(66, cmp_size);
	TEST_ASSERT_EQUAL_HEX(0xDF6002AB, be32_to_cpu(output_buf[0]));
	TEST_ASSERT_EQUAL_HEX(0xFEB70000, be32_to_cpu(output_buf[1]));
	TEST_ASSERT_EQUAL_HEX(0x00000000, be32_to_cpu(output_buf[2]));
	TEST_ASSERT_EQUAL_INT(CMP_MODE_DIFF_ZERO, info.cmp_mode_used);
	TEST_ASSERT_EQUAL_INT(8, info.spill_used);
	TEST_ASSERT_EQUAL_INT(1, info.golomb_par_used);
	TEST_ASSERT_EQUAL_INT(7, info.samples_used);
	TEST_ASSERT_EQUAL_INT(66, info.cmp_size);
	TEST_ASSERT_EQUAL_INT(0, info.ap1_cmp_size);
	TEST_ASSERT_EQUAL_INT(0, info.ap2_cmp_size);
	TEST_ASSERT_EQUAL_INT(0, info.rdcu_new_model_adr_used);
	TEST_ASSERT_EQUAL_INT(0, info.rdcu_cmp_adr_used);
	TEST_ASSERT_EQUAL_INT(8, info.model_value_used);
	TEST_ASSERT_EQUAL_INT(0, info.round_used);
	TEST_ASSERT_EQUAL_INT(0, info.cmp_err);

	rcfg.ap1_golomb_par = 0;
	rcfg.ap1_spill = 1000;
	rcfg.ap2_golomb_par = 0;
	rcfg.ap2_spill = 0;
	cmp_size = compress_like_rdcu(&rcfg, &info);
	TEST_ASSERT_EQUAL_INT(66, cmp_size);
	TEST_ASSERT_EQUAL_HEX(0xDF6002AB, be32_to_cpu(output_buf[0]));
	TEST_ASSERT_EQUAL_HEX(0xFEB70000, be32_to_cpu(output_buf[1]));
	TEST_ASSERT_EQUAL_HEX(0x00000000, be32_to_cpu(output_buf[2]));
	TEST_ASSERT_EQUAL_INT(CMP_MODE_DIFF_ZERO, info.cmp_mode_used);
	TEST_ASSERT_EQUAL_INT(8, info.spill_used);
	TEST_ASSERT_EQUAL_INT(1, info.golomb_par_used);
	TEST_ASSERT_EQUAL_INT(7, info.samples_used);
	TEST_ASSERT_EQUAL_INT(66, info.cmp_size);
	TEST_ASSERT_EQUAL_INT(0, info.ap1_cmp_size);
	TEST_ASSERT_EQUAL_INT(0, info.ap2_cmp_size);
	TEST_ASSERT_EQUAL_INT(0, info.rdcu_new_model_adr_used);
	TEST_ASSERT_EQUAL_INT(0, info.rdcu_cmp_adr_used);
	TEST_ASSERT_EQUAL_INT(8, info.model_value_used);
	TEST_ASSERT_EQUAL_INT(0, info.round_used);
	TEST_ASSERT_EQUAL_INT(0, info.cmp_err);

	/* small buffer error */
	rcfg.ap1_golomb_par = 1;
	rcfg.ap1_spill = 8;
	rcfg.ap2_golomb_par = 1;
	rcfg.ap2_spill = 8;
	rcfg.buffer_length = 3;
	cmp_size = compress_like_rdcu(&rcfg, &info);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUFFER, cmp_get_error_code(cmp_size));
	TEST_ASSERT_EQUAL_HEX(0xDF6002AB, be32_to_cpu(output_buf[0]));
	TEST_ASSERT_EQUAL_HEX(0xFEB70000, be32_to_cpu(output_buf[1]));
	TEST_ASSERT_EQUAL_HEX(0x00000000, be32_to_cpu(output_buf[2]));
	TEST_ASSERT_EQUAL_INT(CMP_MODE_DIFF_ZERO, info.cmp_mode_used);
	TEST_ASSERT_EQUAL_INT(8, info.spill_used);
	TEST_ASSERT_EQUAL_INT(1, info.golomb_par_used);
	TEST_ASSERT_EQUAL_INT(7, info.samples_used);
	TEST_ASSERT_EQUAL_INT(0, info.cmp_size);
	TEST_ASSERT_EQUAL_INT(0, info.ap1_cmp_size);
	TEST_ASSERT_EQUAL_INT(0, info.ap2_cmp_size);
	TEST_ASSERT_EQUAL_INT(0, info.rdcu_new_model_adr_used);
	TEST_ASSERT_EQUAL_INT(0, info.rdcu_cmp_adr_used);
	TEST_ASSERT_EQUAL_INT(8, info.model_value_used);
	TEST_ASSERT_EQUAL_INT(0, info.round_used);
	TEST_ASSERT_EQUAL_INT(0x1, info.cmp_err);

	/* test: icu_output_buf = NULL */
	rcfg.icu_output_buf = NULL;
	cmp_size = compress_like_rdcu(&rcfg, NULL);
	TEST_ASSERT_EQUAL_INT(66, cmp_size);
}


/**
 * @test compress_imagette
 */

void test_compress_imagette_model(void)
{
	uint16_t data[]  = {0x0000, 0x0001, 0x0042, 0x8000, 0x7FFF, 0xFFFF, 0xFFFF};
	uint16_t model[] = {0x0000, 0xFFFF, 0xF301, 0x8FFF, 0x0000, 0xFFFF, 0x0000};
	uint16_t model_up[7] = {0};
	uint32_t output_buf[4] = {~0U, ~0U, ~0U, ~0U};
	struct rdcu_cfg rcfg = {0};
	uint32_t model_value = 8;
	uint32_t samples = 7;
	uint32_t buffer_length = 8;
	uint32_t golomb_par = 3;
	uint32_t spill = 8;
	uint32_t cmp_size;
	int error;

	error = rdcu_cfg_create(&rcfg, CMP_MODE_MODEL_MULTI, model_value, CMP_LOSSLESS);
	TEST_ASSERT_FALSE(error);
	rcfg.input_buf = data;
	rcfg.samples = samples;
	rcfg.model_buf = model;
	rcfg.icu_new_model_buf = model_up;
	rcfg.icu_output_buf = output_buf;
	rcfg.buffer_length = buffer_length;
	rcfg.golomb_par = golomb_par;
	rcfg.spill = spill;

	cmp_size = compress_like_rdcu(&rcfg, NULL);

	TEST_ASSERT_EQUAL_INT(76, cmp_size);
	TEST_ASSERT_EQUAL_HEX(0x2BDB4F5E, be32_to_cpu(output_buf[0]));
	TEST_ASSERT_EQUAL_HEX(0xDFF5F9FF, be32_to_cpu(output_buf[1]));
	TEST_ASSERT_EQUAL_HEX(0xEC200000, be32_to_cpu(output_buf[2]));

	TEST_ASSERT_EQUAL_HEX(0x0000, model_up[0]);
	TEST_ASSERT_EQUAL_HEX(0x8000, model_up[1]);
	TEST_ASSERT_EQUAL_HEX(0x79A1, model_up[2]);
	TEST_ASSERT_EQUAL_HEX(0x87FF, model_up[3]);
	TEST_ASSERT_EQUAL_HEX(0x3FFF, model_up[4]);
	TEST_ASSERT_EQUAL_HEX(0xFFFF, model_up[5]);
	TEST_ASSERT_EQUAL_HEX(0x7FFF, model_up[6]);


	/* error case: model mode without model data */
	rcfg.model_buf = NULL; /* this is the error */
	cmp_size = compress_like_rdcu(&rcfg, NULL);
	TEST_ASSERT_TRUE(cmp_is_error(cmp_size));
	TEST_ASSERT_EQUAL(CMP_ERROR_PAR_NO_MODEL, cmp_get_error_code(cmp_size));
}


/**
 * @test compress_imagette
 */

void test_compress_imagette_raw(void)
{
	uint16_t data[] = {0x0, 0x1, 0x23, 0x42, (uint16_t)INT16_MIN, INT16_MAX, UINT16_MAX};
	void *output_buf = malloc(7*sizeof(uint16_t));
	uint16_t cmp_data[7];
	struct rdcu_cfg rcfg = {0};
	uint32_t cmp_size;

	rcfg.cmp_mode = CMP_MODE_RAW;
	rcfg.model_buf = NULL;
	rcfg.input_buf = data;
	rcfg.samples = 7;
	rcfg.icu_output_buf = output_buf;
	rcfg.buffer_length = 7;

	cmp_size = compress_like_rdcu(&rcfg, NULL);
	memcpy(cmp_data, output_buf, sizeof(cmp_data));
	TEST_ASSERT_EQUAL_INT(7*16, cmp_size);
	TEST_ASSERT_EQUAL_HEX16(0x0, be16_to_cpu(cmp_data[0]));
	TEST_ASSERT_EQUAL_HEX16(0x1, be16_to_cpu(cmp_data[1]));
	TEST_ASSERT_EQUAL_HEX16(0x23, be16_to_cpu(cmp_data[2]));
	TEST_ASSERT_EQUAL_HEX16(0x42, be16_to_cpu(cmp_data[3]));
	TEST_ASSERT_EQUAL_HEX16(INT16_MIN, be16_to_cpu(cmp_data[4]));
	TEST_ASSERT_EQUAL_HEX16(INT16_MAX, be16_to_cpu(cmp_data[5]));
	TEST_ASSERT_EQUAL_HEX16(UINT16_MAX, be16_to_cpu(cmp_data[6]));


	/* compressed data buf = NULL test */
	memset(&rcfg, 0, sizeof(rcfg));
	rcfg.input_buf = data;
	rcfg.samples = 7;
	rcfg.icu_output_buf = NULL;
	rcfg.buffer_length = 7;

	cmp_size = compress_like_rdcu(&rcfg, NULL);
	TEST_ASSERT_EQUAL_INT(7*16, cmp_size);

	/* error case: cfg = NULL */
	cmp_size = compress_like_rdcu(NULL, NULL);
	TEST_ASSERT_TRUE(cmp_is_error(cmp_size));
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_GENERIC, cmp_get_error_code(cmp_size));

	/* error case: input_buf = NULL */
	memset(&rcfg, 0, sizeof(rcfg));
	rcfg.input_buf = NULL; /* no data to compress */
	rcfg.samples = 7;
	rcfg.icu_output_buf = output_buf;
	rcfg.buffer_length = 7;

	cmp_size = compress_like_rdcu(&rcfg, NULL);
	TEST_ASSERT_TRUE(cmp_is_error(cmp_size));
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_CHUNK_NULL, cmp_get_error_code(cmp_size));


	/* error case: compressed data buffer to small */
	memset(&rcfg, 0, sizeof(rcfg));
	rcfg.input_buf = data;
	rcfg.samples = 7;
	rcfg.icu_output_buf = output_buf;
	rcfg.buffer_length = 6; /* the buffer is to small */

	cmp_size = compress_like_rdcu(&rcfg, NULL);
	TEST_ASSERT_TRUE(cmp_is_error(cmp_size));
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUFFER, cmp_get_error_code(cmp_size));

	free(output_buf);
}


/**
 * @test compress_imagette
 */

void test_compress_imagette_error_cases(void)
{
	uint16_t data[] = {0xFFFF, 1, 0, 42, 0x8000, 0x7FFF, 0xFFFF};
	uint32_t output_buf[2] = {0xFFFF, 0xFFFF};
	struct rdcu_cfg rcfg = {0};
	uint32_t cmp_size;

	rcfg.cmp_mode = CMP_MODE_DIFF_ZERO;
	rcfg.input_buf = data;
	rcfg.samples = 0;  /* nothing to compress */
	rcfg.golomb_par = 1;
	rcfg.spill = 8;
	rcfg.icu_output_buf = NULL;
	rcfg.buffer_length = 0;

	cmp_size = compress_like_rdcu(&rcfg, NULL);
	TEST_ASSERT_EQUAL_INT(0, cmp_size);


	/* compressed data buffer to small test */
	rcfg.cmp_mode = CMP_MODE_DIFF_ZERO;
	rcfg.input_buf = data;
	rcfg.samples = 7;
	rcfg.golomb_par = 1;
	rcfg.spill = 8;
	rcfg.icu_output_buf = (uint32_t *)output_buf;
	rcfg.buffer_length = 4;

	cmp_size = compress_like_rdcu(&rcfg, NULL);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUFFER, cmp_get_error_code(cmp_size));

	/* compressed data buffer to small test part 2 */
	rcfg.cmp_mode = CMP_MODE_DIFF_ZERO;
	rcfg.input_buf = data;
	rcfg.samples = 7;
	rcfg.golomb_par = 1;
	rcfg.spill = 8;
	rcfg.icu_output_buf = (uint32_t *)output_buf;
	rcfg.buffer_length = 1;

	cmp_size = compress_like_rdcu(&rcfg, NULL);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUFFER, cmp_get_error_code(cmp_size));


	/* test unknown cmp_mode */
	rcfg.cmp_mode = (enum cmp_mode)(MAX_CMP_MODE+1);
	rcfg.input_buf = data;
	rcfg.samples = 2;
	rcfg.golomb_par = 1;
	rcfg.spill = 8;
	rcfg.icu_output_buf = (uint32_t *)output_buf;
	rcfg.buffer_length = 4;

	cmp_size = compress_like_rdcu(&rcfg, NULL);
	TEST_ASSERT_TRUE(cmp_is_error(cmp_size));
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_PAR_GENERIC, cmp_get_error_code(cmp_size));

	/* test golomb_par = 0 */
	rcfg.cmp_mode = CMP_MODE_DIFF_ZERO;
	rcfg.input_buf = data;
	rcfg.samples = 2;
	rcfg.golomb_par = 0;
	rcfg.spill = 8;
	rcfg.icu_output_buf = (uint32_t *)output_buf;
	rcfg.buffer_length = 4;

	cmp_size = compress_like_rdcu(&rcfg, NULL);
	TEST_ASSERT_TRUE(cmp_is_error(cmp_size));
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_PAR_SPECIFIC, cmp_get_error_code(cmp_size));

	/* test golomb_par to high */
	rcfg.cmp_mode = CMP_MODE_DIFF_ZERO;
	rcfg.input_buf = data;
	rcfg.samples = 2;
	rcfg.golomb_par = MAX_CHUNK_CMP_PAR+1;
	rcfg.spill = 8;
	rcfg.icu_output_buf = (uint32_t *)output_buf;
	rcfg.buffer_length = 4;

	cmp_size = compress_like_rdcu(&rcfg, NULL);
	TEST_ASSERT_TRUE(cmp_is_error(cmp_size));
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_PAR_SPECIFIC, cmp_get_error_code(cmp_size));

	/* round to high */
	rcfg.cmp_mode = CMP_MODE_DIFF_ZERO;
	rcfg.input_buf = data;
	rcfg.samples = 2;
	rcfg.golomb_par = MAX_CHUNK_CMP_PAR;
	rcfg.spill = 8;
	rcfg.icu_output_buf = (uint32_t *)output_buf;
	rcfg.buffer_length = 4;
	rcfg.round = MAX_ICU_ROUND+1;

	cmp_size = compress_like_rdcu(&rcfg, NULL);
	TEST_ASSERT_TRUE(cmp_is_error(cmp_size));
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_PAR_GENERIC, cmp_get_error_code(cmp_size));

	/* model_value to high */
	rcfg.cmp_mode = CMP_MODE_MODEL_ZERO;
	rcfg.input_buf = data;
	rcfg.samples = 2;
	rcfg.golomb_par = MAX_CHUNK_CMP_PAR;
	rcfg.spill = 8;
	rcfg.icu_output_buf = (uint32_t *)output_buf;
	rcfg.buffer_length = 4;
	rcfg.round = MAX_ICU_ROUND;
	rcfg.model_value = MAX_MODEL_VALUE+1;

	cmp_size = compress_like_rdcu(&rcfg, NULL);
	TEST_ASSERT_TRUE(cmp_is_error(cmp_size));
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_PAR_GENERIC, cmp_get_error_code(cmp_size));
}


/**
 * @test pad_bitstream
 */

void test_pad_bitstream(void)
{
	struct cmp_cfg cfg = {0};
	uint32_t cmp_size;
	uint32_t cmp_size_return;
	uint32_t cmp_data[3];
	const int MAX_BIT_LEN = 96;

	memset(cmp_data, 0xFF, sizeof(cmp_data));
	cfg.dst = cmp_data;
	cfg.data_type = DATA_TYPE_IMAGETTE; /* 16 bit samples */
	cfg.stream_size = sizeof(cmp_data); /* 6 * 16 bit samples -> 3 * 32 bit */

	/* test negative cmp_size */
	cmp_size = -1U;
	cmp_size_return = pad_bitstream(&cfg, cmp_size);
	TEST_ASSERT_EQUAL_UINT32(-1U, cmp_size_return);
	cmp_size = -3U;
	cmp_size_return = pad_bitstream(&cfg, cmp_size);
	TEST_ASSERT_EQUAL_UINT32(-3U, cmp_size_return);

	/* test RAW_MODE */
	cfg.cmp_mode = CMP_MODE_RAW;
	cmp_size = MAX_BIT_LEN;
	cmp_size_return = pad_bitstream(&cfg, cmp_size);
	TEST_ASSERT_EQUAL_INT(MAX_BIT_LEN, cmp_size_return);
	TEST_ASSERT_EQUAL_INT(cmp_data[0], 0xFFFFFFFF);
	TEST_ASSERT_EQUAL_INT(cmp_data[1], 0xFFFFFFFF);
	TEST_ASSERT_EQUAL_INT(cmp_data[2], 0xFFFFFFFF);

	/* test Normal operation */
	cfg.cmp_mode = CMP_MODE_MODEL_MULTI;
	cmp_size = 0;
	/* set the first 32 bits zero no change should occur */
	cmp_size = put_n_bits32(0, 32, cmp_size, cfg.dst, MAX_BIT_LEN);
	cmp_size_return = pad_bitstream(&cfg, cmp_size);
	TEST_ASSERT_EQUAL_INT(cmp_size, cmp_size_return);
	TEST_ASSERT_EQUAL_INT(cmp_data[0], 0);
	TEST_ASSERT_EQUAL_INT(cmp_data[1], 0xFFFFFFFF);
	TEST_ASSERT_EQUAL_INT(cmp_data[2], 0xFFFFFFFF);

	/* set the first 33 bits zero; and checks the padding  */
	cmp_size = put_n_bits32(0, 1, cmp_size, cfg.dst, MAX_BIT_LEN);
	cmp_size_return = pad_bitstream(&cfg, cmp_size);
	TEST_ASSERT_EQUAL_INT(cmp_size, cmp_size_return);
	TEST_ASSERT_EQUAL_INT(cmp_data[0], 0);
	TEST_ASSERT_EQUAL_INT(cmp_data[1], 0);
	TEST_ASSERT_EQUAL_INT(cmp_data[2], 0xFFFFFFFF);

	/* set the first 63 bits zero; and checks the padding  */
	cmp_data[1] = 0xFFFFFFFF;
	cmp_size = 32;
	cmp_size = put_n_bits32(0, 31, cmp_size, cfg.dst, MAX_BIT_LEN);
	cmp_size_return = pad_bitstream(&cfg, cmp_size);
	TEST_ASSERT_EQUAL_INT(cmp_size, cmp_size_return);
	TEST_ASSERT_EQUAL_INT(cmp_data[0], 0);
	TEST_ASSERT_EQUAL_INT(cmp_data[1], 0);
	TEST_ASSERT_EQUAL_INT(cmp_data[2], 0xFFFFFFFF);

	/* error case the rest of the compressed data are to small for a 32 bit
	 * access  */
	cfg.stream_size -= 1;
	cmp_size = 64;
	cmp_size = put_n_bits32(0, 1, cmp_size, cfg.dst, MAX_BIT_LEN);
	cmp_size_return = pad_bitstream(&cfg, cmp_size);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUFFER, cmp_get_error_code(cmp_size_return));
}


/**
 * @test compress_data_internal
 */

void test_compress_data_internal_error_cases(void)
{
	struct cmp_cfg cfg = {0};
	uint32_t cmp_size;

	uint16_t data[2] = {0, 0};
	uint32_t dst[3] = {0};

	cfg.data_type = DATA_TYPE_IMAGETTE;
	cfg.src = data;
	cfg.samples = 2;
	cfg.dst = dst;
	cfg.stream_size = sizeof(dst);


	cmp_size = compress_data_internal(&cfg, -2U);
	TEST_ASSERT_EQUAL_INT(-2U, cmp_size);

	cmp_size = compress_data_internal(&cfg, 7);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_GENERIC, cmp_get_error_code(cmp_size));

	cmp_size = compress_data_internal(&cfg, 7);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_GENERIC, cmp_get_error_code(cmp_size));

	cfg.data_type = DATA_TYPE_UNKNOWN;
	cmp_size = compress_data_internal(&cfg, 0);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_INT_DATA_TYPE_UNSUPPORTED, cmp_get_error_code(cmp_size));

	cfg.cmp_mode = CMP_MODE_DIFF_MULTI;
	cmp_size = compress_data_internal(&cfg, 0);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_INT_DATA_TYPE_UNSUPPORTED, cmp_get_error_code(cmp_size));

	cmp_size = compress_data_internal(&cfg, 0);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_INT_DATA_TYPE_UNSUPPORTED, cmp_get_error_code(cmp_size));

	cfg.data_type = DATA_TYPE_F_FX_EFX;
	cmp_size = compress_data_internal(&cfg, 0);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_INT_DATA_TYPE_UNSUPPORTED, cmp_get_error_code(cmp_size));

	cfg.data_type = DATA_TYPE_F_FX_NCOB;
	cmp_size = compress_data_internal(&cfg, 0);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_INT_DATA_TYPE_UNSUPPORTED, cmp_get_error_code(cmp_size));

	cfg.data_type = DATA_TYPE_F_FX_EFX_NCOB_ECOB;
	cmp_size = compress_data_internal(&cfg, 0);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_INT_DATA_TYPE_UNSUPPORTED, cmp_get_error_code(cmp_size));

	cfg.data_type = DATA_TYPE_F_FX_EFX_NCOB_ECOB;
	cmp_size = compress_data_internal(&cfg, 0);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_INT_DATA_TYPE_UNSUPPORTED, cmp_get_error_code(cmp_size));

	cfg.data_type = 101;
	cmp_size = compress_data_internal(&cfg, 0);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_INT_DATA_TYPE_UNSUPPORTED, cmp_get_error_code(cmp_size));
}


/**
 * @test compress_chunk
 */

void test_compress_chunk_raw_singel_col(void)
{
	enum {	DATA_SIZE = 2*sizeof(struct s_fx),
		CHUNK_SIZE = COLLECTION_HDR_SIZE + DATA_SIZE
	};
	uint8_t chunk[CHUNK_SIZE];
	struct collection_hdr *col = (struct collection_hdr *)chunk;
	struct s_fx *data = (struct s_fx *)col->entry;
	struct cmp_par cmp_par = {0};
	uint32_t *dst;
	uint32_t cmp_size;
	uint32_t dst_capacity = 43; /* random non zero value */

	/* create a chunk with a single collection */
	memset(col, 0, COLLECTION_HDR_SIZE);
	TEST_ASSERT_FALSE(cmp_col_set_subservice(col, SST_NCxx_S_SCIENCE_S_FX));
	TEST_ASSERT_FALSE(cmp_col_set_data_length(col, DATA_SIZE));
	data[0].exp_flags = 0;
	data[0].fx = 1;
	data[1].exp_flags = 0xF0;
	data[1].fx = 0xABCDE0FF;


	/* compress the data */
	cmp_par.cmp_mode = CMP_MODE_RAW;
	dst = NULL;

	cmp_size = compress_chunk(chunk, CHUNK_SIZE, NULL, NULL, dst,
				  dst_capacity, &cmp_par);
	TEST_ASSERT_EQUAL_INT(GENERIC_HEADER_SIZE + CHUNK_SIZE, cmp_size);
	dst_capacity = cmp_size;
	dst = malloc(dst_capacity); TEST_ASSERT_NOT_NULL(dst);
	cmp_size = compress_chunk(chunk, CHUNK_SIZE, NULL, NULL, dst,
				  dst_capacity, &cmp_par);
	TEST_ASSERT_EQUAL_INT(GENERIC_HEADER_SIZE + CHUNK_SIZE, cmp_size);

	/* test results */
	{	struct cmp_entity *ent = (struct cmp_entity *)dst;
		struct s_fx *raw_cmp_data = (struct s_fx *)(
			(uint8_t *)cmp_ent_get_data_buf(ent) + COLLECTION_HDR_SIZE);

		TEST_ASSERT_EQUAL_UINT(CHUNK_SIZE, cmp_ent_get_cmp_data_size(ent));
		TEST_ASSERT_EQUAL_UINT(CHUNK_SIZE, cmp_ent_get_original_size(ent));
		TEST_ASSERT_EQUAL_UINT(cmp_par.cmp_mode, cmp_ent_get_cmp_mode(ent));
		TEST_ASSERT_TRUE(cmp_ent_get_data_type_raw_bit(ent));
		TEST_ASSERT_EQUAL_INT(DATA_TYPE_CHUNK, cmp_ent_get_data_type(ent));

		TEST_ASSERT_EQUAL_HEX8_ARRAY(col, cmp_ent_get_data_buf(ent), COLLECTION_HDR_SIZE);

		TEST_ASSERT_EQUAL_HEX(data[0].exp_flags, raw_cmp_data[0].exp_flags);
		TEST_ASSERT_EQUAL_HEX(data[0].fx, be32_to_cpu(raw_cmp_data[0].fx));
		TEST_ASSERT_EQUAL_HEX(data[1].exp_flags, raw_cmp_data[1].exp_flags);
		TEST_ASSERT_EQUAL_HEX(data[1].fx, be32_to_cpu(raw_cmp_data[1].fx));
	}
	free(dst);

	/* error case: dst buffer to small */
	dst_capacity -= 1;
	dst = malloc(dst_capacity); TEST_ASSERT_NOT_NULL(dst);
	cmp_size = compress_chunk(chunk, CHUNK_SIZE, NULL, NULL, dst,
				  dst_capacity, &cmp_par);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUFFER, cmp_get_error_code(cmp_size));
	free(dst);
}


/**
 * @test compress_chunk
 */

void test_compress_chunk_raw_two_col(void)
{
	enum {	DATA_SIZE_1 = 2*sizeof(struct s_fx),
		DATA_SIZE_2 = 3*sizeof(struct s_fx_efx_ncob_ecob),
		CHUNK_SIZE = 2*COLLECTION_HDR_SIZE + DATA_SIZE_1 + DATA_SIZE_2
	};
	uint8_t chunk[CHUNK_SIZE];
	struct collection_hdr *col1 = (struct collection_hdr *)chunk;
	struct collection_hdr *col2;
	struct s_fx *data1 = (struct s_fx *)col1->entry;
	struct s_fx_efx_ncob_ecob *data2;
	struct cmp_par cmp_par = {0};
	uint32_t *dst;
	uint32_t cmp_size;
	uint32_t dst_capacity = 0;

	/* create a chunk with two collection */
	memset(col1, 0, COLLECTION_HDR_SIZE);
	TEST_ASSERT_FALSE(cmp_col_set_subservice(col1, SST_NCxx_S_SCIENCE_S_FX));
	TEST_ASSERT_FALSE(cmp_col_set_data_length(col1, DATA_SIZE_1));
	data1[0].exp_flags = 0;
	data1[0].fx = 1;
	data1[1].exp_flags = 0xF0;
	data1[1].fx = 0xABCDE0FF;
	col2 = (struct collection_hdr *)(chunk + COLLECTION_HDR_SIZE + DATA_SIZE_1);
	memset(col2, 0, COLLECTION_HDR_SIZE);
	TEST_ASSERT_FALSE(cmp_col_set_subservice(col2, SST_NCxx_S_SCIENCE_S_FX_EFX_NCOB_ECOB));
	TEST_ASSERT_FALSE(cmp_col_set_data_length(col2, DATA_SIZE_2));
	data2 = (struct s_fx_efx_ncob_ecob *)col2->entry;
	data2[0].exp_flags = 1;
	data2[0].fx = 2;
	data2[0].efx = 3;
	data2[0].ncob_x = 4;
	data2[0].ncob_y = 5;
	data2[0].ecob_x = 6;
	data2[0].ecob_y = 7;
	data2[1].exp_flags = 0;
	data2[1].fx = 0;
	data2[1].efx = 0;
	data2[1].ncob_x = 0;
	data2[1].ncob_y = 0;
	data2[1].ecob_x = 0;
	data2[1].ecob_y = 0;
	data2[2].exp_flags = 0xF;
	data2[2].fx = ~0U;
	data2[2].efx = ~0U;
	data2[2].ncob_x = ~0U;
	data2[2].ncob_y = ~0U;
	data2[2].ecob_x = ~0U;
	data2[2].ecob_y = ~0U;

	/* compress the data */
	cmp_par.cmp_mode = CMP_MODE_RAW;
	dst = NULL;

	cmp_size = compress_chunk(chunk, CHUNK_SIZE, NULL, NULL, dst,
				  dst_capacity, &cmp_par);
	TEST_ASSERT_EQUAL_INT(GENERIC_HEADER_SIZE + CHUNK_SIZE, cmp_size);
	dst_capacity = cmp_size;
	dst = malloc(dst_capacity); TEST_ASSERT_NOT_NULL(dst);
	cmp_size = compress_chunk(chunk, CHUNK_SIZE, NULL, NULL, dst,
				  dst_capacity, &cmp_par);
	TEST_ASSERT_EQUAL_INT(GENERIC_HEADER_SIZE + CHUNK_SIZE, cmp_size);

	/* test results */
	{	struct cmp_entity *ent = (struct cmp_entity *)dst;
		struct s_fx *raw_cmp_data1 = (struct s_fx *)(
			(uint8_t *)cmp_ent_get_data_buf(ent) + COLLECTION_HDR_SIZE);
		struct s_fx_efx_ncob_ecob *raw_cmp_data2 = (struct s_fx_efx_ncob_ecob *)(
			(uint8_t *)cmp_ent_get_data_buf(ent) + 2*COLLECTION_HDR_SIZE +
			DATA_SIZE_1);
		int i;

		TEST_ASSERT_EQUAL_UINT(CHUNK_SIZE, cmp_ent_get_cmp_data_size(ent));
		TEST_ASSERT_EQUAL_UINT(CHUNK_SIZE, cmp_ent_get_original_size(ent));
		TEST_ASSERT_EQUAL_UINT(cmp_par.cmp_mode, cmp_ent_get_cmp_mode(ent));
		TEST_ASSERT_TRUE(cmp_ent_get_data_type_raw_bit(ent));
		TEST_ASSERT_EQUAL_INT(DATA_TYPE_CHUNK, cmp_ent_get_data_type(ent));

		TEST_ASSERT_EQUAL_HEX8_ARRAY(col1, cmp_ent_get_data_buf(ent), COLLECTION_HDR_SIZE);

		for (i = 0; i < 2; i++) {
			TEST_ASSERT_EQUAL_HEX(data1[i].exp_flags, raw_cmp_data1[i].exp_flags);
			TEST_ASSERT_EQUAL_HEX(data1[i].fx, be32_to_cpu(raw_cmp_data1[i].fx));
		}

		TEST_ASSERT_EQUAL_HEX8_ARRAY(col1, cmp_ent_get_data_buf(ent), COLLECTION_HDR_SIZE);

		for (i = 0; i < 2; i++) {
			TEST_ASSERT_EQUAL_HEX(data1[i].exp_flags, raw_cmp_data1[i].exp_flags);
			TEST_ASSERT_EQUAL_HEX(data1[i].fx, be32_to_cpu(raw_cmp_data1[i].fx));
		}

		TEST_ASSERT_EQUAL_HEX8_ARRAY(col2, (uint8_t *)cmp_ent_get_data_buf(ent)+cmp_col_get_size(col1), COLLECTION_HDR_SIZE);

		for (i = 0; i < 2; i++) {
			TEST_ASSERT_EQUAL_HEX(data2[i].exp_flags, raw_cmp_data2[i].exp_flags);
			TEST_ASSERT_EQUAL_HEX(data2[i].fx, be32_to_cpu(raw_cmp_data2[i].fx));
			TEST_ASSERT_EQUAL_HEX(data2[i].efx, be32_to_cpu(raw_cmp_data2[i].efx));
			TEST_ASSERT_EQUAL_HEX(data2[i].ncob_x, be32_to_cpu(raw_cmp_data2[i].ncob_x));
			TEST_ASSERT_EQUAL_HEX(data2[i].ncob_y, be32_to_cpu(raw_cmp_data2[i].ncob_y));
			TEST_ASSERT_EQUAL_HEX(data2[i].ecob_x, be32_to_cpu(raw_cmp_data2[i].ecob_x));
			TEST_ASSERT_EQUAL_HEX(data2[i].ecob_y, be32_to_cpu(raw_cmp_data2[i].ecob_y));
		}
	}
	free(dst);

	/* error case: dst buffer to small */
	dst_capacity -= 1;
	dst = malloc(dst_capacity); TEST_ASSERT_NOT_NULL(dst);
	cmp_size = compress_chunk(chunk, CHUNK_SIZE, NULL, NULL, dst,
				  dst_capacity, &cmp_par);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUFFER, cmp_get_error_code(cmp_size));
	free(dst);
}


/**
 * @test compress_chunk
 */

void test_compress_chunk_aux(void)
{
	enum {	DATA_SIZE_1 = 2*sizeof(struct offset),
		DATA_SIZE_2 = 3*sizeof(struct background),
		CHUNK_SIZE = 2*COLLECTION_HDR_SIZE + DATA_SIZE_1 + DATA_SIZE_2
	};
	uint8_t chunk[CHUNK_SIZE+100];
	struct collection_hdr *col1 = (struct collection_hdr *)chunk;
	struct collection_hdr *col2;
	struct offset *data1 = (struct offset *)col1->entry;
	struct background *data2;
	struct cmp_par cmp_par = {0};
	uint32_t *dst;
	uint32_t cmp_size;
	uint32_t dst_capacity = 0;

	/* create a chunk with two collection */
	memset(col1, 0, COLLECTION_HDR_SIZE);
	TEST_ASSERT_FALSE(cmp_col_set_subservice(col1, SST_NCxx_S_SCIENCE_OFFSET));
	TEST_ASSERT_FALSE(cmp_col_set_data_length(col1, DATA_SIZE_1));
	data1[0].mean = 0;
	data1[0].variance = 1;
	data1[1].mean = 0xF0;
	data1[1].variance = 0xABCDE0FF;
	col2 = (struct collection_hdr *)(chunk + COLLECTION_HDR_SIZE + DATA_SIZE_1);
	memset(col2, 0, COLLECTION_HDR_SIZE);
	TEST_ASSERT_FALSE(cmp_col_set_subservice(col2, SST_NCxx_S_SCIENCE_BACKGROUND));
	TEST_ASSERT_FALSE(cmp_col_set_data_length(col2, DATA_SIZE_2));
	data2 = (struct background *)col2->entry;
	data2[0].mean = 1;
	data2[0].variance = 2;
	data2[0].outlier_pixels = 3;
	data2[1].mean = 0;
	data2[1].variance = 0;
	data2[1].outlier_pixels = 0;
	data2[2].mean = 0xF;
	data2[2].variance = 0xFFFF;
	data2[2].outlier_pixels = 0xFFFF;

	/* compress the data */
	cmp_par.cmp_mode = CMP_MODE_DIFF_MULTI;
	cmp_par.nc_offset_mean = 1;
	cmp_par.nc_offset_variance = UINT16_MAX;

	cmp_par.nc_background_mean = UINT16_MAX;
	cmp_par.nc_background_variance = 1;
	cmp_par.nc_background_outlier_pixels = 42;
	dst = NULL;

	cmp_size = compress_chunk(chunk, CHUNK_SIZE, NULL, NULL, dst,
				  dst_capacity, &cmp_par);
	TEST_ASSERT_EQUAL_INT(124, cmp_size);
	dst_capacity = ROUND_UP_TO_4(cmp_size);
	dst = malloc(dst_capacity); TEST_ASSERT_NOT_NULL(dst);
	cmp_size = compress_chunk(chunk, CHUNK_SIZE, NULL, NULL, dst,
				  dst_capacity, &cmp_par);
	TEST_ASSERT_EQUAL_INT(124, cmp_size);


	/* error case wrong cmp_par */
	cmp_par.nc_background_outlier_pixels = 0;
	cmp_size = compress_chunk(chunk, CHUNK_SIZE, NULL, NULL, dst,
				  dst_capacity, &cmp_par);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_PAR_SPECIFIC, cmp_get_error_code(cmp_size));

	cmp_par.nc_background_outlier_pixels = MAX_CHUNK_CMP_PAR + 1;
	cmp_size = compress_chunk(chunk, CHUNK_SIZE, NULL, NULL, dst,
				  dst_capacity, &cmp_par);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_PAR_SPECIFIC, cmp_get_error_code(cmp_size));

	/* wrong cmp_mode */
	cmp_par.nc_background_outlier_pixels = MAX_CHUNK_CMP_PAR;
	cmp_par.cmp_mode = 5;
	cmp_size = compress_chunk(chunk, CHUNK_SIZE, NULL, NULL, dst,
				  dst_capacity, &cmp_par);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_PAR_GENERIC, cmp_get_error_code(cmp_size));

	/* wrong model value */
	cmp_par.cmp_mode = CMP_MODE_MODEL_ZERO;
	cmp_par.model_value = MAX_MODEL_VALUE + 1;
	cmp_size = compress_chunk(chunk, CHUNK_SIZE, NULL, NULL, dst,
				  dst_capacity, &cmp_par);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_PAR_GENERIC, cmp_get_error_code(cmp_size));
	free(dst);
}


/**
 * @test compress_chunk
 */

void test_collection_zero_data_length(void)
{
	enum {	DATA_SIZE = 0,
		CHUNK_SIZE = COLLECTION_HDR_SIZE + DATA_SIZE
	};
	uint8_t chunk[CHUNK_SIZE];
	struct collection_hdr *col = (struct collection_hdr *)chunk;
	struct cmp_par cmp_par = {0};
	uint32_t *dst;
	uint32_t cmp_size;
	uint32_t dst_capacity = 43; /* random non zero value */

	/* create a chunk with a single collection */
	memset(col, 0, COLLECTION_HDR_SIZE);
	TEST_ASSERT_FALSE(cmp_col_set_subservice(col, SST_NCxx_S_SCIENCE_IMAGETTE));

	/* compress the data */
	cmp_par.cmp_mode = CMP_MODE_DIFF_MULTI;
	cmp_par.nc_imagette = 1;
	dst = NULL;

	cmp_size = compress_chunk(chunk, CHUNK_SIZE, NULL, NULL, dst,
				  dst_capacity, &cmp_par);
	TEST_ASSERT_EQUAL_INT(NON_IMAGETTE_HEADER_SIZE + CHUNK_SIZE + CMP_COLLECTION_FILD_SIZE, cmp_size);
	dst_capacity = cmp_size;
	dst = malloc(dst_capacity); TEST_ASSERT_NOT_NULL(dst);
	cmp_size = compress_chunk(chunk, CHUNK_SIZE, NULL, NULL, dst,
				  dst_capacity, &cmp_par);
	TEST_ASSERT_EQUAL_INT(NON_IMAGETTE_HEADER_SIZE + CHUNK_SIZE + CMP_COLLECTION_FILD_SIZE, cmp_size);

	/* test results */
	{	struct cmp_entity *ent = (struct cmp_entity *)dst;

		TEST_ASSERT_EQUAL_UINT(CHUNK_SIZE+CMP_COLLECTION_FILD_SIZE, cmp_ent_get_cmp_data_size(ent));
		TEST_ASSERT_EQUAL_UINT(CHUNK_SIZE, cmp_ent_get_original_size(ent));
		TEST_ASSERT_EQUAL_UINT(cmp_par.cmp_mode, cmp_ent_get_cmp_mode(ent));
		TEST_ASSERT_FALSE(cmp_ent_get_data_type_raw_bit(ent));
		TEST_ASSERT_EQUAL_INT(DATA_TYPE_CHUNK, cmp_ent_get_data_type(ent));

		TEST_ASSERT_EQUAL_HEX8_ARRAY(col, (uint8_t *)cmp_ent_get_data_buf(ent)+CMP_COLLECTION_FILD_SIZE, COLLECTION_HDR_SIZE);
	}

	/* error case: dst buffer to small */
	dst_capacity -= 1;
	cmp_size = compress_chunk(chunk, CHUNK_SIZE, NULL, NULL, dst,
				  dst_capacity, &cmp_par);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUFFER, cmp_get_error_code(cmp_size));
	free(dst);
}


static int n_timestamp_fail; /* fail after n calls */
static uint64_t get_timstamp_test(void)
{
	static int n;

	if (n < n_timestamp_fail) {
		n++;
		return 42;
	}
	n = 0;
	return 1ULL << 48; /* invalid time stamp */
}


/**
 * @test compress_chunk
 */

void test_compress_chunk_error_cases(void)
{
	enum {	DATA_SIZE_1 = 2*sizeof(struct s_fx),
		DATA_SIZE_2 = 3*sizeof(struct s_fx_efx_ncob_ecob),
		CHUNK_SIZE = 2*COLLECTION_HDR_SIZE + DATA_SIZE_1 + DATA_SIZE_2
	};
	uint8_t chunk[CHUNK_SIZE];
	uint8_t const chunk_model[CHUNK_SIZE] = {0}; /* model is set to zero */
	uint8_t updated_chunk_model[CHUNK_SIZE];
	uint32_t dst[COMPRESS_CHUNK_BOUND(CHUNK_SIZE, 2)/sizeof(uint32_t)];
	uint32_t dst_capacity = sizeof(dst);
	struct cmp_par cmp_par = {0};
	uint32_t cmp_size;
	struct collection_hdr *col2;

	{ /* create a chunk with two collection */
		struct collection_hdr *col1 = (struct collection_hdr *)chunk;
		struct s_fx *data1 = (struct s_fx *)col1->entry;
		struct s_fx_efx_ncob_ecob *data2;

		memset(col1, 0, COLLECTION_HDR_SIZE);
		TEST_ASSERT_FALSE(cmp_col_set_subservice(col1, SST_NCxx_S_SCIENCE_S_FX));
		TEST_ASSERT_FALSE(cmp_col_set_data_length(col1, DATA_SIZE_1));
		data1[0].exp_flags = 0;
		data1[0].fx = 1;
		data1[1].exp_flags = 0xF0;
		data1[1].fx = 0xABCDE0FF;
		col2 = (struct collection_hdr *)(chunk + COLLECTION_HDR_SIZE + DATA_SIZE_1);
		memset(col2, 0, COLLECTION_HDR_SIZE);
		TEST_ASSERT_FALSE(cmp_col_set_subservice(col2, SST_NCxx_S_SCIENCE_S_FX_EFX_NCOB_ECOB));
		TEST_ASSERT_FALSE(cmp_col_set_data_length(col2, DATA_SIZE_2));
		data2 = (struct s_fx_efx_ncob_ecob *)col2->entry;
		data2[0].exp_flags = 1;
		data2[0].fx = 2;
		data2[0].efx = 3;
		data2[0].ncob_x = 4;
		data2[0].ncob_y = 5;
		data2[0].ecob_x = 6;
		data2[0].ecob_y = 7;
		data2[1].exp_flags = 0;
		data2[1].fx = 0;
		data2[1].efx = 0;
		data2[1].ncob_x = 0;
		data2[1].ncob_y = 0;
		data2[1].ecob_x = 0;
		data2[1].ecob_y = 0;
		data2[2].exp_flags = 0xF;
		data2[2].fx = ~0U;
		data2[2].efx = ~0U;
		data2[2].ncob_x = ~0U;
		data2[2].ncob_y = ~0U;
		data2[2].ecob_x = ~0U;
		data2[2].ecob_y = ~0U;
	}

	/* compress the data */
	cmp_par.cmp_mode = CMP_MODE_MODEL_ZERO;
	cmp_par.model_value = 16;
	cmp_par.lossy_par = 0;

	cmp_par.nc_imagette = 1;

	cmp_par.s_exp_flags = MAX_CHUNK_CMP_PAR;
	cmp_par.s_fx = MIN_CHUNK_CMP_PAR;
	cmp_par.s_ncob = 2;
	cmp_par.s_efx = 0xFFFE;
	cmp_par.s_ecob = 1;

	compress_chunk_init(NULL, 23);

	/* this sound not return an error */
	cmp_size = compress_chunk(chunk, CHUNK_SIZE, chunk_model,
				  NULL, dst, dst_capacity,
				  &cmp_par);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_NO_ERROR, cmp_get_error_code(cmp_size));
	TEST_ASSERT_EQUAL(124, cmp_size);

	/* error: no chunk */
	cmp_size = compress_chunk(NULL, CHUNK_SIZE, chunk_model,
				  updated_chunk_model, dst, dst_capacity,
				  &cmp_par);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_CHUNK_NULL, cmp_get_error_code(cmp_size));

	/* error: chunk_size = 0 */
	cmp_size = compress_chunk(chunk, 0, chunk_model,
				  updated_chunk_model, dst, dst_capacity,
				  &cmp_par);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_CHUNK_SIZE_INCONSISTENT, cmp_get_error_code(cmp_size));

	/* error: chunk_size does not match up with the collection size */
	TEST_ASSERT_FALSE(cmp_col_set_data_length((struct collection_hdr *) chunk, 100));
	cmp_size = compress_chunk(chunk, CHUNK_SIZE, chunk_model,
				  updated_chunk_model, dst, dst_capacity,
				  &cmp_par);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_CHUNK_SIZE_INCONSISTENT, cmp_get_error_code(cmp_size));
	TEST_ASSERT_FALSE(cmp_col_set_data_length((struct collection_hdr *) chunk, DATA_SIZE_1));

	/* error: no model when needed */
	cmp_size = compress_chunk(chunk, CHUNK_SIZE, NULL,
				  updated_chunk_model, dst, dst_capacity,
				  &cmp_par);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_PAR_NO_MODEL, cmp_get_error_code(cmp_size));
	/* this should work */
	cmp_par.cmp_mode = CMP_MODE_DIFF_ZERO;
	cmp_size = compress_chunk(chunk, CHUNK_SIZE, NULL,
				  updated_chunk_model, dst, dst_capacity,
				  &cmp_par);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_NO_ERROR, cmp_get_error_code(cmp_size));
	cmp_par.cmp_mode = CMP_MODE_MODEL_ZERO;

	memset(dst, 0xFF, sizeof(dst));

	/* error chunk and model are the same */
	cmp_size = compress_chunk(chunk, CHUNK_SIZE, chunk,
				  updated_chunk_model, dst, dst_capacity,
				  &cmp_par);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_PAR_BUFFERS, cmp_get_error_code(cmp_size));

	/* error chunk and updated model are the same */
	cmp_size = compress_chunk(chunk, CHUNK_SIZE, chunk_model,
				  chunk, dst, dst_capacity,
				  &cmp_par);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_PAR_BUFFERS, cmp_get_error_code(cmp_size));

	/* buffer and dst are the same */
	cmp_size = compress_chunk(chunk, CHUNK_SIZE, dst,
				  updated_chunk_model, dst, dst_capacity, &cmp_par);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_PAR_BUFFERS, cmp_get_error_code(cmp_size));

	/* error updated model buffer and dst are the same */
	cmp_size = compress_chunk(chunk, CHUNK_SIZE, chunk_model,
				  dst, dst, dst_capacity, &cmp_par);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_PAR_BUFFERS, cmp_get_error_code(cmp_size));

	/* chunk buffer and dst are the same */
	cmp_size = compress_chunk(chunk, CHUNK_SIZE, chunk_model,
				  updated_chunk_model, (void *)chunk, dst_capacity,
				  &cmp_par);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_PAR_BUFFERS, cmp_get_error_code(cmp_size));

	/* error: cmp_par = NULL */
	cmp_size = compress_chunk(chunk, CHUNK_SIZE, chunk_model,
				  updated_chunk_model, dst, dst_capacity,
				  NULL);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_PAR_NULL, cmp_get_error_code(cmp_size));

	/* error: cmp_par invalid*/
	cmp_par.s_exp_flags = 0;
	cmp_size = compress_chunk(chunk, CHUNK_SIZE, chunk_model,
				  updated_chunk_model, dst, dst_capacity,
				  &cmp_par);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_PAR_SPECIFIC, cmp_get_error_code(cmp_size));
	cmp_par.s_exp_flags = MAX_CHUNK_CMP_PAR;

	/* error: chunk size to big */
	cmp_size = compress_chunk(chunk, CMP_ENTITY_MAX_ORIGINAL_SIZE+1, chunk_model,
				  updated_chunk_model, dst, dst_capacity,
				  &cmp_par);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_CHUNK_TOO_LARGE, cmp_get_error_code(cmp_size));

	/* error: dst buffer smaller than entity header */
	cmp_size = compress_chunk(chunk, CHUNK_SIZE, chunk_model,
				  updated_chunk_model, dst, 5, &cmp_par);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_SMALL_BUFFER, cmp_get_error_code(cmp_size));

	/* error: invalid collection type */
	TEST_ASSERT_FALSE(cmp_col_set_subservice((struct collection_hdr *)chunk, 42));
	cmp_size = compress_chunk(chunk, CHUNK_SIZE, chunk_model,
				  updated_chunk_model, dst, dst_capacity,
				  &cmp_par);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_COL_SUBSERVICE_UNSUPPORTED, cmp_get_error_code(cmp_size));
	TEST_ASSERT_FALSE(cmp_col_set_subservice((struct collection_hdr *)chunk, SST_NCxx_S_SCIENCE_S_FX));

	/* error: collection size no a multiple of the data size */
	TEST_ASSERT_FALSE(cmp_col_set_data_length(col2, DATA_SIZE_2-1));
	cmp_size = compress_chunk(chunk, CHUNK_SIZE-1, chunk_model,
				  updated_chunk_model, NULL, dst_capacity,
				  &cmp_par);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_COL_SIZE_INCONSISTENT, cmp_get_error_code(cmp_size));
	TEST_ASSERT_FALSE(cmp_col_set_data_length(col2, DATA_SIZE_2));

	/* this sound work */
	cmp_par.lossy_par = 0x1;
	{
		size_t i;

		for (i = 0; i < ARRAY_SIZE(dst); i++)
			TEST_ASSERT_EQUAL_HEX(0xFFFFFFFF, dst[i]);
	}
	cmp_size = compress_chunk(chunk, CHUNK_SIZE, chunk_model,
				  updated_chunk_model, dst, dst_capacity,
				  &cmp_par);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_NO_ERROR, cmp_get_error_code(cmp_size));
	TEST_ASSERT_EQUAL(124, cmp_size);

	/* error: invalid collection combination */
	TEST_ASSERT_FALSE(cmp_col_set_subservice((struct collection_hdr *)(chunk + COLLECTION_HDR_SIZE + DATA_SIZE_1),
						 SST_NCxx_S_SCIENCE_SMEARING));
	cmp_size = compress_chunk(chunk, CHUNK_SIZE, chunk_model,
				  updated_chunk_model, dst, dst_capacity,
				  &cmp_par);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_CHUNK_SUBSERVICE_INCONSISTENT, cmp_get_error_code(cmp_size));
	TEST_ASSERT_FALSE(cmp_col_set_subservice((struct collection_hdr *)(chunk + COLLECTION_HDR_SIZE + DATA_SIZE_1),
						 SST_NCxx_S_SCIENCE_S_FX_EFX_NCOB_ECOB));

	/* error: start time stamp error */
	compress_chunk_init(&get_timstamp_test, 23);
	cmp_size = compress_chunk(chunk, CHUNK_SIZE, chunk_model,
				  updated_chunk_model, dst, dst_capacity,
				  &cmp_par);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_ENTITY_TIMESTAMP, cmp_get_error_code(cmp_size));

	/* error: end time stamp error */
	n_timestamp_fail = 1;
	compress_chunk_init(&get_timstamp_test, 23);
	cmp_size = compress_chunk(chunk, CHUNK_SIZE, chunk_model,
				  updated_chunk_model, dst, dst_capacity,
				  &cmp_par);
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_ENTITY_TIMESTAMP, cmp_get_error_code(cmp_size));
	n_timestamp_fail = INT_MAX;

	{ /* error: trigger CMP_ERROR_ENTITY_HEADER */
		uint32_t ent_hdr_size;
		uint32_t entity[25];
		uint32_t chunk_size = 42;
		struct cmp_cfg cfg = {0};
		uint64_t start_timestamp = 123;
		uint32_t cmp_ent_size_byte = sizeof(entity);

		cfg.cmp_mode = CMP_MODE_DIFF_ZERO;
		cfg.cmp_par_1 = UINT16_MAX + 1; /* to big for entity header */
		ent_hdr_size = cmp_ent_build_chunk_header(entity, chunk_size, &cfg,
							  start_timestamp, cmp_ent_size_byte);
		TEST_ASSERT_EQUAL_INT(CMP_ERROR_ENTITY_HEADER, cmp_get_error_code(ent_hdr_size));
	}
}


/**
 * @test zero_escape_mech_is_used
 */

void test_zero_escape_mech_is_used(void)
{
	enum cmp_mode cmp_mode;

	for (cmp_mode = 0; cmp_mode <= MAX_CMP_MODE; cmp_mode++) {
		int res = zero_escape_mech_is_used(cmp_mode);

		if (cmp_mode == CMP_MODE_DIFF_ZERO ||
		    cmp_mode == CMP_MODE_MODEL_ZERO)
			TEST_ASSERT_TRUE(res);
		else
			TEST_ASSERT_FALSE(res);
	}
}


/**
 * @test ROUND_UP_TO_4
 * @test COMPRESS_CHUNK_BOUND
 */

void test_COMPRESS_CHUNK_BOUND(void)
{
	uint32_t chunk_size;
	unsigned int  num_col;
	uint32_t bound, bound_exp;

	TEST_ASSERT_EQUAL(0, ROUND_UP_TO_4(0));
	TEST_ASSERT_EQUAL(4, ROUND_UP_TO_4(1));
	TEST_ASSERT_EQUAL(4, ROUND_UP_TO_4(3));
	TEST_ASSERT_EQUAL(4, ROUND_UP_TO_4(4));
	TEST_ASSERT_EQUAL(8, ROUND_UP_TO_4(5));
	TEST_ASSERT_EQUAL(0xFFFFFFFC, ROUND_UP_TO_4(0xFFFFFFFB));
	TEST_ASSERT_EQUAL(0xFFFFFFFC, ROUND_UP_TO_4(0xFFFFFFFC));
	TEST_ASSERT_EQUAL(0, ROUND_UP_TO_4(0xFFFFFFFD));
	TEST_ASSERT_EQUAL(0, ROUND_UP_TO_4(0xFFFFFFFF));

	chunk_size = 0;
	num_col = 0;
	bound = COMPRESS_CHUNK_BOUND(chunk_size, num_col);
	TEST_ASSERT_EQUAL(0, bound);

	chunk_size = 0;
	num_col = 1;
	bound = COMPRESS_CHUNK_BOUND(chunk_size, num_col);
	TEST_ASSERT_EQUAL(0, bound);

	chunk_size = COLLECTION_HDR_SIZE - 1;
	num_col = 1;
	bound = COMPRESS_CHUNK_BOUND(chunk_size, num_col);
	TEST_ASSERT_EQUAL(0, bound);

	chunk_size = 2*COLLECTION_HDR_SIZE - 1;
	num_col = 2;
	bound = COMPRESS_CHUNK_BOUND(chunk_size, num_col);
	TEST_ASSERT_EQUAL(0, bound);

	chunk_size = COLLECTION_HDR_SIZE;
	num_col = 0;
	bound = COMPRESS_CHUNK_BOUND(chunk_size, num_col);
	TEST_ASSERT_EQUAL(0, bound);

	chunk_size = CMP_ENTITY_MAX_SIZE;
	num_col = 1;
	bound = COMPRESS_CHUNK_BOUND(chunk_size, num_col);
	TEST_ASSERT_EQUAL(0, bound);

	chunk_size = UINT32_MAX;
	num_col = 1;
	bound = COMPRESS_CHUNK_BOUND(chunk_size, num_col);
	TEST_ASSERT_EQUAL(0, bound);

	num_col = (CMP_ENTITY_MAX_SIZE-NON_IMAGETTE_HEADER_SIZE)/COLLECTION_HDR_SIZE + 1;
	chunk_size = num_col*COLLECTION_HDR_SIZE;
	bound = COMPRESS_CHUNK_BOUND(chunk_size, num_col);
	TEST_ASSERT_EQUAL(0, bound);


	chunk_size = COLLECTION_HDR_SIZE;
	num_col = 1;
	bound = COMPRESS_CHUNK_BOUND(chunk_size, num_col);
	bound_exp = ROUND_UP_TO_4(NON_IMAGETTE_HEADER_SIZE + CMP_COLLECTION_FILD_SIZE +
		COLLECTION_HDR_SIZE);
	TEST_ASSERT_EQUAL(bound_exp, bound);

	chunk_size = COLLECTION_HDR_SIZE + 7;
	num_col = 1;
	bound = COMPRESS_CHUNK_BOUND(chunk_size, num_col);
	bound_exp = ROUND_UP_TO_4(NON_IMAGETTE_HEADER_SIZE + CMP_COLLECTION_FILD_SIZE +
		chunk_size);
	TEST_ASSERT_EQUAL(bound_exp, bound);

	chunk_size = 42*COLLECTION_HDR_SIZE;
	num_col = 42;
	bound = COMPRESS_CHUNK_BOUND(chunk_size, num_col);
	bound_exp = ROUND_UP_TO_4(NON_IMAGETTE_HEADER_SIZE + 42*CMP_COLLECTION_FILD_SIZE +
		chunk_size);
	TEST_ASSERT_EQUAL(bound_exp, bound);

	chunk_size = 42*COLLECTION_HDR_SIZE + 7;
	num_col = 42;
	bound = COMPRESS_CHUNK_BOUND(chunk_size, num_col);
	bound_exp = ROUND_UP_TO_4(NON_IMAGETTE_HEADER_SIZE + 42*CMP_COLLECTION_FILD_SIZE +
		chunk_size);
	TEST_ASSERT_EQUAL(bound_exp, bound);

	chunk_size = (CMP_ENTITY_MAX_SIZE & ~3U) - NON_IMAGETTE_HEADER_SIZE - CMP_COLLECTION_FILD_SIZE;
	num_col = 1;
	bound = COMPRESS_CHUNK_BOUND(chunk_size, num_col);
	bound_exp = ROUND_UP_TO_4(NON_IMAGETTE_HEADER_SIZE + CMP_COLLECTION_FILD_SIZE +
		chunk_size);
	TEST_ASSERT_EQUAL(bound_exp, bound);

	chunk_size++;
	bound = COMPRESS_CHUNK_BOUND(chunk_size, num_col);
	TEST_ASSERT_EQUAL(0, bound);

	num_col = (CMP_ENTITY_MAX_SIZE-NON_IMAGETTE_HEADER_SIZE)/(COLLECTION_HDR_SIZE+CMP_COLLECTION_FILD_SIZE);
	chunk_size = num_col*COLLECTION_HDR_SIZE + 10;
	bound = COMPRESS_CHUNK_BOUND(chunk_size, num_col);
	bound_exp = ROUND_UP_TO_4(NON_IMAGETTE_HEADER_SIZE + num_col*CMP_COLLECTION_FILD_SIZE +
		chunk_size);
	TEST_ASSERT_EQUAL_HEX(bound_exp, bound);

	chunk_size++;
	bound = COMPRESS_CHUNK_BOUND(chunk_size, num_col);
	TEST_ASSERT_EQUAL_HEX(0, bound);
}


/**
 * @test compress_chunk_cmp_size_bound
 */

void test_compress_chunk_cmp_size_bound(void)
{
	enum {
		CHUNK_SIZE_1 = 42,
		CHUNK_SIZE_2 = 3
	};
	uint8_t chunk[2*COLLECTION_HDR_SIZE + CHUNK_SIZE_1 + CHUNK_SIZE_2] = {0};
	uint32_t chunk_size;
	uint32_t bound, bound_exp;
	struct collection_hdr *col2 = (struct collection_hdr *)
		(chunk+COLLECTION_HDR_SIZE + CHUNK_SIZE_1);

	TEST_ASSERT_FALSE(cmp_col_set_data_length((struct collection_hdr *)chunk, 0));

	chunk_size = 0;
	bound = compress_chunk_cmp_size_bound(chunk, chunk_size);
	TEST_ASSERT_TRUE(cmp_is_error(bound));
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_CHUNK_SIZE_INCONSISTENT, cmp_get_error_code(bound));

	chunk_size = COLLECTION_HDR_SIZE-1;
	bound = compress_chunk_cmp_size_bound(chunk, chunk_size);
	TEST_ASSERT_TRUE(cmp_is_error(bound));
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_CHUNK_SIZE_INCONSISTENT, cmp_get_error_code(bound));

	chunk_size = UINT32_MAX;
	bound = compress_chunk_cmp_size_bound(chunk, chunk_size);
	TEST_ASSERT_TRUE(cmp_is_error(bound));
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_CHUNK_TOO_LARGE, cmp_get_error_code(bound));

	chunk_size = COLLECTION_HDR_SIZE;
	bound = compress_chunk_cmp_size_bound(chunk, chunk_size);
	bound_exp = ROUND_UP_TO_4(NON_IMAGETTE_HEADER_SIZE + COLLECTION_HDR_SIZE + CMP_COLLECTION_FILD_SIZE);
	TEST_ASSERT_EQUAL(bound_exp, bound);

	chunk_size = COLLECTION_HDR_SIZE + CHUNK_SIZE_1;
	TEST_ASSERT_FALSE(cmp_col_set_data_length((struct collection_hdr *)chunk, CHUNK_SIZE_1));
	bound = compress_chunk_cmp_size_bound(chunk, chunk_size);
	bound_exp = ROUND_UP_TO_4(NON_IMAGETTE_HEADER_SIZE + CMP_COLLECTION_FILD_SIZE +
		chunk_size);
	TEST_ASSERT_EQUAL(bound_exp, bound);

	/* chunk is NULL */
	bound = compress_chunk_cmp_size_bound(NULL, chunk_size);
	TEST_ASSERT_TRUE(cmp_is_error(bound));
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_CHUNK_NULL, cmp_get_error_code(bound));


	/* two collections */
	TEST_ASSERT_FALSE(cmp_col_set_data_length(col2, CHUNK_SIZE_2));
	chunk_size = sizeof(chunk);
	bound = compress_chunk_cmp_size_bound(chunk, chunk_size);
	bound_exp = ROUND_UP_TO_4(NON_IMAGETTE_HEADER_SIZE + 2*CMP_COLLECTION_FILD_SIZE +
		chunk_size);
	TEST_ASSERT_EQUAL(bound_exp, bound);

	TEST_ASSERT_FALSE(cmp_col_set_data_length(col2, 0));
	chunk_size = 2*COLLECTION_HDR_SIZE + CHUNK_SIZE_1;
	bound = compress_chunk_cmp_size_bound(chunk, chunk_size);
	bound_exp = ROUND_UP_TO_4(NON_IMAGETTE_HEADER_SIZE + 2*CMP_COLLECTION_FILD_SIZE +
		chunk_size);
	TEST_ASSERT_EQUAL(bound_exp, bound);

	/* wrong chunk_size */
	TEST_ASSERT_FALSE(cmp_col_set_data_length(col2, 0));
	chunk_size = 1 + 2*COLLECTION_HDR_SIZE + CHUNK_SIZE_1;
	bound = compress_chunk_cmp_size_bound(chunk, chunk_size);
	TEST_ASSERT_TRUE(cmp_is_error(bound));
	TEST_ASSERT_EQUAL_INT(CMP_ERROR_CHUNK_SIZE_INCONSISTENT, cmp_get_error_code(bound));

	{ /* containing only zero data size collections */
		size_t i;
		uint8_t *chunk_big;
		size_t const max_chunk_size = CMP_ENTITY_MAX_ORIGINAL_SIZE
			- NON_IMAGETTE_HEADER_SIZE - CMP_COLLECTION_FILD_SIZE;

		chunk_big = malloc(max_chunk_size);
		for (i = 0; i < max_chunk_size-COLLECTION_HDR_SIZE; i = i+COLLECTION_HDR_SIZE)
			TEST_ASSERT_FALSE(cmp_col_set_data_length((struct collection_hdr *)&chunk_big[i], 0));

		bound = compress_chunk_cmp_size_bound(chunk_big, i);
		TEST_ASSERT_EQUAL_INT(CMP_ERROR_CHUNK_TOO_LARGE, cmp_get_error_code(bound));
		free(chunk_big);
	}
}


/**
 * @test compress_chunk_set_model_id_and_counter
 */

void test_compress_chunk_set_model_id_and_counter(void)
{
	uint32_t ret;
	struct cmp_entity dst;
	uint32_t dst_size = sizeof(dst);
	uint16_t model_id;
	uint8_t model_counter;

	memset(&dst, 0x42, sizeof(dst));

	model_id = 0;
	model_counter = 0;
	ret = compress_chunk_set_model_id_and_counter(&dst, dst_size, model_id, model_counter);
	TEST_ASSERT_FALSE(cmp_is_error(ret));
	TEST_ASSERT_EQUAL(dst_size, ret);
	TEST_ASSERT_EQUAL(model_id, cmp_ent_get_model_id(&dst));
	TEST_ASSERT_EQUAL(model_counter, cmp_ent_get_model_counter(&dst));

	model_id = UINT16_MAX;
	model_counter = UINT8_MAX;
	ret = compress_chunk_set_model_id_and_counter(&dst, dst_size, model_id, model_counter);
	TEST_ASSERT_FALSE(cmp_is_error(ret));
	TEST_ASSERT_EQUAL(dst_size, ret);
	TEST_ASSERT_EQUAL(model_id, cmp_ent_get_model_id(&dst));
	TEST_ASSERT_EQUAL(model_counter, cmp_ent_get_model_counter(&dst));

	/* error cases */
	ret = compress_chunk_set_model_id_and_counter(&dst, GENERIC_HEADER_SIZE-1, model_id, model_counter);
	TEST_ASSERT_TRUE(cmp_is_error(ret));
	TEST_ASSERT_EQUAL(CMP_ERROR_ENTITY_TOO_SMALL, cmp_get_error_code(ret));

	ret = compress_chunk_set_model_id_and_counter(NULL, dst_size, model_id, model_counter);
	TEST_ASSERT_TRUE(cmp_is_error(ret));
	TEST_ASSERT_EQUAL(CMP_ERROR_ENTITY_NULL, cmp_get_error_code(ret));

	ret = compress_chunk_set_model_id_and_counter(&dst, CMP_ERROR(PAR_GENERIC), model_id, model_counter);
	TEST_ASSERT_TRUE(cmp_is_error(ret));
	TEST_ASSERT_EQUAL(CMP_ERROR_PAR_GENERIC, cmp_get_error_code(ret));
}



void test_support_function_call_NULL(void)
{
	struct cmp_cfg cfg = {0};

	cfg.cmp_mode = CMP_MODE_DIFF_ZERO;

	TEST_ASSERT_TRUE(cmp_cfg_gen_par_is_invalid(NULL));
	TEST_ASSERT_TRUE(cmp_cfg_gen_par_is_invalid(&cfg));
	cfg.data_type = DATA_TYPE_F_FX_EFX;
	TEST_ASSERT_TRUE(cmp_cfg_imagette_is_invalid(&cfg));
	TEST_ASSERT_TRUE(cmp_cfg_imagette_is_invalid(NULL));
	cfg.data_type = DATA_TYPE_IMAGETTE;
	TEST_ASSERT_TRUE(cmp_cfg_fx_cob_is_invalid(NULL));
	TEST_ASSERT_TRUE(cmp_cfg_fx_cob_is_invalid(&cfg));
	TEST_ASSERT_TRUE(cmp_cfg_aux_is_invalid(NULL));
	TEST_ASSERT_TRUE(cmp_cfg_aux_is_invalid(&cfg));
	TEST_ASSERT_FALSE(cmp_aux_data_type_is_used(DATA_TYPE_S_FX));
	TEST_ASSERT_TRUE(cmp_cfg_fx_cob_get_need_pars(DATA_TYPE_S_FX, NULL));
	TEST_ASSERT_TRUE(check_compression_buffers(NULL));
	cfg.cmp_mode = 5;
	TEST_ASSERT_TRUE(cmp_cfg_imagette_is_invalid(&cfg));
}


/*
 * @test cmp_cfg_fx_cob_get_need_pars
 */

void test_missing_cmp_cfg_fx_cob_get_need_pars(void)
{
	struct fx_cob_par used_par;
	enum cmp_data_type data_type;

	data_type = DATA_TYPE_F_FX;
	TEST_ASSERT_FALSE(cmp_cfg_fx_cob_get_need_pars(data_type, &used_par));
	data_type = DATA_TYPE_F_FX_EFX;
	TEST_ASSERT_FALSE(cmp_cfg_fx_cob_get_need_pars(data_type, &used_par));
	data_type = DATA_TYPE_F_FX_NCOB;
	TEST_ASSERT_FALSE(cmp_cfg_fx_cob_get_need_pars(data_type, &used_par));
	data_type = DATA_TYPE_F_FX_EFX_NCOB_ECOB;
	TEST_ASSERT_FALSE(cmp_cfg_fx_cob_get_need_pars(data_type, &used_par));
}


/**
 * @test test_print_cmp_info
 */

void test_print_cmp_info(void)
{
	struct cmp_info info;

	info.cmp_mode_used = 1;
	info.spill_used = 2;
	info.golomb_par_used = 3;
	info.samples_used = 4;
	info.cmp_size = 5;
	info.ap1_cmp_size = 6;
	info.ap2_cmp_size = 7;
	info.rdcu_new_model_adr_used = 8;
	info.rdcu_cmp_adr_used = 9;
	info.model_value_used = 10;
	info.round_used = 11;
	info.cmp_err = 12;

	print_cmp_info(&info);
	print_cmp_info(NULL);
}


/**
 * @test detect_buf_overlap
 */

void test_buffer_overlaps(void)
{
	char buf_a[3] = {0};
	char buf_b[3] = {0};
	int overlap;

	overlap = buffer_overlaps(buf_a, sizeof(buf_a), buf_b, sizeof(buf_b));
	TEST_ASSERT_FALSE(overlap);
	overlap = buffer_overlaps(NULL, sizeof(buf_a), buf_b, sizeof(buf_b));
	TEST_ASSERT_FALSE(overlap);
	overlap = buffer_overlaps(buf_a, sizeof(buf_a), NULL, sizeof(buf_b));
	TEST_ASSERT_FALSE(overlap);

	overlap = buffer_overlaps(buf_a, sizeof(buf_a), buf_a, sizeof(buf_a));
	TEST_ASSERT_TRUE(overlap);

	overlap = buffer_overlaps(&buf_a[1], 1, buf_a, sizeof(buf_a));
	TEST_ASSERT_TRUE(overlap);

	overlap = buffer_overlaps(&buf_a[0], 2, &buf_a[1], 2);
	TEST_ASSERT_TRUE(overlap);
	overlap = buffer_overlaps(&buf_a[1], 2, &buf_a[0], 2);
	TEST_ASSERT_TRUE(overlap);
}


/**
 * @test cmp_get_error_string
 */

void test_cmp_get_error_string(void)
{
	enum cmp_error code;
	const char *str;

	for (code = CMP_ERROR_NO_ERROR; code <= CMP_ERROR_MAX_CODE; code++) {
		str = cmp_get_error_string(code);
		TEST_ASSERT_NOT_EQUAL('\0', str[0]);

		str = cmp_get_error_name((-code));
		TEST_ASSERT_NOT_EQUAL('\0', str[0]);
	}
}
