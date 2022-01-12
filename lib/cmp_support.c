/**
 * @file   cmp_support.c
 * @author Dominik Loidolt (dominik.loidolt@univie.ac.at),
 * @date   2019
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
 * @brief compressor support library
 * @see Data Compression User Manual PLATO-UVIE-PL-UM-0001
 */

#include <stdio.h>

#include "../include/cmp_support.h"
#include "../include/n_dpu_pkt.h"


/**
 * @brief Default configuration of the Compressor in model imagette mode.
 * @warning All ICU buffers are set to NULL. Samples and buffer_length are set to 0
 * @note see PLATO-IWF-PL-RS-0005 V1.1
 */


const struct cmp_cfg DEFAULT_CFG_MODEL = {
	MODE_MODEL_MULTI, /* cmp_mode */
	4, /* golomb_par */
	48, /* spill */
	8, /* model_value */
	0, /* round */
	3, /* ap1_golomb_par */
	35, /* ap1_spill */
	5, /* ap2_golomb_par */
	60, /* ap2_spill */
	NULL, /* *input_buf */
	0x000000, /* rdcu_data_adr */
	NULL, /* *model_buf */
	0x200000, /* rdcu_model_adr */
	NULL, /* *icu_new_model_buf */
	0x400000, /* rdcu_up_model_adr */
	0, /* samples */
	NULL, /* *icu_output_buf */
	0x600000, /* rdcu_buffer_adr */
	0x0 /* buffer_length */
};


/**
 * @brief Default configuration of the Compressor in 1d-differencing imagette mode.
 * @warning All ICU buffers are set to NULL. Samples and buffer_length are set to 0
 * @note see PLATO-IWF-PL-RS-0005 V1.1
 */

const struct cmp_cfg DEFAULT_CFG_DIFF = {
	MODE_DIFF_ZERO, /* cmp_mode */
	7, /* golomb_par */
	60, /* spill */
	8, /* model_value */
	0, /* round */
	6, /* ap1_golomb_par */
	48, /* ap1_spill */
	8, /* ap2_golomb_par */
	72, /* ap2_spill */
	NULL, /* *input_buf */
	0x000000, /* rdcu_data_adr */
	NULL, /* *model_buf */
	0x000000, /* rdcu_model_adr */
	NULL, /* *icu_new_model_buf */
	0x000000, /* rdcu_up_model_adr */
	0, /* samples */
	NULL, /* *icu_output_buf */
	0x600000, /* rdcu_buffer_adr */
	0x0 /* buffer_length */
};


/**
 * @brief implementation of the logarithm base of floor(log2(x)) for integers
 * @note ilog_2(0) = -1 defined
 *
 * @param x input parameter
 *
 * @returns the result of floor(log2(x))
 */

int ilog_2(uint32_t x)
{
	if (!x)
		return -1;

	return 31 - __builtin_clz(x);
}


/**
 * @brief Determining if an integer is a power of 2
 * @note 0 is incorrectly considered a power of 2 here
 *
 * @param v we want to see if v is a power of 2
 *
 * @returns 1 if v is a power of 2, otherwise 0
 *
 * @note see: https://graphics.stanford.edu/~seander/bithacks.html#DetermineIfPowerOf2
 */

int is_a_pow_of_2(unsigned int v)
{
	return (((v) & ((v) - 1)) == 0);
}


/**
 * @brief check if a model mode is selected
 *
 * @param cmp_mode compression mode
 *
 * @returns 1 when model mode is set, otherwise 0
 */

int model_mode_is_used(unsigned int cmp_mode)
{
	switch (cmp_mode) {
	case MODE_MODEL_ZERO:
	case MODE_MODEL_ZERO_S_FX:
	case MODE_MODEL_ZERO_S_FX_EFX:
	case MODE_MODEL_ZERO_S_FX_NCOB:
	case MODE_MODEL_ZERO_S_FX_EFX_NCOB_ECOB:
	case MODE_MODEL_ZERO_F_FX:
	case MODE_MODEL_ZERO_F_FX_EFX:
	case MODE_MODEL_ZERO_F_FX_NCOB:
	case MODE_MODEL_ZERO_F_FX_EFX_NCOB_ECOB:
	case MODE_MODEL_MULTI:
	case MODE_MODEL_MULTI_S_FX:
	case MODE_MODEL_MULTI_S_FX_EFX:
	case MODE_MODEL_MULTI_S_FX_NCOB:
	case MODE_MODEL_MULTI_S_FX_EFX_NCOB_ECOB:
	case MODE_MODEL_MULTI_F_FX:
	case MODE_MODEL_MULTI_F_FX_EFX:
	case MODE_MODEL_MULTI_F_FX_NCOB:
	case MODE_MODEL_MULTI_F_FX_EFX_NCOB_ECOB:
	case MODE_MODEL_ZERO_32:
	case MODE_MODEL_MULTI_32:
		return 1;
		break;
	default:
		return 0;
		break;
	}
}


/**
 * @brief check if a 1d-differencing mode is selected
 *
 * @param cmp_mode compression mode
 *
 * @returns 1 when 1d-differencing mode is set, otherwise 0
 */

int diff_mode_is_used(unsigned int cmp_mode)
{
	switch (cmp_mode) {
	case MODE_DIFF_ZERO:
	case MODE_DIFF_ZERO_S_FX:
	case MODE_DIFF_ZERO_S_FX_EFX:
	case MODE_DIFF_ZERO_S_FX_NCOB:
	case MODE_DIFF_ZERO_S_FX_EFX_NCOB_ECOB:
	case MODE_DIFF_ZERO_F_FX:
	case MODE_DIFF_ZERO_F_FX_EFX:
	case MODE_DIFF_ZERO_F_FX_NCOB:
	case MODE_DIFF_ZERO_F_FX_EFX_NCOB_ECOB:
	case MODE_DIFF_MULTI:
	case MODE_DIFF_MULTI_S_FX:
	case MODE_DIFF_MULTI_S_FX_EFX:
	case MODE_DIFF_MULTI_S_FX_NCOB:
	case MODE_DIFF_MULTI_S_FX_EFX_NCOB_ECOB:
	case MODE_DIFF_MULTI_F_FX:
	case MODE_DIFF_MULTI_F_FX_EFX:
	case MODE_DIFF_MULTI_F_FX_NCOB:
	case MODE_DIFF_MULTI_F_FX_EFX_NCOB_ECOB:
	case MODE_DIFF_ZERO_32:
	case MODE_DIFF_MULTI_32:
		return 1;
		break;
	default:
		return 0;
		break;
	}
}


/**
 * @brief check if the raw mode is selected
 *
 * @param cmp_mode compression mode
 *
 * @returns 1 when raw mode is set, otherwise 0
 */

int raw_mode_is_used(unsigned int cmp_mode)
{
	switch (cmp_mode) {
	case MODE_RAW:
	case MODE_RAW_S_FX:
	case MODE_RAW_32:
		return 1;
		break;
	default:
		return 0;
		break;
	}
}


/**
 * @brief check if the mode is supported by the RDCU compressor
 *
 * @param cmp_mode compression mode
 *
 * @returns 1 when mode is supported by the RDCU, otherwise 0
 */

int rdcu_supported_mode_is_used(unsigned int cmp_mode)
{
	switch (cmp_mode) {
	case MODE_RAW:
	case MODE_MODEL_ZERO:
	case MODE_DIFF_ZERO:
	case MODE_MODEL_MULTI:
	case MODE_DIFF_MULTI:
		return 1;
		break;
	default:
		return 0;
		break;
	}
}


/**
 * @brief check if zero escape symbol mechanism mode is used
 *
 * @param cmp_mode compression mode
 *
 * @returns 1 when zero escape symbol mechanism is set, otherwise 0
 */

int zero_escape_mech_is_used(unsigned int cmp_mode)
{
	switch (cmp_mode) {
	case MODE_MODEL_ZERO:
	case MODE_DIFF_ZERO:
	case MODE_MODEL_ZERO_S_FX:
	case MODE_DIFF_ZERO_S_FX:
	case MODE_MODEL_ZERO_S_FX_EFX:
	case MODE_DIFF_ZERO_S_FX_EFX:
	case MODE_MODEL_ZERO_S_FX_NCOB:
	case MODE_DIFF_ZERO_S_FX_NCOB:
	case MODE_MODEL_ZERO_S_FX_EFX_NCOB_ECOB:
	case MODE_DIFF_ZERO_S_FX_EFX_NCOB_ECOB:
	case MODE_MODEL_ZERO_F_FX:
	case MODE_DIFF_ZERO_F_FX:
	case MODE_MODEL_ZERO_F_FX_EFX:
	case MODE_DIFF_ZERO_F_FX_EFX:
	case MODE_MODEL_ZERO_F_FX_NCOB:
	case MODE_DIFF_ZERO_F_FX_NCOB:
	case MODE_MODEL_ZERO_F_FX_EFX_NCOB_ECOB:
	case MODE_DIFF_ZERO_F_FX_EFX_NCOB_ECOB:
	case MODE_MODEL_ZERO_32:
	case MODE_DIFF_ZERO_32:
		return 1;
		break;
	default:
		return 0;
		break;
	}

}


/**
 * @brief check if multi escape symbol mechanism mode is used
 *
 * @param cmp_mode compression mode
 *
 * @returns 1 when multi escape symbol mechanism is set, otherwise 0
 */

int multi_escape_mech_is_used(unsigned int cmp_mode)
{
	switch (cmp_mode) {
	case MODE_MODEL_MULTI:
	case MODE_DIFF_MULTI:
	case MODE_MODEL_MULTI_S_FX:
	case MODE_DIFF_MULTI_S_FX:
	case MODE_MODEL_MULTI_S_FX_EFX:
	case MODE_DIFF_MULTI_S_FX_EFX:
	case MODE_MODEL_MULTI_S_FX_NCOB:
	case MODE_DIFF_MULTI_S_FX_NCOB:
	case MODE_MODEL_MULTI_S_FX_EFX_NCOB_ECOB:
	case MODE_DIFF_MULTI_S_FX_EFX_NCOB_ECOB:
	case MODE_MODEL_MULTI_F_FX:
	case MODE_DIFF_MULTI_F_FX:
	case MODE_MODEL_MULTI_F_FX_EFX:
	case MODE_DIFF_MULTI_F_FX_EFX:
	case MODE_MODEL_MULTI_F_FX_NCOB:
	case MODE_DIFF_MULTI_F_FX_NCOB:
	case MODE_MODEL_MULTI_F_FX_EFX_NCOB_ECOB:
	case MODE_DIFF_MULTI_F_FX_EFX_NCOB_ECOB:
	case MODE_MODEL_MULTI_32:
	case MODE_DIFF_MULTI_32:
		return 1;
		break;
	default:
		return 0;
		break;
	}
}


/**
 * @brief method for lossy rounding
 *
 * @param value value to round
 * @param round round parameter
 *
 * @return rounded value
 */

unsigned int round_fwd(unsigned int value, unsigned int round)
{
	return value >> round;
}


/**
 * @brief inverse method for lossy rounding
 *
 * @param value value to round back
 * @param round round parameter
 *
 * @return back rounded value
 */

unsigned int round_inv(unsigned int value, unsigned int round)
{
	return value << round;
}


/**
 * @brief implantation of the model update equation
 *
 * @note check before that model_value is not greater than MAX_MODEL_VALUE

 * @param data		data to process
 * @param model		(current) model of the data to process
 * @param model_value	model weighting parameter
 *
 * @returns (new) updated model
 */

unsigned int cal_up_model(unsigned int data, unsigned int model, unsigned int
			  model_value)
{
	/* cast uint64_t to prevent overflow in the multiplication */
	uint64_t weighted_model = (uint64_t)model * model_value;
	uint64_t weighted_data = (uint64_t)data * (MAX_MODEL_VALUE - model_value);
	/* truncation is intended */
	return (unsigned int)((weighted_model + weighted_data) / MAX_MODEL_VALUE);
}


/**
 * @brief get the maximum valid spill threshold value for a given golomb_par
 *
 * @param golomb_par Golomb parameter
 * @param cmp_mode   compression mode
 *
 * @returns the highest still valid spill threshold value
 */

uint32_t get_max_spill(unsigned int golomb_par, unsigned int cmp_mode)
{
	unsigned int cutoff;
	unsigned int max_n_sym_offset;
	unsigned int max_cw_bits;

	if (golomb_par == 0)
		return 0;

	if (rdcu_supported_mode_is_used(cmp_mode)) {
		max_cw_bits = 16; /* the RDCU compressor can only generate code
				   * words with a length of maximal 16 bits.
				   */
	} else {
		max_cw_bits = 32; /* the ICU compressor can generate code
				   * words with a length of maximal 32 bits.
				   */
	}

	cutoff = (1UL << (ilog_2(golomb_par)+1)) - golomb_par;
	max_n_sym_offset = max_cw_bits/2 - 1;

	return (max_cw_bits-1-ilog_2(golomb_par))*golomb_par + cutoff -
		max_n_sym_offset - 1;
}


/**
 * @brief calculate the size of a sample for the different compression modes
 *
 * @param cmp_mode compression mode
 *
 * @returns the size of a data sample in bytes for the selected compression
 *	mode;
 */

size_t size_of_a_sample(unsigned int cmp_mode)
{
	size_t sample_len;

	switch (cmp_mode) {
	case MODE_RAW:
	case MODE_MODEL_ZERO:
	case MODE_MODEL_MULTI:
	case MODE_DIFF_ZERO:
	case MODE_DIFF_MULTI:
		sample_len = sizeof(uint16_t);
		break;
	case MODE_RAW_S_FX:
	case MODE_MODEL_ZERO_S_FX:
	case MODE_MODEL_MULTI_S_FX:
	case MODE_DIFF_ZERO_S_FX:
	case MODE_DIFF_MULTI_S_FX:
		sample_len = sizeof(struct S_FX);
		break;
	case MODE_MODEL_ZERO_S_FX_EFX:
	case MODE_MODEL_MULTI_S_FX_EFX:
	case MODE_DIFF_ZERO_S_FX_EFX:
	case MODE_DIFF_MULTI_S_FX_EFX:
		sample_len = sizeof(struct S_FX_NCOB);
		break;
	case MODE_MODEL_ZERO_S_FX_EFX_NCOB_ECOB:
	case MODE_MODEL_MULTI_S_FX_EFX_NCOB_ECOB:
	case MODE_DIFF_ZERO_S_FX_EFX_NCOB_ECOB:
	case MODE_DIFF_MULTI_S_FX_EFX_NCOB_ECOB:
		sample_len = sizeof(struct S_FX_EFX_NCOB_ECOB);
		break;
	case MODE_MODEL_ZERO_F_FX:
	case MODE_MODEL_MULTI_F_FX:
	case MODE_DIFF_ZERO_F_FX:
	case MODE_DIFF_MULTI_F_FX:
		sample_len = sizeof(struct F_FX);
		break;
	case MODE_MODEL_ZERO_F_FX_EFX:
	case MODE_MODEL_MULTI_F_FX_EFX:
	case MODE_DIFF_ZERO_F_FX_EFX:
	case MODE_DIFF_MULTI_F_FX_EFX:
		sample_len = sizeof(struct F_FX_EFX);
		break;
	case MODE_MODEL_ZERO_F_FX_NCOB:
	case MODE_MODEL_MULTI_F_FX_NCOB:
	case MODE_DIFF_ZERO_F_FX_NCOB:
	case MODE_DIFF_MULTI_F_FX_NCOB:
		sample_len = sizeof(struct F_FX_NCOB);
		break;
	case MODE_MODEL_ZERO_F_FX_EFX_NCOB_ECOB:
	case MODE_MODEL_MULTI_F_FX_EFX_NCOB_ECOB:
	case MODE_DIFF_ZERO_F_FX_EFX_NCOB_ECOB:
	case MODE_DIFF_MULTI_F_FX_EFX_NCOB_ECOB:
		sample_len = sizeof(struct F_FX_EFX_NCOB_ECOB);
		break;
	case MODE_RAW_32:
	case MODE_MODEL_ZERO_32:
	case MODE_MODEL_MULTI_32:
	case MODE_DIFF_ZERO_32:
	case MODE_DIFF_MULTI_32:
		sample_len = sizeof(uint32_t);
		break;
	default:
#if DEBUG
		printf("Error: Compression mode not supported.\n");
#endif
		return 0;
		break;
	}
	return sample_len;
}


/**
 * @brief calculate the need bytes to hold a bitstream
 *
 * @param cmp_size_bit compressed data size, measured in bits
 *
 * @returns the size in bytes to store the hole bitstream
 * @note we round up the result to multiples of 4 bytes
 */

unsigned int cmp_bit_to_4byte(unsigned int cmp_size_bit)
{
	return (((cmp_size_bit + 7) / 8) + 3) & ~0x3UL;
}


/**
 * @brief calculate the need bytes for the data
 *
 * @param samples number of data samples
 * @param cmp_mode used compression mode
 *
 * @returns the size in bytes to store the data sample
 */

unsigned int cmp_cal_size_of_data(unsigned int samples, unsigned int cmp_mode)
{
	return samples * size_of_a_sample(cmp_mode);
}


/**
 * @brief calculate the need bytes for the model
 *
 * @param samples number of model samples
 * @param cmp_mode used compression mode
 *
 * @returns the size in bytes to store the model sample
 */

unsigned int cmp_cal_size_of_model(unsigned int samples, unsigned int cmp_mode)
{
	return cmp_cal_size_of_data(samples, cmp_mode);
}


/**
 * @brief print the cmp_cfg structure
 *
 * @param cfg	compressor configuration contains all parameters required for
 *		compression
 *
 */

void print_cmp_cfg(const struct cmp_cfg *cfg)
{
	size_t i;

	printf("cmp_mode: %lu\n", cfg->cmp_mode);
	printf("golomb_par: %lu\n", cfg->golomb_par);
	printf("spill: %lu\n", cfg->spill);
	printf("model_value: %lu\n", cfg->model_value);
	printf("round: %lu\n", cfg->round);
	printf("ap1_golomb_par: %lu\n", cfg->ap1_golomb_par);
	printf("ap1_spill: %lu\n", cfg->ap1_spill);
	printf("ap2_golomb_par: %lu\n", cfg->ap2_golomb_par);
	printf("ap2_spill: %lu\n", cfg->ap2_spill);
	printf("input_buf: %p\n", (void *)cfg->input_buf);
	if (cfg->input_buf != NULL) {
		printf("input data:");
		for (i = 0; i < cfg->samples; i++) {
			printf(" %04X", ((uint16_t *)cfg->input_buf)[i]);
		}
		printf("\n");
	}
	printf("rdcu_data_adr: 0x%06lX\n", cfg->rdcu_data_adr);
	printf("model_buf: %p\n", (void *)cfg->model_buf);
	if (cfg->model_buf != NULL) {
		printf("model data:");
		for (i = 0; i < cfg->samples; i++) {
			printf(" %04X", ((uint16_t *)cfg->model_buf)[i]);
		}
		printf("\n");
	}
	printf("rdcu_model_adr: 0x%06lX\n", cfg->rdcu_model_adr);
	printf("rdcu_new_model_adr: 0x%06lX\n", cfg->rdcu_new_model_adr);
	printf("samples: %lu\n", cfg->samples);
	printf("icu_output_buf: %p\n", (void *)cfg->icu_output_buf);
	printf("rdcu_buffer_adr: 0x%06lX\n", cfg->rdcu_buffer_adr);
	printf("buffer_length: %lu\n", cfg->buffer_length);
}


/**
 * @brief print the cmp_info structure
 *
 * @param info	 compressor information contains information of an executed
 *		 compression
 */

void print_cmp_info(const struct cmp_info *info)
{
	printf("cmp_mode_used: %lu\n", info->cmp_mode_used);
	printf("model_value_used: %u\n", info->model_value_used);
	printf("round_used: %u\n", info->round_used);
	printf("spill_used: %lu\n", info->spill_used);
	printf("golomb_par_used: %lu\n", info->golomb_par_used);
	printf("samples_used: %lu\n", info->samples_used);
	printf("cmp_size: %lu\n", info->cmp_size);
	printf("ap1_cmp_size: %lu\n", info->ap1_cmp_size);
	printf("ap2_cmp_size: %lu\n", info->ap2_cmp_size);
	printf("rdcu_new_model_adr_used: 0x%06lX\n", info->rdcu_new_model_adr_used);
	printf("rdcu_cmp_adr_used: 0x%06lX\n", info->rdcu_cmp_adr_used);
	printf("cmp_err: %#X\n", info->cmp_err);
}
