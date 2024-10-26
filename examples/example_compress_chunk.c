/**
 * @file   example_compress_chunk.c
 * @author Dominik Loidolt (dominik.loidolt@univie.ac.at)
 * @date   Feb 2024
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
 * @brief demonstration of the chunk compressor
 */


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <leon_inttypes.h>

#include <cmp_chunk.h>
#include <cmp_data_types.h>


/*
 * The asw_version_id, model_id and counter have to be managed by the ASW here we
 * use arbitrary values for demonstration
 */
#define ASW_VERSION_ID 1
#define MODEL_ID 42
#define MODEL_COUNTER 1


/**
 * @brief This is a dummy implementation of a function returning a current timestamp
 */

static uint64_t dummy_return_timestamp(void)
{
	return 0x0FF1CC0FFEE; /* arbitrary value */
}


/**
 * @brief Demonstration of a 1d chunk compression
 *
 * Compressing a background/offset chunk in 1d-differencing mode with zero escape
 * mechanism
 */

static int demo_comperss_chunk_1d(void)
{
	struct background background_data[1] = {{0, 1, 0xF0}};
	struct offset offset_data[2] = {{1, 2}, {3, 4}};
	enum { CHUNK_SIZE = 2*COLLECTION_HDR_SIZE + sizeof(background_data)
			    + sizeof(offset_data) };
	uint8_t chunk[CHUNK_SIZE] = {0}; /* Do not put large amount of data on the stack! */

	uint32_t *compressed_data;
	uint32_t cmp_size_bytes;

	{	/* build a chunk of a background and an offset collection */
		struct collection_hdr *col = (struct collection_hdr *)chunk;

		if (cmp_col_set_subservice(col, SST_NCxx_S_SCIENCE_BACKGROUND))
			return -1;
		if (cmp_col_set_data_length(col, sizeof(background_data)))
			return -1;
		memcpy(col->entry, background_data, sizeof(background_data));

		col = (struct collection_hdr *)(chunk + cmp_col_get_size(col));
		if (cmp_col_set_subservice(col, SST_NCxx_S_SCIENCE_OFFSET))
			return -1;
		if (cmp_col_set_data_length(col, sizeof(offset_data)))
			return -1;
		memcpy(col->entry, offset_data, sizeof(offset_data));
	}

	/* the chunk compression only needs to be initialised once */
	compress_chunk_init(&dummy_return_timestamp, ASW_VERSION_ID);

	{	/* compress the chunk */
		struct cmp_par cmp_par;
		uint32_t cmp_size_bound;

		/* prepare the compression parameters needed to compress a
		 * background/offset chunk (arbitrary values)
		 */
		memset(&cmp_par, 0, sizeof(cmp_par));
		cmp_par.cmp_mode = CMP_MODE_DIFF_ZERO;
		cmp_par.nc_offset_mean = 1;
		cmp_par.nc_offset_variance = 2;
		cmp_par.nc_background_mean = 3;
		cmp_par.nc_background_variance = 4;
		cmp_par.nc_background_outlier_pixels = 5;
		/* Only the compression parameters needed to compress offset and
		 * background collections are set.
		 */

		/* prepare the buffer for the compressed data */
		cmp_size_bound = compress_chunk_cmp_size_bound(chunk, CHUNK_SIZE);
		if (cmp_is_error(cmp_size_bound)) {
			printf("Error occurred during compress_chunk_cmp_size_bound()\n");
			printf("Failed with error code %d: %s\n",
				cmp_get_error_code(cmp_size_bound),
				cmp_get_error_name(cmp_size_bound));
			/* error handling */
			return -1;
		}
		compressed_data = malloc(cmp_size_bound);
		if (!compressed_data) {
			printf("Error occurred during malloc()\n");
			/* error handling */
			return -1;
		}

		cmp_size_bytes = compress_chunk(chunk, CHUNK_SIZE, NULL, NULL,
						compressed_data, cmp_size_bound,
						&cmp_par);
		/* this is another way to check if a function failed */
		if (cmp_get_error_code(cmp_size_bytes) != CMP_ERROR_NO_ERROR) {
			printf("Error occurred during compress_chunk()\n");
			printf("Failed with error code %d: %s\n",
				cmp_get_error_code(cmp_size_bytes),
				cmp_get_error_name(cmp_size_bytes));
			free(compressed_data);
			/* error handling */
			return -1;
		}
		cmp_size_bytes = compress_chunk_set_model_id_and_counter(compressed_data,
							cmp_size_bytes, MODEL_ID, 0);
		if (cmp_is_error(cmp_size_bytes)) {
			printf("Error occurred during compress_chunk_set_model_id_and_counter()\n");
			printf("we get error code %d: %s\n",
				cmp_get_error_code(cmp_size_bytes),
				cmp_get_error_name(cmp_size_bytes));
			free(compressed_data);
			/* error handling */
			return -1;
		}
	}

	{	/* have a look at the compressed data */
		uint32_t i;

		printf("Here's the compressed data including the compression entity header (size %u):\n", cmp_size_bytes);
		for (i = 0; i < cmp_size_bytes; i++) {
			const uint8_t *p = (uint8_t *)compressed_data; /* the compression entity is big-endian */
			printf("%02X ", p[i]);
			if (i && !((i + 1) % 32))
				printf("\n");
		}
		printf("\n\n");
	}
	free(compressed_data);
	return 0;
}


/**
 * @brief Demonstration of a model chunk compression
 *
 * Compressing a background/offset chunk in model mode with multi escape
 * mechanism
 */

static int demo_comperss_chunk_model(void)
{
	struct background background_model[1] = {{0, 1, 0xF0}};
	struct offset offset_model[2] = {{1, 2}, {3, 4}};
	struct background background_data[1] = {{1, 2, 0xFA}};
	struct offset offset_data[2] = {{1, 32}, {23, 42}};
	enum { CHUNK_SIZE = 2*COLLECTION_HDR_SIZE + sizeof(background_data)
			    + sizeof(offset_data) };
	uint8_t chunk[CHUNK_SIZE] = {0}; /* Do not put large amount of data on the stack! */
	uint8_t model_chunk[(CHUNK_SIZE)] = {0}; /* Do not put large amount of data on the stack! */
	uint8_t updated_chunk_model[CHUNK_SIZE] = {0}; /* Do not put large amount of data on the stack! */

	/*
	 * Here we use the COMPRESS_CHUNK_BOUND macro to determine the worst
	 * case compression size; to do this we need to know the chunk_size and
	 * the number of collections in the chunk (2 in this demo)
	 */
	uint32_t compressed_data[COMPRESS_CHUNK_BOUND(CHUNK_SIZE, 2)/4]; /* Do not put large amount of data on the stack! */
	uint32_t cmp_size_bytes;

	{	/* build a chunk of a background and an offset collection */
		struct collection_hdr *col = (struct collection_hdr *)chunk;

		if (cmp_col_set_subservice(col, SST_NCxx_S_SCIENCE_BACKGROUND))
			return -1;
		if (cmp_col_set_data_length(col, sizeof(background_data)))
			return -1;
		memcpy(col->entry, background_data, sizeof(background_data));

		col = (struct collection_hdr *)(chunk + cmp_col_get_size(col));
		if (cmp_col_set_subservice(col, SST_NCxx_S_SCIENCE_OFFSET))
			return -1;
		if (cmp_col_set_data_length(col, sizeof(offset_data)))
			return -1;
		memcpy(col->entry, offset_data, sizeof(offset_data));

		/* build a model_chunk of a background and an offset collection */
		col = (struct collection_hdr *)model_chunk;

		if (cmp_col_set_subservice(col, SST_NCxx_S_SCIENCE_BACKGROUND))
			return -1;
		if (cmp_col_set_data_length(col, sizeof(background_model)))
			return -1;
		memcpy(col->entry, background_model, sizeof(background_model));

		col = (struct collection_hdr *)(model_chunk + cmp_col_get_size(col));
		if (cmp_col_set_subservice(col, SST_NCxx_S_SCIENCE_OFFSET))
			return -1;
		if (cmp_col_set_data_length(col, sizeof(offset_model)))
			return -1;
		memcpy(col->entry, offset_model, sizeof(offset_model));
	}

	/* the chunk compression only needs to be initialised once */
	compress_chunk_init(&dummy_return_timestamp, ASW_VERSION_ID);

	{	/* compress the chunk */
		struct cmp_par cmp_par;

		/* prepare the compression parameters needed to compress a
		 * background/offset chunk
		 */
		memset(&cmp_par, 0, sizeof(cmp_par));
		cmp_par.cmp_mode = CMP_MODE_MODEL_MULTI;
		cmp_par.model_value = 11;
		cmp_par.nc_offset_mean = 1;
		cmp_par.nc_offset_variance = 2;
		cmp_par.nc_background_mean = 3;
		cmp_par.nc_background_variance = 4;
		cmp_par.nc_background_outlier_pixels = 5;
		/* Only the compression parameters needed to compress offset and
		 * background collections are set.
		 */

		cmp_size_bytes = compress_chunk(chunk, CHUNK_SIZE, model_chunk,
						updated_chunk_model, compressed_data,
						sizeof(compressed_data), &cmp_par);
		if (cmp_is_error(cmp_size_bytes)) {
			printf("Error occurred during compress_chunk()\n");
			printf("Failed with error code %d: %s\n",
				cmp_get_error_code(cmp_size_bytes),
				cmp_get_error_name(cmp_size_bytes));
			/* error handling */
			return -1;
		}
		cmp_size_bytes = compress_chunk_set_model_id_and_counter(compressed_data,
						cmp_size_bytes, MODEL_ID, MODEL_COUNTER);
		if (cmp_is_error(cmp_size_bytes)) {
			printf("Error occurred during compress_chunk_set_model_id_and_counter()\n");
			printf("Failed with error code %d: %s\n",
				cmp_get_error_code(cmp_size_bytes),
				cmp_get_error_name(cmp_size_bytes));
			/* error handling */
			return -1;
		}
	}

	{	/* have a look at the compressed data */
		uint32_t i;

		printf("Here's the compressed data including the compression entity header (size %u):\n", cmp_size_bytes);
		for (i = 0; i < cmp_size_bytes; i++) {
			const uint8_t *p = (uint8_t *)compressed_data; /* the compression entity is big-endian */
			printf("%02X ", p[i]);
			if (i && !((i + 1) % 32))
				printf("\n");
		}
		printf("\n");
	}

	return 0;
}


int main(void)
{
	int error = 0;

	error |= demo_comperss_chunk_1d();
	error |= demo_comperss_chunk_model();

	if (error)
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
