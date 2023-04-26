#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "ref_data.h"
#include "../../include/cmp_icu.h"
#include "../../include/cmp_rdcu.h"
#include "../../include/rdcu_cmd.h"
#include "../../include/decmp.h"


#define EXP_CMP_INTERRUPTED 0xFFFF


void init_rdcu(void);

int rdcu_inject_edac_error(struct cmp_cfg *cfg, uint32_t addr); /* TODO: remove this */
int rdcu_start_compression(void);


void gen_ref_data(void* buffer, size_t size)
{
	uint8_t *p = buffer;

	while(size > REF_DATA_SIZE) {
		memcpy(p, ref_data2, REF_DATA_SIZE);
		p += REF_DATA_SIZE;
		size -= REF_DATA_SIZE;
	}
	if (size)
		memcpy(p, ref_data2, size);
}


void gen_ref_model(void* buffer, size_t size)
{
	uint8_t *p = buffer;

	while(size > REF_DATA_SIZE) {
		memcpy(p, ref_data1, REF_DATA_SIZE);
		size -= REF_DATA_SIZE;
		p += REF_DATA_SIZE;
	}
	if (size)
		memcpy(p, ref_data1, size);
}

/**
 * @brief generate a random number in a range
 *
 * @param min minimum value (inclusive)
 * @param max maximum value (inclusive)
 *
 * @returns "random" numbers in the range [min, max]
 *
 * @see https://c-faq.com/lib/randrange.html
 */

static uint32_t random_between(unsigned int min, unsigned int max)
{
	assert(min < max);
	assert(max-min < RAND_MAX);
	return min + rand() / (RAND_MAX / (max - min + 1ULL) + 1);
}


/**
 * @brief generate a random data
 *
 * @param buffer	destination buffer
 * @param samples	length of the random data in 16-bit samples
 */

static void gen_random_samples(uint16_t *buffer, uint32_t samples)
{
	size_t i;

	assert(RAND_MAX > 1 << (sizeof(*buffer)*8));
	for (i = 0; i < samples; i++)
		buffer[i] = rand();
}


/**
 * @brief check the cmp_size, ap1_cmp_size, ap2_cmp_size parameters
 *
 * @param cfg	pointer to the used compression configuration
 * @param info	pointer to the compression information of the related compression
 */

static void check_cmp_size(struct cmp_cfg *cfg, struct cmp_info *info, struct cmp_status *status)
{
	uint16_t const small_buf_err = (info->cmp_err >> SMALL_BUFFER_ERR_BIT) & 1U;
	struct cmp_cfg sw_cfg = *cfg;
	int sw_cmp_size;

	sw_cfg.icu_output_buf = NULL; /* we do not need the compressed data, only the cmp_size */
	sw_cfg.icu_new_model_buf = NULL; /* we do not update the model */

	/* if we have a compression error the cmp_size should be zero */
	if (info->cmp_err || status->cmp_interrupted) {
#if 1
		/* the compression size is not set back after an input condition
		 * error*/
		if ((info->cmp_err >> CMP_MODE_ERR_BIT) & 1U ||
		    (info->cmp_err >> MODEL_VALUE_ERR_BIT) & 1U ||
		    (info->cmp_err >> CMP_PAR_ERR_BIT) & 1U ||
		    (info->cmp_err >> AP1_CMP_PAR_ERR_BIT) & 1U ||
		    (info->cmp_err >> AP2_CMP_PAR_ERR_BIT) & 1U ||
		    ((info->cmp_err >> SMALL_BUFFER_ERR_BIT) & 1U && (info->cmp_mode_used == CMP_MODE_RAW)))
			return;
#endif
		assert(0 == info->cmp_size);
		assert(0 == info->ap1_cmp_size);
		assert(0 == info->ap2_cmp_size);
		if (!small_buf_err) /* we check the small error against the software compression */
			return;
	}

	/* calculate the expected compression bit size */
	if (cfg->cmp_mode == CMP_MODE_RAW) {
		if (cfg->buffer_length < cfg->samples)
			sw_cmp_size = ~0; /* buffer to small to hold the "compressed" data */
		else
			sw_cmp_size = cfg->samples * IMA_SAM2BYT * 8;
	} else {
		sw_cmp_size = icu_compress_data(&sw_cfg);
		assert(sw_cmp_size >= 0);
	}

	if ((unsigned int)sw_cmp_size > cfg->buffer_length*IMA_SAM2BYT*8) {
		assert(info->cmp_err == 1 << SMALL_BUFFER_ERR_BIT);
		if (info->cmp_mode_used != CMP_MODE_RAW) {
			assert(0 == info->cmp_size);
			assert(0 == info->ap1_cmp_size);
			assert(0 == info->ap2_cmp_size);
		}
	} else if (info->cmp_mode_used != CMP_MODE_RAW &&
		   (unsigned int)sw_cmp_size == cfg->buffer_length*IMA_SAM2BYT*8) {
		/* note: to do the implication of the HW compressor a condition where
		 * the compressed size fits *actually* in the compressed data buffer
		 * results in a small buffer err, in a non-raw mode.
		 */
		assert(info->cmp_err == 1 << SMALL_BUFFER_ERR_BIT);
		assert(0 == info->cmp_size);
		assert(0 == info->ap1_cmp_size);
		assert(0 == info->ap2_cmp_size);
	} else {
		assert(!small_buf_err);
		if ((uint32_t)sw_cmp_size != info->cmp_size) {
			printf("cmp_size: %lu, cmp_size_exp: %d\n", info->cmp_size, sw_cmp_size);
			print_cmp_info(info);
		}
		assert((uint32_t)sw_cmp_size == info->cmp_size);

		sw_cfg.golomb_par = sw_cfg.ap1_golomb_par;
		sw_cfg.spill = sw_cfg.ap1_spill;
		sw_cmp_size = icu_compress_data(&sw_cfg);
		assert(sw_cmp_size >= 0);
		assert((uint32_t)sw_cmp_size == info->ap1_cmp_size);

		sw_cfg.golomb_par = sw_cfg.ap2_golomb_par;
		sw_cfg.spill = sw_cfg.ap2_spill;
		sw_cmp_size = icu_compress_data(&sw_cfg);
		assert(sw_cmp_size >= 0);
		assert((uint32_t)sw_cmp_size == info->ap2_cmp_size);
	}
}


/**
 * @brief check the content of the compression status registers
 *
 * @param cfg		pointer to the used compression configuration
 * @param cmp_err_exp	expected compression error
 */

static void check_cmp_register(struct cmp_cfg *cfg, uint16_t cmp_err_exp)
{
	struct cmp_status status;
	struct cmp_info info;

	memset(&status, 0, sizeof(struct cmp_status));
	memset(&info, 0, sizeof(struct cmp_info));

	if (rdcu_read_cmp_status(&status))
		assert(0);
	assert(1 == status.cmp_ready);
	assert(0 == status.cmp_active);
	if (cmp_err_exp == EXP_CMP_INTERRUPTED) {
		assert(1 == status.cmp_interrupted);
	} else
		assert(0 == status.cmp_interrupted);
	assert(1 == status.rdcu_interrupt_en);
	/* we check data_valid flag later */

	if (rdcu_read_cmp_info(&info))
		assert(0);
	assert(cfg->cmp_mode == info.cmp_mode_used);
	assert(cfg->spill == info.spill_used);
	assert(cfg->golomb_par == info.golomb_par_used);
	assert(cfg->samples == info.samples_used);
	assert(cfg->rdcu_new_model_adr == info.rdcu_new_model_adr_used);
	assert(cfg->rdcu_buffer_adr == info.rdcu_cmp_adr_used);
	assert(cfg->model_value == info.model_value_used);
	assert(cfg->round == info.round_used);
	if (cmp_err_exp == EXP_CMP_INTERRUPTED) {
		assert(0 == info.cmp_err);
	} else if (cmp_err_exp != info.cmp_err) {
		print_cmp_info(&info);
		printf("\n\nCompression error exp: %x, act: %x\n", cmp_err_exp, info.cmp_err);
		assert(0);
	}
	if (!!cmp_err_exp == status.data_valid) {
		printf("data_valid exp: %x, act: %x\n", !!cmp_err_exp , status.data_valid);
		assert(0);
	}
	check_cmp_size(cfg, &info, &status);
}


/**
 * @brief round the data for lossy compression check
 */

static uint16_t round_data(uint16_t data, uint32_t round)
{
	return (data >> round) << round;
}


/**
 * @brief check the compressed data by decompression them
 *
 * @param data_to_compress input data for comparison
 * @param model_of_data model of the input data for comparison
 */

static void check_compressed_data(const uint16_t *data_to_compress, uint16_t *model_of_data)
{
	uint32_t i;
	struct cmp_info info;
#ifdef __sparc__
	uint32_t *compressed_data =  (void *)0x63000000;
	uint16_t *updated_model =    (void *)0x64000000;
	uint16_t *updated_model_exp =(void *)0x65000000;
	uint16_t *decompressed_data =(void *)0x66000000;
#else /* assume PC */
	uint32_t *compressed_data = malloc(RDCU_SRAM_SIZE);
	uint16_t *updated_model = malloc(RDCU_SRAM_SIZE);
	uint16_t *updated_model_exp = malloc(RDCU_SRAM_SIZE);
	uint16_t *decompressed_data = malloc(RDCU_SRAM_SIZE);
#endif
	if (!compressed_data) {
		printf("Error allocating memory\n");
		return;
	}
	if (!updated_model) {
		printf("Error allocating memory\n");
		return;
	}
	if (!decompressed_data) {
		printf("Error allocating memory\n");
		return;
	}

	if (rdcu_read_cmp_info(&info))
		assert(0);

	if (!info.cmp_err) {
		memset(&info, 0, sizeof(struct cmp_info));
		memset(compressed_data, 0, RDCU_SRAM_SIZE);
		memset(decompressed_data, 0, RDCU_SRAM_SIZE);
		if (model_mode_is_used(info.cmp_mode_used)) {
			memset(updated_model, 0, RDCU_SRAM_SIZE);
			memset(updated_model_exp, 0, RDCU_SRAM_SIZE);
		}


		if (rdcu_read_cmp_bitstream(&info, compressed_data) < 0)
			assert(0);

		if (decompress_rdcu_data(compressed_data, &info, model_of_data,
					 updated_model_exp, decompressed_data) < 0) {
			print_cmp_info(&info);
			assert(0);
		}

		for (i = 0; i < info.samples_used; i++) {
			if (round_data(data_to_compress[i], info.round_used) != decompressed_data[i]) {
				print_cmp_info(&info);
				printf("\n\nDecompressed data are not the same as the original data:\n");
				printf("%lu: exp: %x act: %x\n", i, round_data(data_to_compress[i], info.round_used), decompressed_data[i]);
					assert(0);
				}
		}

		if (model_mode_is_used(info.cmp_mode_used)) {
			if (rdcu_read_model(&info, updated_model) < 0) {
				printf("Error occurred by reading in the updated model from the RDCU\n");
				assert(0);
			}
			/* check if updated model are check up */
			for (i = 0; i < info.samples_used; i++)
				assert(updated_model_exp[i] == updated_model[i]);
		}
	}
#ifndef __sparc__
	free(compressed_data);
	free(updated_model);
	free(updated_model_exp);
	free(decompressed_data);
#endif
}


/**
 * @brief Waiting for the hardware compressor to finish.
 */

static void wait_compression_is_done(void)
{
	struct cmp_status status;
	int cnt = 0;

	/* start polling the compression status */
	/* alternative you can wait for an interrupt form the RDCU */
	do {
		/* check compression status */
		if (rdcu_read_cmp_status(&status)) {
			printf("Error occur during rdcu_read_cmp_status\n");
			exit(EXIT_FAILURE);
		}

		cnt++;

		if (cnt > 0x1FF) {	/* wait for some polls */
			printf("poll cycle :%x\n", cnt);
			printf("Not waiting for compressor to become ready, will check status and abort\n");

			/* interrupt the data compression */
			rdcu_interrupt_compression();
		}
	} while (!status.cmp_ready);
}


/**
 * @brief run the configuration on the RDCU; wait for the compression is
 *	finished, check the results
 *
 * @param cfg		pointer to the compression configuration to be tested
 * @param cmp_err_exp	expected compression error
 */

static void run_compression_check_results(struct cmp_cfg *cfg, uint16_t cmp_err_exp)
{
	if (rdcu_compress_data(cfg)) {
		printf("Error occur during rdcu_compress_data()\n");
		assert(0);
	}
	wait_compression_is_done();
	check_cmp_register(cfg, cmp_err_exp);
	if (!cmp_err_exp)
		check_compressed_data(cfg->input_buf, cfg->model_buf);
}


/**
 * @brief generate a test compressing configuration in RAW mode (output = input)
 *	using the whole SRAM
 *
 * @param data_to_compress	buffer to store input data
 * @param model_of_data		buffer to store the mode data (if needed)
 * @param cfg			compression configuration to test
 *
 * @returns the expect compression error
 */

static uint16_t test_raw_mode_max_samples(uint16_t *data_to_compress, uint16_t *model_of_data, struct cmp_cfg *cfg)
{
	/* we use the whole SRAM: fist 1/2 -> data to compress
	 * second 1/2 -> "compressed" raw data */
	uint32_t data_samples = (RDCU_SRAM_SIZE/IMA_SAM2BYT)/2;
	uint32_t buffer_length = data_samples;
	uint32_t const rdcu_data_adr = 0x0;
	uint32_t const rdcu_model_adr = 0x0; /* model not used */
	uint32_t const rdcu_new_model_adr = 0x0; /* model not used */
	uint32_t const rdcu_buffer_adr = RDCU_SRAM_SIZE/2;
	int err;

	model_of_data = NULL; /* model not used */

#ifdef FPGA_VERSION_0_7
	/* there is a bug in FPGA version 0.7 where in RAW mode the buffer_length
	 * has to be at least two bigger than the samples parameter
	 */
	buffer_length += 2;
#endif
	*cfg = rdcu_cfg_create(DATA_TYPE_IMAGETTE_ADAPTIVE, CMP_MODE_RAW,
			       CMP_DEF_IMA_MODEL_MODEL_VALUE, CMP_LOSSLESS);
	assert(cfg->data_type != DATA_TYPE_UNKNOWN);

	gen_random_samples(data_to_compress, data_samples);
	err = rdcu_cfg_buffers(cfg, data_to_compress, data_samples, model_of_data, rdcu_data_adr,
			       rdcu_model_adr, rdcu_new_model_adr, rdcu_buffer_adr, buffer_length);
	assert(err == 0);

	err = rdcu_cfg_imagette_default(cfg);
	assert(err == 0);

	return 0;
}


/**
 * @brief generate a 1d-diff mode default test compressing configuration
 *
 * @param data_to_compress	buffer to store input data
 * @param model_of_data		buffer to store the mode data (if needed)
 * @param cfg			compression configuration to test
 *
 * @returns the expect compression error
 */

static uint16_t test_diff_default(uint16_t *data_to_compress, uint16_t *model_of_data, struct cmp_cfg *cfg)
{
	uint32_t data_samples  = 0x141D8D;
	uint32_t buffer_length = 0x100000;
	int err;

	model_of_data = NULL; /* model not used */

	*cfg = rdcu_cfg_create(DATA_TYPE_IMAGETTE_ADAPTIVE, CMP_DEF_IMA_DIFF_CMP_MODE,
			       CMP_DEF_IMA_DIFF_MODEL_VALUE, CMP_LOSSLESS);
	assert(cfg->data_type != DATA_TYPE_UNKNOWN);

	gen_ref_data(data_to_compress, data_samples * IMA_SAM2BYT);
	err = rdcu_cfg_buffers(cfg, data_to_compress, data_samples, model_of_data,
			       CMP_DEF_IMA_DIFF_RDCU_DATA_ADR,
			       CMP_DEF_IMA_DIFF_RDCU_MODEL_ADR,
			       CMP_DEF_IMA_DIFF_RDCU_UP_MODEL_ADR,
			       CMP_DEF_IMA_DIFF_RDCU_BUFFER_ADR,
			       buffer_length);
	assert(err == 0);

	err = rdcu_cfg_imagette_default(cfg);
	assert(err == 0);

	return 0;
}


/**
 * @brief generate a model mode default test compressing configuration
 *
 * @param data_to_compress	buffer to store input data
 * @param model_of_data		buffer to store the mode data (if needed)
 * @param cfg			compression configuration to test
 *
 * @returns the expect compression error
 */

static uint16_t test_model_default(uint16_t *data_to_compress, uint16_t *model_of_data, struct cmp_cfg *cfg)
{
	uint32_t const data_samples  = 0x100000;
	uint32_t const buffer_length = 0x04F101;
	int err;

	*cfg = rdcu_cfg_create(DATA_TYPE_IMAGETTE_ADAPTIVE, CMP_DEF_IMA_MODEL_CMP_MODE,
			       CMP_DEF_IMA_MODEL_MODEL_VALUE, CMP_LOSSLESS);
	assert(cfg->data_type != DATA_TYPE_UNKNOWN);

	gen_ref_data(data_to_compress, data_samples*IMA_SAM2BYT);
	gen_ref_model(model_of_data, data_samples*IMA_SAM2BYT);
	err = rdcu_cfg_buffers(cfg, data_to_compress, data_samples, model_of_data,
			       CMP_DEF_IMA_MODEL_RDCU_DATA_ADR,
			       CMP_DEF_IMA_MODEL_RDCU_MODEL_ADR,
			       CMP_DEF_IMA_MODEL_RDCU_UP_MODEL_ADR,
			       CMP_DEF_IMA_MODEL_RDCU_BUFFER_ADR,
			       buffer_length);
	assert(err == 0);

	err = rdcu_cfg_imagette_default(cfg);
	assert(err == 0);

	return 0;
}


/**
 * @brief generate a model mode zero esm test compressing configuration
 *
 * @param data_to_compress	buffer to store input data
 * @param model_of_data		buffer to store the mode data (if needed)
 * @param cfg			compression configuration to test
 *
 * @returns the expect compression error
 */

static uint16_t test_model_zero(uint16_t *data_to_compress, uint16_t *model_of_data, struct cmp_cfg *cfg)
{
	enum cmp_mode const cmp_mode = CMP_MODE_MODEL_ZERO;
	uint32_t data_samples  = 0x1BA580;
	uint32_t buffer_length = 0x08B500;
	uint32_t const rdcu_model_adr = 0x0;
	uint32_t const rdcu_new_model_adr = rdcu_model_adr; /* model update in-place */
	uint32_t const rdcu_buffer_adr = rdcu_model_adr + data_samples*IMA_SAM2BYT;
	uint32_t const rdcu_data_adr = rdcu_buffer_adr + buffer_length*IMA_SAM2BYT;
	uint32_t const model_value = 11;
	uint32_t const golomb_par = 6;
	uint32_t const spillover_par = 44;
	uint32_t const ap1_golomb_par = 3;
	uint32_t const ap1_spillover_par = 8;
	uint32_t const ap2_golomb_par = 4;
	uint32_t const ap2_spillover_par = 13;
	int err;

	*cfg = rdcu_cfg_create(DATA_TYPE_IMAGETTE_ADAPTIVE, cmp_mode, model_value, CMP_LOSSLESS);
	assert(cfg->data_type != DATA_TYPE_UNKNOWN);

	gen_ref_data(data_to_compress, data_samples*IMA_SAM2BYT);
	gen_ref_model(model_of_data, data_samples*IMA_SAM2BYT);
	err = rdcu_cfg_buffers(cfg, data_to_compress, data_samples, model_of_data, rdcu_data_adr,
			       rdcu_model_adr, rdcu_new_model_adr, rdcu_buffer_adr, buffer_length);
	assert(err == 0);

	err =  rdcu_cfg_imagette(cfg, golomb_par, spillover_par, ap1_golomb_par,
				 ap1_spillover_par, ap2_golomb_par, ap2_spillover_par);
	assert(err == 0);

	return 0;
}


/**
 * @brief generate a 1d-diff mode multi esm test compressing configuration
 *
 * @param data_to_compress	buffer to store input data
 * @param model_of_data		buffer to store the mode data (if needed)
 * @param cfg			compression configuration to test
 *
 * @returns the expect compression error
 */

static uint16_t test_diff_multi(uint16_t *data_to_compress, uint16_t *model_of_data, struct cmp_cfg *cfg)
{
	enum cmp_mode const cmp_mode = CMP_MODE_DIFF_MULTI;
	uint32_t const data_samples  = 0x25C740;
	uint32_t const buffer_length = 0x1A38C0;
	uint32_t const rdcu_buffer_adr = 0x0;
	uint32_t const rdcu_data_adr = rdcu_buffer_adr + buffer_length*IMA_SAM2BYT;
	uint32_t const rdcu_model_adr = 0x0; /* model not used */
	uint32_t const rdcu_new_model_adr = 0x0; /* model not used */

	uint32_t const model_value = 11;
	uint32_t const golomb_par = 4;
	uint32_t const spillover_par = 2;
	uint32_t const ap1_golomb_par = 1;
	uint32_t const ap1_spillover_par = 2;
	uint32_t const ap2_golomb_par = MAX_IMA_GOLOMB_PAR;
	uint32_t const ap2_spillover_par = cmp_ima_max_spill(ap2_golomb_par);
	int err;

	*cfg = rdcu_cfg_create(DATA_TYPE_IMAGETTE_ADAPTIVE, cmp_mode, model_value, CMP_LOSSLESS);
	assert(cfg->data_type != DATA_TYPE_UNKNOWN);

	gen_ref_data(data_to_compress, data_samples*IMA_SAM2BYT);
	gen_ref_model(model_of_data, data_samples*IMA_SAM2BYT);
	err = rdcu_cfg_buffers(cfg, data_to_compress, data_samples, model_of_data, rdcu_data_adr,
			       rdcu_model_adr, rdcu_new_model_adr, rdcu_buffer_adr, buffer_length);
	assert(err == 0);

	err =  rdcu_cfg_imagette(cfg, golomb_par, spillover_par, ap1_golomb_par,
				 ap1_spillover_par, ap2_golomb_par, ap2_spillover_par);
	assert(err == 0);

	return 0;
}


/**
 * @brief test the lossy compression
 *
 * @param data_to_compress	buffer to store input data
 * @param model_of_data		buffer to store the mode data (if needed)
 * @param cfg			compression configuration to test
 *
 * @returns the expect compression error
 */

static uint16_t round_test(uint16_t *data_to_compress, uint16_t *model_of_data, struct cmp_cfg *cfg)
{
	int  j;

	for (j = 0; j < 2; j++) {
		/* test the different samples in model and diff mode */
		switch (j) {
		case 0:
			test_diff_default(data_to_compress, model_of_data, cfg);
			break;
		case 1:
			test_model_default(data_to_compress, model_of_data, cfg);
			break;
		default:
			assert(0);
		}

		for (cfg->round = 0;  cfg->round <= 3; cfg->round++)
			run_compression_check_results(cfg, 0);
	}

	test_diff_multi(data_to_compress, model_of_data, cfg);
	cfg->round = 2;
	return 0;
}


/**
 * @brief test different model values
 *
 * @param data_to_compress	buffer to store input data
 * @param model_of_data		buffer to store the mode data (if needed)
 * @param cfg			compression configuration to test
 *
 * @returns the expect compression error
 */

static uint16_t model_value_test(uint16_t *data_to_compress, uint16_t *model_of_data, struct cmp_cfg *cfg)
{
	int i;

	test_model_default(data_to_compress, model_of_data, cfg);

	for (i = 0; i < 6; i++)
	{
		switch (i) {
		case 0 :
			cfg->model_value = 0;
			break;
		case 1 :
			cfg->model_value = 16;
			break;
		case 2 :
			cfg->model_value = 1;
			break;
		case 3 :
			cfg->model_value = 15;
			break;
		case 4 :
			cfg->model_value = 4;
			break;
		case 5 :
			cfg->model_value = 13;
			break;
		default:
			assert(0);
			break;
		}
		run_compression_check_results(cfg, 0);
	}
	cfg->model_value = random_between(0, 16);
	return 0;
}


/**
 * @brief test small, uneven samples and samples = 0
 *
 * @param data_to_compress	buffer to store input data
 * @param model_of_data		buffer to store the mode data (if needed)
 * @param cfg			compression configuration to test
 *
 * @returns the expect compression error
 */

static uint16_t small_samples_test(uint16_t *data_to_compress, uint16_t *model_of_data, struct cmp_cfg *cfg)
{
	int  j;

	for (j = 0; j < 3; j++) {
		/* test the different samples in model and diff mode */
		switch (j) {
		case 0:
			test_diff_default(data_to_compress, model_of_data, cfg);
			break;
		case 1:
			test_model_default(data_to_compress, model_of_data, cfg);
			break;
		case 2:
			test_raw_mode_max_samples(data_to_compress, model_of_data, cfg);
			break;
		default:
			assert(0);
		}

		for (cfg->samples = 0;  cfg->samples < 6; cfg->samples++)
			run_compression_check_results(cfg, 0);
	}
	cfg->samples++;
	return 0;
}


/**
 * @brief generate a compressing configuration to tiger a compression mode error
 *
 * @param data_to_compress	buffer to store input data
 * @param model_of_data		buffer to store the mode data (if needed)
 * @param cfg			compression configuration to test
 *
 * @returns the expect compression error
 */

static uint16_t cmp_mode_err_test(uint16_t *data_to_compress, uint16_t *model_of_data, struct cmp_cfg *cfg)
{
	test_diff_default(data_to_compress, model_of_data, cfg);

	/* the compression mode 5 is not supported */
	cfg->cmp_mode = 5;

	return 1 << CMP_MODE_ERR_BIT;
}


/**
 * @brief generate a compressing configuration to tiger a compression model value error
 *
 * @param data_to_compress	buffer to store input data
 * @param model_of_data		buffer to store the mode data (if needed)
 * @param cfg			compression configuration to test
 *
 * @returns the expect compression error
 */

static uint16_t model_value_err_test(uint16_t *data_to_compress, uint16_t *model_of_data, struct cmp_cfg *cfg)
{
	test_model_default(data_to_compress, model_of_data, cfg);

	/* the model value 17 is not supported */
	cfg->model_value = 17;

	return 1 << MODEL_VALUE_ERR_BIT;
}


/**
 * @brief test procedures for the normal compression and adaptive compression
 *	parameters
 *
 * @returns the expect compression error
 */

static uint16_t cmp_par_err_test_template(uint16_t *data_to_compress, uint16_t *model_of_data, struct cmp_cfg *cfg,
					  uint32_t *p_golomb_par, uint32_t *p_spill, uint16_t cmp_err_exp)
{
	uint32_t j;

	test_diff_default(data_to_compress, model_of_data, cfg);
	cfg->samples = 2;

	for(*p_golomb_par = 0; *p_golomb_par <= MAX_IMA_GOLOMB_PAR; *p_golomb_par+=1) {
		for (j = 0; j < 2; j++) {
			/* first round should work
			 * second round should not work */
			*p_spill = cmp_ima_max_spill(*p_golomb_par) + j;

			if (j < 1 && *p_golomb_par != 0) /* golomb_par = zero is not a valid parameter */
				run_compression_check_results(cfg, 0);
			else /* invalid configuration; spill to high */
				run_compression_check_results(cfg, cmp_err_exp);
		}
	}

	/* this configuration should not trigger an error */
	*p_golomb_par = 1;
	*p_spill = 3;
	run_compression_check_results(cfg, 0);

	/* spill = 0 not supported */
	*p_golomb_par = 1;
	*p_spill = 0;
	run_compression_check_results(cfg, cmp_err_exp);

	/* this configuration should not trigger an error */
	*p_golomb_par = 1;
	*p_spill = 7;
	run_compression_check_results(cfg, 0);

	/* spill = 1 not supported */
	*p_golomb_par = 1;
	*p_spill = 1;
	run_compression_check_results(cfg, cmp_err_exp);

	/* generate a random invalid golomb_par spill pair */
	*p_golomb_par = random_between(1, MAX_IMA_GOLOMB_PAR);
	*p_spill = random_between(cmp_ima_max_spill(*p_golomb_par)+1, 0x3FF);

	return cmp_err_exp;
}


/**
 * @brief generate a compressing configuration to tiger a compression parameter error
 *
 * @param data_to_compress	buffer to store input data
 * @param model_of_data		buffer to store the mode data (if needed)
 * @param cfg			compression configuration to test
 *
 * @returns the expect compression error
 */

static uint16_t cmp_par_err_test(uint16_t *data_to_compress, uint16_t *model_of_data, struct cmp_cfg *cfg)
{
	return cmp_par_err_test_template(data_to_compress, model_of_data, cfg, &cfg->golomb_par,
					 &cfg->spill, 1 << CMP_PAR_ERR_BIT);
}


/**
 * @brief generate a compressing configuration to tiger a adaptive 1 compression parameter error
 *
 * @param data_to_compress	buffer to store input data
 * @param model_of_data		buffer to store the mode data (if needed)
 * @param cfg			compression configuration to test
 *
 * @returns the expect compression error
 */

static uint16_t ap1_cmp_par_err_test(uint16_t *data_to_compress, uint16_t *model_of_data, struct cmp_cfg *cfg)
{
	return cmp_par_err_test_template(data_to_compress, model_of_data, cfg, &cfg->ap1_golomb_par,
					 &cfg->ap1_spill, 1 << AP1_CMP_PAR_ERR_BIT);
}


/**
 * @brief generate a compressing configuration to tiger a adaptive 2 compression parameter error
 *
 * @param data_to_compress	buffer to store input data
 * @param model_of_data		buffer to store the mode data (if needed)
 * @param cfg			compression configuration to test
 *
 * @returns the expect compression error
 */

static uint16_t ap2_cmp_par_err_test(uint16_t *data_to_compress, uint16_t *model_of_data, struct cmp_cfg *cfg)
{
	return cmp_par_err_test_template(data_to_compress, model_of_data, cfg, &cfg->ap2_golomb_par,
					 &cfg->ap2_spill, 1 << AP2_CMP_PAR_ERR_BIT);
}


/**
 * @brief generate a compressing configuration to tiger a small buffer error
 *	edge case
 *
 * @param data_to_compress	buffer to store input data
 * @param model_of_data		buffer to store the mode data (if needed)
 * @param cfg			compression configuration to test
 *
 * @returns the expect compression error
 */

static uint16_t small_buffer_err_test1(uint16_t *data_to_compress, uint16_t *model_of_data, struct cmp_cfg *cfg)
{
	int err;
	uint16_t data[6] = {23,42,42,420,23,42};
	uint32_t data_samples = 6;
	uint32_t buffer_length = 3;

	model_of_data = NULL; /* model not used */

	memcpy(data_to_compress, data, sizeof(data));

	/* set up compressor configuration */
	*cfg = rdcu_cfg_create(DATA_TYPE_IMAGETTE_ADAPTIVE, CMP_MODE_DIFF_ZERO,
			       CMP_DEF_IMA_DIFF_MODEL_VALUE, CMP_LOSSLESS);
	assert(cfg->data_type != DATA_TYPE_UNKNOWN);

	err = rdcu_cfg_buffers(cfg, data_to_compress, data_samples, model_of_data,
			       CMP_DEF_IMA_DIFF_RDCU_DATA_ADR,
			       CMP_DEF_IMA_DIFF_RDCU_MODEL_ADR,
			       CMP_DEF_IMA_DIFF_RDCU_UP_MODEL_ADR,
			       CMP_DEF_IMA_DIFF_RDCU_BUFFER_ADR,
			       buffer_length);
	err = rdcu_cfg_imagette_default(cfg);
	assert(err == 0);

	/* the buffer should be to small to store the complete compressed
	 * bitstream to trigger a small_buffer_err */
	run_compression_check_results(cfg, 1 << SMALL_BUFFER_ERR_BIT);

	/* buffer is still to small */
	cfg->buffer_length = 4;
	run_compression_check_results(cfg, 1 << SMALL_BUFFER_ERR_BIT);

	/* now the buffer is big enough to store the bitstream */
	cfg->buffer_length = 5;

	return 0;
}


/**
 * @brief generate a compressing configuration to tiger a small buffer error
 *  case
 *
 * @param data_to_compress	buffer to store input data
 * @param model_of_data		buffer to store the mode data (if needed)
 * @param cfg			compression configuration to test
 *
 * @returns the expect compression error
 */

static uint16_t small_buffer_err_test2(uint16_t *data_to_compress, uint16_t *model_of_data, struct cmp_cfg *cfg)
{
	int err;
	uint32_t i;
	uint32_t data_samples = 8;
	uint32_t buffer_length = 4;
	uint32_t golomb_par = 1;
	uint32_t spillover_par = 8;

	model_of_data = NULL; /* model not used */

	/* generate a bitstream with a length of 64 bits */
	cfg->buffer_length = 4;
	data_to_compress[0] = (uint16_t)4;
	for (i = 1; i < data_samples; i++)
		data_to_compress[i] = data_to_compress[i-1]+4;

	/* set up compressor configuration */
	*cfg = rdcu_cfg_create(DATA_TYPE_IMAGETTE_ADAPTIVE, CMP_MODE_DIFF_ZERO,
			       CMP_DEF_IMA_DIFF_MODEL_VALUE, CMP_LOSSLESS);
	assert(cfg->data_type != DATA_TYPE_UNKNOWN);

	err = rdcu_cfg_buffers(cfg, data_to_compress, data_samples, model_of_data,
			       CMP_DEF_IMA_DIFF_RDCU_DATA_ADR,
			       CMP_DEF_IMA_DIFF_RDCU_MODEL_ADR,
			       CMP_DEF_IMA_DIFF_RDCU_UP_MODEL_ADR,
			       CMP_DEF_IMA_DIFF_RDCU_BUFFER_ADR,
			       buffer_length);
	err =  rdcu_cfg_imagette(cfg, golomb_par, spillover_par, CMP_DEF_IMA_DIFF_AP1_GOLOMB_PAR,
				 CMP_DEF_IMA_DIFF_AP1_SPILL_PAR, CMP_DEF_IMA_DIFF_AP2_GOLOMB_PAR,
				 CMP_DEF_IMA_DIFF_AP2_SPILL_PAR);
	assert(err == 0);

	/* NOTE: to do the implication of the HW compressor a condition where
	 * the compressed size fits *actually* in the compressed data buffer
	 * results in a small buffer err
	 */
	run_compression_check_results(cfg, 1 << SMALL_BUFFER_ERR_BIT);

	/* generate a bitstream with a length of 63 bits */
	cfg->samples = 9;
	data_to_compress[0] = (uint16_t)-3;
	for (i = 1; i < cfg->samples; i++) {
		data_to_compress[i] = data_to_compress[i-1] - 3;
	}

	return 0;
}


/**
 * @brief generate a compressing configuration to tiger a small buffer error in
 *	raw compression mode
 *
 * @param data_to_compress	buffer to store input data
 * @param model_of_data		buffer to store the mode data (if needed)
 * @param cfg			compression configuration to test
 *
 * @returns the expect compression error
 */

static uint16_t small_buffer_err_raw_mode_test(uint16_t *data_to_compress, uint16_t *model_of_data, struct cmp_cfg *cfg)
{
	int err;
	uint16_t data[6] = {23,42,42,420,23,42};
	uint32_t data_samples = 6;
	uint32_t buffer_length = 5;

	model_of_data = NULL; /* model not used */

	memcpy(data_to_compress, data, sizeof(data));

	/* set up compressor configuration */
	*cfg = rdcu_cfg_create(DATA_TYPE_IMAGETTE_ADAPTIVE, CMP_MODE_RAW,
			       CMP_DEF_IMA_DIFF_MODEL_VALUE, CMP_LOSSLESS);
	assert(cfg->data_type != DATA_TYPE_UNKNOWN);

	err = rdcu_cfg_buffers(cfg, data_to_compress, data_samples, model_of_data,
			       CMP_DEF_IMA_DIFF_RDCU_DATA_ADR,
			       CMP_DEF_IMA_DIFF_RDCU_MODEL_ADR,
			       CMP_DEF_IMA_DIFF_RDCU_UP_MODEL_ADR,
			       CMP_DEF_IMA_DIFF_RDCU_BUFFER_ADR,
			       buffer_length);
	err = rdcu_cfg_imagette_default(cfg);
	assert(err == 0);

	/* the buffer should be to small to store the complete compressed
	 * bitstream -> trigger a small_buffer_err */
	run_compression_check_results(cfg, 1 << SMALL_BUFFER_ERR_BIT);

	/* now the buffer is big enough */
	cfg->buffer_length = 6;
	/* cfg->buffer_length = 7; */
#ifdef FPGA_VERSION_0_7
	/* cfg->buffer_length += 1; */
#endif

	return 0;
}


/**
 * @brief generate a compressing configuration to tiger a multi bit edac error
 *
 * @param data_to_compress	buffer to store input data
 * @param model_of_data		buffer to store the mode data (if needed)
 * @param cfg			compression configuration to test
 *
 * @returns the expect compression error
 */

static uint16_t multi_bit_err_test(uint16_t *data_to_compress, uint16_t *model_of_data, struct cmp_cfg *cfg)
{
	/* multi error in the 1. SRAM chip in raw mode */
	test_raw_mode_max_samples(data_to_compress, model_of_data, cfg);
	rdcu_inject_edac_error(cfg, 0x8);
	rdcu_start_compression();
	wait_compression_is_done();
	check_cmp_register(cfg, 1 << MB_ERR_BIT);

	/* multi error in the 1. SRAM chip */
	test_diff_default(data_to_compress, model_of_data, cfg);
	rdcu_inject_edac_error(cfg, 0xCCC);
	rdcu_start_compression();
	wait_compression_is_done();
	check_cmp_register(cfg, 1 << MB_ERR_BIT);

	/* multi error in the 2. SRAM chip */
	test_model_default(data_to_compress, model_of_data, cfg);
	rdcu_inject_edac_error(cfg, 0x200100);
	rdcu_start_compression();
	wait_compression_is_done();
	check_cmp_register(cfg, 1 << MB_ERR_BIT);

	/* test recovering from multi bit error */
	return test_model_default(data_to_compress, model_of_data, cfg);
}


/**
 * @brief generate a compressing configuration to tiger a invalid address error
 *
 * @param data_to_compress	buffer to store input data
 * @param model_of_data		buffer to store the mode data (if needed)
 * @param cfg			compression configuration to test
 *
 * @returns the expect compression error
 */

uint16_t invalid_address_err_test(uint16_t *data_to_compress, uint16_t *model_of_data, struct cmp_cfg *cfg)
{
	/* set up compressor configuration */
	test_diff_default(data_to_compress, model_of_data, cfg);
	/* SRAM address range: 0x0000 0000	0x007F FFFF */
	cfg->samples = 6;
	cfg->rdcu_buffer_adr = 0x7FFFFC;
	cfg->buffer_length = 10;
	run_compression_check_results(cfg, 1 << INVALID_ADDRESS_ERR_BIT);

	/* this configuration should not trigger an bus error */
	cfg->rdcu_buffer_adr = 0x7FFFEC;
	cfg->buffer_length = 8;

	return 0;
}


/**
 * @brief test the compression interrupt feature
 *
 * @param data_to_compress	buffer to store input data
 * @param model_of_data		buffer to store the mode data (if needed)
 * @param cfg			compression configuration to test
 *
 * @returns the expect compression error
 */

static uint16_t interrupt_compression_test(uint16_t *data_to_compress, uint16_t *model_of_data, struct cmp_cfg *cfg)
{
	/* interrupt during compression */
	test_model_default(data_to_compress, model_of_data, cfg);
	rdcu_compress_data(cfg);
	printf("i\n");
	rdcu_interrupt_compression();
	check_cmp_register(cfg, EXP_CMP_INTERRUPTED);

	/* interrupt after compression is done */
	run_compression_check_results(cfg, 0);
	rdcu_interrupt_compression();
	check_cmp_register(cfg, EXP_CMP_INTERRUPTED);

	/* test recovering form interrupt */
	return 0;
}

static uint16_t test_random_configutation(uint16_t *data_to_compress, uint16_t *model_of_data, struct cmp_cfg *cfg)
{
	int sw_cmp_size;
	cfg->samples = random_between(1, 0x100000);
	cfg->cmp_mode = random_between(0, MAX_RDCU_CMP_MODE);
	cfg->model_value = random_between(0, MAX_MODEL_VALUE);
	/* cfg->round = random_between(CMP_LOSSLESS, MAX_RDCU_ROUND); */
	cfg->round = CMP_LOSSLESS;

	cfg->golomb_par = random_between(MIN_IMA_GOLOMB_PAR, MAX_IMA_GOLOMB_PAR);
	cfg->ap1_golomb_par = random_between(MIN_IMA_GOLOMB_PAR, MAX_IMA_GOLOMB_PAR);
	cfg->ap2_golomb_par = random_between(MIN_IMA_GOLOMB_PAR, MAX_IMA_GOLOMB_PAR);
	cfg->spill = random_between(MIN_IMA_SPILL, cmp_ima_max_spill(cfg->golomb_par));
	cfg->ap1_spill = random_between(MIN_IMA_SPILL, cmp_ima_max_spill(cfg->ap1_golomb_par));
	cfg->ap2_spill = random_between(MIN_IMA_SPILL, cmp_ima_max_spill(cfg->ap2_golomb_par));

	/* use the default SRAM address */
	cfg->rdcu_buffer_adr = CMP_DEF_IMA_MODEL_RDCU_DATA_ADR;
	cfg->rdcu_model_adr = CMP_DEF_IMA_MODEL_RDCU_MODEL_ADR;
	cfg->rdcu_new_model_adr = CMP_DEF_IMA_MODEL_RDCU_UP_MODEL_ADR;
	cfg->rdcu_buffer_adr = CMP_DEF_IMA_MODEL_RDCU_BUFFER_ADR;
	cfg->buffer_length = 0x100000;

	gen_random_samples(data_to_compress, cfg->samples);
	if (model_mode_is_used(cfg->cmp_mode))
		gen_random_samples(model_of_data, cfg->samples);
	else
		model_of_data = NULL;

	/* check if the compressed data fit into the compressed data buffer */
	cfg->data_type = DATA_TYPE_IMAGETTE_ADAPTIVE;
	cfg->icu_output_buf = NULL;
	cfg->icu_new_model_buf = NULL;
	cfg->input_buf = data_to_compress;
	cfg->model_buf = model_of_data;
	sw_cmp_size = icu_compress_data(cfg);
	assert(sw_cmp_size > 0);
	if (cmp_bit_to_4byte(sw_cmp_size) > cfg->buffer_length*IMA_SAM2BYT) {
		printf("cmp_size_exp: 0\n");
		return 1 << SMALL_BUFFER_ERR_BIT;
	}
	cfg->buffer_length = cmp_bit_to_4byte(sw_cmp_size)/IMA_SAM2BYT;
	if (sw_cmp_size % 32 == 0)
		cfg->buffer_length++;
	return 0;
}


int main(void)
{
	uint16_t *data_to_compress;
	uint16_t *model_of_data;
	struct cmp_cfg cfg;
	struct cmp_info info;
	int test_case, i;

	srand(1);

	init_rdcu();

#ifdef __sparc__
	data_to_compress = (void *)0x61000000;
	model_of_data =    (void *)0x62000000;
#else /* assume PC */
	data_to_compress = malloc(RDCU_SRAM_SIZE);
	model_of_data = malloc(RDCU_SRAM_SIZE);
#endif
	if (!data_to_compress) {
		printf("Error allocating memory\n");
		return -1;
	}
	if (!model_of_data) {
		printf("Error allocating memory\n");
		return -1;
	}


	for (test_case = 0; test_case < 600; test_case++) {
		char *test_name;
		uint16_t (*gen_test_setup_f)(uint16_t* data_to_compress, uint16_t *model_of_data, struct cmp_cfg *cfg);
		uint16_t cmp_err_exp;

		/* setup test case */
		switch (test_case) {
		case 1:
			test_name = "raw mode test";
			gen_test_setup_f = test_raw_mode_max_samples;
			break;
		case 2:
			test_name = "1d-diff mode default configuration test";
			gen_test_setup_f = test_diff_default;
			break;
		case 3:
			test_name = "model mode default configuration test";
			gen_test_setup_f = test_model_default;
			break;
		case 4:
			test_name = "zero escape symbol model mode test";
			gen_test_setup_f = test_model_zero;
			break;
		case 5:
			test_name = "multi escape symbol 1d-diff mode test";
			gen_test_setup_f = test_diff_multi;
			break;
		case 6:
			test_name = "lossy compression test";
			gen_test_setup_f = round_test;
			break;
		case 7:
			test_name = "model value test";
			gen_test_setup_f = model_value_test;
			break;
		case 8:
			test_name = "small samples test";
			gen_test_setup_f = small_samples_test;
			break;
		/* /1* compression error test cases *1/ */
		case 50:
			test_name = "compression mode error test";
			gen_test_setup_f = cmp_mode_err_test;
			break;
		case 51:
			test_name = "model value error test";
			gen_test_setup_f = model_value_err_test;
			break;
		case 52:
			test_name = "compression parameter error test";
			gen_test_setup_f = cmp_par_err_test;
			break;
		case 53:
			test_name = "adaptive 1 compression parameter error test";
			gen_test_setup_f = ap1_cmp_par_err_test;
			break;
		case 54:
			test_name = "adaptive 2 compression parameter error test";
			gen_test_setup_f = ap2_cmp_par_err_test;
			break;
		case 55:
			test_name = "small buffer err/buffer overflow error test";
			gen_test_setup_f = small_buffer_err_test1;
			break;
		case 56:
			test_name = "small buffer err/buffer overflow error edge case test";
			gen_test_setup_f = small_buffer_err_test2;
			break;
		case 57:
			test_name = "small buffer err/buffer overflow error raw mode test";
			gen_test_setup_f = small_buffer_err_raw_mode_test;
			break;
		case 58:
			test_name = "multi bit error test";
			gen_test_setup_f = multi_bit_err_test;
			break;
		case 59:
			test_name = "invalid SRAM address error test";
			gen_test_setup_f = invalid_address_err_test;
			break;
		case 60:
			test_name = "interrupt compression test";
			gen_test_setup_f = interrupt_compression_test;
			break;
		case 90:
			test_name = "random configuration test";
			gen_test_setup_f = test_random_configutation;
			break;
		default:
			continue;
		}

		memset(&cfg, 0, sizeof(struct cmp_cfg));
		memset(&info, 0, sizeof(struct cmp_info));
		memset(data_to_compress, 0, RDCU_SRAM_SIZE);
		memset(model_of_data, 0, RDCU_SRAM_SIZE);

		printf("--------------------------------------------------------------------------------\n");
		printf("--------------------------------------------------------------------------------\n");
		printf("\n%s\n\n", test_name);
		printf("--------------------------------------------------------------------------------\n");
		printf("--------------------------------------------------------------------------------\n");
		cmp_err_exp = gen_test_setup_f(data_to_compress, model_of_data, &cfg);

		run_compression_check_results(&cfg, cmp_err_exp);
	}
	printf("\nFINISHED\n\n");

	printf("\nTry some random configurations\n\n");
	for (i = 0; i < 5000; ++i) {
		uint16_t cmp_err_exp;

		printf("%i\n", i);
		memset(&cfg, 0, sizeof(struct cmp_cfg));
		memset(&info, 0, sizeof(struct cmp_info));
		cmp_err_exp = test_random_configutation(data_to_compress, model_of_data, &cfg);

		run_compression_check_results(&cfg, cmp_err_exp);
	}

#ifndef __sparc__
	free(data_to_compress);
	free(model_of_data);
#endif

	return 0;
}
