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

#include "../common/cmp_debug.h"
#include "../common/cmp_support.h"
#include "../common/leon_inttypes.h"
#include "../common/compiler.h"
#include "rdcu_cmd.h"
#include "cmp_rdcu_cfg.h"


/**
 * @brief check if the compression mode, model value and the lossy rounding
 *	parameters are invalid for an RDCU compression
 *
 * @param rcfg	pointer to a RDCU compression configuration containing the
 *	compression mode, model value and the rounding parameters
 *
 * @returns 0 if the compression mode, model value and the lossy rounding
 *	parameters are valid for an RDCU compression, non-zero if parameters are
 *	invalid
 */

static int rdcu_cfg_gen_pars_are_invalid(const struct rdcu_cfg *rcfg)
{
	int rcfg_invalid = 0;

	if (!cmp_mode_is_supported(rcfg->cmp_mode)) {
		debug_print("Error: selected cmp_mode: %i is not supported for a RDCU compression.", rcfg->cmp_mode);
		rcfg_invalid++;
	}

	if (rcfg->model_value > MAX_MODEL_VALUE) {
		debug_print("Error: selected model_value: %" PRIu32 " is invalid. The largest supported value is: %u.",
			    rcfg->model_value, MAX_MODEL_VALUE);
		rcfg_invalid++;
	}

	if (rcfg->round > MAX_RDCU_ROUND) {
		debug_print("Error: selected lossy parameter: %" PRIu32 " is not supported for a RDCU compression. The largest supported value is: %" PRIu32 ".",
			    rcfg->round, MAX_RDCU_ROUND);
		rcfg_invalid++;
	}

#ifdef SKIP_CMP_PAR_CHECK
	return 0;
#endif
	return rcfg_invalid;
}


/**
 * @brief create an RDCU compression configuration
 *
 * @param rcfg		pointer to an RDCU compression configuration to be created
 * @param cmp_mode	compression mode
 * @param model_value	model weighting parameter (only needed for model compression mode)
 * @param round		lossy rounding parameter (use CMP_LOSSLESS for lossless compression)
 *
 * @returns 0 if parameters are valid, non-zero if parameters are invalid
 */

int rdcu_cfg_create(struct rdcu_cfg *rcfg, enum cmp_mode cmp_mode,
		    uint32_t model_value, uint32_t round)
{
	if (!rcfg)
		return 1;

	memset(rcfg, 0, sizeof(*rcfg));

	rcfg->cmp_mode = cmp_mode;
	rcfg->model_value = model_value;
	rcfg->round = round;

	return rdcu_cfg_gen_pars_are_invalid(rcfg);
}


/**
 * @brief check if a buffer is outside the RDCU SRAM
 *
 * @param addr	start address of the buffer
 * @param size	length of the buffer in bytes
 *
 * @returns 0 if the buffer is inside the RDCU SRAM, 1 when the buffer is outside
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
 * @returns 0 if buffers are not overlapping, otherwise buffers are
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
 * @param rcfg	a pointer to a RDCU compression configuration
 *
 * @returns 0 if the buffer configuration is valid, otherwise the configuration
 *	is invalid
 */

static int rdcu_cfg_buffers_is_invalid(const struct rdcu_cfg *rcfg)
{
	int rcfg_invalid = 0;

	if (rcfg->cmp_mode == CMP_MODE_RAW) {
		if (rcfg->buffer_length < rcfg->samples) {
			debug_print("rdcu_buffer_length is smaller than the samples parameter. There is not enough space to copy the data in RAW mode.");
			rcfg_invalid++;
		}
	}

	if (rcfg->rdcu_data_adr & 0x3) {
		debug_print("Error: The RDCU data to compress start address is not 4-Byte aligned.");
		rcfg_invalid++;
	}

	if (rcfg->rdcu_buffer_adr & 0x3) {
		debug_print("Error: The RDCU compressed data start address is not 4-Byte aligned.");
		rcfg_invalid++;
	}

	if (outside_sram_range(rcfg->rdcu_data_adr, rcfg->samples * IMA_SAM2BYT)) {
		debug_print("Error: The RDCU data to compress buffer is outside the RDCU SRAM address space.");
		rcfg_invalid++;
	}

	if (outside_sram_range(rcfg->rdcu_buffer_adr, rcfg->buffer_length * IMA_SAM2BYT)) {
		debug_print("Error: The RDCU compressed data buffer is outside the RDCU SRAM address space.");
		rcfg_invalid++;
	}

	if (buffers_overlap(rcfg->rdcu_data_adr,
			    rcfg->rdcu_data_adr + rcfg->samples * IMA_SAM2BYT,
			    rcfg->rdcu_buffer_adr,
			    rcfg->rdcu_buffer_adr + rcfg->buffer_length * IMA_SAM2BYT)) {
		debug_print("Error: The RDCU data to compress buffer and the RDCU compressed data buffer are overlapping.");
		rcfg_invalid++;
	}

	if (model_mode_is_used(rcfg->cmp_mode)) {
		if (rcfg->model_buf && rcfg->model_buf == rcfg->input_buf) {
			debug_print("Error: The model buffer (model_buf) and the data to be compressed (input_buf) are equal.");
			rcfg_invalid++;
		}

		if (rcfg->rdcu_model_adr & 0x3) {
			debug_print("Error: The RDCU model start address is not 4-Byte aligned.");
			rcfg_invalid++;
		}

		if (outside_sram_range(rcfg->rdcu_model_adr, rcfg->samples * IMA_SAM2BYT)) {
			debug_print("Error: The RDCU model buffer is outside the RDCU SRAM address space.");
			rcfg_invalid++;
		}

		if (buffers_overlap(
			    rcfg->rdcu_model_adr,
			    rcfg->rdcu_model_adr + rcfg->samples * IMA_SAM2BYT,
			    rcfg->rdcu_data_adr,
			    rcfg->rdcu_data_adr + rcfg->samples * IMA_SAM2BYT)) {
			debug_print("Error: The model buffer and the data to compress buffer are overlapping.");
			rcfg_invalid++;
		}

		if (buffers_overlap(
			rcfg->rdcu_model_adr,
			rcfg->rdcu_model_adr + rcfg->samples * IMA_SAM2BYT,
			rcfg->rdcu_buffer_adr,
			rcfg->rdcu_buffer_adr + rcfg->buffer_length * IMA_SAM2BYT)
		    ) {
			debug_print("Error: The model buffer and the compressed data buffer are overlapping.");
			rcfg_invalid++;
		}

		if (rcfg->rdcu_model_adr != rcfg->rdcu_new_model_adr) {
			if (rcfg->rdcu_new_model_adr & 0x3) {
				debug_print("Error: The RDCU updated model start address (rdcu_new_model_adr) is not 4-Byte aligned.");
				rcfg_invalid++;
			}

			if (outside_sram_range(rcfg->rdcu_new_model_adr,
					   rcfg->samples * IMA_SAM2BYT)) {
				debug_print("Error: The RDCU updated model buffer is outside the RDCU SRAM address space.");
				rcfg_invalid++;
			}

			if (buffers_overlap(
				rcfg->rdcu_new_model_adr,
				rcfg->rdcu_new_model_adr + rcfg->samples * IMA_SAM2BYT,
				rcfg->rdcu_data_adr,
				rcfg->rdcu_data_adr + rcfg->samples * IMA_SAM2BYT)
			    ) {
				debug_print("Error: The updated model buffer and the data to compress buffer are overlapping.");
				rcfg_invalid++;
			}

			if (buffers_overlap(
				rcfg->rdcu_new_model_adr,
				rcfg->rdcu_new_model_adr + rcfg->samples * IMA_SAM2BYT,
				rcfg->rdcu_buffer_adr,
				rcfg->rdcu_buffer_adr + rcfg->buffer_length * IMA_SAM2BYT)
			    ) {
				debug_print("Error: The updated model buffer and the compressed data buffer are overlapping.");
				rcfg_invalid++;
			}
			if (buffers_overlap(
				rcfg->rdcu_new_model_adr,
				rcfg->rdcu_new_model_adr + rcfg->samples * IMA_SAM2BYT,
				rcfg->rdcu_model_adr,
				rcfg->rdcu_model_adr + rcfg->samples * IMA_SAM2BYT)
			    ) {
				debug_print("Error: The updated model buffer and the model buffer are overlapping.");
				rcfg_invalid++;
			}
		}
	}

#ifdef SKIP_CMP_PAR_CHECK
	return 0;
#endif
	return rcfg_invalid;
}


/**
 * @brief setup of the different data buffers for an RDCU compression
 *
 * @param rcfg			pointer to a RDCU compression configuration
 *				(created with the rdcu_cfg_create() function)
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

int rdcu_cfg_buffers(struct rdcu_cfg *rcfg, uint16_t *data_to_compress,
		     uint32_t data_samples, uint16_t *model_of_data,
		     uint32_t rdcu_data_adr, uint32_t rdcu_model_adr,
		     uint32_t rdcu_new_model_adr, uint32_t rdcu_buffer_adr,
		     uint32_t rdcu_buffer_lenght)
{
	if (!rcfg) {
		debug_print("Error: pointer to the compression configuration structure is NULL.");
		return -1;
	}

	rcfg->input_buf = data_to_compress;
	rcfg->samples = data_samples;
	rcfg->model_buf = model_of_data;
	rcfg->rdcu_data_adr = rdcu_data_adr;
	rcfg->rdcu_model_adr = rdcu_model_adr;
	rcfg->rdcu_new_model_adr = rdcu_new_model_adr;
	rcfg->rdcu_buffer_adr = rdcu_buffer_adr;
	rcfg->buffer_length = rdcu_buffer_lenght;

	return rdcu_cfg_buffers_is_invalid(rcfg);
}


/**
 * @brief check if the combination of the Golomb and spill parameters is invalid
 *
 * @param golomb_par	Golomb parameter
 * @param spill		spillover threshold parameter
 * @param par_name	string describing the use of the compression par. for
 *			debug messages (can be NULL)
 *
 * @returns 0 if the parameter combination is valid, otherwise the combination is invalid
 */

static int rdcu_cfg_golomb_spill_are_invalid(uint32_t golomb_par, uint32_t spill,
					 const char *par_name MAYBE_UNUSED)
{
	int rcfg_invalid = 0;

	if (golomb_par < MIN_IMA_GOLOMB_PAR || golomb_par > MAX_IMA_GOLOMB_PAR) {
		debug_print("Error: The selected %s compression parameter: %" PRIu32 " is not supported in the selected compression mode. The compression parameter has to be between [%" PRIu32 ", %" PRIu32 "] in this mode.",
			    par_name, golomb_par, MIN_IMA_GOLOMB_PAR, MAX_IMA_GOLOMB_PAR);
		rcfg_invalid++;
	}
	if (spill < MIN_IMA_SPILL) {
		debug_print("Error: The selected %s spillover threshold value: %" PRIu32 " is too small. The smallest possible spillover value is: %" PRIu32 ".",
			    par_name, spill, MIN_IMA_SPILL);
		rcfg_invalid++;
	}
	if (spill > cmp_ima_max_spill(golomb_par)) {
		debug_print("Error: The selected %s spillover threshold value: %" PRIu32 " is too large for the selected %s compression parameter: %" PRIu32 ". The largest possible spillover value in the selected compression mode is: %" PRIu32 ".",
			    par_name, spill, par_name, golomb_par, cmp_ima_max_spill(golomb_par));
		rcfg_invalid++;
	}


#ifdef SKIP_CMP_PAR_CHECK
	return 0;
#endif
	return rcfg_invalid;
}


/**
 * @brief check if the all Golomb and spill parameters pairs are invalid
 *
 * @param rcfg	pointer to a RDCU compressor configuration
 *
 * @returns 0 if the parameters are valid, otherwise invalid
 */

static int rdcu_cfg_imagette_is_invalid(const struct rdcu_cfg *rcfg)
{
	int rcfg_invalid = 0;

	rcfg_invalid += rdcu_cfg_golomb_spill_are_invalid(rcfg->golomb_par, rcfg->spill,
							 "imagette");
	rcfg_invalid += rdcu_cfg_golomb_spill_are_invalid(rcfg->ap1_golomb_par, rcfg->ap1_spill,
							 "adaptive 1 imagette");
	rcfg_invalid += rdcu_cfg_golomb_spill_are_invalid(rcfg->ap2_golomb_par, rcfg->ap2_spill,
							 "adaptive 2 imagette");

	return rcfg_invalid;
}


/**
 * @brief set up the configuration parameters for an RDCU imagette compression
 *
 * @param rcfg			pointer to a RDCU compression configuration
 *				(created with the rdcu_cfg_create() function)
 * @param golomb_par		imagette compression parameter
 * @param spillover_par		imagette spillover threshold parameter
 * @param ap1_golomb_par	adaptive 1 imagette compression parameter
 * @param ap1_spillover_par	adaptive 1 imagette spillover threshold parameter
 * @param ap2_golomb_par	adaptive 2 imagette compression parameter
 * @param ap2_spillover_par	adaptive 2 imagette spillover threshold parameter
 *
 * @returns 0 if parameters are valid, non-zero if parameters are invalid
 */

int rdcu_cfg_imagette(struct rdcu_cfg *rcfg,
		      uint32_t golomb_par, uint32_t spillover_par,
		      uint32_t ap1_golomb_par, uint32_t ap1_spillover_par,
		      uint32_t ap2_golomb_par, uint32_t ap2_spillover_par)
{
	if (!rcfg) {
		debug_print("Error: pointer to the compression configuration structure is NULL.");
		return -1;
	}

	rcfg->golomb_par = golomb_par;
	rcfg->spill = spillover_par;
	rcfg->ap1_golomb_par = ap1_golomb_par;
	rcfg->ap1_spill = ap1_spillover_par;
	rcfg->ap2_golomb_par = ap2_golomb_par;
	rcfg->ap2_spill = ap2_spillover_par;

	return rdcu_cfg_imagette_is_invalid(rcfg);
}


/**
 * @brief set up the default configuration parameters for an RDCU imagette
 *	compression based on the set compression mode
 *
 * @param rcfg	pointer to a RDCU compression configuration (created with the
 *		rdcu_cfg_create() function)
 *
 * @returns 0 if parameters are valid, non-zero if parameters are invalid
 */

int rdcu_cfg_imagette_default(struct rdcu_cfg *rcfg)
{

	if (!rcfg) {
		debug_print("Error: pointer to the compression configuration structure is NULL.");
		return -1;
	}

	if (model_mode_is_used(rcfg->cmp_mode)) {
		return rdcu_cfg_imagette(rcfg, CMP_DEF_IMA_MODEL_GOLOMB_PAR,
					 CMP_DEF_IMA_MODEL_SPILL_PAR,
					 CMP_DEF_IMA_MODEL_AP1_GOLOMB_PAR,
					 CMP_DEF_IMA_MODEL_AP1_SPILL_PAR,
					 CMP_DEF_IMA_MODEL_AP2_GOLOMB_PAR,
					 CMP_DEF_IMA_MODEL_AP2_SPILL_PAR);
	} else {
		return rdcu_cfg_imagette(rcfg, CMP_DEF_IMA_DIFF_GOLOMB_PAR,
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
 * @param rcfg	pointer to a RDCU compression configuration contains all
 *		parameters required for compression
 *
 * @returns 0 if parameters are valid, non-zero if parameters are invalid
 */

int rdcu_cmp_cfg_is_invalid(const struct rdcu_cfg *rcfg)
{
	int rcfg_invalid = 0;

	if (!rcfg) {
		debug_print("Error: pointer to the compression configuration structure is NULL.");
		return -1;
	}

	if (!rcfg->input_buf)
		debug_print("Warning: The data to compress buffer is set to NULL. No data will be transferred to the rdcu_data_adr in the RDCU SRAM.");

	if (model_mode_is_used(rcfg->cmp_mode)) {
		if (!rcfg->model_buf)
			debug_print("Warning: The model buffer is set to NULL. No model data will be transferred to the rdcu_model_adr in the RDCU SRAM.");
	}

	if (rcfg->samples == 0)
		debug_print("Warning: The samples parameter is set to 0. No data will be compressed.");

	if (rcfg->icu_new_model_buf)
		debug_print("Warning: ICU updated model buffer is set. This buffer is not used for an RDCU compression.");

	if (rcfg->icu_output_buf)
		debug_print("Warning: ICU compressed data buffer is set. This buffer is not used for an RDCU compression.");

	if (rcfg->buffer_length == 0) {
		debug_print("Error: The buffer_length is set to 0. There is no place to store the compressed data.");
		rcfg_invalid++;
	}

	rcfg_invalid += rdcu_cfg_gen_pars_are_invalid(rcfg);
	rcfg_invalid += rdcu_cfg_buffers_is_invalid(rcfg);
	rcfg_invalid += rdcu_cfg_imagette_is_invalid(rcfg);

	return rcfg_invalid;
}
