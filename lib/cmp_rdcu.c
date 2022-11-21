/**
 * @file   cmp_rdcu.c
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

#include <cmp_debug.h>
#include <cmp_support.h>
#include <cmp_rdcu_cfg.h>
#include <rdcu_ctrl.h>
#include <rdcu_rmap.h>


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
 * @note Before the rdcu_compress function can be used, an initialisation of
 *	the RMAP library is required. This is achieved with the functions
 *	rdcu_ctrl_init() and rdcu_rmap_init().
 * @note The validity of the cfg structure is checked before the compression is
 *	 started.
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
 * @brief read out the metadata of an RDCU compression
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
		info->cmp_size = rdcu_get_compr_data_size_bit();
		info->ap1_cmp_size = rdcu_get_compr_data_adaptive_1_size_bit();
		info->ap2_cmp_size = rdcu_get_compr_data_adaptive_2_size_bit();
		info->cmp_err = rdcu_get_compr_error();
	}
	return 0;
}


/**
 * @brief read the compressed bitstream from the RDCU SRAM
 *
 * @param info			compression information contains the metadata of a compression
 * @param compressed_data	the buffer to store the bitstream (if NULL, the
 *				required size is returned)
 *
 * @returns the number of bytes read, < 0 on error
 */

int rdcu_read_cmp_bitstream(const struct cmp_info *info, void *compressed_data)
{
	uint32_t s;

	if (info == NULL)
		return -1;

	/* calculate the need bytes for the bitstream */
	s = cmp_bit_to_4byte(info->cmp_size);

	if (compressed_data == NULL)
		return (int)s;

	if (rdcu_sync_sram_to_mirror(info->rdcu_cmp_adr_used, s,
				     rdcu_get_data_mtu()))
		return -1;

	/* wait for it */
	sync();

	return rdcu_read_sram(compressed_data, info->rdcu_cmp_adr_used, s);
}


/**
 * @brief read the updated model from the RDCU SRAM
 *
 * @param info		compression information contains the metadata of a compression
 *
 * @param updated_model	the buffer to store the updated model (if NULL, the required size
 *			is returned)
 *
 * @returns the number of bytes read, < 0 on error
 */

int rdcu_read_model(const struct cmp_info *info, void *updated_model)
{
	uint32_t s;

	if (info == NULL)
		return -1;

	/* calculate the need bytes for the model */
	s = info->samples_used * IMA_SAM2BYT;

	if (updated_model == NULL)
		return (int)s;

	if (rdcu_sync_sram_to_mirror(info->rdcu_new_model_adr_used, (s+3) & ~3U,
				     rdcu_get_data_mtu()))
		return -1;

	/* wait for it */
	sync();

	return rdcu_read_sram(updated_model, info->rdcu_new_model_adr_used, s);
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

