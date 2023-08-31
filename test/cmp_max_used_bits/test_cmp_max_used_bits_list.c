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


#include <string.h>

#include <unity.h>

#include <cmp_max_used_bits_list.h>


/**
 * @test cmp_max_used_bits_list_add
 * @test cmp_max_used_bits_list_get
 * @test cmp_max_used_bits_list_delet
 * @test cmp_max_used_bits_list_empty
 */

void test_cmp_max_used_bits_list(void)
{
	struct cmp_max_used_bits i_17, i_21, i_22, i_23, i_255, i_0;
	struct cmp_max_used_bits *p;
	int return_val;

	/* set up max_used_bits item */
	memset(&i_17, 17, sizeof(struct cmp_max_used_bits));
	i_17.version = 17;
	memset(&i_21, 21, sizeof(struct cmp_max_used_bits));
	i_21.version = 21;
	memset(&i_22, 22, sizeof(struct cmp_max_used_bits));
	i_22.version = 22;
	memset(&i_23, 23, sizeof(struct cmp_max_used_bits));
	i_23.version = 23;
	memset(&i_255, 0xFF, sizeof(struct cmp_max_used_bits));
	i_255.version = 255;
	memset(&i_0, 0, sizeof(struct cmp_max_used_bits));
	i_0.version = 0;

	return_val = cmp_max_used_bits_list_add(&i_17);
	TEST_ASSERT_EQUAL_INT(return_val, 0);
	return_val = cmp_max_used_bits_list_add(&i_21);
	TEST_ASSERT_EQUAL_INT(return_val, 0);
	return_val = cmp_max_used_bits_list_add(&i_22);
	TEST_ASSERT_EQUAL_INT(return_val, 0);
	return_val = cmp_max_used_bits_list_add(&i_23);
	TEST_ASSERT_EQUAL_INT(return_val, 0);
	return_val = cmp_max_used_bits_list_add(&i_255);
	TEST_ASSERT_EQUAL_INT(return_val, 0);

	/* error cases */
	return_val = cmp_max_used_bits_list_add(NULL);
	TEST_ASSERT_EQUAL_INT(return_val, -1);
	return_val = cmp_max_used_bits_list_add(&i_0);
	TEST_ASSERT_EQUAL_INT(return_val, -1);
	i_0.version = 16;
	return_val = cmp_max_used_bits_list_add(&i_0);
	TEST_ASSERT_EQUAL_INT(return_val, -1);

	p = cmp_max_used_bits_list_get(17);
	TEST_ASSERT_EQUAL_INT(p->version, 17);
	TEST_ASSERT(!memcmp(p, &i_17, sizeof(struct cmp_max_used_bits)));

	p = cmp_max_used_bits_list_get(23);
	TEST_ASSERT_EQUAL_INT(p->version, 23);
	TEST_ASSERT(!memcmp(p, &i_23, sizeof(struct cmp_max_used_bits)));

	p = cmp_max_used_bits_list_get(22);
	TEST_ASSERT_EQUAL_INT(p->version, 22);
	TEST_ASSERT(!memcmp(p, &i_22, sizeof(struct cmp_max_used_bits)));

	p = cmp_max_used_bits_list_get(255);
	TEST_ASSERT_EQUAL_INT(p->version, 255);
	TEST_ASSERT(!memcmp(p, &i_255, sizeof(struct cmp_max_used_bits)));

	p = cmp_max_used_bits_list_get(21);
	TEST_ASSERT_EQUAL_INT(p->version, 21);
	TEST_ASSERT(!memcmp(p, &i_21, sizeof(struct cmp_max_used_bits)));

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
	memset(&i_22, 0x42, sizeof(struct cmp_max_used_bits));
	i_22.version = 22;
	return_val = cmp_max_used_bits_list_add(&i_22);
	TEST_ASSERT_EQUAL_INT(return_val, 1);
	p = cmp_max_used_bits_list_get(22);
	TEST_ASSERT_EQUAL_INT(p->version, 22);
	TEST_ASSERT(!memcmp(p, &i_22, sizeof(struct cmp_max_used_bits)));

	/* delete item */
	cmp_max_used_bits_list_delet(22);
	p = cmp_max_used_bits_list_get(22);
	TEST_ASSERT_NULL(p);

	cmp_max_used_bits_list_delet(21);
	p = cmp_max_used_bits_list_get(21);
	TEST_ASSERT_NULL(p);

	/* empty item */
	cmp_max_used_bits_list_empty();
	p = cmp_max_used_bits_list_get(23);
	TEST_ASSERT_NULL(p);

	cmp_max_used_bits_list_empty();
	p = cmp_max_used_bits_list_get(21);
	TEST_ASSERT_NULL(p);

	p = cmp_max_used_bits_list_get(0);
	TEST_ASSERT_EQUAL_INT(p->version, 0);
	TEST_ASSERT(!memcmp(p, &MAX_USED_BITS_SAFE, sizeof(struct cmp_max_used_bits)));

	p = cmp_max_used_bits_list_get(1);
	TEST_ASSERT_EQUAL_INT(p->version, 1);
	TEST_ASSERT(!memcmp(p, &MAX_USED_BITS_V1, sizeof(struct cmp_max_used_bits)));

	return_val = cmp_max_used_bits_list_add(&i_23);
	TEST_ASSERT_EQUAL_INT(return_val, 0);

	p = cmp_max_used_bits_list_get(23);
	TEST_ASSERT_EQUAL_INT(p->version, 23);
	TEST_ASSERT(!memcmp(p, &i_23, sizeof(struct cmp_max_used_bits)));

	cmp_max_used_bits_list_empty();
}
