/**
 * @file   cmp_support.h
 * @author Dominik Loidolt (dominik.loidolt@univie.ac.at),
 * @date   2018
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
 * @brief compression/decompression helper functions
 */

#ifndef CMP_SUPPORT_H
#define CMP_SUPPORT_H

#include <stdint.h>
#include <stddef.h>

#define GOLOMB_PAR_EXPOSURE_FLAGS 1

/* Compression Error Register bits definition, see RDCU-FRS-FN-0952 */
#define SMALL_BUFFER_ERR_BIT	0x00 /* The length for the compressed data buffer is too small */
#define CMP_MODE_ERR_BIT	0x01 /* The cmp_mode parameter is not set correctly */
#define MODEL_VALUE_ERR_BIT	0x02 /* The model_value parameter is not set correctly */
#define CMP_PAR_ERR_BIT		0x03 /* The spill, golomb_par combination is not set correctly */
#define AP1_CMP_PAR_ERR_BIT	0x04 /* The ap1_spill, ap1_golomb_par combination is not set correctly (only HW compression) */
#define AP2_CMP_PAR_ERR_BIT	0x05 /* The ap2_spill, ap2_golomb_par combination is not set correctly (only HW compression) */
#define MB_ERR_BIT		0x06 /* Multi bit error detected by the memory controller (only HW compression) */
#define SLAVE_BUSY_ERR_BIT	0x07 /* The bus master has received the "slave busy" status (only HW compression) */

#define MODE_RAW 0
#define MODE_MODEL_ZERO 1
#define MODE_DIFF_ZERO 2
#define MODE_MODEL_MULTI 3
#define MODE_DIFF_MULTI 4

#define MAX_MODEL_VALUE                                                        \
	16UL /* the maximal model values used in the update equation for the new model */

/* valid compression parameter ranges for RDCU compression according to PLATO-UVIE-PL-UM-0001 */
#define MAX_RDCU_CMP_MODE	4UL
#define MIN_RDCU_GOLOMB_PAR	1UL
#define MAX_RDCU_GOLOMB_PAR	63UL
#define MIN_RDCU_SPILL		2UL
#define MAX_RDCU_ROUND		2UL
/* for maximum spill value look at get_max_spill function */

/* valid compression parameter ranges for ICU compression */
#define MIN_ICU_GOLOMB_PAR	1UL
#define MAX_ICU_GOLOMB_PAR	UINT32_MAX
#define MIN_ICU_SPILL		2UL
/* for maximum spill value look at get_max_spill function */
#define MAX_ICU_ROUND		2UL

#define SAM2BYT                                                                \
	2 /* sample to byte conversion factor; one samples has 16 bits (2 bytes) */

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
				     * 4: 1d differencing mode without input model multi escape symbol mechanism
				     */
	uint32_t golomb_par;        /* Golomb parameter for dictionary selection */
	uint32_t spill;             /* Spillover threshold for encoding outliers */
	uint32_t model_value;       /* Model weighting parameter */
	uint32_t round;             /* Number of noise bits to be rounded */
	uint32_t ap1_golomb_par;    /* Adaptive 1 spillover threshold; HW only */
	uint32_t ap1_spill;         /* Adaptive 1 Golomb parameter; HW only */
	uint32_t ap2_golomb_par;    /* Adaptive 2 spillover threshold; HW only */
	uint32_t ap2_spill;         /* Adaptive 2 Golomb parameter; HW only */
	void *input_buf;            /* Pointer to the data to compress buffer */
	uint32_t rdcu_data_adr;     /* RDCU data to compress start address, the first data address in the RDCU SRAM; HW only */
	void *model_buf;            /* Pointer to the model buffer */
	uint32_t rdcu_model_adr;    /* RDCU model start address, the first model address in the RDCU SRAM */
	void *icu_new_model_buf;    /* Pointer to the updated model buffer */
	uint32_t rdcu_new_model_adr;/* RDCU updated model start address, the address in the RDCU SRAM where the updated model is stored*/
	uint32_t samples;           /* Number of samples (16 bit value) to compress, length of the data and model buffer */
	void *icu_output_buf;       /* Pointer to the compressed data buffer (not used for RDCU compression) */
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

struct cmp_status {
	uint8_t cmp_ready; /* Data Compressor Ready; 0: Compressor is busy 1: Compressor is ready */
	uint8_t cmp_active; /* Data Compressor Active; 0: Compressor is on hold; 1: Compressor is active */
	uint8_t data_valid; /* Compressor Data Valid; 0: Data is invalid; 1: Data is valid */
	uint8_t cmp_interrupted; /* Data Compressor Interrupted; HW only; 0: No compressor interruption; 1: Compressor was interrupted */
	uint8_t rdcu_interrupt_en; /* RDCU Interrupt Enable; HW only; 0: Interrupt is disabled; 1: Interrupt is enabled */
};

/**
 * @brief The cmp_info structure can contain the information and metadata of an
 *      executed compression of the HW as well as the SW compressor.
 *
 * @note if SW compression is used the parameters rdcu_model_adr_used, rdcu_cmp_adr_used,
 *      ap1_cmp_size_byte, ap2_cmp_size_byte are not used and are therefore set to zero
 */

struct cmp_info {
	uint32_t cmp_mode_used;       /* Compression mode used */
	uint8_t  model_value_used;    /* Model weighting parameter used */
	uint8_t  round_used;          /* Number of noise bits to be rounded used */
	uint32_t spill_used;          /* Spillover threshold used */
	uint32_t golomb_par_used;     /* Golomb parameter used */
	uint32_t samples_used;        /* Number of samples (16 bit value) to be stored */
	uint32_t cmp_size_byte;       /* Compressed data size; measured in bytes */
	uint32_t ap1_cmp_size_byte;  /* Adaptive compressed data size 1; measured in bytes */
	uint32_t ap2_cmp_size_byte;  /* Adaptive compressed data size 2; measured in bytes */
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

int is_a_pow_of_2(unsigned int v);
int ilog_2(uint32_t x);

int model_mode_is_used(unsigned int cmp_mode);
int diff_mode_is_used(unsigned int cmp_mode);
int raw_mode_is_used(unsigned int cmp_mode);
int rdcu_supported_mode_is_used(unsigned int cmp_mode);

int zero_escape_mech_is_used(unsigned int cmp_mode);
int multi_escape_mech_is_used(unsigned int cmp_mode);

unsigned int round_fwd(unsigned int value, unsigned int round);
unsigned int round_inv(unsigned int value, unsigned int round);
unsigned int cal_up_model(unsigned int data, unsigned int model, unsigned int
			  model_value);

uint32_t get_max_spill(unsigned int golomb_par, unsigned int cmp_mode);

size_t size_of_a_sample(unsigned int cmp_mode);
unsigned int cmp_bit_to_4byte(unsigned int cmp_size_bit);
unsigned int cmp_cal_size_of_data(unsigned int samples, unsigned int cmp_mode);
unsigned int cmp_cal_size_of_model(unsigned int samples, unsigned int cmp_mode);

void print_cmp_cfg(const struct cmp_cfg *cfg);
void print_cmp_info(const struct cmp_info *info);

#endif /* CMP_SUPPORT_H */
