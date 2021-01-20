/**
 * @file   cmp_ctrl.h
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
 */


#ifndef _CMP_CTRL_H_
#define _CMP_CTRL_H_

#include <stdint.h>
#include <stddef.h>

#define MAX_PAYLOAD_SIZE	4096
#define FRAMENUM 8

/* XXX include extra for RMAP headers, 128 bytes is plenty */
#undef GRSPW2_DEFAULT_MTU
#define GRSPW2_DEFAULT_MTU (MAX_PAYLOAD_SIZE + 128)

/* Compression mode definition according to PLATO-UVIE-PL-UM-0001 */
#define MODE_RAW		0
#define MODE_MODEL_ZERO		1
#define MODE_DIFF_ZERO		2
#define MODE_MODEL_MULTI	3
#define MODE_DIFF_MULTI		4

#define MODE_S_FX_NCOB		100

/* valid compression parameter ranges according to PLATO-UVIE-PL-UM-0001 */
#define MAX_CMP_MODE_RDCU	4UL
#define MAX_CMP_MODE_ICU	4UL
#define MAX_MODEL_VALUE		16UL
#define MIN_RDCU_GOLOMB_PAR	1UL
#define MAX_RDCU_GOLOMB_PAR	63UL
#define MIN_RDCU_SPILL		2UL
/* for maximum spill value look at get_max_spill */
#define MAX_ROUND		2UL
#define SAM2BYT  2 /* sample to byte conversion factor; one samples has 16 bits (2 bytes) */


/**
 * @brief The cmp_cfg structure can contain the complete configuration of the HW as
 *      well as the SW compressor.
 * @note when using the 1d-differentiating mode or the raw mode (cmp_error =
 *	0,2,4), the model parameters (model_value, model_buf, rdcu_model_adr,
 *	rdcu_new_model_adr) are ignored
 * @note the icu_output_buf will not be used for HW compression
 * @note the rdcu_***_adr parameters are ignored for SW compression
 * @note semi adaptive compression not supported for SW compression;
 *		configuration parameters ap1\_golomb\_par, ap2\_golomb\_par, ap1\_spill,
 *		ap2\_spill will be ignored;
 */

struct cmp_cfg {
	uint32_t cmp_mode;          /* 0: raw mode
				     * 1: model mode with zero escape symbol mechanism
				     * 2: 1d differencing mode without input model with zero escape symbol mechanism
				     * 3: model mode with multi escape symbol mechanism
				     * 4: 1d differencing mode without input model multi escape symbol mechanism */
	uint32_t golomb_par;        /* Golomb parameter for dictionary selection */
	uint32_t spill;             /* Spillover threshold for encoding outliers */
	uint32_t model_value;       /* Model weighting parameter */
	uint32_t round;             /* Number of noise bits to be rounded */
	uint32_t ap1_golomb_par;    /* Adaptive 1 spillover threshold; HW only */
	uint32_t ap1_spill;         /* Adaptive 1 Golomb parameter; HW only */
	uint32_t ap2_golomb_par;    /* Adaptive 2 spillover threshold; HW only */
	uint32_t ap2_spill;         /* Adaptive 2 Golomb parameter; HW only */
	void     *input_buf;        /* Pointer to the data to compress buffer */
	uint32_t rdcu_data_adr;     /* RDCU data to compress start address, the first data address in the RDCU SRAM; HW only */
	void     *model_buf;        /* Pointer to the model buffer */
	uint32_t rdcu_model_adr;    /* RDCU model start address, the first model address in the RDCU SRAM */
	uint16_t *icu_new_model_buf;/* Pointer to the updated model buffer */
	uint32_t rdcu_new_model_adr;/* RDCU updated model start address, the address in the RDCU SRAM where the updated model is stored*/
	uint32_t samples;           /* Number of samples (16 bit value) to compress, length of the data and model buffer */
	void     *icu_output_buf;   /* Pointer to the compressed data buffer (not used for RDCU compression) */
	uint32_t rdcu_buffer_adr;   /* RDCU compressed data start address, the first output data address in the RDCU SRAM */
	uint32_t buffer_length;     /* Length of the compressed data buffer in number of samples (16 bit values)*/
};


extern const struct cmp_cfg DEFAULT_CFG_MODEL;

extern const struct cmp_cfg DEFAULT_CFG_DIFF;


/**
 * @brief The cmp_status structure can contain the information of the
 *      compressor status register from the RDCU, see RDCU-FRS-FN-0632,
 *      but can also be used for the SW compression.
 */

__extension__
struct cmp_status {
	uint8_t cmp_ready : 1; /* Data Compressor Ready; 0: Compressor is busy 1: Compressor is ready */
	uint8_t cmp_active : 1; /* Data Compressor Active; 0: Compressor is on hold; 1: Compressor is active */
	uint8_t data_valid : 1; /* Compressor Data Valid; 0: Data is invalid; 1: Data is valid */
	uint8_t cmp_interrupted : 1; /* Data Compressor Interrupted; HW only; 0: No compressor interruption; 1: Compressor was interrupted */
	uint8_t rdcu_interrupt_en : 1; /* RDCU Interrupt Enable; HW only; 0: Interrupt is disabled; 1: Interrupt is enabled */
};


/**
 * @brief The cmp_info structure can contain the information and metadata of an
 *      executed compression of the HW as well as the SW compressor.
 *
 * @note if SW compression is used the parameters rdcu_new_model_adr_used, rdcu_cmp_adr_used,
 *      ap1_cmp_size, ap2_cmp_size are not used and are therefore set to zero
 */

__extension__
struct cmp_info {
	uint8_t  cmp_mode_used;       /* Compression mode used */
	uint8_t  model_value_used;    /* Model weighting parameter used */
	uint8_t  round_used: 4;       /* Number of noise bits to be rounded used */
	uint16_t spill_used: 12;      /* Spillover threshold used */
	uint8_t  golomb_par_used;     /* Golomb parameter used */
	uint32_t samples_used: 24;    /* Number of samples (16 bit value) to be stored */
	uint32_t cmp_size;	      /* Compressed data size; measured in bits */
	uint32_t ap1_cmp_size;        /* Adaptive compressed data size 1; measured in bits */
	uint32_t ap2_cmp_size;        /* Adaptive compressed data size 2; measured in bits */
	uint32_t rdcu_new_model_adr_used; /* Updated model start  address used */
	uint32_t rdcu_cmp_adr_used;   /* Compressed data start address */
	uint16_t cmp_err;             /* Compressor errors
				       * [bit 0] small_buffer_err; The length for the compressed data buffer is too small
				       * [bit 1] cmp_mode_err; The cmp_mode parameter is not set correctly
				       * [bit 2] model_value_err; The model_value parameter is not set correctly
				       * [bit 3] cmp_par_err; The spill, golomb_par combination is not set correctly
				       * [bit 4] ap1_cmp_par_err; The ap1_spill, ap1_golomb_par combination is not set correctly (only HW compression)
				       * [bit 5] ap2_cmp_par_err; The ap2_spill, ap2_golomb_par combination is not set correctly (only HW compression)
				       * [bit 6] mb_err; Multi bit error detected by the memory controller (only HW compression)
				       * [bit 7] slave_busy_err; The bus master has received the "slave busy" status (only HW compression)
				       * [bit 8] slave_blocked_err; The bus master has received the “slave blocked” status (only HW compression)
				       * [bit 9] invalid address_err; The bus master has received the “invalid address” status (only HW compression) */
};

int model_mode_is_used(const struct cmp_cfg *cfg);

/* int icu_compress_data(const struct cmp_cfg *cfg, struct cmp_status *status, */
/* 		      struct cmp_info *info); */

int icu_compress_data_adaptive_save(const struct cmp_cfg *cfg, struct cmp_status
				   *status, struct cmp_info *info, void *tmp_buf);

int rdcu_compress_data(const struct cmp_cfg *cfg);

int rdcu_start_compression(void);

int rdcu_read_cmp_status(struct cmp_status *status);

int rdcu_read_cmp_info(struct cmp_info *info);

int rdcu_read_cmp_bitstream(const struct cmp_info *info, void *output_buf);

int rdcu_read_model(const struct cmp_info *info, void *model_buf);

int cmp_update_cfg(const struct cmp_cfg cfg, const struct cmp_info info);

int rdcu_interrupt_compression(void);

uint32_t get_max_spill(uint32_t golomb_par);

void print_cmp_cfg(const struct cmp_cfg *cfg);

void print_cmp_info(const struct cmp_info *info);

#endif /* _CMP_CTRL_H_ */
