/**
 * @file   example_cmp_icu.c
 * @author Dominik Loidolt (dominik.loidolt@univie.ac.at)
 * @date   Oct 2023
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
 * @brief demonstration of the use of the software compressor and the
 *	compression entity library
 */


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <leon_inttypes.h>

#include <cmp_entity.h>
#include <cmp_icu.h>


#define DATA_SAMPLES 6 /* number of 16 bit samples to compress */
#define CMP_BUF_LEN_SAMPLES DATA_SAMPLES /* compressed buffer has the same sample size as the data buffer */
#define CMP_ASW_VERSION_ID 1
/* The start_time, end_time, model_id and counter have to be managed by the ASW
 * here we use arbitrary values for demonstration */
#define START_TIME 0
#define END_TIME 0x23
#define MODEL_ID 42
#define MODEL_COUNTER 1


static int demo_icu_compression(void)
{
	struct cmp_cfg example_cfg;
	struct cmp_entity *cmp_entity = NULL;
	uint32_t i, cmp_buf_size, entity_buf_size, entity_size;
	int cmp_size_bits;
	void *ent_cmp_data;

	/* declare data buffers with some example data */
	enum cmp_data_type example_data_type = DATA_TYPE_IMAGETTE;
	uint16_t example_data[DATA_SAMPLES] = {42, 23, 1, 13, 20, 1000};
	uint16_t example_model[DATA_SAMPLES] = {0, 22, 3, 42, 23, 16};
	uint16_t updated_model[DATA_SAMPLES] = {0};

	/* create a compression configuration with default values */
	example_cfg = cmp_cfg_icu_create(example_data_type, CMP_DEF_IMA_MODEL_CMP_MODE,
					 CMP_DEF_IMA_MODEL_MODEL_VALUE, CMP_LOSSLESS);
	if (example_cfg.data_type == DATA_TYPE_UNKNOWN) {
		printf("Error occurred during cmp_cfg_icu_create()\n");
		goto fail;
	}

	/* configure imagette specific compression parameters with default values */
	if (cmp_cfg_icu_imagette(&example_cfg, CMP_DEF_IMA_MODEL_GOLOMB_PAR,
				 CMP_DEF_IMA_MODEL_SPILL_PAR)) {
		printf("Error occurred during cmp_cfg_icu_imagette()\n");
		goto fail;
	}

	/* get the size of the buffer for the compressed data in bytes */
	cmp_buf_size = cmp_cfg_icu_buffers(&example_cfg, example_data,
					   DATA_SAMPLES, example_model,
					   updated_model, NULL,
					   CMP_BUF_LEN_SAMPLES);
	if (!cmp_buf_size) {
		printf("Error occurred during cmp_cfg_icu_buffers()\n");
		goto fail;
	}

	/* create a compression entity */
	#define NO_CMP_MODE_RAW_USED 0
	entity_buf_size = cmp_ent_create(NULL, example_data_type, NO_CMP_MODE_RAW_USED,
					 cmp_buf_size);
	if (!entity_buf_size) {
		printf("Error occurred during cmp_ent_create()\n");
		goto fail;
	}
	cmp_entity = malloc(entity_buf_size); /* allocated memory for the compression entity */
	if (!cmp_entity) {
		printf("malloc failed!\n");
		goto fail;
	}
	entity_buf_size = cmp_ent_create(cmp_entity, example_data_type,
					 NO_CMP_MODE_RAW_USED, cmp_buf_size);
	if (!entity_buf_size) {
		printf("Error occurred during cmp_ent_create()\n");
		goto fail;
	}

	/*
	 * Configure the buffer related settings. We put the compressed data directly
	 * into the compression entity. In this way we do not need to copy the
	 * compressed data into the compression entity
	 */
	ent_cmp_data = cmp_ent_get_data_buf(cmp_entity);
	if (!ent_cmp_data) {
		printf("Error occurred during cmp_ent_get_data_buf()\n");
		goto fail;
	}
	cmp_buf_size = cmp_cfg_icu_buffers(&example_cfg, example_data,
					   DATA_SAMPLES, example_model,
					   updated_model, ent_cmp_data,
					   CMP_BUF_LEN_SAMPLES);
	if (!cmp_buf_size) {
		printf("Error occurred during cmp_cfg_icu_buffers()\n");
		goto fail;
	}

	/* now we compress the data on the ICU */
	cmp_size_bits = icu_compress_data(&example_cfg);
	if (cmp_size_bits < 0) {
		printf("Error occurred during icu_compress_data()\n");
		if (cmp_size_bits == CMP_ERROR_SMALL_BUF)
			printf("The compressed data buffer is too small to hold all compressed data!\n");
		if (cmp_size_bits == CMP_ERROR_HIGH_VALUE)
			printf("A data or model value is bigger than the max_used_bits parameter "
			       "allows (set with the cmp_cfg_icu_max_used_bits() function)!\n");
		goto fail;
	}

	/* now we set all the parameters in the compression entity header */
	/*
	 * NOTE: the size of the compress entity is smaller than the buffer size
	 * we have allocated for it (entity_buf_size), because the compressed
	 * data (fortunately) does not use the entire buffer we have provided
	 * for it
	 */
	entity_size = cmp_ent_build(cmp_entity, CMP_ASW_VERSION_ID, START_TIME, END_TIME,
				    MODEL_ID, MODEL_COUNTER, &example_cfg, cmp_size_bits);
	if (!entity_size) {
		printf("Error occurred during cmp_ent_build()\n");
		goto fail;
	}

	printf("Here's the compressed entity (size %"PRIu32"):\n"
	       "=========================================\n", entity_size);
	for (i = 0; i < entity_size; i++) {
		uint8_t *p = (uint8_t *)cmp_entity; /* the compression entity is big-endian */
		printf("%02X ", p[i]);
		if (i && !((i + 1) % 32))
			printf("\n");
	}
	printf("\n\nHere's the updated model (samples=%u):\n"
	       "=========================================\n", DATA_SAMPLES);
	for (i = 0; i < DATA_SAMPLES; i++) {
		printf("%04X ", updated_model[i]);
		if (i && !((i + 1) % 20))
			printf("\n");
	}
	printf("\n");

	free(cmp_entity);
	return 0;

fail:
	free(cmp_entity);
	return -1;
}


int main(void)
{
	return demo_icu_compression();
}
