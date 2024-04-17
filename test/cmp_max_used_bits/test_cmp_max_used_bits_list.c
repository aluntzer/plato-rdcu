/**
 * @file   test_cmp_max_used_bits_list.c
 * @author Dominik Loidolt (dominik.loidolt@univie.ac.at)
 * @date   2023
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
 * @brief max_used_bits list tests
 */


#if !defined(__sparc__) && !defined(_WIN32) && !defined(_WIN64)
#  define MAKE_MALLOC_FAIL_TEST
#  define _GNU_SOURCE
#  include <dlfcn.h>
#  include <stdlib.h>
#endif

#include <string.h>

#include <unity.h>

#include <cmp_max_used_bits_list.h>

#ifdef MAKE_MALLOC_FAIL_TEST
/* if set the mock malloc will fail (return NULL) */
static int malloc_fail;


/*
 * mock of the malloc function; can controlled with the global malloc_fail variable
 * see: https://jayconrod.com/posts/23/tutorial--function-interposition-in-linux
 */

void* malloc(size_t size)
{
	static void* (*real_malloc)(size_t size) = NULL;

	if(malloc_fail)
		return NULL;

	if (!real_malloc) {
		*(void **)(&real_malloc) = dlsym(RTLD_NEXT, "malloc");
		/* The cast removes a gcc warning https://stackoverflow.com/a/31528674 */
		TEST_ASSERT_NOT_NULL(real_malloc);
	}

	return real_malloc(size);
}
#endif


/**
 * @test cmp_max_used_bits_list_add
 * @test cmp_max_used_bits_list_get
 * @test cmp_max_used_bits_list_delet
 * @test cmp_max_used_bits_list_empty
 */

void test_cmp_max_used_bits_list(void)
{
	struct cmp_max_used_bits i_32, i_34, i_35, i_36, i_255, i_0;
	const struct cmp_max_used_bits *p;
	int return_val;

	/* set up max_used_bits item */
	memset(&i_32, 32, sizeof(struct cmp_max_used_bits));
	i_32.version = 32;
	memset(&i_34, 34, sizeof(struct cmp_max_used_bits));
	i_34.version = 34;
	memset(&i_35, 35, sizeof(struct cmp_max_used_bits));
	i_35.version = 35;
	memset(&i_36, 36, sizeof(struct cmp_max_used_bits));
	i_36.version = 36;
	memset(&i_255, 0xFF, sizeof(struct cmp_max_used_bits));
	i_255.version = 255;
	memset(&i_0, 0, sizeof(struct cmp_max_used_bits));
	i_0.version = 0;

	return_val = cmp_max_used_bits_list_add(&i_32);
	TEST_ASSERT_EQUAL_INT(return_val, 0);
	return_val = cmp_max_used_bits_list_add(&i_34);
	TEST_ASSERT_EQUAL_INT(return_val, 0);
	return_val = cmp_max_used_bits_list_add(&i_35);
	TEST_ASSERT_EQUAL_INT(return_val, 0);
	return_val = cmp_max_used_bits_list_add(&i_36);
	TEST_ASSERT_EQUAL_INT(return_val, 0);
	return_val = cmp_max_used_bits_list_add(&i_255);
	TEST_ASSERT_EQUAL_INT(return_val, 0);

	/* error cases */
	return_val = cmp_max_used_bits_list_add(NULL);
	TEST_ASSERT_EQUAL_INT(return_val, -1);
	return_val = cmp_max_used_bits_list_add(&i_0);
	TEST_ASSERT_EQUAL_INT(return_val, -1);
	i_0.version = CMP_MAX_USED_BITS_RESERVED_VERSIONS-1;
	return_val = cmp_max_used_bits_list_add(&i_0);
	TEST_ASSERT_EQUAL_INT(return_val, -1);

	p = cmp_max_used_bits_list_get(32);
	TEST_ASSERT_EQUAL_INT(p->version, 32);
	TEST_ASSERT(!memcmp(p, &i_32, sizeof(struct cmp_max_used_bits)));

	p = cmp_max_used_bits_list_get(36);
	TEST_ASSERT_EQUAL_INT(p->version, 36);
	TEST_ASSERT(!memcmp(p, &i_36, sizeof(struct cmp_max_used_bits)));

	p = cmp_max_used_bits_list_get(35);
	TEST_ASSERT_EQUAL_INT(p->version, 35);
	TEST_ASSERT(!memcmp(p, &i_35, sizeof(struct cmp_max_used_bits)));

	p = cmp_max_used_bits_list_get(255);
	TEST_ASSERT_EQUAL_INT(p->version, 255);
	TEST_ASSERT(!memcmp(p, &i_255, sizeof(struct cmp_max_used_bits)));

	p = cmp_max_used_bits_list_get(34);
	TEST_ASSERT_EQUAL_INT(p->version, 34);
	TEST_ASSERT(!memcmp(p, &i_34, sizeof(struct cmp_max_used_bits)));

	p = cmp_max_used_bits_list_get(0);
	TEST_ASSERT_EQUAL_INT(p->version, 0);
	TEST_ASSERT(!memcmp(p, &MAX_USED_BITS_SAFE, sizeof(struct cmp_max_used_bits)));

	p = cmp_max_used_bits_list_get(1);
	TEST_ASSERT_EQUAL_INT(p->version, 1);
	TEST_ASSERT(!memcmp(p, &MAX_USED_BITS_V1, sizeof(struct cmp_max_used_bits)));

	/* Try to get an element that is not in the list */
	p = cmp_max_used_bits_list_get(42);
	TEST_ASSERT_NULL(p);

	p = cmp_max_used_bits_list_get(3);
	TEST_ASSERT_NULL(p);


	/* overwrite a list item */
	memset(&i_35, 0x42, sizeof(struct cmp_max_used_bits));
	i_35.version = 35;
	return_val = cmp_max_used_bits_list_add(&i_35);
	TEST_ASSERT_EQUAL_INT(return_val, 1);
	p = cmp_max_used_bits_list_get(35);
	TEST_ASSERT_EQUAL_INT(p->version, 35);
	TEST_ASSERT(!memcmp(p, &i_35, sizeof(struct cmp_max_used_bits)));

	/* delete item */
	cmp_max_used_bits_list_delet(35);
	p = cmp_max_used_bits_list_get(35);
	TEST_ASSERT_NULL(p);

	cmp_max_used_bits_list_delet(34);
	p = cmp_max_used_bits_list_get(34);
	TEST_ASSERT_NULL(p);

	/* empty item */
	cmp_max_used_bits_list_empty();
	p = cmp_max_used_bits_list_get(36);
	TEST_ASSERT_NULL(p);

	cmp_max_used_bits_list_empty();
	p = cmp_max_used_bits_list_get(34);
	TEST_ASSERT_NULL(p);

	p = cmp_max_used_bits_list_get(0);
	TEST_ASSERT_EQUAL_INT(p->version, 0);
	TEST_ASSERT(!memcmp(p, &MAX_USED_BITS_SAFE, sizeof(struct cmp_max_used_bits)));

	p = cmp_max_used_bits_list_get(1);
	TEST_ASSERT_EQUAL_INT(p->version, 1);
	TEST_ASSERT(!memcmp(p, &MAX_USED_BITS_V1, sizeof(struct cmp_max_used_bits)));

	return_val = cmp_max_used_bits_list_add(&i_36);
	TEST_ASSERT_EQUAL_INT(return_val, 0);

	p = cmp_max_used_bits_list_get(36);
	TEST_ASSERT_EQUAL_INT(p->version, 36);
	TEST_ASSERT(!memcmp(p, &i_36, sizeof(struct cmp_max_used_bits)));

	cmp_max_used_bits_list_empty();

	/* error case */
#ifdef MAKE_MALLOC_FAIL_TEST
	malloc_fail = 1;
	return_val = cmp_max_used_bits_list_add(&i_36);
	TEST_ASSERT_EQUAL_INT(return_val, -1);
	malloc_fail = 0;
#endif
}
