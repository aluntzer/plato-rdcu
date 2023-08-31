/**
 * @file   cmp_rdcu_cfg.c
 * @author Dominik Loidolt (dominik.loidolt@univie.ac.at)
 * @date   2020
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
 * @brief hardware compressor configuration library
 * @see Data Compression User Manual PLATO-UVIE-PL-UM-0001
 */


#include <stdint.h>
#include <string.h>

#include <cmp_debug.h>
#include <cmp_support.h>
#include <rdcu_cmd.h>
#include <cmp_rdcu_cfg.h>


/**
 * @brief create an RDCU compression configuration
 *
 * @param data_type	compression data product type
 * @param cmp_mode	compression mode
 * @param model_value	model weighting parameter (only needed for model compression mode)
 * @param lossy_par	lossy rounding parameter (use CMP_LOSSLESS for lossless compression)
 *
 * @returns a compression configuration containing the chosen parameters;
 *	on error the data_type record is set to DATA_TYPE_UNKNOWN
 */

struct cmp_cfg rdcu_cfg_create(enum cmp_data_type data_type, enum cmp_mode cmp_mode,
			       uint32_t model_value, uint32_t lossy_par)
{
	struct cmp_cfg cfg;

	memset(&cfg, 0, sizeof(cfg));

	cfg.data_type = data_type;
	cfg.cmp_mode = cmp_mode;
	cfg.model_value = model_value;
	cfg.round = lossy_par;
	cfg.max_used_bits = &MAX_USED_BITS_SAFE;

	if (cmp_cfg_gen_par_is_invalid(&cfg, RDCU_CHECK))
		cfg.data_type = DATA_TYPE_UNKNOWN;

	return cfg;
}


/**
 * @brief check if a buffer is in outside the RDCU SRAM
 *
 * @param addr	start address of the buffer
 * @param size	length of the buffer in bytes
 *
 * @returns 0 if buffer in inside the RDCU SRAM, 1 when the buffer is outside
 */

static int outside_sram_range(uint32_t addr, uint32_t size)
{
	if (addr + size > RDCU_SRAM_START + RDCU_SRAM_SIZE)
		return 1;

	if (addr > RDCU_SRAM_END)
		return 1;

	if (size > RDCU_SRAM_SIZE)
		return 1;

	return 0;
}


/**
 * @brief check if two buffers are overlapping
 * @note implement according to https://stackoverflow.com/a/325964
 *
 * @param start_a	start address of the 1st buffer
 * @param end_a		end address of the 1st buffer
 * @param start_b	start address of the 2nd buffer
 * @param end_b		end address of the 2nd buffer
 *
 * @returns 0 if buffers are not overlapping, otherwise buffer are
 *	overlapping
 */

static int buffers_overlap(uint32_t start_a, uint32_t end_a, uint32_t start_b,
			   uint32_t end_b)
{
	if (start_a < end_b && end_a > start_b)
		return 1;
	else
		return 0;
}


/**
 * @brief check if RDCU buffer settings are invalid
 *
 * @param cfg	a pointer to a compression configuration
 *
 * @returns 0 if buffers configuration is valid, otherwise the configuration is
 *	invalid
 */

static int rdcu_cfg_buffers_is_invalid(const struct cmp_cfg *cfg)
{
	int cfg_invalid = 0;

	if (cfg->cmp_mode == CMP_MODE_RAW) {
		if (cfg->buffer_length < cfg->samples) {
			debug_print("rdcu_buffer_length is smaller than the samples parameter. There is not enough space to copy the data in RAW mode.\n");
			cfg_invalid++;
		}
	}

	if (cfg->rdcu_data_adr & 0x3) {
		debug_print("Error: The RDCU data to compress start address is not 4-Byte aligned.\n");
		cfg_invalid++;
	}

	if (cfg->rdcu_buffer_adr & 0x3) {
		debug_print("Error: The RDCU compressed data start address is not 4-Byte aligned.\n");
		cfg_invalid++;
	}

	if (outside_sram_range(cfg->rdcu_data_adr, cfg->samples * IMA_SAM2BYT)) {
		debug_print("Error: The RDCU data to compress buffer is outside the RDCU SRAM address space.\n");
		cfg_invalid++;
	}

	if (outside_sram_range(cfg->rdcu_buffer_adr, cfg->buffer_length * IMA_SAM2BYT)) {
		debug_print("Error: The RDCU compressed data buffer is outside the RDCU SRAM address space.\n");
		cfg_invalid++;
	}

	if (buffers_overlap(cfg->rdcu_data_adr,
			    cfg->rdcu_data_adr + cfg->samples * IMA_SAM2BYT,
			    cfg->rdcu_buffer_adr,
			    cfg->rdcu_buffer_adr + cfg->buffer_length * IMA_SAM2BYT)) {
		debug_print("Error: The RDCU data to compress buffer and the RDCU compressed data buffer are overlapping.\n");
		cfg_invalid++;
	}

	if (model_mode_is_used(cfg->cmp_mode)) {
		if (cfg->model_buf && cfg->model_buf == cfg->input_buf) {
			debug_print("Error: The model buffer (model_buf) and the data to be compressed (input_buf) are equal.\n");
			cfg_invalid++;
		}

		if (cfg->rdcu_model_adr & 0x3) {
			debug_print("Error: The RDCU model start address is not 4-Byte aligned.\n");
			cfg_invalid++;
		}

		if (outside_sram_range(cfg->rdcu_model_adr, cfg->samples * IMA_SAM2BYT)) {
			debug_print("Error: The RDCU model buffer is outside the RDCU SRAM address space.\n");
			cfg_invalid++;
		}

		if (buffers_overlap(
			    cfg->rdcu_model_adr,
			    cfg->rdcu_model_adr + cfg->samples * IMA_SAM2BYT,
			    cfg->rdcu_data_adr,
			    cfg->rdcu_data_adr + cfg->samples * IMA_SAM2BYT)) {
			debug_print("Error: The model buffer and the data to compress buffer are overlapping.\n");
			cfg_invalid++;
		}

		if (buffers_overlap(
			cfg->rdcu_model_adr,
			cfg->rdcu_model_adr + cfg->samples * IMA_SAM2BYT,
			cfg->rdcu_buffer_adr,
			cfg->rdcu_buffer_adr + cfg->buffer_length * IMA_SAM2BYT)
		    ) {
			debug_print("Error: The model buffer and the compressed data buffer are overlapping.\n");
			cfg_invalid++;
		}

		if (cfg->rdcu_model_adr != cfg->rdcu_new_model_adr) {
			if (cfg->rdcu_new_model_adr & 0x3) {
				debug_print("Error: The RDCU updated model start address (rdcu_new_model_adr) is not 4-Byte aligned.\n");
				cfg_invalid++;
			}

			if (outside_sram_range(cfg->rdcu_new_model_adr,
					   cfg->samples * IMA_SAM2BYT)) {
				debug_print("Error: The RDCU updated model buffer is outside the RDCU SRAM address space.\n");
				cfg_invalid++;
			}

			if (buffers_overlap(
				cfg->rdcu_new_model_adr,
				cfg->rdcu_new_model_adr + cfg->samples * IMA_SAM2BYT,
				cfg->rdcu_data_adr,
				cfg->rdcu_data_adr + cfg->samples * IMA_SAM2BYT)
			    ) {
				debug_print("Error: The updated model buffer and the data to compress buffer are overlapping.\n");
				cfg_invalid++;
			}

			if (buffers_overlap(
				cfg->rdcu_new_model_adr,
				cfg->rdcu_new_model_adr + cfg->samples * IMA_SAM2BYT,
				cfg->rdcu_buffer_adr,
				cfg->rdcu_buffer_adr + cfg->buffer_length * IMA_SAM2BYT)
			    ) {
				debug_print("Error: The updated model buffer and the compressed data buffer are overlapping.\n");
				cfg_invalid++;
			}
			if (buffers_overlap(
				cfg->rdcu_new_model_adr,
				cfg->rdcu_new_model_adr + cfg->samples * IMA_SAM2BYT,
				cfg->rdcu_model_adr,
				cfg->rdcu_model_adr + cfg->samples * IMA_SAM2BYT)
			    ) {
				debug_print("Error: The updated model buffer and the model buffer are overlapping.\n");
				cfg_invalid++;
			}
		}
	}

#ifdef SKIP_CMP_PAR_CHECK
	return 0;
#endif
	return cfg_invalid;
}


/**
 * @brief setup of the different data buffers for an RDCU compression
 *
 * @param cfg			pointer to a compression configuration (created
 *				with the rdcu_cfg_create() function)
 * @param data_to_compress	pointer to the data to be compressed (if NULL no
 *				data transfer to the RDCU)
 * @param data_samples		length of the data to be compressed (plus the
 *				collection header) measured in 16-bit data samples
 * @param model_of_data		pointer to the model data buffer (only needed for
 *				model compression mode, if NULL no model data is
 *				transferred to the RDCU)
 * @param rdcu_data_adr		RDCU SRAM data to compress start address
 * @param rdcu_model_adr	RDCU SRAM model start address (only needed for
 *				model compression mode)
 * @param rdcu_new_model_adr	RDCU SRAM new/updated model start address (can be
 *				the same as rdcu_model_adr for in-place model update)
 * @param rdcu_buffer_adr	RDCU SRAM compressed data start address
 * @param rdcu_buffer_lenght	length of the RDCU compressed data SRAM buffer
 *				measured in 16-bit units (same as data_samples)
 *
 * @returns 0 if parameters are valid, non-zero if parameters are invalid
 */

int rdcu_cfg_buffers(struct cmp_cfg *cfg, uint16_t *data_to_compress,
		     uint32_t data_samples, uint16_t *model_of_data,
		     uint32_t rdcu_data_adr, uint32_t rdcu_model_adr,
		     uint32_t rdcu_new_model_adr, uint32_t rdcu_buffer_adr,
		     uint32_t rdcu_buffer_lenght)
{
	if (!cfg) {
		debug_print("Error: pointer to the compression configuration structure is NULL.\n");
		return -1;
	}

	cfg->input_buf = data_to_compress;
	cfg->samples = data_samples;
	cfg->model_buf = model_of_data;
	cfg->rdcu_data_adr = rdcu_data_adr;
	cfg->rdcu_model_adr = rdcu_model_adr;
	cfg->rdcu_new_model_adr = rdcu_new_model_adr;
	cfg->rdcu_buffer_adr = rdcu_buffer_adr;
	cfg->buffer_length = rdcu_buffer_lenght;

	return rdcu_cfg_buffers_is_invalid(cfg);
}


/**
 * @brief set up the configuration parameters for an RDCU imagette compression
 *
 * @param cfg			pointer to a compression configuration (created
 *				with the rdcu_cfg_create() function)
 * @param golomb_par		imagette compression parameter
 * @param spillover_par		imagette spillover threshold parameter
 * @param ap1_golomb_par	adaptive 1 imagette compression parameter
 * @param ap1_spillover_par	adaptive 1 imagette spillover threshold parameter
 * @param ap2_golomb_par	adaptive 2 imagette compression parameter
 * @param ap2_spillover_par	adaptive 2 imagette spillover threshold parameter
 *
 * @returns 0 if parameters are valid, non-zero if parameters are invalid
 */

int rdcu_cfg_imagette(struct cmp_cfg *cfg,
		      uint32_t golomb_par, uint32_t spillover_par,
		      uint32_t ap1_golomb_par, uint32_t ap1_spillover_par,
		      uint32_t ap2_golomb_par, uint32_t ap2_spillover_par)
{
	if (!cfg) {
		debug_print("Error: pointer to the compression configuration structure is NULL.\n");
		return -1;
	}

	cfg->golomb_par = golomb_par;
	cfg->spill = spillover_par;
	cfg->ap1_golomb_par = ap1_golomb_par;
	cfg->ap1_spill = ap1_spillover_par;
	cfg->ap2_golomb_par = ap2_golomb_par;
	cfg->ap2_spill = ap2_spillover_par;

	return cmp_cfg_imagette_is_invalid(cfg, RDCU_CHECK);
}


/**
 * @brief set up the default configuration parameters for an RDCU imagette
 *	compression based on the set compression mode
 *
 * @returns 0 if parameters are valid, non-zero if parameters are invalid
 */

int rdcu_cfg_imagette_default(struct cmp_cfg *cfg)
{

	if (!cfg) {
		debug_print("Error: pointer to the compression configuration structure is NULL.\n");
		return -1;
	}

	if (model_mode_is_used(cfg->cmp_mode)) {
		return rdcu_cfg_imagette(cfg, CMP_DEF_IMA_MODEL_GOLOMB_PAR,
					 CMP_DEF_IMA_MODEL_SPILL_PAR,
					 CMP_DEF_IMA_MODEL_AP1_GOLOMB_PAR,
					 CMP_DEF_IMA_MODEL_AP1_SPILL_PAR,
					 CMP_DEF_IMA_MODEL_AP2_GOLOMB_PAR,
					 CMP_DEF_IMA_MODEL_AP2_SPILL_PAR);
	} else {
		return rdcu_cfg_imagette(cfg, CMP_DEF_IMA_DIFF_GOLOMB_PAR,
					 CMP_DEF_IMA_DIFF_SPILL_PAR,
					 CMP_DEF_IMA_DIFF_AP1_GOLOMB_PAR,
					 CMP_DEF_IMA_DIFF_AP1_SPILL_PAR,
					 CMP_DEF_IMA_DIFF_AP2_GOLOMB_PAR,
					 CMP_DEF_IMA_DIFF_AP2_SPILL_PAR);
	}
}


/**
 * @brief check if the compressor configuration is invalid for an RDCU compression,
 *	see the user manual for more information (PLATO-UVIE-PL-UM-0001).
 *
 * @param cfg	pointer to a compression configuration contains all parameters
 *	required for compression
 *
 * @returns 0 if parameters are valid, non-zero if parameters are invalid
 */

int rdcu_cmp_cfg_is_invalid(const struct cmp_cfg *cfg)
{
	int cfg_invalid = 0;

	if (!cfg) {
		debug_print("Error: pointer to the compression configuration structure is NULL.\n");
		return -1;
	}

	if (!cfg->input_buf)
		debug_print("Warning: The data to compress buffer is set to NULL. No data will be transferred to the rdcu_data_adr in the RDCU SRAM.\n");

	if (model_mode_is_used(cfg->cmp_mode)) {
		if (!cfg->model_buf)
			debug_print("Warning: The model buffer is set to NULL. No model data will be transferred to the rdcu_model_adr in the RDCU SRAM.\n");
	}

	if (cfg->samples == 0)
		debug_print("Warning: The samples parameter is set to 0. No data will be compressed.\n");

	if (cfg->icu_new_model_buf)
		debug_print("Warning: ICU updated model buffer is set. This buffer is not used for an RDCU compression.\n");

	if (cfg->icu_output_buf)
		debug_print("Warning: ICU compressed data buffer is set. This buffer is not used for an RDCU compression.\n");

	if (cfg->buffer_length == 0) {
		debug_print("Error: The buffer_length is set to 0. There is no place to store the compressed data.\n");
		cfg_invalid++;
	}

	cfg_invalid += cmp_cfg_gen_par_is_invalid(cfg, RDCU_CHECK);
	cfg_invalid += rdcu_cfg_buffers_is_invalid(cfg);
	cfg_invalid += cmp_cfg_imagette_is_invalid(cfg, RDCU_CHECK);

	return cfg_invalid;
}
