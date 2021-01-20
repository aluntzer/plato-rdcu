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
 * @brief compressor control library
 * @see Data Compression User Manual PLATO-UVIE-PL-UM-0001
 */

#include <stdint.h>
#include <stdio.h>

#include "../include/rdcu_cmd.h"
#include "../include/cmp_support.h"
#include "../include/rdcu_ctrl.h"
#include "../include/rdcu_rmap.h"

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
 * @brief check if the compressor configuration is valid for a RDCU compression,
 * see the user manual for more information (PLATO-UVIE-PL-UM-0001).
 *
 * @param cfg	configuration contains all parameters required for compression
 *
 * @returns  >= 0 on success, error otherwise
 */

int rdcu_cmp_cfg_valid(const struct cmp_cfg *cfg)
{
	int cfg_invalid = 0;
	int cfg_warning = 0;

	if (cfg == NULL)
		return -1;

	if (cfg->cmp_mode > MAX_RDCU_CMP_MODE) {
		printf("Error: selected cmp_mode: %lu is not supported. "
		       "Largest supported mode is: %lu.\n", cfg->cmp_mode,
		       MAX_RDCU_CMP_MODE);
		cfg_invalid++;
	}

	if (cfg->model_value > MAX_MODEL_VALUE) {
		printf("Error: selected model_value: %lu is invalid. "
		       "Largest supported value is: %lu.\n", cfg->model_value,
		       MAX_MODEL_VALUE);
		cfg_invalid++;
	}

	if (cfg->golomb_par < MIN_RDCU_GOLOMB_PAR||
	    cfg->golomb_par > MAX_RDCU_GOLOMB_PAR) {
		printf("Error: The selected Golomb parameter: %lu is not supported. "
		       "The Golomb parameter has to  be between [%lu, %lu].\n",
		       cfg->golomb_par, MIN_RDCU_GOLOMB_PAR,
		       MAX_RDCU_GOLOMB_PAR);
		cfg_invalid++;
	}

	if (cfg->ap1_golomb_par < MIN_RDCU_GOLOMB_PAR ||
	    cfg->ap1_golomb_par > MAX_RDCU_GOLOMB_PAR) {
		printf("Error: The selected adaptive 1 Golomb parameter: %lu is not supported. "
		       "The Golomb parameter has to  be between [%lu, %lu].\n",
		       cfg->ap1_golomb_par, MIN_RDCU_GOLOMB_PAR,
		       MAX_RDCU_GOLOMB_PAR);
		cfg_invalid++;
	}

	if (cfg->ap2_golomb_par < MIN_RDCU_GOLOMB_PAR ||
	    cfg->ap2_golomb_par > MAX_RDCU_GOLOMB_PAR) {
		printf("Error: The selected adaptive 2 Golomb parameter: %lu is not supported. "
		       "The Golomb parameter has to be between [%lu, %lu].\n",
		       cfg->ap2_golomb_par, MIN_RDCU_GOLOMB_PAR,
		       MAX_RDCU_GOLOMB_PAR);
		cfg_invalid++;
	}

	if (cfg->spill < MIN_RDCU_SPILL) {
		printf("Error: The selected spillover threshold value: %lu is too small. "
		       "Smallest possible spillover value is: %lu.\n",
		       cfg->spill, MIN_RDCU_SPILL);
		cfg_invalid++;
	}

	if (cfg->spill > get_max_spill(cfg->golomb_par, cfg->cmp_mode)) {
		printf("Error: The selected spillover threshold value: %lu is "
		       "too large for the selected Golomb parameter: %lu, the "
		       "largest possible spillover value is: %lu.\n",
		       cfg->spill, cfg->golomb_par,
		       get_max_spill(cfg->golomb_par, cfg->cmp_mode));
		cfg_invalid++;
	}

	if (cfg->ap1_spill < MIN_RDCU_SPILL) {
		printf("Error: The selected adaptive 1 spillover threshold "
		       "value: %lu is too small. "
		       "Smallest possible spillover value is: %lu.\n",
		       cfg->ap1_spill, MIN_RDCU_SPILL);
		cfg_invalid++;
	}

	if (cfg->ap1_spill > get_max_spill(cfg->ap1_golomb_par, cfg->cmp_mode)) {
		printf("Error: The selected adaptive 1 spillover threshold "
		       "value: %lu is too large for the selected adaptive 1 "
		       "Golomb parameter: %lu, the largest possible adaptive 1 "
		       "spillover value is: %lu.\n",
		       cfg->ap1_spill, cfg->ap1_golomb_par,
		       get_max_spill(cfg->ap1_golomb_par, cfg->cmp_mode));
		cfg_invalid++;
	}

	if (cfg->ap2_spill < MIN_RDCU_SPILL) {
		printf("Error: The selected adaptive 2 spillover threshold "
		       "value: %lu is too small."
		       "Smallest possible spillover value is: %lu.\n",
		       cfg->ap2_spill, MIN_RDCU_SPILL);
		cfg_invalid++;
	}

	if (cfg->ap2_spill > get_max_spill(cfg->ap2_golomb_par, cfg->cmp_mode)) {
		printf("Error: The selected adaptive 2 spillover threshold "
		       "value: %lu is too large for the selected adaptive 2 "
		       "Golomb parameter: %lu, the largest possible adaptive 2 "
		       "spillover value is: %lu.\n",
		       cfg->ap2_spill, cfg->ap2_golomb_par,
		       get_max_spill(cfg->ap2_golomb_par, cfg->cmp_mode));
		cfg_invalid++;
	}

	if (cfg->round > MAX_RDCU_ROUND) {
		printf("Error: selected round parameter: %lu is not supported. "
		       "Largest supported value is: %lu.\n",
		       cfg->round, MAX_RDCU_ROUND);
		cfg_invalid++;
	}

	if (cfg->samples == 0) {
		printf("Warning: The samples parameter is set to 0. No data will be compressed.\n");
		cfg_warning++;
	}

	if (cfg->buffer_length == 0) {
		printf("Error: The buffer_length is set to 0. There is no place "
		       "to store the compressed data.\n");
		cfg_invalid++;
	}

	if (cfg->cmp_mode == MODE_RAW) {
		if (cfg->buffer_length < cfg->samples) {
			printf("buffer_length is smaller than samples parameter. "
			       "There is not enough space to copy the data in "
			       "RAW mode.\n");
			cfg_invalid++;
		}
	}

	if (!cfg->input_buf) {
		printf("Warning: The data to compress buffer is set to NULL. "
		       "No data will be transferred to the rdcu_data_adr in  "
		       "the RDCU-SRAM.\n");
		cfg_warning++;
	}

	if (cfg->rdcu_data_adr & 0x3) {
		printf("Error: The RDCU data to compress start address is not 4-Byte aligned.\n");
		cfg_invalid++;
	}

	if (cfg->rdcu_buffer_adr & 0x3) {
		printf("Error: The RDCU compressed data start address is not 4-Byte aligned.\n");
		cfg_invalid++;
	}

	if (!in_sram_range(cfg->rdcu_data_adr, cfg->samples * SAM2BYT)) {
		printf("Error: The RDCU data to compress buffer is outside the RDCU SRAM address space.\n");
		cfg_invalid++;
	}

	if (!in_sram_range(cfg->rdcu_buffer_adr, cfg->buffer_length * SAM2BYT)) {
		printf("Error: The RDCU compressed data buffer is outside the RDCU SRAM address space.\n");
		cfg_invalid++;
	}

	if (buffers_overlap(cfg->rdcu_data_adr,
			    cfg->rdcu_data_adr + cfg->samples * SAM2BYT,
			    cfg->rdcu_buffer_adr,
			    cfg->rdcu_buffer_adr + cfg->buffer_length * SAM2BYT)) {
		printf("Error: The RDCU data to compress buffer and the RDCU "
		       "compressed data buffer are overlapping.\n");
		cfg_invalid++;
	}

	if (model_mode_is_used(cfg->cmp_mode)) {
		if (cfg->model_buf == cfg->input_buf) {
			printf("Error: The model buffer (model_buf) and the data "
			       "to be compressed (input_buf) are equal.");
			cfg_invalid++;
		}

		if (!cfg->model_buf) {
			printf("Warning: The model buffer is set to NULL. No "
			       "model data will be transferred to the "
			       "rdcu_model_adr in the RDCU-SRAM.\n");
			cfg_warning++;
		}

		if (cfg->rdcu_model_adr & 0x3) {
			printf("Error: The RDCU model start address is not 4-Byte aligned.\n");
			cfg_invalid++;
		}

		if (!in_sram_range(cfg->rdcu_model_adr, cfg->samples * SAM2BYT)) {
			printf("Error: The RDCU model buffer is outside the RDCU SRAM address space.\n");
			cfg_invalid++;
		}

		if (buffers_overlap(
			    cfg->rdcu_model_adr,
			    cfg->rdcu_model_adr + cfg->samples * SAM2BYT,
			    cfg->rdcu_data_adr,
			    cfg->rdcu_data_adr + cfg->samples * SAM2BYT)) {
			printf("Error: The model buffer and the data to compress buffer are overlapping.\n");
			cfg_invalid++;
		}

		if (buffers_overlap(
			cfg->rdcu_model_adr,
			cfg->rdcu_model_adr + cfg->samples * SAM2BYT,
			cfg->rdcu_buffer_adr,
			cfg->rdcu_buffer_adr + cfg->buffer_length * SAM2BYT)
		    ){
			printf("Error: The model buffer and the compressed data buffer are overlapping.\n");
			cfg_invalid++;
		}

		if (cfg->rdcu_model_adr != cfg->rdcu_new_model_adr) {
			if (cfg->rdcu_new_model_adr & 0x3) {
				printf("Error: The RDCU updated model start address "
				       "(rdcu_new_model_adr) is not 4-Byte aligned.\n");
				cfg_invalid++;
			}

			if (!in_sram_range(cfg->rdcu_new_model_adr,
					   cfg->samples * SAM2BYT)) {
				printf("Error: The RDCU updated model buffer is "
				       "outside the RDCU SRAM address space.\n");
				cfg_invalid++;
			}

			if (buffers_overlap(
				cfg->rdcu_new_model_adr,
				cfg->rdcu_new_model_adr + cfg->samples * SAM2BYT,
				cfg->rdcu_data_adr,
				cfg->rdcu_data_adr + cfg->samples * SAM2BYT)
			    ){
				printf("Error: The updated model buffer and the data to "
				       "compress buffer are overlapping.\n");
				cfg_invalid++;
			}

			if (buffers_overlap(
				cfg->rdcu_new_model_adr,
				cfg->rdcu_new_model_adr + cfg->samples * SAM2BYT,
				cfg->rdcu_buffer_adr,
				cfg->rdcu_buffer_adr + cfg->buffer_length * SAM2BYT)
			    ){
				printf("Error: The updated model buffer and the compressed "
				       "data buffer are overlapping.\n");
				cfg_invalid++;
			}
			if (buffers_overlap(
				cfg->rdcu_new_model_adr,
				cfg->rdcu_new_model_adr + cfg->samples * SAM2BYT,
				cfg->rdcu_model_adr,
				cfg->rdcu_model_adr + cfg->samples * SAM2BYT)
			    ){
				printf("Error: The updated model buffer and the "
				       "model buffer are overlapping.\n");
				cfg_invalid++;
			}
		}
	}

	if (cfg->icu_new_model_buf) {
		printf("Warning: ICU updated model buffer is set. This "
		       "buffer is not used for an RDCU compression.\n");
		cfg_warning++;
	}

	if (cfg->icu_output_buf) {
		printf("Warning: ICU compressed data buffer is set. This "
		       "buffer is not used for an RDCU compression.\n");
		cfg_warning++;
	}

	if (cfg_invalid)
		return -cfg_invalid;
	else
		return cfg_warning;
}


/**
 * @brief set up RDCU compression register
 *
 * @param cfg  configuration contains all parameters required for compression
 *
 * @returns 0 on success, error otherwise
 */

int rdcu_set_compression_register(const struct cmp_cfg *cfg)
{
	if (rdcu_cmp_cfg_valid(cfg) < 0)
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
 * TODO: implement a function to enable/disable rdcu interrupt
 * @note rdcu interrupt signal is default enabled
 * @returns 0 on success, error otherwise
 */

int rdcu_start_compression(void)
{
	/* enable the interrupt signal to the ICU */
	rdcu_set_rdcu_interrupt();
	/* start the compression */
	rdcu_set_data_compr_start();
	if (rdcu_sync_compr_ctrl())
		return -1;
	sync();

	/* clear local bit immediately, this is a write-only register.
	 * we would not want to restart compression by accidentially calling
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
 * @note the icu_output_buf will not be used for the RDCU compression
 * @note the overlapping of the different rdcu buffers is not checked
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
	s = size_of_bitstream(info->cmp_size);

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
	s = size_of_model(info->samples_used, info->cmp_mode_used);

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
