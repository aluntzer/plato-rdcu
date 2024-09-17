/**
 * @file chunk_round_trip.c
 * @date 2024
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
 * @brief chunk compression/decompression test
 *
 */

#include <string.h>
#include <stdlib.h>

#include "../test_common/test_common.h"
#include "../../lib/cmp_chunk.h"
#include "../../lib/decmp.h"
#include "chunk_round_trip.h"

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
#  include "../fuzz/fuzz_helpers.h"
#  define TEST_ASSERT(cond) FUZZ_ASSERT(cond)
#else
#  include <unity.h>
#endif


/**
 * @brief performs chunk compression and checks if a decompression is possible
 *
 * @param chunk			pointer to the chunk to be compressed
 * @param chunk_size		byte size of the chunk
 * @param chunk_model		pointer to a model of a chunk; has the same size
 *				as the chunk (can be NULL if no model compression
 *				mode is used)
 * @param updated_chunk_model	pointer to store the updated model for the next
 *				model mode compression; has the same size as the
 *				chunk (can be the same as the model_of_data
 *				buffer for in-place update or NULL if updated
 *				model is not needed)
 * @param dst			destination pointer to the compressed data
 *				buffer; has to be 4-byte aligned; can be NULL to
 *				only get the compressed data size
 * @param dst_capacity		capacity of the dst buffer; it's recommended to
 *				provide a dst_capacity >=
 *				compress_chunk_cmp_size_bound(chunk, chunk_size)
 *				as it eliminates one potential failure scenario:
 *				not enough space in the dst buffer to write the
 *				compressed data; size is internally rounded down
 *				to a multiple of 4
 * @param use_decmp_buf		if non-zero, a buffer is allocated for the
 *				decompressed data
 * @param use_decmp_up_model	if non-zero, a buffer for the updated model is
 *				allocated during the decompression
 *
 * @returns the return value of the compress_chunk() function
 */

uint32_t chunk_round_trip(const void *chunk, uint32_t chunk_size,
			  const void *chunk_model, void *updated_chunk_model,
			  uint32_t *dst, uint32_t dst_capacity,
			  const struct cmp_par *cmp_par,
			  int use_decmp_buf, int use_decmp_up_model)
{
	uint32_t cmp_size;
	void *model_cpy_p = NULL;
	const void *model_cpy = NULL;

	/* if in-place model update is used (up_model == model), the model
	 * needed for decompression is destroyed; therefore we make a copy
	 */
	if (chunk_model) {
		if (updated_chunk_model == chunk_model) {
			model_cpy_p = TEST_malloc(chunk_size);
			memcpy(model_cpy_p, chunk_model, chunk_size);
			model_cpy = model_cpy_p;
		} else {
			model_cpy = chunk_model;
		}
	}

	cmp_size = compress_chunk(chunk, chunk_size, chunk_model, updated_chunk_model,
				  dst, dst_capacity, cmp_par);

	if (!cmp_is_error(cmp_size)) { /* secound run wich dst = NULL */
		uint32_t cmp_size2;
		/* reset model if in-place update was used */
		if (chunk_model && updated_chunk_model == chunk_model) {
			memcpy(updated_chunk_model, model_cpy, chunk_size);
		}
		cmp_size2 = compress_chunk(chunk, chunk_size, chunk_model, updated_chunk_model,
					   NULL, dst_capacity, cmp_par);
		if (cmp_get_error_code(cmp_size) == CMP_ERROR_SMALL_BUF_) {
			TEST_ASSERT(!cmp_is_error(cmp_size));
		} else {
			TEST_ASSERT(cmp_size == cmp_size2);
		}
	}


	/* decompress the data if the compression was successful */
	if (!cmp_is_error(cmp_size) && dst) {
		void *decmp_data = NULL;
		void *up_model_decmp = NULL;
		int decmp_size;

		decmp_size = decompress_cmp_entiy((struct cmp_entity *)dst, model_cpy, NULL, NULL);
		TEST_ASSERT(decmp_size >= 0);
		TEST_ASSERT((uint32_t)decmp_size == chunk_size);

		if (use_decmp_buf)
			decmp_data = TEST_malloc(chunk_size);
		if (use_decmp_up_model) {
			up_model_decmp = TEST_malloc(chunk_size);
			if (!model_mode_is_used(cmp_par->cmp_mode))
				memset(up_model_decmp, 0, chunk_size); /* up_model is not used */
		}

		decmp_size = decompress_cmp_entiy((struct cmp_entity *)dst, model_cpy,
						  up_model_decmp, decmp_data);
		TEST_ASSERT(decmp_size >= 0);
		TEST_ASSERT((uint32_t)decmp_size == chunk_size);

		if (use_decmp_buf) {
			TEST_ASSERT(!memcmp(chunk, decmp_data, chunk_size));

			/*
			 * the model is only updated when the decompressed_data
			 * buffer is set
			 */
			if (use_decmp_up_model && updated_chunk_model)
				TEST_ASSERT(!memcmp(updated_chunk_model, up_model_decmp, chunk_size));
		}

		free(decmp_data);
		free(up_model_decmp);
	}

	if (updated_chunk_model == chunk_model)
		free(model_cpy_p);

	return cmp_size;
}
