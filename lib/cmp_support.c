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


#include "cmp_support.h"
#include "cmp_debug.h"


/**
 * @brief implementation of the logarithm base of floor(log2(x)) for integers
 * @note ilog_2(0) = -1 defined
 *
 * @param x	input parameter
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
 * @returns zero if data_type is invalid; non-zero if data_type is valid
 */

int cmp_data_type_valid(enum cmp_data_type data_type)
{
	if (data_type <= DATA_TYPE_UNKOWN || data_type > DATA_TYPE_F_CAM_OFFSET)
		return 0;

	return 1;
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
 * @brief check if a 1d-differencing mode is selected
 *
 * @param cmp_mode	compression mode
 *
 * @returns 1 when the 1d-differencing mode is used, otherwise 0
 */

int diff_mode_is_used(enum cmp_mode cmp_mode)
{
	if (cmp_mode == CMP_MODE_DIFF_ZERO ||
	    cmp_mode == CMP_MODE_DIFF_MULTI)
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
	default:
		return 0;
	}

}


/**
 * @brief check if the data product data type is supported by the RDCU compressor
 *
 * @param data_type	compression data product types
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
	case DATA_TYPE_S_FX_DFX:
	case DATA_TYPE_S_FX_NCOB:
	case DATA_TYPE_S_FX_DFX_NCOB_ECOB:
	case DATA_TYPE_L_FX:
	case DATA_TYPE_L_FX_DFX:
	case DATA_TYPE_L_FX_NCOB:
	case DATA_TYPE_L_FX_DFX_NCOB_ECOB:
	case DATA_TYPE_F_FX:
	case DATA_TYPE_F_FX_DFX:
	case DATA_TYPE_F_FX_NCOB:
	case DATA_TYPE_F_FX_DFX_NCOB_ECOB:
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
 * @brief method for lossy rounding
 *
 * @param value	the value to round
 * @param round	rounding parameter
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
 * @param value	the value to round back
 * @param round	rounding parameter
 *
 * @return back rounded value
 */

unsigned int round_inv(unsigned int value, unsigned int round)
{
	return value << round;
}


/**
 * @brief implantation of the model update equation
 * @note check before that model_value is not greater than MAX_MODEL_VALUE
 *
 * @param data		data to process
 * @param model		(current) model of the data to process
 * @param model_value	model weighting parameter
 *
 * @returns (new) updated model
 */

unsigned int cmp_up_model(unsigned int data, unsigned int model,
			  unsigned int model_value, unsigned int round)

{
	/* round and round back input because for decompression the accurate
	 * data values are not available
	 */
	data = round_inv(round_fwd(data, round), round);
	/* cast uint64_t to prevent overflow in the multiplication */
	uint64_t weighted_model = (uint64_t)model * model_value;
	uint64_t weighted_data = (uint64_t)data * (MAX_MODEL_VALUE - model_value);
	/* truncation is intended */
	return (unsigned int)((weighted_model + weighted_data) / MAX_MODEL_VALUE);
}


/**
 * @brief get the maximum valid spill threshold value for a given golomb_par
 *
 * @param golomb_par	Golomb parameter
 * @param data_type	compression data type
 *
 * @returns the highest still valid spill threshold value
 */

uint32_t get_max_spill(unsigned int golomb_par, enum cmp_data_type data_type)
{
	const uint32_t LUT_MAX_RDCU[MAX_RDCU_GOLOMB_PAR+1] = { 0, 8, 22, 35, 48,
		60, 72, 84, 96, 107, 118, 129, 140, 151, 162, 173, 184, 194,
		204, 214, 224, 234, 244, 254, 264, 274, 284, 294, 304, 314, 324,
		334, 344, 353, 362, 371, 380, 389, 398, 407, 416, 425, 434, 443,
		452, 461, 470, 479, 488, 497, 506, 515, 524, 533, 542, 551, 560,
		569, 578, 587, 596, 605, 614, 623 };

	if (golomb_par == 0)
		return 0;

	/* the RDCU can only generate 16 bit long code words -> lower max spill needed */
	if (rdcu_supported_data_type_is_used(data_type)) {
		if (golomb_par > MAX_RDCU_GOLOMB_PAR)
			return 0;

		return LUT_MAX_RDCU[golomb_par];
	}

	if (golomb_par > MAX_ICU_GOLOMB_PAR) {
		return 0;
	} else {
		/* the ICU compressor can generate code words with a length of
		 * maximal 32 bits.
		 */
		unsigned int max_cw_bits = 32;
		unsigned int cutoff = (1UL << (ilog_2(golomb_par)+1)) - golomb_par;
		unsigned int max_n_sym_offset = max_cw_bits/2 - 1;

		return (max_cw_bits-1-ilog_2(golomb_par))*golomb_par + cutoff -
			max_n_sym_offset - 1;
	}
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
 *	the lossy rounding parameters are valid for a ICU compression
 *
 * @param cfg	pointer to the compressor configuration
 *
 * @returns 1 if generic compression parameters are valid, otherwise 0
 */

int cmp_cfg_icu_gen_par_is_valid(const struct cmp_cfg *cfg)
{
	int cfg_invalid = 0;

	if (!cmp_data_type_valid(cfg->data_type)) {
		debug_print("Error: selected compression data type is not supported.\n");
		cfg_invalid++;
	}

	if (cfg->cmp_mode > CMP_MODE_STUFF) {
		debug_print("Error: selected cmp_mode: %u is not supported\n.", cfg->cmp_mode);
		cfg_invalid++;
	}

	if (model_mode_is_used(cfg->cmp_mode)) {
		if (cfg->model_value > MAX_MODEL_VALUE) {
			debug_print("Error: selected model_value: %u is invalid. Largest supported value is: %u.\n",
				    cfg->model_value, MAX_MODEL_VALUE);
			cfg_invalid++;
		}
	}

	if (cfg->round > MAX_ICU_ROUND) {
		debug_print("Error: selected lossy parameter: %u is not supported. Largest supported value is: %u.\n",
			    cfg->round, MAX_ICU_ROUND);
		cfg_invalid++;
	}

	if (cfg_invalid)
		return 0;

	return 1;
}


/**
 * @brief check if the buffer parameters are valid
 *
 * @param cfg	pointer to the compressor configuration
 *
 * @returns 1 if the buffer parameters are valid, otherwise 0
 */

int cmp_cfg_icu_buffers_is_valid(const struct cmp_cfg *cfg)
{
	int cfg_invalid = 0;

	if (!cfg)
		return 0;

	if (cfg->input_buf == NULL) {
		debug_print("Error: The data_to_compress buffer for the data to be compressed is NULL.\n");
		cfg_invalid++;
	}

	if (cfg->samples == 0)
		debug_print("Warning: The samples parameter is 0. No data are compressed. This behavior may not be intended.\n");

	if (cfg->icu_output_buf && cfg->buffer_length == 0 && cfg->samples != 0) {
		debug_print("Error: The buffer_length is set to 0. There is no space to store the compressed data.\n");
		cfg_invalid++;
	}

	if (cfg->icu_output_buf == cfg->input_buf) {
		debug_print("Error: The compressed_data buffer is the same as the data_to_compress buffer.\n");
		cfg_invalid++;
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

	if (raw_mode_is_used(cfg->cmp_mode)) {
		if (cfg->buffer_length < cfg->samples) {
			debug_print("Error: The compressed_data_len_samples is to small to hold the data form the data_to_compress.\n");
			cfg_invalid++;
		}
	} else {
		if (cfg->samples < cfg->buffer_length/3)
			debug_print("Warning: The size of the compressed_data buffer is 3 times smaller than the data_to_compress. This is probably unintended.This is probably unintended.\n");
	}

	if (cfg_invalid)
		return 0;

	return 1;
}


/**
 * @brief check if the combination of the different compression parameters is valid
 *
 * @param cmp_par	compression parameter
 * @param spill		spillover threshold parameter
 * @param cmp_mode	compression mode
 * @param data_type	compression data type
 * @param par_name	string describing the use of the compression par. for
 *			debug messages (can be NULL)
 *
 * @returns 1 if the parameter combination is valid, otherwise 0
 */

static int cmp_pars_are_valid(uint32_t cmp_par, uint32_t spill, enum cmp_mode cmp_mode,
			      enum cmp_data_type data_type, char *par_name)
{
	int cfg_invalid = 0;

	if (!par_name)
		par_name = "";

	switch (cmp_mode) {
	case CMP_MODE_RAW:
		/* no checks needed */
		break;
	case CMP_MODE_STUFF:
		if (cmp_par > MAX_STUFF_CMP_PAR) {
			debug_print("Error: The selected %s stuff mode compression parameter: %u is too large, the largest possible value in the selected compression mode is: %u.\n",
				    par_name, cmp_par, MAX_STUFF_CMP_PAR);
			cfg_invalid++;
		}
		break;
	case CMP_MODE_DIFF_ZERO:
	case CMP_MODE_DIFF_MULTI:
	case CMP_MODE_MODEL_ZERO:
	case CMP_MODE_MODEL_MULTI:
		if (cmp_par < MIN_ICU_GOLOMB_PAR ||
		    cmp_par > MAX_ICU_GOLOMB_PAR) {
			debug_print("Error: The selected %s compression parameter: %u is not supported. The compression parameter has to be between [%u, %u].\n",
				    par_name, cmp_par, MIN_ICU_GOLOMB_PAR, MAX_ICU_GOLOMB_PAR);
			cfg_invalid++;
		}
		if (spill < MIN_ICU_SPILL) {
			debug_print("Error: The selected %s spillover threshold value: %u is too small. Smallest possible spillover value is: %u.\n",
				    par_name, spill, MIN_ICU_SPILL);
			cfg_invalid++;
		}
		if (spill > get_max_spill(cmp_par, data_type)) {
			debug_print("Error: The selected %s spillover threshold value: %u is too large for the selected %s compression parameter: %u, the largest possible spillover value in the selected compression mode is: %u.\n",
				    par_name, spill, par_name, cmp_par, get_max_spill(cmp_par, data_type));
			cfg_invalid++;
		}

		break;
	default:
		debug_print("Error: The compression mode is not supported.\n");
		cfg_invalid++;
		break;
	}

	if (cfg_invalid)
		return 0;

	return 1;
}


/**
 * @brief check if the imagette specific compression parameters are valid
 *
 * @param cfg	pointer to the compressor configuration
 *
 * @returns 1 if the imagette specific parameters are valid, otherwise 0
 */

int cmp_cfg_imagette_is_valid(const struct cmp_cfg *cfg)
{
	int cfg_invalid = 0;

	if (!cfg)
		return 0;

	if (!cmp_imagette_data_type_is_used(cfg->data_type)) {
		debug_print("Error: The compression data type is not an imagette compression data type.!\n");
		cfg_invalid++;
	}

	if (!cmp_pars_are_valid(cfg->golomb_par, cfg->spill, cfg->cmp_mode,
				cfg->data_type, "imagette"))
		cfg_invalid++;

	if (cmp_ap_imagette_data_type_is_used(cfg->data_type)) {
		if (!cmp_pars_are_valid(cfg->ap1_golomb_par, cfg->ap1_spill,
					cfg->cmp_mode, cfg->data_type, "adaptive 1 imagette"))
			cfg_invalid++;
		if (!cmp_pars_are_valid(cfg->ap2_golomb_par, cfg->ap2_spill,
					cfg->cmp_mode, cfg->data_type, "adaptive 2 imagette"))
			cfg_invalid++;
	}

	if (cfg_invalid)
		return 0;

	return 1;
}


/**
 * @brief check if the flux/center of brightness specific compression parameters are valid
 *
 * @param cfg	pointer to the compressor configuration
 *
 * @returns 1 if the flux/center of brightness specific parameters are valid, otherwise 0
 */

int cmp_cfg_fx_cob_is_valid(const struct cmp_cfg *cfg)
{
	int cfg_invalid = 0;
	int check_exp_flags = 0, check_ncob = 0, check_efx = 0, check_ecob = 0, check_var = 0;

	if (!cfg)
		return 0;

	if (!cmp_fx_cob_data_type_is_used(cfg->data_type)) {
		debug_print("Error: The compression data type is not a flux/center of brightness compression data type.!\n");
		cfg_invalid++;
	}
	/* flux parameter is needed for every fx_cob data_type */
	if (!cmp_pars_are_valid(cfg->cmp_par_fx, cfg->spill_fx, cfg->cmp_mode, cfg->data_type, "flux"))
		cfg_invalid++;

	switch (cfg->data_type) {
	case DATA_TYPE_S_FX:
		check_exp_flags = 1;
		break;
	case DATA_TYPE_S_FX_DFX:
		check_exp_flags = 1;
		check_efx = 1;
		break;
	case DATA_TYPE_S_FX_NCOB:
		check_exp_flags = 1;
		check_ncob = 1;
		break;
	case DATA_TYPE_S_FX_DFX_NCOB_ECOB:
		check_exp_flags = 1;
		check_ncob = 1;
		check_efx = 1;
		check_ecob = 1;
		break;
	case DATA_TYPE_L_FX:
		check_exp_flags = 1;
		check_var = 1;
		break;
	case DATA_TYPE_L_FX_DFX:
		check_exp_flags = 1;
		check_efx = 1;
		check_var = 1;
		break;
	case DATA_TYPE_L_FX_NCOB:
		check_exp_flags = 1;
		check_ncob = 1;
		check_var = 1;
		break;
	case DATA_TYPE_L_FX_DFX_NCOB_ECOB:
		check_exp_flags = 1;
		check_ncob = 1;
		check_efx = 1;
		check_ecob = 1;
		check_var = 1;
		break;
	case DATA_TYPE_F_FX:
		break;
	case DATA_TYPE_F_FX_DFX:
		check_efx = 1;
		break;
	case DATA_TYPE_F_FX_NCOB:
		check_ncob = 1;
		break;
	case DATA_TYPE_F_FX_DFX_NCOB_ECOB:
		check_ncob = 1;
		check_efx = 1;
		check_ecob = 1;
		break;
	default:
		cfg_invalid++;
		break;
	}

	if (check_exp_flags && !cmp_pars_are_valid(cfg->cmp_par_exp_flags, cfg->spill_exp_flags, cfg->cmp_mode, cfg->data_type, "exposure flags"))
		cfg_invalid++;
	if (check_ncob && !cmp_pars_are_valid(cfg->cmp_par_ncob, cfg->spill_ncob, cfg->cmp_mode, cfg->data_type, "center of brightness"))
		cfg_invalid++;
	if (check_efx && !cmp_pars_are_valid(cfg->cmp_par_efx, cfg->spill_efx, cfg->cmp_mode, cfg->data_type, "extended flux"))
		cfg_invalid++;
	if (check_ecob && !cmp_pars_are_valid(cfg->cmp_par_ecob, cfg->spill_ecob, cfg->cmp_mode, cfg->data_type, "extended center of brightness"))
		cfg_invalid++;
	if (check_var && !cmp_pars_are_valid(cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance, cfg->cmp_mode, cfg->data_type, "flux COB varianc"))
		cfg_invalid++;

	if (cfg_invalid)
		return 0;

	return 1;
}


/**
 * @brief check if the auxiliary science specific compression parameters are valid
 *
 * @param cfg	pointer to the compressor configuration
 *
 * @returns 1 if the auxiliary science specific parameters are valid, otherwise 0
 */

int cmp_cfg_aux_is_valid(const struct cmp_cfg *cfg)
{
	int cfg_invalid = 0;

	if (!cfg)
		return 0;

	if (!cmp_aux_data_type_is_used(cfg->data_type)) {
		debug_print("Error: The compression data type is not an auxiliary science compression data type.!\n");
		cfg_invalid++;
	}

	if (!cmp_pars_are_valid(cfg->cmp_par_mean, cfg->spill_mean, cfg->cmp_mode, cfg->data_type, "mean"))
		cfg_invalid++;
	if (!cmp_pars_are_valid(cfg->cmp_par_variance, cfg->spill_variance, cfg->cmp_mode, cfg->data_type, "variance"))
		cfg_invalid++;
	if (cfg->data_type != DATA_TYPE_OFFSET && cfg->data_type != DATA_TYPE_F_CAM_OFFSET)
		if (!cmp_pars_are_valid(cfg->cmp_par_pixels_error, cfg->spill_pixels_error, cfg->cmp_mode, cfg->data_type, "outlier pixls num"))
			cfg_invalid++;

	if (cfg_invalid)
		return 0;

	return 1;
}


/**
 * @brief check if a compression configuration is valid
 *
 * @param cfg	pointer to the compressor configuration
 *
 * @returns 1 if the compression configuration is valid, otherwise 0
 */

int cmp_cfg_is_valid(const struct cmp_cfg *cfg)
{
	int cfg_invalid = 0;

	if (!cfg)
		return 0;

	if (!cmp_cfg_icu_gen_par_is_valid(cfg))
		cfg_invalid++;

	if (!cmp_cfg_icu_buffers_is_valid(cfg))
		cfg_invalid++;

	if (cmp_imagette_data_type_is_used(cfg->data_type)) {
		if (!cmp_cfg_imagette_is_valid(cfg))
			cfg_invalid++;
	} else if (cmp_fx_cob_data_type_is_used(cfg->data_type)) {
		if (!cmp_cfg_fx_cob_is_valid(cfg))
			cfg_invalid++;
	} else if (cmp_aux_data_type_is_used(cfg->data_type)) {
		if (!cmp_cfg_aux_is_valid(cfg))
			cfg_invalid++;
	} else {
		cfg_invalid++;
	}

	if (cfg_invalid)
		return 0;

	return 1;
}


/**
 * @brief print the cmp_cfg structure
 *
 * @param cfg	compressor configuration contains all parameters required for
 *		compression
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
		for (i = 0; i < cfg->samples; i++)
			printf(" %04X", ((uint16_t *)cfg->input_buf)[i]);
		printf("\n");
	}
	printf("rdcu_data_adr: 0x%06lX\n", cfg->rdcu_data_adr);
	printf("model_buf: %p\n", (void *)cfg->model_buf);
	if (cfg->model_buf != NULL) {
		printf("model data:");
		for (i = 0; i < cfg->samples; i++)
			printf(" %04X", ((uint16_t *)cfg->model_buf)[i]);
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
