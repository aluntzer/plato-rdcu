/**
 * @file   cmp_rdcu.c
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
 * @brief hardware compressor control library
 * @see Data Compression User Manual PLATO-UVIE-PL-UM-0001
 *
 * To compress data, first create a compression configuration with the
 * rdcu_cfg_create() function.
 * Then set the different data buffers with the data to compressed, the model
 * data and the RDCU SRAM addresses with the rdcu_cfg_buffers() function.
 * Then set the imagette compression parameters with the rdcu_cfg_imagette()
 * function.
 * Finally, you can compress the data with the RDCU using the
 * rdcu_compress_data() function.
 */

#include <stdint.h>
#include <stdio.h>

#include "rdcu_cmd.h"
#include "cmp_support.h"
#include "cmp_data_types.h"
#include "rdcu_ctrl.h"
#include "rdcu_rmap.h"
#include "cmp_debug.h"


#define IMA_SAM2BYT                                                            \
	2 /* imagette sample to byte conversion factor; one imagette samples has 16 bits (2 bytes) */

#define RDCU_INTR_SIG_ENA 1 /* RDCU interrupt signal enabled */
#define RDCU_INTR_SIG_DIS 0 /* RDCU interrupt signal disable */
#define RDCU_INTR_SIG_DEFAULT RDCU_INTR_SIG_ENA /* default start value for RDCU interrupt signal */


/* RDCU interrupt signal status */
static int interrupt_signal_enabled = RDCU_INTR_SIG_DEFAULT;


/**
 * @brief save repeating 3 lines of code...
 *
 * @note This function depends on the SpW implantation and must be adjusted to it.
 *
 * @note prints abort message if pending status is non-zero after 10 retries
 */

static void sync(void)
{
	int cnt = 0;
	printf("syncing...");
	while (rdcu_rmap_sync_status()) {
		printf("pending: %d\n", rdcu_rmap_sync_status());

		if (cnt++ > 10) {
			printf("aborting; de");
			break;
		}

	}
	printf("synced\n");
}


/**
 * @brief check if the compression data product type, compression mode, model
 *	value and the lossy rounding parameters are valid for a RDCU compression
 *
 * @param cfg	pointer to a compression configuration containing the compression
 *	data product type, compression mode, model value and the rounding parameters
 *
 * @returns 0 if the compression data type, compression mode, model value and
 *	the lossy rounding parameters are valid for a RDCU compression, non-zero
 *	if parameters are invalid
 */

static int rdcu_cfg_gen_par_is_invalid(const struct cmp_cfg *cfg)
{
	int cfg_invalid = 0;

	if (!cfg)
		return -1;

	if (!cmp_imagette_data_type_is_used(cfg->data_type)) {
		debug_print("Error: The selected compression data type is not supported for RDCU compression");
		cfg_invalid++;
	}

	if (cfg->cmp_mode > MAX_RDCU_CMP_MODE) {
		debug_print("Error: selected cmp_mode: %u is not supported. Largest supported mode is: %u.\n",
			    cfg->cmp_mode, MAX_RDCU_CMP_MODE);
		cfg_invalid++;
	}

	if (cfg->model_value > MAX_MODEL_VALUE) {
		debug_print("Error: selected model_value: %u is invalid. Largest supported value is: %u.\n",
			    cfg->model_value, MAX_MODEL_VALUE);
		cfg_invalid++;
	}

	if (cfg->round > MAX_RDCU_ROUND) {
		debug_print("Error: selected round parameter: %u is not supported. Largest supported value is: %u.\n",
			    cfg->round, MAX_RDCU_ROUND);
		cfg_invalid++;
	}

#ifdef SKIP_CMP_PAR_CHECK
	return 0;
#endif

	return -cfg_invalid;
}


/**
 * @brief create a RDCU compression configuration
 *
 * @param data_type	compression data product types
 * @param cmp_mode	compression mode
 * @param model_value	model weighting parameter (only need for model compression mode)
 * @param lossy_par	lossy rounding parameter (use CMP_LOSSLESS for lossless compression)
 *
 * @returns compression configuration containing the chosen parameters;
 *	on error the data_type record is set to DATA_TYPE_UNKOWN
 */

struct cmp_cfg rdcu_cfg_create(enum cmp_data_type data_type, enum cmp_mode cmp_mode,
			       uint32_t model_value, uint32_t lossy_par)
{
	struct cmp_cfg cfg = {0};

	cfg.data_type = data_type;
	cfg.cmp_mode = cmp_mode;
	cfg.model_value = model_value;
	cfg.round = lossy_par;

	if (rdcu_cfg_gen_par_is_invalid(&cfg))
		cfg.data_type = DATA_TYPE_UNKOWN;

	return cfg;
}

/**
 * @brief check if a buffer is in inside the RDCU SRAM
 *
 * @param addr	start address of the buffer
 * @param size	length of the buffer in bytes
 *
 * @returns 1 if buffer in inside the RDCU SRAM, 0 when the buffer is outside
 */

static int in_sram_range(uint32_t addr, uint32_t size)
{
	if (addr > RDCU_SRAM_END)
		return 0;

	if (size > RDCU_SRAM_SIZE)
		return 0;

	if (addr + size > RDCU_SRAM_END)
		return 0;

	return 1;
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
			debug_print("rdcu_buffer_length is smaller than samples parameter. There is not enough space to copy the data in RAW mode.\n");
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

	if (!in_sram_range(cfg->rdcu_data_adr, cfg->samples * IMA_SAM2BYT)) {
		debug_print("Error: The RDCU data to compress buffer is outside the RDCU SRAM address space.\n");
		cfg_invalid++;
	}

	if (!in_sram_range(cfg->rdcu_buffer_adr, cfg->buffer_length * IMA_SAM2BYT)) {
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
			debug_print("Error: The model buffer (model_buf) and the data to be compressed (input_buf) are equal.");
			cfg_invalid++;
		}

		if (cfg->rdcu_model_adr & 0x3) {
			debug_print("Error: The RDCU model start address is not 4-Byte aligned.\n");
			cfg_invalid++;
		}

		if (!in_sram_range(cfg->rdcu_model_adr, cfg->samples * IMA_SAM2BYT)) {
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

			if (!in_sram_range(cfg->rdcu_new_model_adr,
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

	if (cfg->icu_new_model_buf)
		debug_print("Warning: ICU updated model buffer is set. This buffer is not used for an RDCU compression.\n");

	if (cfg->icu_output_buf)
		debug_print("Warning: ICU compressed data buffer is set. This buffer is not used for an RDCU compression.\n");

#ifdef SKIP_CMP_PAR_CHECK
	return 0;
#endif
	return -cfg_invalid;
}


/**
 *@brief setup of the different data buffers for an RDCU compression
 *
 * @param cfg			pointer to a compression configuration (created
 *				with the rdcu_cfg_create() function)
 * @param data_to_compress	pointer to the data to be compressed (if NULL no
 *				data transfer to the RDCU)
 * @param data_samples		length of the data to be compressed measured in
 *				16-bit data samples (ignoring the multi entity header)
 * @param model_of_data		pointer to model data buffer (only needed for
 *				model compression mode, if NULL no model data
 *				transfer to the RDCU)
 * @param rdcu_data_adr		RDCU data to compress start address, the first
 *				data address in the RDCU SRAM
 * @param rdcu_model_adr	RDCU model start address, the first model address
 *				in the RDCU SRAM (only need for model compression mode)
 * @param rdcu_new_model_adr	RDCU new/updated model start address(can be the
 *				by the same as rdcu_model_adr for in-place model update)
 * @param rdcu_buffer_adr	RDCU compressed data start address, the first
 *				output data address in the RDCU SRAM
 * @param rdcu_buffer_lenght	length of the RDCU compressed data SRAM buffer
 *				in number of 16-bit samples
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

	if (rdcu_cfg_buffers_is_invalid(cfg))
		return -1;

	return 0;
}


/**
 * @brief check if the Golomb and spillover threshold parameter combination is
 *	invalid for a RDCU compression
 * @note also checked the adaptive Golomb and spillover threshold parameter combinations
 *
 * @param cfg	a pointer to a compression configuration
 *
 * @returns 0 if (adaptive) Golomb spill threshold parameter combinations are
 *	valid, otherwise the configuration is invalid
 */

static int rdcu_cfg_imagette_is_invalid(const struct cmp_cfg *cfg)
{
	int cfg_invalid = 0;

	if (cfg->golomb_par < MIN_RDCU_GOLOMB_PAR ||
	    cfg->golomb_par > MAX_RDCU_GOLOMB_PAR) {
		debug_print("Error: The selected Golomb parameter: %u is not supported. The Golomb parameter has to  be between [%u, %u].\n",
			    cfg->golomb_par, MIN_RDCU_GOLOMB_PAR, MAX_RDCU_GOLOMB_PAR);
		cfg_invalid++;
	}

	if (cfg->ap1_golomb_par < MIN_RDCU_GOLOMB_PAR ||
	    cfg->ap1_golomb_par > MAX_RDCU_GOLOMB_PAR) {
		debug_print("Error: The selected adaptive 1 Golomb parameter: %u is not supported. The Golomb parameter has to  be between [%u, %u].\n",
			    cfg->ap1_golomb_par, MIN_RDCU_GOLOMB_PAR, MAX_RDCU_GOLOMB_PAR);
		cfg_invalid++;
	}

	if (cfg->ap2_golomb_par < MIN_RDCU_GOLOMB_PAR ||
	    cfg->ap2_golomb_par > MAX_RDCU_GOLOMB_PAR) {
		debug_print("Error: The selected adaptive 2 Golomb parameter: %u is not supported. The Golomb parameter has to be between [%u, %u].\n",
			    cfg->ap2_golomb_par, MIN_RDCU_GOLOMB_PAR, MAX_RDCU_GOLOMB_PAR);
		cfg_invalid++;
	}

	if (cfg->spill < MIN_RDCU_SPILL) {
		debug_print("Error: The selected spillover threshold value: %u is too small. Smallest possible spillover value is: %u.\n",
			    cfg->spill, MIN_RDCU_SPILL);
		cfg_invalid++;
	}

	if (cfg->spill > get_max_spill(cfg->golomb_par, cfg->data_type)) {
		debug_print("Error: The selected spillover threshold value: %u is too large for the selected Golomb parameter: %u, the largest possible spillover value is: %u.\n",
			    cfg->spill, cfg->golomb_par, get_max_spill(cfg->golomb_par, cfg->data_type));
		cfg_invalid++;
	}

	if (cfg->ap1_spill < MIN_RDCU_SPILL) {
		debug_print("Error: The selected adaptive 1 spillover threshold value: %u is too small. Smallest possible spillover value is: %u.\n",
			    cfg->ap1_spill, MIN_RDCU_SPILL);
		cfg_invalid++;
	}

	if (cfg->ap1_spill > get_max_spill(cfg->ap1_golomb_par, cfg->data_type)) {
		debug_print("Error: The selected adaptive 1 spillover threshold value: %u is too large for the selected adaptive 1 Golomb parameter: %u, the largest possible adaptive 1 spillover value is: %u.\n",
			    cfg->ap1_spill, cfg->ap1_golomb_par, get_max_spill(cfg->ap1_golomb_par, cfg->data_type));
		cfg_invalid++;
	}

	if (cfg->ap2_spill < MIN_RDCU_SPILL) {
		debug_print("Error: The selected adaptive 2 spillover threshold value: %u is too small. Smallest possible spillover value is: %u.\n",
			    cfg->ap2_spill, MIN_RDCU_SPILL);
		cfg_invalid++;
	}

	if (cfg->ap2_spill > get_max_spill(cfg->ap2_golomb_par, cfg->data_type)) {
		debug_print("Error: The selected adaptive 2 spillover threshold value: %u is too large for the selected adaptive 2 Golomb parameter: %u, the largest possible adaptive 2 spillover value is: %u.\n",
			    cfg->ap2_spill, cfg->ap2_golomb_par, get_max_spill(cfg->ap2_golomb_par, cfg->data_type));
		cfg_invalid++;
	}

#ifdef SKIP_CMP_PAR_CHECK
	return 0;
#endif

	return -cfg_invalid;
}


/**
 * @brief set up the configuration parameters for an RDCU imagette compression
 *
 * @param cfg			pointer to a compression configuration (created
 *				with the rdcu_cfg_create() function)
 * @param golomb_par		imagette compression parameter (Golomb parameter)
 * @param spillover_par		imagette spillover threshold parameter
 * @param ap1_golomb_par	adaptive 1 imagette compression parameter (ap1_golomb parameter)
 * @param ap1_spillover_par	adaptive 1 imagette spillover threshold parameter
 * @param ap2_golomb_par	adaptive 2 imagette compression parameter (ap2_golomb parameter)
 * @param ap2_spillover_par	adaptive 1 imagette spillover threshold parameter
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

	if (rdcu_cfg_imagette_is_invalid(cfg))
		return -1;

	return 0;
}


/**
 * @brief check if the compressor configuration is invalid for a RDCU compression,
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
		debug_print("Warning: The data to compress buffer is set to NULL. No data will be transferred to the rdcu_data_adr in the RDCU-SRAM.\n");

	if (model_mode_is_used(cfg->cmp_mode)) {
		if (!cfg->model_buf)
			debug_print("Warning: The model buffer is set to NULL. No model data will be transferred to the rdcu_model_adr in the RDCU-SRAM.\n");
	}

	if (cfg->input_buf && cfg->samples == 0)
		debug_print("Warning: The samples parameter is set to 0. No data will be compressed.\n");

	if (cfg->buffer_length == 0) {
		debug_print("Error: The buffer_length is set to 0. There is no place to store the compressed data.\n");
		cfg_invalid++;
	}

	if (rdcu_cfg_gen_par_is_invalid(cfg))
		cfg_invalid++;
	if (rdcu_cfg_buffers_is_invalid(cfg))
		cfg_invalid++;
	if (rdcu_cfg_imagette_is_invalid(cfg))
		cfg_invalid++;

	return -cfg_invalid;
}


/**
 * @brief set up RDCU compression register
 *
 * @param cfg  pointer to a compression configuration contains all parameters
 *	required for compression
 *
 * @returns 0 on success, error otherwise
 */

int rdcu_set_compression_register(const struct cmp_cfg *cfg)
{
	if (rdcu_cmp_cfg_is_invalid(cfg))
		return -1;

	/* first, set compression parameters in local mirror registers */
	if (rdcu_set_compression_mode(cfg->cmp_mode))
		return -1;
	if (rdcu_set_golomb_param(cfg->golomb_par))
		return -1;
	if (rdcu_set_spillover_threshold(cfg->spill))
		return -1;
	if (rdcu_set_weighting_param(cfg->model_value))
		return -1;
	if (rdcu_set_noise_bits_rounded(cfg->round))
		return -1;

	if (rdcu_set_adaptive_1_golomb_param(cfg->ap1_golomb_par))
		return -1;
	if (rdcu_set_adaptive_1_spillover_threshold(cfg->ap1_spill))
		return -1;

	if (rdcu_set_adaptive_2_golomb_param(cfg->ap2_golomb_par))
		return -1;
	if (rdcu_set_adaptive_2_spillover_threshold(cfg->ap2_spill))
		return -1;

	if (rdcu_set_data_start_addr(cfg->rdcu_data_adr))
		return -1;
	if (rdcu_set_model_start_addr(cfg->rdcu_model_adr))
		return -1;
	if (rdcu_set_num_samples(cfg->samples))
		return -1;
	if (rdcu_set_new_model_start_addr(cfg->rdcu_new_model_adr))
		return -1;

	if (rdcu_set_compr_data_buf_start_addr(cfg->rdcu_buffer_adr))
		return -1;
	if (rdcu_set_compr_data_buf_len(cfg->buffer_length))
		return -1;

	/* now sync the configuration registers to the RDCU... */
	if (rdcu_sync_compressor_param1())
		return -1;
	if (rdcu_sync_compressor_param2())
		return -1;
	if (rdcu_sync_adaptive_param1())
		return -1;
	if (rdcu_sync_adaptive_param2())
		return -1;
	if (rdcu_sync_data_start_addr())
		return -1;
	if (rdcu_sync_model_start_addr())
		return -1;
	if (rdcu_sync_num_samples())
		return -1;
	if (rdcu_sync_new_model_start_addr())
		return -1;
	if (rdcu_sync_compr_data_buf_start_addr())
		return -1;
	if (rdcu_sync_compr_data_buf_len())
		return -1;

	return 0;
}


/**
 * @brief start the RDCU data compressor
 *
 * @returns 0 on success, error otherwise
 */

int rdcu_start_compression(void)
{
	if (interrupt_signal_enabled) {
		/* enable the interrupt signal to the ICU */
		rdcu_set_rdcu_interrupt();
	} else {
		/* disable the interrupt signal to the ICU */
		rdcu_clear_rdcu_interrupt();
	}

	/* start the compression */
	rdcu_set_data_compr_start();
	if (rdcu_sync_compr_ctrl())
		return -1;
	sync();

	/* clear local bit immediately, this is a write-only register.
	 * we would not want to restart compression by accidentally calling
	 * rdcu_sync_compr_ctrl() again
	 */
	rdcu_clear_data_compr_start();

	return 0;
}


/**
 * @brief compressing data with the help of the RDCU hardware compressor
 *
 * @param cfg  configuration contains all parameters required for compression
 *
 * @note when using the 1d-differencing mode or the raw mode (cmp_mode = 0,2,4),
 *      the model parameters (model_value, model_buf, rdcu_model_adr) are ignored
 * @note the validity of the cfg structure is checked before the compression is
 *	 started
 *
 * @returns 0 on success, error otherwise
 */

int rdcu_compress_data(const struct cmp_cfg *cfg)
{
	if (!cfg)
		return -1;

	if (rdcu_set_compression_register(cfg))
		return -1;

	if (cfg->input_buf != NULL) {
		/* round up needed size must be a multiple of 4 bytes */
		uint32_t size = (cfg->samples * 2 + 3) & ~3U;
		/* now set the data in the local mirror... */
		if (rdcu_write_sram_16(cfg->input_buf, cfg->rdcu_data_adr,
				       cfg->samples * 2) < 0)
			return -1;
		if (rdcu_sync_mirror_to_sram(cfg->rdcu_data_adr, size,
					     rdcu_get_data_mtu()))
			return -1;
	}
	/*...and the model when needed */
	if (cfg->model_buf != NULL) {
		/* set model only when model mode is used */
		if (model_mode_is_used(cfg->cmp_mode)) {
			/* round up needed size must be a multiple of 4 bytes */
			uint32_t size = (cfg->samples * 2 + 3) & ~3U;
			/* set the model in the local mirror... */
			if (rdcu_write_sram_16(cfg->model_buf,
					       cfg->rdcu_model_adr,
					       cfg->samples * 2) < 0)
				return -1;
			if (rdcu_sync_mirror_to_sram(cfg->rdcu_model_adr, size,
						     rdcu_get_data_mtu()))
				return -1;
		}
	}

	/* ...and wait for completion */
	sync();

	/* start the compression */
	if (rdcu_start_compression())
		return -1;

	return 0;
}


/**
 * @brief read out the status register of the RDCU compressor
 *
 * @param status  compressor status contains the stats of the HW compressor
 *
 * @note access to the status registers is also possible during compression
 *
 * @returns 0 on success, error otherwise
 */

int rdcu_read_cmp_status(struct cmp_status *status)
{

	if (rdcu_sync_compr_status())
		return -1;
	sync();

	if (status) {
		status->data_valid = (uint8_t)rdcu_get_compr_status_valid();
		status->cmp_ready = (uint8_t)rdcu_get_data_compr_ready();
		status->cmp_interrupted = (uint8_t)rdcu_get_data_compr_interrupted();
		status->cmp_active = (uint8_t)rdcu_get_data_compr_active();
		status->rdcu_interrupt_en = (uint8_t)rdcu_get_rdcu_interrupt_enabled();
	}
	return 0;
}


/**
 * @brief read out the metadata of a RDCU compression
 *
 * @param info  compression information contains the metadata of a compression
 *
 * @note the compression information registers cannot be accessed during a
 *	 compression
 *
 * @returns 0 on success, error otherwise
 */

int rdcu_read_cmp_info(struct cmp_info *info)
{
	/* read out the compressor information register*/
	if (rdcu_sync_used_param1())
		return -1;
	if (rdcu_sync_used_param2())
		return -1;
	if (rdcu_sync_compr_data_start_addr())
		return -1;
	if (rdcu_sync_compr_data_size())
		return -1;
	if (rdcu_sync_compr_data_adaptive_1_size())
		return -1;
	if (rdcu_sync_compr_data_adaptive_2_size())
		return -1;
	if (rdcu_sync_compr_error())
		return -1;
	if (rdcu_sync_new_model_addr_used())
		return -1;
	if (rdcu_sync_samples_used())
		return -1;

	sync();

	if (info) {
		/* put the data in the cmp_info structure */
		info->cmp_mode_used = rdcu_get_compression_mode();
		info->golomb_par_used = rdcu_get_golomb_param();
		info->spill_used = rdcu_get_spillover_threshold();
		info->model_value_used = rdcu_get_weighting_param();
		info->round_used = rdcu_get_noise_bits_rounded();
		info->rdcu_new_model_adr_used = rdcu_get_new_model_addr_used();
		info->samples_used = rdcu_get_samples_used();
		info->rdcu_cmp_adr_used = rdcu_get_compr_data_start_addr();
		info->cmp_size = rdcu_get_compr_data_size();
		info->ap1_cmp_size = rdcu_get_compr_data_adaptive_1_size();
		info->ap2_cmp_size = rdcu_get_compr_data_adaptive_2_size();
		info->cmp_err = rdcu_get_compr_error();
	}
	return 0;
}


/**
 * @brief read the compressed bitstream from the RDCU SRAM
 *
 * @param info  compression information contains the metadata of a compression
 *
 * @param output_buf  the buffer to store the bitstream (if NULL, the required
 *		      size is returned)
 *
 * @returns the number of bytes read, < 0 on error
 */

int rdcu_read_cmp_bitstream(const struct cmp_info *info, void *output_buf)
{
	uint32_t s;

	if (info == NULL)
		return -1;

	/* calculate the need bytes for the bitstream */
	s = cmp_bit_to_4byte(info->cmp_size);

	if (output_buf == NULL)
		return (int)s;

	if (rdcu_sync_sram_to_mirror(info->rdcu_cmp_adr_used, s,
				     rdcu_get_data_mtu()))
		return -1;

	/* wait for it */
	sync();

	return rdcu_read_sram(output_buf, info->rdcu_cmp_adr_used, s);
}


/**
 * @brief read the model from the RDCU SRAM
 *
 * @param info  compression information contains the metadata of a compression
 *
 * @param model_buf  the buffer to store the model (if NULL, the required size
 *		     is returned)
 *
 * @returns the number of bytes read, < 0 on error
 */

int rdcu_read_model(const struct cmp_info *info, void *model_buf)
{
	uint32_t s;

	if (info == NULL)
		return -1;

	/* calculate the need bytes for the model */
	s = info->samples_used * IMA_SAM2BYT;

	if (model_buf == NULL)
		return (int)s;

	if (rdcu_sync_sram_to_mirror(info->rdcu_new_model_adr_used, (s+3) & ~3U,
				     rdcu_get_data_mtu()))
		return -1;

	/* wait for it */
	sync();

	return rdcu_read_sram(model_buf, info->rdcu_new_model_adr_used, s);
}


/**
 * @brief interrupt a data compression
 *
 * @returns 0 on success, error otherwise
 */

int rdcu_interrupt_compression(void)
{
	/* interrupt a compression */
	rdcu_set_data_compr_interrupt();
	if (rdcu_sync_compr_ctrl())
		return -1;
	sync();

	/* clear local bit immediately, this is a write-only register.
	 * we would not want to restart compression by accidentially calling
	 * rdcu_sync_compr_ctrl() again
	 */
	rdcu_clear_data_compr_interrupt();

	return 0;
}


/**
 * @brief enable the RDCU to signal a finished compression with an interrupt signal
 */

void rdcu_enable_interrput_signal(void)
{
	interrupt_signal_enabled = RDCU_INTR_SIG_ENA;
}


/**
 * @brief deactivated the RDCU interrupt signal
 */

void rdcu_disable_interrput_signal(void)
{
	interrupt_signal_enabled = RDCU_INTR_SIG_DIS;
}


/**
 * @brief compressing data with the help of the RDCU hardware compressor; read
 *	data from the last compression before starting compression
 *
 * @param cfg	     configuration contains all parameters required for compression
 * @param last_info  compression information of last compression run
 *
 * @note when using the 1d-differencing mode or the raw mode (cmp_mode = 0,2,4),
 *      the model parameters (model_value, model_buf, rdcu_model_adr) are ignored
 * @note the overlapping of the different rdcu buffers is not checked
 * @note the validity of the cfg structure is checked before the compression is
 *	 started
 *
 * @returns 0 on success, error otherwise
 */

int rdcu_compress_data_parallel(const struct cmp_cfg *cfg,
				const struct cmp_info *last_info)
{
	uint32_t samples_4byte;

	if (!cfg)
		return -1;

	if (!last_info)
		return rdcu_compress_data(cfg);

	if (last_info->cmp_err)
		return -1;

	rdcu_set_compression_register(cfg);

	/* round up needed size must be a multiple of 4 bytes */
	samples_4byte = (cfg->samples * IMA_SAM2BYT + 3) & ~3U;

	if (cfg->input_buf != NULL) {
		uint32_t cmp_size_4byte;

		/* now set the data in the local mirror... */
		if (rdcu_write_sram_16(cfg->input_buf, cfg->rdcu_data_adr,
				       cfg->samples * IMA_SAM2BYT) < 0)
			return -1;

		/* calculate the need bytes for the bitstream */
		cmp_size_4byte = ((last_info->cmp_size >> 3) + 3) & ~0x3U;

		/* parallel read compressed data and write input data from sram
		 * to mirror*/
		if (rdcu_sync_sram_mirror_parallel(last_info->rdcu_cmp_adr_used,
				cmp_size_4byte, cfg->rdcu_data_adr, samples_4byte,
				rdcu_get_data_mtu()))
			return -1;
		/* wait for it */
		sync();
		if (cfg->icu_output_buf) {
			if (rdcu_read_sram(cfg->icu_output_buf,
					   last_info->rdcu_cmp_adr_used,
					   cmp_size_4byte))
				return -1;
		}
	} else {
		debug_print("Warning: input_buf = NULL; input_buf is not written to the sram and compressed data is not read from the SRAM\n");
	}

	/* read model and write model in parallel */
	if (cfg->model_buf && model_mode_is_used(cfg->cmp_mode) && model_mode_is_used(last_info->cmp_mode_used)) {
		uint32_t new_model_size_4byte;

		/* set the model in the local mirror... */
		if (rdcu_write_sram_16(cfg->model_buf, cfg->rdcu_model_adr,
				       cfg->samples * IMA_SAM2BYT) < 0)
			return -1;

		new_model_size_4byte = last_info->samples_used * IMA_SAM2BYT;
		if (rdcu_sync_sram_mirror_parallel(last_info->rdcu_new_model_adr_used,
						   (new_model_size_4byte+3) & ~0x3U,
						   cfg->rdcu_model_adr,
						   samples_4byte, rdcu_get_data_mtu()))
			return -1;
		/* wait for it */
		sync();
		if (cfg->icu_new_model_buf) {
			if (rdcu_read_sram(cfg->icu_new_model_buf,
					   last_info->rdcu_new_model_adr_used,
					   new_model_size_4byte))
				return -1;
		}
	/* write model */
	} else if (cfg->model_buf && model_mode_is_used(cfg->cmp_mode)) {
		/* set the model in the local mirror... */
		if (rdcu_write_sram_16(cfg->model_buf, cfg->rdcu_model_adr,
				       cfg->samples * IMA_SAM2BYT) < 0)
			return -1;

		if (rdcu_sync_mirror_to_sram(cfg->rdcu_model_adr, samples_4byte,
					     rdcu_get_data_mtu()))
			return -1;
	/* read model */
	} else if (model_mode_is_used(last_info->cmp_mode_used)) {
		if (rdcu_read_model(last_info, cfg->icu_new_model_buf) < 0)
			return -1;
	}

	/* ...and wait for completion */
	sync();

	if (rdcu_start_compression())
		return -1;

	return 0;
}

