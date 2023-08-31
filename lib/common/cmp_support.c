/**
 * @file   cmp_support.c
 * @author Dominik Loidolt (dominik.loidolt@univie.ac.at)
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

#include "compiler.h"

#include "cmp_support.h"
#include "cmp_debug.h"
#include "my_inttypes.h"


/**
 * @brief implementation of the logarithm base of floor(log2(x)) for integers
 * @note ilog_2(0) = -1 defined
 *
 * @param x	input parameter
 *
 * @returns the result of floor(log2(x))
 */

unsigned int ilog_2(uint32_t x)
{
	if (!x)
		return -1U;

	return 31 - (unsigned int)__builtin_clz(x);
}


/**
 * @brief determining if an integer is a power of 2
 * @note 0 is incorrectly considered a power of 2 here
 *
 * @param v	we want to see if v is a power of 2
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
 * @brief check if the compression entity data product type is supported
 *
 * @param data_type	compression entity data product type to check
 *
 * @returns non-zero if data_type is invalid; zero if data_type is valid
 */

int cmp_data_type_is_invalid(enum cmp_data_type data_type)
{
	if (data_type == DATA_TYPE_F_CAM_OFFSET)
		debug_print("Error: DATA_TYPE_F_CAM_OFFSET is TBD and not implemented yet.\n");
	if (data_type == DATA_TYPE_F_CAM_BACKGROUND)
		debug_print("Error: DATA_TYPE_F_CAM_BACKGROUND is TBD  and not implemented yet.\n");

	if (data_type <= DATA_TYPE_UNKNOWN || data_type > DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE)
		return 1;

	return 0;
}


/**
 * @brief check if a model mode is selected
 *
 * @param cmp_mode	compression mode
 *
 * @returns 1 when the model mode is used, otherwise 0
 */

int model_mode_is_used(enum cmp_mode cmp_mode)
{
	if (cmp_mode == CMP_MODE_MODEL_ZERO ||
	    cmp_mode == CMP_MODE_MODEL_MULTI)
		return 1;

	return 0;
}


/**
 * @brief check if the raw mode is selected
 *
 * @param cmp_mode	compression mode
 *
 * @returns 1 when the raw mode is used, otherwise 0
 */

int raw_mode_is_used(enum cmp_mode cmp_mode)
{
	if (cmp_mode == CMP_MODE_RAW)
		return 1;

	return 0;
}


/**
 * @brief check if the compression mode is supported by the RDCU compressor
 *
 * @param cmp_mode	compression mode
 *
 * @returns 1 when the compression mode is supported by the RDCU, otherwise 0
 */

int rdcu_supported_cmp_mode_is_used(enum cmp_mode cmp_mode)
{
	switch (cmp_mode) {
	case CMP_MODE_RAW:
	case CMP_MODE_MODEL_ZERO:
	case CMP_MODE_DIFF_ZERO:
	case CMP_MODE_MODEL_MULTI:
	case CMP_MODE_DIFF_MULTI:
		return 1;
	case CMP_MODE_STUFF:
	default:
		return 0;
	}

}


/**
 * @brief check if the data product data type is supported by the RDCU compressor
 *
 * @param data_type	compression data product type
 *
 * @returns 1 when the data type is supported by the RDCU, otherwise 0
 */

int rdcu_supported_data_type_is_used(enum cmp_data_type data_type)
{
	switch (data_type) {
	case DATA_TYPE_IMAGETTE:
	case DATA_TYPE_IMAGETTE_ADAPTIVE:
	case DATA_TYPE_SAT_IMAGETTE:
	case DATA_TYPE_SAT_IMAGETTE_ADAPTIVE:
	case DATA_TYPE_F_CAM_IMAGETTE:
	case DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE:
		return 1;
	default:
		return 0;
	}
}


/**
 * @brief check if the compression mode is supported for an ICU compression
 *
 * @param cmp_mode	compression mode
 *
 * @returns 1 when the compression mode is supported, otherwise 0
 */

int cmp_mode_is_supported(enum cmp_mode cmp_mode)
{
	switch (cmp_mode) {
	case CMP_MODE_RAW:
	case CMP_MODE_MODEL_ZERO:
	case CMP_MODE_DIFF_ZERO:
	case CMP_MODE_MODEL_MULTI:
	case CMP_MODE_DIFF_MULTI:
	case CMP_MODE_STUFF:
		return 1;
	}
	return 0;
}


/**
 * @brief check if zero escape symbol mechanism mode is used
 *
 * @param cmp_mode	compression mode
 *
 * @returns 1 when zero escape symbol mechanism is set, otherwise 0
 */

int zero_escape_mech_is_used(enum cmp_mode cmp_mode)
{
	if (cmp_mode == CMP_MODE_MODEL_ZERO ||
	    cmp_mode == CMP_MODE_DIFF_ZERO)
		return 1;

	return 0;
}


/**
 * @brief check if multi escape symbol mechanism mode is used
 *
 * @param cmp_mode	compression mode
 *
 * @returns 1 when multi escape symbol mechanism is set, otherwise 0
 */

int multi_escape_mech_is_used(enum cmp_mode cmp_mode)
{
	if (cmp_mode == CMP_MODE_MODEL_MULTI ||
	    cmp_mode == CMP_MODE_DIFF_MULTI)
		return 1;

	return 0;
}


/**
 * @brief check if an imagette compression data type is used
 * @note adaptive imagette compression data types included
 *
 * @param data_type	compression data type
 *
 * @returns 1 when data_type is an imagette data type, otherwise 0
 */

int cmp_imagette_data_type_is_used(enum cmp_data_type data_type)
{
	return rdcu_supported_data_type_is_used(data_type);
}


/**
 * @brief check if an adaptive imagette compression data type is used
 *
 * @param data_type	compression data type
 *
 * @returns 1 when data_type is an adaptive imagette data type, otherwise 0
 */

int cmp_ap_imagette_data_type_is_used(enum cmp_data_type data_type)
{
	switch (data_type) {
	case DATA_TYPE_IMAGETTE_ADAPTIVE:
	case DATA_TYPE_SAT_IMAGETTE_ADAPTIVE:
	case DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE:
		return 1;
	default:
		return 0;
	}
}


/**
 * @brief check if a flux/center of brightness compression data type is used
 *
 * @param data_type	compression data type
 *
 * @returns 1 when data_type is a flux/center of brightness data type, otherwise 0
 */

int cmp_fx_cob_data_type_is_used(enum cmp_data_type data_type)
{
	switch (data_type) {
	case DATA_TYPE_S_FX:
	case DATA_TYPE_S_FX_EFX:
	case DATA_TYPE_S_FX_NCOB:
	case DATA_TYPE_S_FX_EFX_NCOB_ECOB:
	case DATA_TYPE_L_FX:
	case DATA_TYPE_L_FX_EFX:
	case DATA_TYPE_L_FX_NCOB:
	case DATA_TYPE_L_FX_EFX_NCOB_ECOB:
	case DATA_TYPE_F_FX:
	case DATA_TYPE_F_FX_EFX:
	case DATA_TYPE_F_FX_NCOB:
	case DATA_TYPE_F_FX_EFX_NCOB_ECOB:
		return 1;
	default:
		return 0;
	}
}


/**
 * @brief check if an auxiliary science compression data type is used
 *
 * @param data_type	compression data type
 *
 * @returns 1 when data_type is an auxiliary science data type, otherwise 0
 */

int cmp_aux_data_type_is_used(enum cmp_data_type data_type)
{
	switch (data_type) {
	case DATA_TYPE_OFFSET:
	case DATA_TYPE_BACKGROUND:
	case DATA_TYPE_SMEARING:
	case DATA_TYPE_F_CAM_OFFSET:
	case DATA_TYPE_F_CAM_BACKGROUND:
		return 1;
	default:
		return 0;
	}
}


/**
 * @brief get the maximum valid spill threshold value for a imagette
 *	compression in diff or model mode
 *
 * @param golomb_par	Golomb parameter
 *
 * @returns the highest still valid spill threshold value for a diff of model
 *	 mode compression; 0 if golomb_par is invalid
 */

uint32_t cmp_ima_max_spill(unsigned int golomb_par)
{
	/* the RDCU can only generate 16 bit long code words -> lower max spill needed */
	const uint32_t LUT_MAX_RDCU[MAX_IMA_GOLOMB_PAR+1] = { 0, 8, 22, 35, 48,
		60, 72, 84, 96, 107, 118, 129, 140, 151, 162, 173, 184, 194,
		204, 214, 224, 234, 244, 254, 264, 274, 284, 294, 304, 314, 324,
		334, 344, 353, 362, 371, 380, 389, 398, 407, 416, 425, 434, 443,
		452, 461, 470, 479, 488, 497, 506, 515, 524, 533, 542, 551, 560,
		569, 578, 587, 596, 605, 614, 623 };

	if (golomb_par >= ARRAY_SIZE(LUT_MAX_RDCU))
		return 0;

	return LUT_MAX_RDCU[golomb_par];
}


/**
 * @brief get the maximum valid spill threshold value for a non-imagette compression
 *	in diff or model mode
 *
 * @param cmp_par	compression parameter
 *
 * @returns the highest still valid spill threshold value for diff or model
 *	mode compression; 0 if the cmp_par is not valid
 */

uint32_t cmp_icu_max_spill(unsigned int cmp_par)
{
	/* the ICU compressor can generate code words with a length of maximal 32 bits. */
	unsigned int const max_cw_bits = 32;
	unsigned int const cutoff = (0x2U << (ilog_2(cmp_par) & 0x1FU)) - cmp_par;
	unsigned int const max_n_sym_offset = max_cw_bits/2 - 1;

	if (!cmp_par || cmp_par > MAX_NON_IMA_GOLOMB_PAR)
		return 0;

	return (max_cw_bits-1-ilog_2(cmp_par))*cmp_par + cutoff
		- max_n_sym_offset - 1;
}


/**
 * @brief calculate the need bytes to hold a bitstream
 *
 * @param cmp_size_bit	compressed data size, measured in bits
 *
 * @returns the size in bytes to store the hole bitstream
 * @note we round up the result to multiples of 4 bytes
 */

unsigned int cmp_bit_to_4byte(unsigned int cmp_size_bit)
{
	return (((cmp_size_bit + 7) / 8) + 3) & ~0x3UL;
}


/**
 * @brief check if the compression data type, compression mode, model value and
 *	the lossy rounding parameters are invalid for a RDCU or ICU compression
 *
 * @param cfg	pointer to a compression configuration containing the compression
 *	data product type, compression mode, model value and the rounding parameters
 * @param opt		check options:
 *			RDCU_CHECK for RDCU compression check
 *			ICU_CHECK for ICU compression check
 *
 * @returns 0 if the compression data type, compression mode, model value and
 *	the lossy rounding parameters are valid for an RDCU or ICU compression,
 *	non-zero if parameters are invalid
 */

int cmp_cfg_gen_par_is_invalid(const struct cmp_cfg *cfg, enum check_opt opt)
{
	int cfg_invalid = 0;
	int invalid_data_type = 1;
	int unsupported_cmp_mode = 1;
	int check_model_value = 1;
	uint32_t max_round_value = 0;
	char *str = "";

	if (!cfg)
		return 1;

	switch (opt) {
	case RDCU_CHECK:
		/* the RDCU can only compress imagette data */
		invalid_data_type = !cmp_imagette_data_type_is_used(cfg->data_type);
		unsupported_cmp_mode = !rdcu_supported_cmp_mode_is_used(cfg->cmp_mode);
		max_round_value = MAX_RDCU_ROUND;
		/* for the RDCU the model vale has to be always in the allowed range */
		check_model_value = 1;
		str = " for a RDCU compression";
		break;
	case ICU_CHECK:
		invalid_data_type = cmp_data_type_is_invalid(cfg->data_type);
		unsupported_cmp_mode = !cmp_mode_is_supported(cfg->cmp_mode);
		max_round_value = MAX_ICU_ROUND;
		check_model_value = model_mode_is_used(cfg->cmp_mode);
		break;
	}

	if (invalid_data_type) {
		debug_print("Error: selected compression data type is not supported%s.\n", str);
		cfg_invalid++;
	}

	if (unsupported_cmp_mode) {
		debug_print("Error: selected cmp_mode: %i is not supported%s.\n", cfg->cmp_mode, str);
		cfg_invalid++;
	}

	if (check_model_value) {
		if (cfg->model_value > MAX_MODEL_VALUE) {
			debug_print("Error: selected model_value: %" PRIu32 " is invalid. The largest supported value is: %u.\n",
				    cfg->model_value, MAX_MODEL_VALUE);
			cfg_invalid++;
		}
	}

	if (cfg->round > max_round_value) {
		debug_print("Error: selected lossy parameter: %" PRIu32 " is not supported%s. The largest supported value is: %" PRIu32 ".\n",
			    cfg->round, str, max_round_value);
		cfg_invalid++;
	}

#ifdef SKIP_CMP_PAR_CHECK
	return 0;
#endif
	return cfg_invalid;
}


/**
 * @brief check if the ICU buffer parameters are invalid
 *
 * @param cfg	pointer to the compressor configuration
 *
 * @returns 0 if the buffer parameters are valid, otherwise invalid
 */

int cmp_cfg_icu_buffers_is_invalid(const struct cmp_cfg *cfg)
{
	int cfg_invalid = 0;

	if (!cfg)
		return 1;

	if (cfg->input_buf == NULL) {
		debug_print("Error: The data_to_compress buffer for the data to be compressed is NULL.\n");
		cfg_invalid++;
	}

	if (cfg->samples == 0)
		debug_print("Warning: The samples parameter is 0. No data are compressed. This behavior may not be intended.\n");

	if (cfg->icu_output_buf) {
		if (cfg->buffer_length == 0 && cfg->samples != 0) {
			debug_print("Error: The buffer_length is set to 0. There is no space to store the compressed data.\n");
			cfg_invalid++;
		}

		if (raw_mode_is_used(cfg->cmp_mode) && cfg->buffer_length < cfg->samples) {
			debug_print("Error: The compressed_data_len_samples is to small to hold the data form the data_to_compress.\n");
			cfg_invalid++;
		}

		if (cfg->icu_output_buf == cfg->input_buf) {
			debug_print("Error: The compressed_data buffer is the same as the data_to_compress buffer.\n");
			cfg_invalid++;
		}
	}

	if (model_mode_is_used(cfg->cmp_mode)) {
		if (cfg->model_buf == NULL) {
			debug_print("Error: The model_of_data buffer for the model data is NULL.\n");
			cfg_invalid++;
		}

		if (cfg->model_buf == cfg->input_buf) {
			debug_print("Error: The model_of_data buffer is the same as the data_to_compress buffer.\n");
			cfg_invalid++;
		}

		if (cfg->model_buf == cfg->icu_output_buf) {
			debug_print("Error: The model_of_data buffer is the same as the compressed_data buffer.\n");
			cfg_invalid++;
		}

		if (cfg->icu_new_model_buf) {
			if (cfg->icu_new_model_buf == cfg->input_buf) {
				debug_print("Error: The updated_model buffer is the same as the data_to_compress buffer.\n");
				cfg_invalid++;
			}

			if (cfg->icu_new_model_buf == cfg->icu_output_buf) {
				debug_print("Error: The compressed_data buffer is the same as the compressed_data buffer.\n");
				cfg_invalid++;
			}
		}
	}

	return cfg_invalid;
}


/**
 * @brief check if all entries in the max_used_bits structure are in the allowed range
 *
 * @param max_used_bits	pointer to max_used_bits structure to check
 *
 * @returns 0 if all entries are valid, otherwise one or more entries are invalid
 */


int cmp_cfg_icu_max_used_bits_out_of_limit(const struct cmp_max_used_bits *max_used_bits)
{
#define CHECK_MAX_USED_BITS_LIMIT(entry) \
	do { \
		if (max_used_bits->entry > MAX_USED_BITS_SAFE.entry) { \
			debug_print("Error: The " #entry " entry in the max_used_bits structure is too large (actual: %x, max: %x).\n",  max_used_bits->entry, MAX_USED_BITS_SAFE.entry); \
			error++; \
		} \
	} while (0)

	int error = 0;

	if (!max_used_bits) {
		debug_print("Error: The pointer to the max_used_bits structure is NULL.\n");
		return 1;
	}

	CHECK_MAX_USED_BITS_LIMIT(s_exp_flags);
	CHECK_MAX_USED_BITS_LIMIT(s_fx);
	CHECK_MAX_USED_BITS_LIMIT(s_efx);
	CHECK_MAX_USED_BITS_LIMIT(s_ncob);
	CHECK_MAX_USED_BITS_LIMIT(s_ecob);
	CHECK_MAX_USED_BITS_LIMIT(f_fx);
	CHECK_MAX_USED_BITS_LIMIT(f_efx);
	CHECK_MAX_USED_BITS_LIMIT(f_ncob);
	CHECK_MAX_USED_BITS_LIMIT(f_ecob);
	CHECK_MAX_USED_BITS_LIMIT(l_exp_flags);
	CHECK_MAX_USED_BITS_LIMIT(l_fx);
	CHECK_MAX_USED_BITS_LIMIT(l_fx_variance);
	CHECK_MAX_USED_BITS_LIMIT(l_efx);
	CHECK_MAX_USED_BITS_LIMIT(l_ncob);
	CHECK_MAX_USED_BITS_LIMIT(l_ecob);
	CHECK_MAX_USED_BITS_LIMIT(l_cob_variance);
	CHECK_MAX_USED_BITS_LIMIT(nc_imagette);
	CHECK_MAX_USED_BITS_LIMIT(saturated_imagette);
	CHECK_MAX_USED_BITS_LIMIT(nc_offset_mean);
	CHECK_MAX_USED_BITS_LIMIT(nc_offset_variance);
	CHECK_MAX_USED_BITS_LIMIT(nc_background_mean);
	CHECK_MAX_USED_BITS_LIMIT(nc_background_variance);
	CHECK_MAX_USED_BITS_LIMIT(nc_background_outlier_pixels);
	CHECK_MAX_USED_BITS_LIMIT(smearing_mean);
	CHECK_MAX_USED_BITS_LIMIT(smearing_variance_mean);
	CHECK_MAX_USED_BITS_LIMIT(smearing_outlier_pixels);
	CHECK_MAX_USED_BITS_LIMIT(fc_imagette);
	CHECK_MAX_USED_BITS_LIMIT(fc_offset_mean);
	CHECK_MAX_USED_BITS_LIMIT(fc_offset_variance);
	CHECK_MAX_USED_BITS_LIMIT(fc_offset_pixel_in_error);
	CHECK_MAX_USED_BITS_LIMIT(fc_background_mean);
	CHECK_MAX_USED_BITS_LIMIT(fc_background_variance);
	CHECK_MAX_USED_BITS_LIMIT(fc_background_outlier_pixels);

	return error;
}


/**
 * @brief check if the combination of the different compression parameters is invalid
 *
 * @param cmp_par	compression parameter
 * @param spill		spillover threshold parameter
 * @param cmp_mode	compression mode
 * @param data_type	compression data type
 * @param par_name	string describing the use of the compression par. for
 *			debug messages (can be NULL)
 *
 * @returns 0 if the parameter combination is valid, otherwise the combination is invalid
 */

static int cmp_pars_are_invalid(uint32_t cmp_par, uint32_t spill, enum cmp_mode cmp_mode,
				enum cmp_data_type data_type, char *par_name)
{
	int cfg_invalid = 0;
	uint32_t min_golomb_par;
	uint32_t max_golomb_par;
	uint32_t min_spill;
	uint32_t max_spill;

	/* The maximum compression parameter for imagette data are smaller to
	 * fit into the imagette compression entity header */
	if (cmp_imagette_data_type_is_used(data_type)) {
		min_golomb_par = MIN_IMA_GOLOMB_PAR;
		max_golomb_par = MAX_IMA_GOLOMB_PAR;
		min_spill = MIN_IMA_SPILL;
		max_spill = cmp_ima_max_spill(cmp_par);
	} else {
		min_golomb_par = MIN_NON_IMA_GOLOMB_PAR;
		max_golomb_par = MAX_NON_IMA_GOLOMB_PAR;
		min_spill = MIN_NON_IMA_SPILL;
		max_spill = cmp_icu_max_spill(cmp_par);
	}


	switch (cmp_mode) {
	case CMP_MODE_RAW:
		/* no checks needed */
		break;
	case CMP_MODE_DIFF_ZERO:
	case CMP_MODE_DIFF_MULTI:
	case CMP_MODE_MODEL_ZERO:
	case CMP_MODE_MODEL_MULTI:
		if (cmp_par < min_golomb_par || cmp_par > max_golomb_par) {
			debug_print("Error: The selected %s compression parameter: %" PRIu32 " is not supported in the selected compression mode. The compression parameter has to be between [%" PRIu32 ", %" PRIu32 "] in this mode.\n",
				    par_name, cmp_par, min_golomb_par, max_golomb_par);
			cfg_invalid++;
		}
		if (spill < min_spill) {
			debug_print("Error: The selected %s spillover threshold value: %" PRIu32 " is too small. The smallest possible spillover value is: %" PRIu32 ".\n",
				    par_name, spill, min_spill);
			cfg_invalid++;
		}
		if (spill > max_spill) {
			debug_print("Error: The selected %s spillover threshold value: %" PRIu32 " is too large for the selected %s compression parameter: %" PRIu32 ". The largest possible spillover value in the selected compression mode is: %" PRIu32 ".\n",
				    par_name, spill, par_name, cmp_par, max_spill);
			cfg_invalid++;
		}

		break;
	case CMP_MODE_STUFF:
		if (cmp_par > MAX_STUFF_CMP_PAR) {
			debug_print("Error: The selected %s stuff mode compression parameter: %" PRIu32 " is too large. The largest possible value in the selected compression mode is: %u.\n",
				    par_name, cmp_par, MAX_STUFF_CMP_PAR);
			cfg_invalid++;
		}
		break;
	default:
		debug_print("Error: The compression mode is not supported.\n");
		cfg_invalid++;
		break;
	}

#ifdef SKIP_CMP_PAR_CHECK
	return 0;
#endif
	return cfg_invalid;
}


/**
 * @brief check if the imagette specific compression parameters are invalid
 *
 * @param cfg		pointer to a compressor configuration
 * @param opt		check options:
 *			RDCU_CHECK for a imagette RDCU compression check
 *			ICU_CHECK for a imagette ICU compression check
 *
 * @returns 0 if the imagette specific parameters are valid, otherwise invalid
 */

int cmp_cfg_imagette_is_invalid(const struct cmp_cfg *cfg, enum check_opt opt)
{
	int cfg_invalid = 0;
	enum cmp_mode cmp_mode;

	if (!cfg)
		return 1;

	if (!cmp_imagette_data_type_is_used(cfg->data_type)) {
		debug_print("Error: The compression data type is not an imagette compression data type.\n");
		cfg_invalid++;
	}

	/* The RDCU needs valid compression parameters also in RAW_MODE */
	if (opt == RDCU_CHECK && cfg->cmp_mode == CMP_MODE_RAW)
		cmp_mode = CMP_MODE_MODEL_ZERO;
	else
		cmp_mode = cfg->cmp_mode;

	cfg_invalid += cmp_pars_are_invalid(cfg->golomb_par, cfg->spill, cmp_mode,
					    cfg->data_type, "imagette");

	/* for the RDCU the adaptive parameters have to be always valid */
	if (opt == RDCU_CHECK || cmp_ap_imagette_data_type_is_used(cfg->data_type)) {
		cfg_invalid += cmp_pars_are_invalid(cfg->ap1_golomb_par, cfg->ap1_spill,
				cmp_mode, cfg->data_type, "adaptive 1 imagette");
		cfg_invalid += cmp_pars_are_invalid(cfg->ap2_golomb_par, cfg->ap2_spill,
				cmp_mode, cfg->data_type, "adaptive 2 imagette");
	}

	return cfg_invalid;
}


/**
 * @brief get needed compression parameter pairs for a flux/center of brightness
 *	data type
 *
 * @param data_type	a flux/center of brightness data type
 * @param par		pointer to a structure containing flux/COB compression
 *			parameters pairs
 *
 * @returns 0 on success and sets the needed compression parameter pairs in the
 *	par struct, otherwise error
 */

int cmp_cfg_fx_cob_get_need_pars(enum cmp_data_type data_type, struct fx_cob_par *par)
{
	if (!par)
		return -1;

	par->exp_flags = 0;
	par->fx = 0;
	par->ncob = 0;
	par->efx = 0;
	par->ecob = 0;
	par->fx_cob_variance = 0;

	/* flux parameter is needed for every fx_cob data_type */
	par->fx = 1;

	switch (data_type) {
	case DATA_TYPE_S_FX:
		par->exp_flags = 1;
		break;
	case DATA_TYPE_S_FX_EFX:
		par->exp_flags = 1;
		par->efx = 1;
		break;
	case DATA_TYPE_S_FX_NCOB:
		par->exp_flags = 1;
		par->ncob = 1;
		break;
	case DATA_TYPE_S_FX_EFX_NCOB_ECOB:
		par->exp_flags = 1;
		par->ncob = 1;
		par->efx = 1;
		par->ecob = 1;
		break;
	case DATA_TYPE_L_FX:
		par->exp_flags = 1;
		par->fx_cob_variance = 1;
		break;
	case DATA_TYPE_L_FX_EFX:
		par->exp_flags = 1;
		par->efx = 1;
		par->fx_cob_variance = 1;
		break;
	case DATA_TYPE_L_FX_NCOB:
		par->exp_flags = 1;
		par->ncob = 1;
		par->fx_cob_variance = 1;
		break;
	case DATA_TYPE_L_FX_EFX_NCOB_ECOB:
		par->exp_flags = 1;
		par->ncob = 1;
		par->efx = 1;
		par->ecob = 1;
		par->fx_cob_variance = 1;
		break;
	case DATA_TYPE_F_FX:
		break;
	case DATA_TYPE_F_FX_EFX:
		par->efx = 1;
		break;
	case DATA_TYPE_F_FX_NCOB:
		par->ncob = 1;
		break;
	case DATA_TYPE_F_FX_EFX_NCOB_ECOB:
		par->ncob = 1;
		par->efx = 1;
		par->ecob = 1;
		break;
	default:
		return -1;
	}
	return 0;
}


/**
 * @brief check if the flux/center of brightness specific compression parameters
 *	are invalid
 *
 * @param cfg	pointer to the compressor configuration
 *
 * @returns 0 if the flux/center of brightness specific parameters are valid, otherwise invalid
 */

int cmp_cfg_fx_cob_is_invalid(const struct cmp_cfg *cfg)
{
	int cfg_invalid = 0;
	struct fx_cob_par needed_pars;

	if (!cfg)
		return 1;

	if (!cmp_fx_cob_data_type_is_used(cfg->data_type)) {
		debug_print("Error: The compression data type is not a flux/center of brightness compression data type.\n");
		cfg_invalid++;
	}

	cmp_cfg_fx_cob_get_need_pars(cfg->data_type, &needed_pars);

	if (needed_pars.fx) /* this is always true because every flux/center of brightness data type contains a flux parameter */
		cfg_invalid += cmp_pars_are_invalid(cfg->cmp_par_fx, cfg->spill_fx,
						    cfg->cmp_mode, cfg->data_type, "flux");
	if (needed_pars.exp_flags)
		cfg_invalid += cmp_pars_are_invalid(cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
			cfg->cmp_mode, cfg->data_type, "exposure flags");
	if (needed_pars.ncob)
		cfg_invalid += cmp_pars_are_invalid(cfg->cmp_par_ncob, cfg->spill_ncob,
			cfg->cmp_mode, cfg->data_type, "center of brightness");
	if (needed_pars.efx)
		cfg_invalid += cmp_pars_are_invalid(cfg->cmp_par_efx, cfg->spill_efx,
			cfg->cmp_mode, cfg->data_type, "extended flux");
	if (needed_pars.ecob)
		cfg_invalid += cmp_pars_are_invalid(cfg->cmp_par_ecob, cfg->spill_ecob,
			cfg->cmp_mode, cfg->data_type, "extended center of brightness");
	if (needed_pars.fx_cob_variance)
		cfg_invalid += cmp_pars_are_invalid(cfg->cmp_par_fx_cob_variance,
			cfg->spill_fx_cob_variance, cfg->cmp_mode, cfg->data_type, "flux/COB variance");

	return cfg_invalid;
}


/**
 * @brief check if the auxiliary science specific compression parameters are invalid
 *
 * @param cfg	pointer to the compressor configuration
 *
 * @returns 0 if the auxiliary science specific parameters are valid, otherwise
 *	invalid
 * TODO: implemented DATA_TYPE_F_CAM_OFFSET and DATA_TYPE_F_CAM_BACKGROUND
 */

int cmp_cfg_aux_is_invalid(const struct cmp_cfg *cfg)
{
	int cfg_invalid = 0;

	if (!cfg)
		return 1;

	if (!cmp_aux_data_type_is_used(cfg->data_type)) {
		debug_print("Error: The compression data type is not an auxiliary science compression data type.\n");
		cfg_invalid++;
	}

	cfg_invalid += cmp_pars_are_invalid(cfg->cmp_par_mean, cfg->spill_mean,
					    cfg->cmp_mode, cfg->data_type, "mean");
	cfg_invalid += cmp_pars_are_invalid(cfg->cmp_par_variance, cfg->spill_variance,
					    cfg->cmp_mode, cfg->data_type, "variance");

	/* if (cfg->data_type != DATA_TYPE_OFFSET && cfg->data_type != DATA_TYPE_F_CAM_OFFSET) */
	if (cfg->data_type != DATA_TYPE_OFFSET)
		cfg_invalid += cmp_pars_are_invalid(cfg->cmp_par_pixels_error, cfg->spill_pixels_error,
						    cfg->cmp_mode, cfg->data_type, "outlier pixls num");

	return cfg_invalid;
}


/**
 * @brief check if a compression configuration is invalid for a ICU compression
 *
 * @param cfg	pointer to a compressor configuration
 *
 * @returns 0 if the compression configuration is valid, otherwise invalid
 */

int cmp_cfg_icu_is_invalid(const struct cmp_cfg *cfg)
{
	int cfg_invalid = 0;

	if (!cfg)
		return 1;

	cfg_invalid += cmp_cfg_gen_par_is_invalid(cfg, ICU_CHECK);

	cfg_invalid += cmp_cfg_icu_buffers_is_invalid(cfg);

	if (cfg->cmp_mode != CMP_MODE_RAW)
		cfg_invalid += cmp_cfg_icu_max_used_bits_out_of_limit(cfg->max_used_bits);

	if (cmp_imagette_data_type_is_used(cfg->data_type))
		cfg_invalid += cmp_cfg_imagette_is_invalid(cfg, ICU_CHECK);
	else if (cmp_fx_cob_data_type_is_used(cfg->data_type))
		cfg_invalid += cmp_cfg_fx_cob_is_invalid(cfg);
	else if (cmp_aux_data_type_is_used(cfg->data_type))
		cfg_invalid += cmp_cfg_aux_is_invalid(cfg);
	else
		cfg_invalid++;

	return cfg_invalid;
}


/**
 * @brief print the cmp_info structure
 *
 * @param info	pointer to a compressor information contains information of an
 *		executed RDCU compression
 */

void print_cmp_info(const struct cmp_info *info)
{
	if (!info) {
		debug_print("Pointer to the compressor information is NULL.\n");
		return;
	}

	debug_print("cmp_mode_used: %" PRIu32 "\n", info->cmp_mode_used);
	debug_print("spill_used: %" PRIu32 "\n", info->spill_used);
	debug_print("golomb_par_used: %" PRIu32 "\n", info->golomb_par_used);
	debug_print("samples_used: %" PRIu32 "\n", info->samples_used);
	debug_print("cmp_size: %" PRIu32 "\n", info->cmp_size);
	debug_print("ap1_cmp_size: %" PRIu32 "\n", info->ap1_cmp_size);
	debug_print("ap2_cmp_size: %" PRIu32 "\n", info->ap2_cmp_size);
	debug_print("rdcu_new_model_adr_used: 0x%06"PRIX32"\n", info->rdcu_new_model_adr_used);
	debug_print("rdcu_cmp_adr_used: 0x%06"PRIX32"\n", info->rdcu_cmp_adr_used);
	debug_print("model_value_used: %u\n", info->model_value_used);
	debug_print("round_used: %u\n", info->round_used);
	debug_print("cmp_err: %#X\n", info->cmp_err);
}
