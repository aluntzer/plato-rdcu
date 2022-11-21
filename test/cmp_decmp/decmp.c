/**
 * @file   decmp.c
 * @author Dominik Loidolt (dominik.loidolt@univie.ac.at),
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
 * @brief software decompression library
 * @see Data Compression User Manual PLATO-UVIE-PL-UM-0001
 *
 * To decompress a compression entity (consisting of a compression entity header
 * and the compressed data) use the decompress_cmp_entiy() function.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include "byteorder.h"
#include "cmp_debug.h"
#include "cmp_support.h"
#include "cmp_data_types.h"
#include "cmp_entity.h"


#define MAX_CW_LEN 32 /* maximum Golomb code word bit length */


/* maximum used bits registry */
extern struct cmp_max_used_bits max_used_bits;

/* function pointer to a code word decoder function */
typedef int (*decoder_ptr)(uint32_t, unsigned int, unsigned int, uint32_t *);

/* structure to hold a setup to encode a value */
struct decoder_setup {
	decoder_ptr decode_cw_f; /* pointer to the code word decoder (Golomb/Rice)*/
	int (*decode_method_f)(uint32_t *decoded_value, int stream_pos,
			       const struct decoder_setup *setup); /* pointer to the decoding function */
	uint32_t *bitstream_adr; /* start address of the compressed data bitstream */
	uint32_t max_stream_len; /* maximum length of the bitstream/icu_output_buf in bits */
	uint32_t encoder_par1; /* encoding parameter 1 */
	uint32_t encoder_par2; /* encoding parameter 2 */
	uint32_t outlier_par; /* outlier parameter */
	uint32_t lossy_par; /* lossy compression parameter */
	uint32_t model_value; /* model value parameter */
	uint32_t max_data_bits; /* how many bits are needed to represent the highest possible value */
};


/**
 * @brief count leading 1-bits
 *
 * @param value	input vale to count
 *
 * @returns the number of leading 1-bits in value, starting at the most
 *	significant bit position
 */

static unsigned int count_leading_ones(uint32_t value)
{
	if (value == 0xFFFFFFFF)
		return 32;

	return __builtin_clz(~value);
}


/**
 * @brief decode a Rice code word
 *
 * @param code_word	Rice code word bitstream starting at the MSB
 * @param m		Golomb parameter (not used)
 * @param log2_m	Rice parameter, must be the same used for encoding
 * @param decoded_cw	pointer where decoded value is written
 *
 * @returns the length of the decoded code word in bits (NOT the decoded value);
 *	0 on failure
 */

static int rice_decoder(uint32_t code_word, unsigned int m, unsigned int log2_m,
			uint32_t *decoded_cw)
{
	unsigned int q; /* quotient code */
	unsigned int ql; /* length of the quotient code */
	unsigned int r; /* remainder code */
	unsigned int rl = log2_m; /* length of the remainder code */
	unsigned int cw_len; /* length of the decoded code word in bits */

	(void)m; /* we don't need the Golomb parameter */

	if (log2_m > 32) /* because m has 32 bits log2_m can not be bigger than 32 */
		return 0;

	q = count_leading_ones(code_word); /* decode unary coding */
	ql = q + 1; /* Number of 1's + following 0 */

	cw_len = rl + ql;

	if (cw_len > 32) /* can only decode code words with maximum 32 bits */
		return 0;

	code_word = code_word << ql;  /* shift quotient code out */

	/* Right shifting an integer by a number of bits equal or greater than
	 * its size is undefined behavior */
	if (rl == 0)
		r = 0;
	else
		r = code_word >> (32 - rl);

	*decoded_cw = (q << rl) + r;

	return cw_len;
}


/**
 * @brief decode a Golomb code word
 *
 * @param code_word	Golomb code word bitstream starting at the MSB
 * @param m		Golomb parameter (have to be bigger than 0)
 * @param log2_m	is log_2(m) calculate outside function for better
 *			performance
 * @param decoded_cw	pointer where decoded value is written
 *
 * @returns the length of the decoded code word in bits (NOT the decoded value);
 *	0 on failure
 */

static int golomb_decoder(uint32_t code_word, unsigned int m,
			  unsigned int log2_m, uint32_t *decoded_cw)
{
	unsigned int q; /* quotient code */
	unsigned int r1; /* remainder code group 1 */
	unsigned int r2; /* remainder code group 2 */
	unsigned int r; /* remainder code */
	unsigned int rl; /* length of the remainder code */
	unsigned int cutoff; /* cutoff between group 1 and 2 */
	unsigned int cw_len; /* length of the decoded code word in bits */

	q = count_leading_ones(code_word); /* decode unary coding */

	rl = log2_m + 1;
	code_word <<= (q+1);  /* shift quotient code out */

	r2 = code_word >> (32 - rl);
	r1 = r2 >> 1;

	cutoff = (1UL << rl) - m;

	if (r1 < cutoff) { /* group 1 */
		cw_len = q + rl;
		r = r1;
	} else { /* group 2 */
		cw_len = q + rl + 1;
		r = r2 - cutoff;
	}

	if (cw_len > 32)
		return 0;

	*decoded_cw = q*m + r;
	return cw_len;
}


/**
 * @brief select the decoder based on the used Golomb parameter
 * @note if the Golomb parameter is a power of 2 we can use the faster Rice
 *	decoder
 *
 * @param golomb_par	Golomb parameter (have to be bigger than 0)
 *
 * @returns function pointer to the select decoder function; NULL on failure
 */

static decoder_ptr select_decoder(unsigned int golomb_par)
{
	if (!golomb_par)
		return NULL;

	if (is_a_pow_of_2(golomb_par))
		return &rice_decoder;
	else
		return &golomb_decoder;
}


/**
 * @brief read a value of up to 32 bits from a bitstream
 *
 * @param p_value		pointer to the read value, the
 *				read value will be converted to the system
 *				endianness
 * @param n_bits		number of bits to read from the bitstream
 * @param bit_offset		bit index where the bits will be read, seen from
 *				the very beginning of the bitstream
 * @param bitstream_adr		this is the pointer to the beginning of the
 *				bitstream
 * @param max_stream_len	maximum length of the bitstream in bits *
 *
 * @returns bit position of the last read bit in the bitstream on success;
 *	returns negative in case of erroneous input; returns CMP_ERROR_SMALL_BUF
 *	if the bitstream buffer is too small to read the value from the
 *	bitstream
 */

static int get_n_bits32(uint32_t *p_value, unsigned int n_bits, int bit_offset,
			uint32_t *bitstream_adr, unsigned int max_stream_len)
{
	uint32_t *local_adr;
	unsigned int bitsLeft, bitsRight, localEndPos;
	unsigned int mask;
	int stream_len = (int)(n_bits + (unsigned int)bit_offset); /* overflow results in a negative return value */

	/*leave in case of erroneous input */
	if (bit_offset < 0)
		return -1;
	if (n_bits == 0)
		return -1;
	if (n_bits > 32)
		return -1;
	if (!bitstream_adr)
		return -1;
	if (!p_value)
		return -1;

	/* Check if bitstream buffer is large enough */
	if ((unsigned int)stream_len > max_stream_len) {
		debug_print("Error: Buffer overflow detected.\n");
		return CMP_ERROR_SMALL_BUF;

	}

	/* separate the bit_offset into word offset (set local_adr pointer) and
	 * local bit offset (bitsLeft)
	 */
	local_adr = bitstream_adr + (bit_offset >> 5);
	bitsLeft = bit_offset & 0x1f;

	localEndPos = bitsLeft + n_bits;

	if (localEndPos <= 32) {
		unsigned int shiftRight = 32 - n_bits;

		bitsRight = shiftRight - bitsLeft;

		*(p_value) = cpu_to_be32(*(local_adr)) >> bitsRight;

		mask = (0xffffffff >> shiftRight);

		*(p_value) &= mask;
	} else {
		unsigned int n1 = 32 - bitsLeft;
		unsigned int n2 = n_bits - n1;
		/* part 1 ; */
		mask = 0xffffffff >> bitsLeft;
		*(p_value) = cpu_to_be32(*(local_adr)) & mask;
		*(p_value) <<= n2;
		/*part 2: */
		/* adjust address*/
		local_adr += 1;

		bitsRight = 32 - n2;
		*(p_value) |= cpu_to_be32(*(local_adr)) >> bitsRight;
	}

	return stream_len;
}


/**
 * @brief decode a Golomb/Rice encoded code word from the bitstream
 *
 * @param decoded_value	pointer to the decoded value
 * @param stream_pos	start bit position code word to be decoded in the bitstream
 * @param setup		pointer to the decoder setup
 *
 * @returns bit index of the next code word in the bitstream on success; returns
 *	negative in case of erroneous input; returns CMP_ERROR_SMALL_BUF if the
 *	bitstream buffer is too small to read the value from the bitstream
 */

static int decode_normal(uint32_t *decoded_value, int stream_pos,
			 const struct decoder_setup *setup)
{
	uint32_t read_val;
	unsigned int n_read_bits;
	int stream_pos_read, cw_len;

	/* check if we can read max_cw_len or less; we do not know how long the
	 * code word actually is so we try to read the maximum cw length */
	if ((unsigned int)stream_pos + 32 > setup->max_stream_len)
		n_read_bits = setup->max_stream_len - (unsigned int)stream_pos;
	else
		n_read_bits = MAX_CW_LEN;

	stream_pos_read = get_n_bits32(&read_val, n_read_bits, stream_pos,
				       setup->bitstream_adr, setup->max_stream_len);
	if (stream_pos_read < 0)
		return stream_pos_read;

	/* if we read less than 32, we shift the bitstream so that it starts at the MSB */
	read_val = read_val << (32 - n_read_bits);

	cw_len = setup->decode_cw_f(read_val, setup->encoder_par1,
				    setup->encoder_par2, decoded_value);
	if (cw_len <= 0)
		return -1;
	/* consistency check: code word length can not be bigger than the read bits */
	if (cw_len > (int)n_read_bits)
		return -1;

	return stream_pos + cw_len;
}


/**
 * @brief decode a Golomb/Rice encoded code word with zero escape system
 *	mechanism from the bitstream
 *
 * @param decoded_value	pointer to the decoded value
 * @param stream_pos	start bit position code word to be decoded in the bitstream
 * @param setup		pointer to the decoder setup
 *
 * @returns bit index of the next code word in the bitstream on success; returns
 *	negative in case of erroneous input; returns CMP_ERROR_SMALL_BUF if the
 *	bitstream buffer is too small to read the value from the bitstream
 */

static int decode_zero(uint32_t *decoded_value, int stream_pos,
		       const struct decoder_setup *setup)
{
	stream_pos = decode_normal(decoded_value, stream_pos, setup);
	if (stream_pos < 0)
		return stream_pos;

	/* consistency check: value lager than the outlier parameter should not
	 * be Golomb/Rice encoded */
	if (*decoded_value > setup->outlier_par)
		return -1;

	if (*decoded_value == 0) {
		/* escape symbol mechanism was used; read unencoded value */
		uint32_t unencoded_val;

		stream_pos = get_n_bits32(&unencoded_val, setup->max_data_bits, stream_pos,
					  setup->bitstream_adr, setup->max_stream_len);
		if (stream_pos < 0)
			return stream_pos;
		/* consistency check: outliers must be bigger than the outlier_par */
		if (unencoded_val < setup->outlier_par && unencoded_val != 0)
			return -1;

		*decoded_value = unencoded_val;
	}

	(*decoded_value)--;
	if (*decoded_value == 0xFFFFFFFF) /* catch underflow */
		(*decoded_value) >>=  (32 - setup->max_data_bits);

	return stream_pos;
}


/**
 * @brief decode a Golomb/Rice encoded code word with multi escape system
 *	mechanism from the bitstream
 *
 * @param decoded_value	pointer to the decoded value
 * @param stream_pos	start bit position code word to be decoded in the bitstream
 * @param setup		pointer to the decoder setup
 *
 * @returns bit index of the next code word in the bitstream on success; returns
 *	negative in case of erroneous input; returns CMP_ERROR_SMALL_BUF if the
 *	bitstream buffer is too small to read the value from the bitstream
 */

static int decode_multi(uint32_t *decoded_value, int stream_pos,
			const struct decoder_setup *setup)
{
	stream_pos = decode_normal(decoded_value, stream_pos, setup);
	if (stream_pos < 0)
		return stream_pos;

	if (*decoded_value >= setup->outlier_par) {
		/* escape symbol mechanism was used; read unencoded value */
		uint32_t unencoded_val;
		unsigned int unencoded_len;

		unencoded_len = (*decoded_value - setup->outlier_par + 1) * 2;

		stream_pos = get_n_bits32(&unencoded_val, unencoded_len, stream_pos,
					  setup->bitstream_adr, setup->max_stream_len);
		if (stream_pos >= 0)
			*decoded_value = unencoded_val + setup->outlier_par;
	}
	return stream_pos;
}


/**
 * @brief get the value unencoded with setup->cmp_par_1 bits without any
 *	additional changes from the bitstream
 *
 * @param decoded_value	pointer to the decoded value
 * @param stream_pos	start bit position code word to be decoded in the bitstream
 * @param setup		pointer to the decoder setup
 *
 * @returns bit index of the next code word in the bitstream on success; returns
 *	negative in case of erroneous input; returns CMP_ERROR_SMALL_BUF if the
 *	bitstream buffer is too small to read the value from the bitstream
 *
 */

static int decode_none(uint32_t *decoded_value, int stream_pos,
		       const struct decoder_setup *setup)
{
	stream_pos = get_n_bits32(decoded_value, setup->encoder_par1, stream_pos,
				  setup->bitstream_adr, setup->max_stream_len);

	return stream_pos;
}


/**
 * @brief remap an unsigned value back to a signed value
 * @note this is the reverse function of map_to_pos()
 *
 * @param value_to_unmap	unsigned value to remap
 *
 * @returns the signed remapped value
 */

static uint32_t re_map_to_pos(uint32_t value_to_unmap)
{
	if (value_to_unmap & 0x1) { /* if uneven */
		if (value_to_unmap == 0xFFFFFFFF) /* catch overflow */
			return 0x80000000;
		return -((value_to_unmap + 1) / 2);
	} else {
		return value_to_unmap / 2;
	}
}


/**
 * @brief decompress the next code word in the bitstream and decorate it with
 *	the model
 *
 * @param decoded_value	pointer to the decoded value
 * @param model		model of the decoded_value (0 if not used)
 * @param stream_pos	start bit position code word to be decoded in the bitstream
 * @param setup		pointer to the decoder setup
 *
 * @returns bit index of the next code word in the bitstream on success; returns
 *	negative in case of erroneous input; returns CMP_ERROR_SMALL_BUF if the
 *	bitstream buffer is too small to read the value from the bitstream
 */

static int decode_value(uint32_t *decoded_value, uint32_t model,
			int stream_pos, const struct decoder_setup *setup)
{
	uint32_t mask = (~0U >> (32 - setup->max_data_bits)); /* mask the used bits */

	/* decode the next value from the bitstream */
	stream_pos = setup->decode_method_f(decoded_value, stream_pos, setup);
	if (stream_pos <= 0)
		return stream_pos;

	if (setup->decode_method_f == decode_none)
		/* we are done here in stuff mode */
		return stream_pos;

	/* map the unsigned decode value back to a signed value */
	*decoded_value = re_map_to_pos(*decoded_value);

	/* decorate data the data with the model */
	*decoded_value += round_fwd(model, setup->lossy_par);

	/* we mask only the used bits in case there is an overflow when adding the model */
	*decoded_value &= mask;

	/* inverse step of the lossy compression */
	*decoded_value = round_inv(*decoded_value, setup->lossy_par);

	return stream_pos;
}


/**
 * @brief configure a decoder setup structure to have a setup to decode a vale
 *
 * @param setup		pointer to the decoder setup
 * @param cmp_par	compression parameter
 * @param spillover	spillover_par parameter
 * @param lossy_par	lossy compression parameter
 * @param max_data_bits	how many bits are needed to represent the highest possible value
 * @param cfg		pointer to the compression configuration structure
 *
 * @returns 0 on success; otherwise error
 */

static int configure_decoder_setup(struct decoder_setup *setup,
				   uint32_t cmp_par, uint32_t spillover,
				   uint32_t lossy_par, uint32_t max_data_bits,
				   const struct cmp_cfg *cfg)
{
	if (multi_escape_mech_is_used(cfg->cmp_mode))
		setup->decode_method_f = &decode_multi;
	else if (zero_escape_mech_is_used(cfg->cmp_mode))
		setup->decode_method_f = &decode_zero;
	else if (cfg->cmp_mode == CMP_MODE_STUFF)
		setup->decode_method_f = &decode_none;
	else {
		setup->decode_method_f = NULL;
		debug_print("Error: Compression mode not supported.\n");
		return -1;
	}

	setup->bitstream_adr = cfg->icu_output_buf; /* start address of the compressed data bitstream */
	if (cfg->buffer_length & 0x3) {
		debug_print("Error: The length of the compressed data is not a multiple of 4 bytes.");
		return -1;
	}
	setup->max_stream_len = (cfg->buffer_length) * CHAR_BIT;  /* maximum length of the bitstream/icu_output_buf in bits */
	setup->encoder_par1 = cmp_par; /* encoding parameter 1 */
	if (ilog_2(cmp_par) < 0)
		return -1;
	setup->encoder_par2 = ilog_2(cmp_par); /* encoding parameter 2 */
	setup->outlier_par = spillover; /* outlier parameter */
	setup->lossy_par = lossy_par; /* lossy compression parameter */
	setup->model_value = cfg->model_value; /* model value parameter */
	setup->max_data_bits = max_data_bits; /* how many bits are needed to represent the highest possible value */
	setup->decode_cw_f = select_decoder(cmp_par);

	return 0;
}


/**
 * @brief decompress imagette data
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @returns bit position of the last read bit in the bitstream on success;
 *	returns negative on error, returns CMP_ERROR_SMALL_BUF if the bitstream
 *	buffer is too small to read the value from the bitstream
 */

static int decompress_imagette(struct cmp_cfg *cfg)
{
	int err;
	size_t i;
	int stream_pos = 0;
	struct decoder_setup setup;
	uint16_t *decompressed_data = cfg->input_buf;
	uint16_t *model_buf = cfg->model_buf;
	uint16_t *up_model_buf = cfg->icu_new_model_buf;
	uint32_t decoded_value = 0;
	uint16_t model;

	err = configure_decoder_setup(&setup, cfg->golomb_par, cfg->spill,
				      cfg->round, max_used_bits.nc_imagette, cfg);
	if (err)
		return -1;


	for (i = 0; i < cfg->samples; i++) {
		if (model_mode_is_used(cfg->cmp_mode))
			model = model_buf[i];
		else
			model = decoded_value;

		stream_pos = decode_value(&decoded_value, model, stream_pos, &setup);
		if (stream_pos <= 0)
			return stream_pos;
		decompressed_data[i] = decoded_value;

		if (up_model_buf) {
			up_model_buf[i] = cmp_up_model(decoded_value, model,
						       cfg->model_value, setup.lossy_par);
		}
	}

	return stream_pos;
}


/**
 * @brief decompress the multi-entry packet header structure and sets the data,
 *	model and up_model pointers to the data after the header
 *
 * @param data		pointer to a pointer pointing to the data to be compressed
 * @param model		pointer to a pointer pointing to the model of the data
 * @param up_model	pointer to a pointer pointing to the updated model buffer
 * @param cfg		pointer to the compression configuration structure
 *
 * @returns the bit length of the bitstream on success
 *
 * @note the (void **) cast relies on all pointer types having the same internal
 *	representation which is common, but not universal; http://www.c-faq.com/ptrs/genericpp.html
 */

static int decompress_multi_entry_hdr(void **data, void **model, void **up_model,
				      const struct cmp_cfg *cfg)
{
	if (cfg->buffer_length < MULTI_ENTRY_HDR_SIZE)
		return -1;

	if (*data) {
		if (cfg->icu_output_buf)
			memcpy(*data, cfg->icu_output_buf, MULTI_ENTRY_HDR_SIZE);
		*data = (uint8_t *)*data + MULTI_ENTRY_HDR_SIZE;
	}

	if (*model) {
		if (cfg->icu_output_buf)
			memcpy(*model, cfg->icu_output_buf, MULTI_ENTRY_HDR_SIZE);
		*model = (uint8_t *)*model + MULTI_ENTRY_HDR_SIZE;
	}

	if (*up_model) {
		if (cfg->icu_output_buf)
			memcpy(*up_model, cfg->icu_output_buf, MULTI_ENTRY_HDR_SIZE);
		*up_model = (uint8_t *)*up_model + MULTI_ENTRY_HDR_SIZE;
	}

	return MULTI_ENTRY_HDR_SIZE * CHAR_BIT;
}


/**
 * @brief decompress short normal light flux (S_FX) data
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @returns bit position of the last read bit in the bitstream on success;
 *	returns negative on error, returns CMP_ERROR_SMALL_BUF if the bitstream
 *	buffer is too small to read the value from the bitstream
 */

static int decompress_s_fx(const struct cmp_cfg *cfg)
{
	size_t i;
	int stream_pos = 0;
	uint32_t decoded_value;
	struct decoder_setup setup_exp_flags, setup_fx;
	struct s_fx *data_buf = cfg->input_buf;
	struct s_fx *model_buf = cfg->model_buf;
	struct s_fx *up_model_buf = NULL;
	struct s_fx *next_model_p;
	struct s_fx model;

	if (model_mode_is_used(cfg->cmp_mode))
		up_model_buf = cfg->icu_new_model_buf;

	stream_pos = decompress_multi_entry_hdr((void **)&data_buf, (void **)&model_buf,
						(void **)&up_model_buf, cfg);

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	if (configure_decoder_setup(&setup_exp_flags, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				    cfg->round, max_used_bits.s_exp_flags, cfg))
		return -1;
	if (configure_decoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				    cfg->round, max_used_bits.s_fx, cfg))
		return -1;

	for (i = 0; ; i++) {
		stream_pos = decode_value(&decoded_value, model.exp_flags,
					  stream_pos, &setup_exp_flags);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].exp_flags = decoded_value;

		stream_pos = decode_value(&decoded_value, model.fx, stream_pos,
					  &setup_fx);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].fx = decoded_value;

		if (up_model_buf) {
			up_model_buf[i].exp_flags = cmp_up_model(data_buf[i].exp_flags, model.exp_flags,
								 cfg->model_value, setup_exp_flags.lossy_par);
			up_model_buf[i].fx = cmp_up_model(data_buf[i].fx, model.fx,
							  cfg->model_value, setup_fx.lossy_par);
		}

		if (i >= cfg->samples-1)
			break;

		model = next_model_p[i];
	}
	return stream_pos;
}


/**
 * @brief decompress S_FX_EFX data
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @returns bit position of the last read bit in the bitstream on success;
 *	returns negative on error, returns CMP_ERROR_SMALL_BUF if the bitstream
 *	buffer is too small to read the value from the bitstream
 */

static int decompress_s_fx_efx(const struct cmp_cfg *cfg)
{
	size_t i;
	int stream_pos = 0;
	uint32_t decoded_value;
	struct decoder_setup setup_exp_flags, setup_fx, setup_efx;
	struct s_fx_efx *data_buf = cfg->input_buf;
	struct s_fx_efx *model_buf = cfg->model_buf;
	struct s_fx_efx *up_model_buf = NULL;
	struct s_fx_efx *next_model_p;
	struct s_fx_efx model;

	if (model_mode_is_used(cfg->cmp_mode))
		up_model_buf = cfg->icu_new_model_buf;

	stream_pos = decompress_multi_entry_hdr((void **)&data_buf, (void **)&model_buf,
						(void **)&up_model_buf, cfg);

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	if (configure_decoder_setup(&setup_exp_flags, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				    cfg->round, max_used_bits.s_exp_flags, cfg))
		return -1;
	if (configure_decoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				    cfg->round, max_used_bits.s_fx, cfg))
		return -1;
	if (configure_decoder_setup(&setup_efx, cfg->cmp_par_efx, cfg->spill_efx,
				    cfg->round, max_used_bits.s_efx, cfg))
		return -1;

	for (i = 0; ; i++) {
		stream_pos = decode_value(&decoded_value, model.exp_flags,
					  stream_pos, &setup_exp_flags);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].exp_flags = decoded_value;

		stream_pos = decode_value(&decoded_value, model.fx, stream_pos,
					  &setup_fx);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].fx = decoded_value;

		stream_pos = decode_value(&decoded_value, model.efx, stream_pos,
					  &setup_efx);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].efx = decoded_value;

		if (up_model_buf) {
			up_model_buf[i].exp_flags = cmp_up_model(data_buf[i].exp_flags, model.exp_flags,
								 cfg->model_value, setup_exp_flags.lossy_par);
			up_model_buf[i].fx = cmp_up_model(data_buf[i].fx, model.fx,
							  cfg->model_value, setup_fx.lossy_par);
			up_model_buf[i].efx = cmp_up_model(data_buf[i].efx, model.efx,
							   cfg->model_value, setup_efx.lossy_par);
		}

		if (i >= cfg->samples-1)
			break;

		model = next_model_p[i];
	}
	return stream_pos;
}


/**
 * @brief decompress short S_FX_NCOB data
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @returns bit position of the last read bit in the bitstream on success;
 *	returns negative on error, returns CMP_ERROR_SMALL_BUF if the bitstream
 *	buffer is too small to read the value from the bitstream
 */

static int decompress_s_fx_ncob(const struct cmp_cfg *cfg)
{
	size_t i;
	int stream_pos = 0;
	uint32_t decoded_value;
	struct decoder_setup setup_exp_flags, setup_fx, setup_ncob;
	struct s_fx_ncob *data_buf = cfg->input_buf;
	struct s_fx_ncob *model_buf = cfg->model_buf;
	struct s_fx_ncob *up_model_buf = NULL;
	struct s_fx_ncob *next_model_p;
	struct s_fx_ncob model;

	if (model_mode_is_used(cfg->cmp_mode))
		up_model_buf = cfg->icu_new_model_buf;

	stream_pos = decompress_multi_entry_hdr((void **)&data_buf, (void **)&model_buf,
						(void **)&up_model_buf, cfg);

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	if (configure_decoder_setup(&setup_exp_flags, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				    cfg->round, max_used_bits.s_exp_flags, cfg))
		return -1;
	if (configure_decoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				    cfg->round, max_used_bits.s_fx, cfg))
		return -1;
	if (configure_decoder_setup(&setup_ncob, cfg->cmp_par_ncob, cfg->spill_ncob,
				    cfg->round, max_used_bits.s_ncob, cfg))
		return -1;

	for (i = 0; ; i++) {
		stream_pos = decode_value(&decoded_value, model.exp_flags,
					  stream_pos, &setup_exp_flags);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].exp_flags = decoded_value;

		stream_pos = decode_value(&decoded_value, model.fx, stream_pos,
					  &setup_fx);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].fx = decoded_value;

		stream_pos = decode_value(&decoded_value, model.ncob_x, stream_pos,
					  &setup_ncob);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].ncob_x = decoded_value;

		stream_pos = decode_value(&decoded_value, model.ncob_y, stream_pos,
					  &setup_ncob);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].ncob_y = decoded_value;

		if (up_model_buf) {
			up_model_buf[i].exp_flags = cmp_up_model(data_buf[i].exp_flags, model.exp_flags,
								 cfg->model_value, setup_exp_flags.lossy_par);
			up_model_buf[i].fx = cmp_up_model(data_buf[i].fx, model.fx,
							  cfg->model_value, setup_fx.lossy_par);
			up_model_buf[i].ncob_x = cmp_up_model(data_buf[i].ncob_x, model.ncob_x,
							      cfg->model_value, setup_ncob.lossy_par);
			up_model_buf[i].ncob_y = cmp_up_model(data_buf[i].ncob_y, model.ncob_y,
							      cfg->model_value, setup_ncob.lossy_par);
		}

		if (i >= cfg->samples-1)
			break;

		model = next_model_p[i];
	}
	return stream_pos;
}


/**
 * @brief decompress short S_FX_NCOB_ECOB data
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @returns bit position of the last read bit in the bitstream on success;
 *	returns negative on error, returns CMP_ERROR_SMALL_BUF if the bitstream
 *	buffer is too small to read the value from the bitstream
 */

static int decompress_s_fx_efx_ncob_ecob(const struct cmp_cfg *cfg)
{
	size_t i;
	int stream_pos = 0;
	uint32_t decoded_value;
	struct decoder_setup setup_exp_flags, setup_fx, setup_ncob, setup_efx, setup_ecob;
	struct s_fx_efx_ncob_ecob *data_buf = cfg->input_buf;
	struct s_fx_efx_ncob_ecob *model_buf = cfg->model_buf;
	struct s_fx_efx_ncob_ecob *up_model_buf = NULL;
	struct s_fx_efx_ncob_ecob *next_model_p;
	struct s_fx_efx_ncob_ecob model;

	if (model_mode_is_used(cfg->cmp_mode))
		up_model_buf = cfg->icu_new_model_buf;

	stream_pos = decompress_multi_entry_hdr((void **)&data_buf, (void **)&model_buf,
						(void **)&up_model_buf, cfg);

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	if (configure_decoder_setup(&setup_exp_flags, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				    cfg->round, max_used_bits.s_exp_flags, cfg))
		return -1;
	if (configure_decoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				    cfg->round, max_used_bits.s_fx, cfg))
		return -1;
	if (configure_decoder_setup(&setup_ncob, cfg->cmp_par_ncob, cfg->spill_ncob,
				    cfg->round, max_used_bits.s_ncob, cfg))
		return -1;
	if (configure_decoder_setup(&setup_efx, cfg->cmp_par_efx, cfg->spill_efx,
				    cfg->round, max_used_bits.s_efx, cfg))
		return -1;
	if (configure_decoder_setup(&setup_ecob, cfg->cmp_par_ecob, cfg->spill_ecob,
				    cfg->round, max_used_bits.s_ecob, cfg))
		return -1;

	for (i = 0; ; i++) {
		stream_pos = decode_value(&decoded_value, model.exp_flags,
					  stream_pos, &setup_exp_flags);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].exp_flags = decoded_value;

		stream_pos = decode_value(&decoded_value, model.fx, stream_pos,
					  &setup_fx);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].fx = decoded_value;

		stream_pos = decode_value(&decoded_value, model.ncob_x, stream_pos,
					  &setup_ncob);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].ncob_x = decoded_value;

		stream_pos = decode_value(&decoded_value, model.ncob_y, stream_pos,
					  &setup_ncob);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].ncob_y = decoded_value;

		stream_pos = decode_value(&decoded_value, model.efx, stream_pos,
					  &setup_efx);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].efx = decoded_value;

		stream_pos = decode_value(&decoded_value, model.ecob_x, stream_pos,
					  &setup_ecob);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].ecob_x = decoded_value;

		stream_pos = decode_value(&decoded_value, model.ecob_y, stream_pos,
					  &setup_ecob);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].ecob_y = decoded_value;

		if (up_model_buf) {
			up_model_buf[i].exp_flags = cmp_up_model(data_buf[i].exp_flags, model.exp_flags,
								 cfg->model_value, setup_exp_flags.lossy_par);
			up_model_buf[i].fx = cmp_up_model(data_buf[i].fx, model.fx,
							  cfg->model_value, setup_fx.lossy_par);
			up_model_buf[i].ncob_x = cmp_up_model(data_buf[i].ncob_x, model.ncob_x,
							      cfg->model_value, setup_ncob.lossy_par);
			up_model_buf[i].ncob_y = cmp_up_model(data_buf[i].ncob_y, model.ncob_y,
							      cfg->model_value, setup_ncob.lossy_par);
			up_model_buf[i].efx = cmp_up_model(data_buf[i].efx, model.efx,
							   cfg->model_value, setup_efx.lossy_par);
			up_model_buf[i].ecob_x = cmp_up_model(data_buf[i].ecob_x, model.ecob_x,
							      cfg->model_value, setup_ecob.lossy_par);
			up_model_buf[i].ecob_y = cmp_up_model(data_buf[i].ecob_y, model.ecob_y,
							      cfg->model_value, setup_ecob.lossy_par);
		}

		if (i >= cfg->samples-1)
			break;

		model = next_model_p[i];
	}
	return stream_pos;
}


/**
 * @brief decompress fast normal light flux (F_FX) data
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @returns bit position of the last read bit in the bitstream on success;
 *	returns negative on error, returns CMP_ERROR_SMALL_BUF if the bitstream
 *	buffer is too small to read the value from the bitstream
 */

static int decompress_f_fx(const struct cmp_cfg *cfg)
{
	size_t i;
	int stream_pos = 0;
	uint32_t decoded_value;
	struct decoder_setup setup_fx;
	struct f_fx *data_buf = cfg->input_buf;
	struct f_fx *model_buf = cfg->model_buf;
	struct f_fx *up_model_buf = NULL;
	struct f_fx *next_model_p;
	struct f_fx model;

	if (model_mode_is_used(cfg->cmp_mode))
		up_model_buf = cfg->icu_new_model_buf;

	stream_pos = decompress_multi_entry_hdr((void **)&data_buf, (void **)&model_buf,
						(void **)&up_model_buf, cfg);

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	if (configure_decoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				    cfg->round, max_used_bits.f_fx, cfg))
		return -1;

	for (i = 0; ; i++) {
		stream_pos = decode_value(&decoded_value, model.fx, stream_pos,
					  &setup_fx);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].fx = decoded_value;

		if (up_model_buf)
			up_model_buf[i].fx = cmp_up_model(data_buf[i].fx, model.fx,
							  cfg->model_value, setup_fx.lossy_par);

		if (i >= cfg->samples-1)
			break;

		model = next_model_p[i];
	}
	return stream_pos;
}


/**
 * @brief decompress F_FX_EFX data
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @returns bit position of the last read bit in the bitstream on success;
 *	returns negative on error, returns CMP_ERROR_SMALL_BUF if the bitstream
 *	buffer is too small to read the value from the bitstream
 */

static int decompress_f_fx_efx(const struct cmp_cfg *cfg)
{
	size_t i;
	int stream_pos = 0;
	uint32_t decoded_value;
	struct decoder_setup setup_fx, setup_efx;
	struct f_fx_efx *data_buf = cfg->input_buf;
	struct f_fx_efx *model_buf = cfg->model_buf;
	struct f_fx_efx *up_model_buf = NULL;
	struct f_fx_efx *next_model_p;
	struct f_fx_efx model;

	if (model_mode_is_used(cfg->cmp_mode))
		up_model_buf = cfg->icu_new_model_buf;

	stream_pos = decompress_multi_entry_hdr((void **)&data_buf, (void **)&model_buf,
						(void **)&up_model_buf, cfg);

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	if (configure_decoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				    cfg->round, max_used_bits.f_fx, cfg))
		return -1;
	if (configure_decoder_setup(&setup_efx, cfg->cmp_par_efx, cfg->spill_efx,
				    cfg->round, max_used_bits.f_efx, cfg))
		return -1;

	for (i = 0; ; i++) {
		stream_pos = decode_value(&decoded_value, model.fx, stream_pos,
					  &setup_fx);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].fx = decoded_value;

		stream_pos = decode_value(&decoded_value, model.efx, stream_pos,
					  &setup_efx);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].efx = decoded_value;

		if (up_model_buf) {
			up_model_buf[i].fx = cmp_up_model(data_buf[i].fx, model.fx,
							  cfg->model_value, setup_fx.lossy_par);
			up_model_buf[i].efx = cmp_up_model(data_buf[i].efx, model.efx,
							   cfg->model_value, setup_efx.lossy_par);
		}

		if (i >= cfg->samples-1)
			break;

		model = next_model_p[i];
	}
	return stream_pos;
}


/**
 * @brief decompress short F_FX_NCOB data
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @returns bit position of the last read bit in the bitstream on success;
 *	returns negative on error, returns CMP_ERROR_SMALL_BUF if the bitstream
 *	buffer is too small to read the value from the bitstream
 */

static int decompress_f_fx_ncob(const struct cmp_cfg *cfg)
{
	size_t i;
	int stream_pos = 0;
	uint32_t decoded_value;
	struct decoder_setup setup_fx, setup_ncob;
	struct f_fx_ncob *data_buf = cfg->input_buf;
	struct f_fx_ncob *model_buf = cfg->model_buf;
	struct f_fx_ncob *up_model_buf = NULL;
	struct f_fx_ncob *next_model_p;
	struct f_fx_ncob model;

	if (model_mode_is_used(cfg->cmp_mode))
		up_model_buf = cfg->icu_new_model_buf;

	stream_pos = decompress_multi_entry_hdr((void **)&data_buf, (void **)&model_buf,
						(void **)&up_model_buf, cfg);

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	if (configure_decoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				    cfg->round, max_used_bits.f_fx, cfg))
		return -1;
	if (configure_decoder_setup(&setup_ncob, cfg->cmp_par_ncob, cfg->spill_ncob,
				    cfg->round, max_used_bits.f_ncob, cfg))
		return -1;

	for (i = 0; ; i++) {
		stream_pos = decode_value(&decoded_value, model.fx, stream_pos,
					  &setup_fx);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].fx = decoded_value;

		stream_pos = decode_value(&decoded_value, model.ncob_x, stream_pos,
					  &setup_ncob);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].ncob_x = decoded_value;

		stream_pos = decode_value(&decoded_value, model.ncob_y, stream_pos,
					  &setup_ncob);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].ncob_y = decoded_value;

		if (up_model_buf) {
			up_model_buf[i].fx = cmp_up_model(data_buf[i].fx, model.fx,
							  cfg->model_value, setup_fx.lossy_par);
			up_model_buf[i].ncob_x = cmp_up_model(data_buf[i].ncob_x, model.ncob_x,
							      cfg->model_value, setup_ncob.lossy_par);
			up_model_buf[i].ncob_y = cmp_up_model(data_buf[i].ncob_y, model.ncob_y,
							      cfg->model_value, setup_ncob.lossy_par);
		}

		if (i >= cfg->samples-1)
			break;

		model = next_model_p[i];
	}
	return stream_pos;
}


/**
 * @brief decompress short F_FX_NCOB_ECOB data
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @returns bit position of the last read bit in the bitstream on success;
 *	returns negative on error, returns CMP_ERROR_SMALL_BUF if the bitstream
 *	buffer is too small to read the value from the bitstream
 */

static int decompress_f_fx_efx_ncob_ecob(const struct cmp_cfg *cfg)
{
	size_t i;
	int stream_pos = 0;
	uint32_t decoded_value;
	struct decoder_setup setup_fx, setup_ncob, setup_efx, setup_ecob;
	struct f_fx_efx_ncob_ecob *data_buf = cfg->input_buf;
	struct f_fx_efx_ncob_ecob *model_buf = cfg->model_buf;
	struct f_fx_efx_ncob_ecob *up_model_buf = NULL;
	struct f_fx_efx_ncob_ecob *next_model_p;
	struct f_fx_efx_ncob_ecob model;

	if (model_mode_is_used(cfg->cmp_mode))
		up_model_buf = cfg->icu_new_model_buf;

	stream_pos = decompress_multi_entry_hdr((void **)&data_buf, (void **)&model_buf,
						(void **)&up_model_buf, cfg);

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	if (configure_decoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				    cfg->round, max_used_bits.f_fx, cfg))
		return -1;
	if (configure_decoder_setup(&setup_ncob, cfg->cmp_par_ncob, cfg->spill_ncob,
				    cfg->round, max_used_bits.f_ncob, cfg))
		return -1;
	if (configure_decoder_setup(&setup_efx, cfg->cmp_par_efx, cfg->spill_efx,
				    cfg->round, max_used_bits.f_efx, cfg))
		return -1;
	if (configure_decoder_setup(&setup_ecob, cfg->cmp_par_ecob, cfg->spill_ecob,
				    cfg->round, max_used_bits.f_ecob, cfg))
		return -1;

	for (i = 0; ; i++) {
		stream_pos = decode_value(&decoded_value, model.fx, stream_pos,
					  &setup_fx);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].fx = decoded_value;

		stream_pos = decode_value(&decoded_value, model.ncob_x, stream_pos,
					  &setup_ncob);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].ncob_x = decoded_value;

		stream_pos = decode_value(&decoded_value, model.ncob_y, stream_pos,
					  &setup_ncob);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].ncob_y = decoded_value;

		stream_pos = decode_value(&decoded_value, model.efx, stream_pos,
					  &setup_efx);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].efx = decoded_value;

		stream_pos = decode_value(&decoded_value, model.ecob_x, stream_pos,
					  &setup_ecob);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].ecob_x = decoded_value;

		stream_pos = decode_value(&decoded_value, model.ecob_y, stream_pos,
					  &setup_ecob);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].ecob_y = decoded_value;

		if (up_model_buf) {
			up_model_buf[i].fx = cmp_up_model(data_buf[i].fx, model.fx,
				cfg->model_value, setup_fx.lossy_par);
			up_model_buf[i].ncob_x = cmp_up_model(data_buf[i].ncob_x, model.ncob_x,
				cfg->model_value, setup_ncob.lossy_par);
			up_model_buf[i].ncob_y = cmp_up_model(data_buf[i].ncob_y, model.ncob_y,
				cfg->model_value, setup_ncob.lossy_par);
			up_model_buf[i].efx = cmp_up_model(data_buf[i].efx, model.efx,
				cfg->model_value, setup_efx.lossy_par);
			up_model_buf[i].ecob_x = cmp_up_model(data_buf[i].ecob_x, model.ecob_x,
				cfg->model_value, setup_ecob.lossy_par);
			up_model_buf[i].ecob_y = cmp_up_model(data_buf[i].ecob_y, model.ecob_y,
				cfg->model_value, setup_ecob.lossy_par);
		}

		if (i >= cfg->samples-1)
			break;

		model = next_model_p[i];
	}
	return stream_pos;
}


/**
 * @brief decompress long normal light flux (L_FX) data
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @returns bit position of the last read bit in the bitstream on success;
 *	returns negative on error, returns CMP_ERROR_SMALL_BUF if the bitstream
 *	buffer is too small to read the value from the bitstream
 */

static int decompress_l_fx(const struct cmp_cfg *cfg)
{
	size_t i;
	int stream_pos = 0;
	uint32_t decoded_value;
	struct decoder_setup setup_exp_flags, setup_fx, setup_fx_var;
	struct l_fx *data_buf = cfg->input_buf;
	struct l_fx *model_buf = cfg->model_buf;
	struct l_fx *up_model_buf = NULL;
	struct l_fx *next_model_p;
	struct l_fx model;

	if (model_mode_is_used(cfg->cmp_mode))
		up_model_buf = cfg->icu_new_model_buf;

	stream_pos = decompress_multi_entry_hdr((void **)&data_buf, (void **)&model_buf,
						(void **)&up_model_buf, cfg);

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	if (configure_decoder_setup(&setup_exp_flags, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				    cfg->round, max_used_bits.l_exp_flags, cfg))
		return -1;
	if (configure_decoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				    cfg->round, max_used_bits.l_fx, cfg))
		return -1;
	if (configure_decoder_setup(&setup_fx_var, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				    cfg->round, max_used_bits.l_fx_variance, cfg))
		return -1;

	for (i = 0; ; i++) {
		stream_pos = decode_value(&decoded_value, model.exp_flags,
					  stream_pos, &setup_exp_flags);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].exp_flags = decoded_value;

		stream_pos = decode_value(&decoded_value, model.fx, stream_pos,
					  &setup_fx);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].fx = decoded_value;

		stream_pos = decode_value(&decoded_value, model.fx_variance, stream_pos,
					  &setup_fx_var);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].fx_variance = decoded_value;

		if (up_model_buf) {
			up_model_buf[i].exp_flags = cmp_up_model(data_buf[i].exp_flags, model.exp_flags,
								 cfg->model_value, setup_exp_flags.lossy_par);
			up_model_buf[i].fx = cmp_up_model(data_buf[i].fx, model.fx,
							  cfg->model_value, setup_fx.lossy_par);
			up_model_buf[i].fx_variance = cmp_up_model(data_buf[i].fx_variance, model.fx_variance,
								   cfg->model_value, setup_fx_var.lossy_par);
		}

		if (i >= cfg->samples-1)
			break;

		model = next_model_p[i];
	}
	return stream_pos;
}


/**
 * @brief decompress L_FX_EFX data
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @returns bit position of the last read bit in the bitstream on success;
 *	returns negative on error, returns CMP_ERROR_SMALL_BUF if the bitstream
 *	buffer is too small to read the value from the bitstream
 */

static int decompress_l_fx_efx(const struct cmp_cfg *cfg)
{
	size_t i;
	int stream_pos = 0;
	uint32_t decoded_value;
	struct decoder_setup setup_exp_flags, setup_fx, setup_efx, setup_fx_var;
	struct l_fx_efx *data_buf = cfg->input_buf;
	struct l_fx_efx *model_buf = cfg->model_buf;
	struct l_fx_efx *up_model_buf = NULL;
	struct l_fx_efx *next_model_p;
	struct l_fx_efx model;

	if (model_mode_is_used(cfg->cmp_mode))
		up_model_buf = cfg->icu_new_model_buf;

	stream_pos = decompress_multi_entry_hdr((void **)&data_buf, (void **)&model_buf,
						(void **)&up_model_buf, cfg);

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	if (configure_decoder_setup(&setup_exp_flags, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				    cfg->round, max_used_bits.l_exp_flags, cfg))
		return -1;
	if (configure_decoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				    cfg->round, max_used_bits.l_fx, cfg))
		return -1;
	if (configure_decoder_setup(&setup_efx, cfg->cmp_par_efx, cfg->spill_efx,
				    cfg->round, max_used_bits.l_efx, cfg))
		return -1;
	if (configure_decoder_setup(&setup_fx_var, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				    cfg->round, max_used_bits.l_fx_variance, cfg))
		return -1;

	for (i = 0; ; i++) {
		stream_pos = decode_value(&decoded_value, model.exp_flags,
					  stream_pos, &setup_exp_flags);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].exp_flags = decoded_value;

		stream_pos = decode_value(&decoded_value, model.fx, stream_pos,
					  &setup_fx);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].fx = decoded_value;

		stream_pos = decode_value(&decoded_value, model.efx, stream_pos,
					  &setup_efx);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].efx = decoded_value;

		stream_pos = decode_value(&decoded_value, model.fx_variance, stream_pos,
					  &setup_fx_var);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].fx_variance = decoded_value;

		if (up_model_buf) {
			up_model_buf[i].exp_flags = cmp_up_model(data_buf[i].exp_flags, model.exp_flags,
								 cfg->model_value, setup_exp_flags.lossy_par);
			up_model_buf[i].fx = cmp_up_model(data_buf[i].fx, model.fx,
							  cfg->model_value, setup_fx.lossy_par);
			up_model_buf[i].efx = cmp_up_model(data_buf[i].efx, model.efx,
							   cfg->model_value, setup_efx.lossy_par);
			up_model_buf[i].fx_variance = cmp_up_model(data_buf[i].fx_variance, model.fx_variance,
								   cfg->model_value, setup_fx_var.lossy_par);
		}

		if (i >= cfg->samples-1)
			break;

		model = next_model_p[i];
	}
	return stream_pos;
}


/**
 * @brief decompress L_FX_NCOB data
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @returns bit position of the last read bit in the bitstream on success;
 *	returns negative on error, returns CMP_ERROR_SMALL_BUF if the bitstream
 *	buffer is too small to read the value from the bitstream
 */

static int decompress_l_fx_ncob(const struct cmp_cfg *cfg)
{
	size_t i;
	int stream_pos = 0;
	uint32_t decoded_value;
	struct decoder_setup setup_exp_flags, setup_fx, setup_ncob,
			     setup_fx_var, setup_cob_var;
	struct l_fx_ncob *data_buf = cfg->input_buf;
	struct l_fx_ncob *model_buf = cfg->model_buf;
	struct l_fx_ncob *up_model_buf = NULL;
	struct l_fx_ncob *next_model_p;
	struct l_fx_ncob model;

	if (model_mode_is_used(cfg->cmp_mode))
		up_model_buf = cfg->icu_new_model_buf;

	stream_pos = decompress_multi_entry_hdr((void **)&data_buf, (void **)&model_buf,
						(void **)&up_model_buf, cfg);

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	if (configure_decoder_setup(&setup_exp_flags, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				    cfg->round, max_used_bits.l_exp_flags, cfg))
		return -1;
	if (configure_decoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				    cfg->round, max_used_bits.l_fx, cfg))
		return -1;
	if (configure_decoder_setup(&setup_ncob, cfg->cmp_par_ncob, cfg->spill_ncob,
				    cfg->round, max_used_bits.l_ncob, cfg))
		return -1;
	if (configure_decoder_setup(&setup_fx_var, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				    cfg->round, max_used_bits.l_fx_variance, cfg))
		return -1;
	if (configure_decoder_setup(&setup_cob_var, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				    cfg->round, max_used_bits.l_cob_variance, cfg))
		return -1;

	for (i = 0; ; i++) {
		stream_pos = decode_value(&decoded_value, model.exp_flags,
					  stream_pos, &setup_exp_flags);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].exp_flags = decoded_value;

		stream_pos = decode_value(&decoded_value, model.fx, stream_pos,
					  &setup_fx);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].fx = decoded_value;

		stream_pos = decode_value(&decoded_value, model.ncob_x,
					  stream_pos, &setup_ncob);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].ncob_x = decoded_value;

		stream_pos = decode_value(&decoded_value, model.ncob_y,
					  stream_pos, &setup_ncob);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].ncob_y = decoded_value;

		stream_pos = decode_value(&decoded_value, model.fx_variance,
					  stream_pos, &setup_fx_var);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].fx_variance = decoded_value;

		stream_pos = decode_value(&decoded_value, model.cob_x_variance,
					  stream_pos, &setup_cob_var);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].cob_x_variance = decoded_value;

		stream_pos = decode_value(&decoded_value, model.cob_y_variance,
					  stream_pos, &setup_cob_var);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].cob_y_variance = decoded_value;

		if (up_model_buf) {
			up_model_buf[i].exp_flags = cmp_up_model(data_buf[i].exp_flags, model.exp_flags,
				cfg->model_value, setup_exp_flags.lossy_par);
			up_model_buf[i].fx = cmp_up_model(data_buf[i].fx, model.fx,
				cfg->model_value, setup_fx.lossy_par);
			up_model_buf[i].ncob_x = cmp_up_model(data_buf[i].ncob_x, model.ncob_x,
				cfg->model_value, setup_ncob.lossy_par);
			up_model_buf[i].ncob_y = cmp_up_model(data_buf[i].ncob_y, model.ncob_y,
				cfg->model_value, setup_ncob.lossy_par);
			up_model_buf[i].fx_variance = cmp_up_model(data_buf[i].fx_variance, model.fx_variance,
				cfg->model_value, setup_fx_var.lossy_par);
			up_model_buf[i].cob_x_variance = cmp_up_model(data_buf[i].cob_x_variance, model.cob_x_variance,
				cfg->model_value, setup_cob_var.lossy_par);
			up_model_buf[i].cob_y_variance = cmp_up_model(data_buf[i].cob_y_variance, model.cob_y_variance,
				cfg->model_value, setup_cob_var.lossy_par);
		}

		if (i >= cfg->samples-1)
			break;

		model = next_model_p[i];
	}
	return stream_pos;
}


/**
 * @brief decompress L_FX_EFX_NCOB_ECOB data
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @returns bit position of the last read bit in the bitstream on success;
 *	returns negative on error, returns CMP_ERROR_SMALL_BUF if the bitstream
 *	buffer is too small to read the value from the bitstream
 */

static int decompress_l_fx_efx_ncob_ecob(const struct cmp_cfg *cfg)
{
	size_t i;
	int stream_pos = 0;
	uint32_t decoded_value;
	struct decoder_setup setup_exp_flags, setup_fx, setup_ncob, setup_efx,
			     setup_ecob, setup_fx_var, setup_cob_var;
	struct l_fx_efx_ncob_ecob *data_buf = cfg->input_buf;
	struct l_fx_efx_ncob_ecob *model_buf = cfg->model_buf;
	struct l_fx_efx_ncob_ecob *up_model_buf = NULL;
	struct l_fx_efx_ncob_ecob *next_model_p;
	struct l_fx_efx_ncob_ecob model;

	if (model_mode_is_used(cfg->cmp_mode))
		up_model_buf = cfg->icu_new_model_buf;

	stream_pos = decompress_multi_entry_hdr((void **)&data_buf, (void **)&model_buf,
						(void **)&up_model_buf, cfg);

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	if (configure_decoder_setup(&setup_exp_flags, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				    cfg->round, max_used_bits.l_exp_flags, cfg))
		return -1;
	if (configure_decoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				    cfg->round, max_used_bits.l_fx, cfg))
		return -1;
	if (configure_decoder_setup(&setup_ncob, cfg->cmp_par_ncob, cfg->spill_ncob,
				    cfg->round, max_used_bits.l_ncob, cfg))
		return -1;
	if (configure_decoder_setup(&setup_efx, cfg->cmp_par_efx, cfg->spill_efx,
				    cfg->round, max_used_bits.l_efx, cfg))
		return -1;
	if (configure_decoder_setup(&setup_ecob, cfg->cmp_par_ecob, cfg->spill_ecob,
				    cfg->round, max_used_bits.l_ecob, cfg))
		return -1;
	if (configure_decoder_setup(&setup_fx_var, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				    cfg->round, max_used_bits.l_fx_variance, cfg))
		return -1;
	if (configure_decoder_setup(&setup_cob_var, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				    cfg->round, max_used_bits.l_cob_variance, cfg))
		return -1;

	for (i = 0; ; i++) {
		stream_pos = decode_value(&decoded_value, model.exp_flags,
					  stream_pos, &setup_exp_flags);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].exp_flags = decoded_value;

		stream_pos = decode_value(&decoded_value, model.fx, stream_pos,
					  &setup_fx);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].fx = decoded_value;

		stream_pos = decode_value(&decoded_value, model.ncob_x,
					  stream_pos, &setup_ncob);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].ncob_x = decoded_value;

		stream_pos = decode_value(&decoded_value, model.ncob_y,
					  stream_pos, &setup_ncob);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].ncob_y = decoded_value;

		stream_pos = decode_value(&decoded_value, model.efx, stream_pos,
					  &setup_efx);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].efx = decoded_value;

		stream_pos = decode_value(&decoded_value, model.ecob_x,
					  stream_pos, &setup_ecob);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].ecob_x = decoded_value;

		stream_pos = decode_value(&decoded_value, model.ecob_y,
					  stream_pos, &setup_ecob);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].ecob_y = decoded_value;

		stream_pos = decode_value(&decoded_value, model.fx_variance,
					  stream_pos, &setup_fx_var);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].fx_variance = decoded_value;

		stream_pos = decode_value(&decoded_value, model.cob_x_variance,
					  stream_pos, &setup_cob_var);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].cob_x_variance = decoded_value;

		stream_pos = decode_value(&decoded_value, model.cob_y_variance,
					  stream_pos, &setup_cob_var);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].cob_y_variance = decoded_value;

		if (up_model_buf) {
			up_model_buf[i].exp_flags = cmp_up_model(data_buf[i].exp_flags, model.exp_flags,
				cfg->model_value, setup_exp_flags.lossy_par);
			up_model_buf[i].fx = cmp_up_model(data_buf[i].fx, model.fx,
				cfg->model_value, setup_fx.lossy_par);
			up_model_buf[i].ncob_x = cmp_up_model(data_buf[i].ncob_x, model.ncob_x,
				cfg->model_value, setup_ncob.lossy_par);
			up_model_buf[i].ncob_y = cmp_up_model(data_buf[i].ncob_y, model.ncob_y,
				cfg->model_value, setup_ncob.lossy_par);
			up_model_buf[i].efx = cmp_up_model(data_buf[i].efx, model.efx,
				cfg->model_value, setup_efx.lossy_par);
			up_model_buf[i].ecob_x = cmp_up_model(data_buf[i].ecob_x, model.ecob_x,
				cfg->model_value, setup_ecob.lossy_par);
			up_model_buf[i].ecob_y = cmp_up_model(data_buf[i].ecob_y, model.ecob_y,
				cfg->model_value, setup_ecob.lossy_par);
			up_model_buf[i].fx_variance = cmp_up_model(data_buf[i].fx_variance, model.fx_variance,
				cfg->model_value, setup_fx_var.lossy_par);
			up_model_buf[i].cob_x_variance = cmp_up_model(data_buf[i].cob_x_variance, model.cob_x_variance,
				cfg->model_value, setup_cob_var.lossy_par);
			up_model_buf[i].cob_y_variance = cmp_up_model(data_buf[i].cob_y_variance, model.cob_y_variance,
				cfg->model_value, setup_cob_var.lossy_par);
		}

		if (i >= cfg->samples-1)
			break;

		model = next_model_p[i];
	}
	return stream_pos;
}


/**
 * @brief decompress N-CAM offset data
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @returns bit position of the last read bit in the bitstream on success;
 *	returns negative on error, returns CMP_ERROR_SMALL_BUF if the bitstream
 *	buffer is too small to read the value from the bitstream
 */

static int decompress_nc_offset(const struct cmp_cfg *cfg)
{
	size_t i;
	int stream_pos = 0;
	uint32_t decoded_value;
	struct decoder_setup setup_mean, setup_var;
	struct nc_offset *data_buf = cfg->input_buf;
	struct nc_offset *model_buf = cfg->model_buf;
	struct nc_offset *up_model_buf = NULL;
	struct nc_offset *next_model_p;
	struct nc_offset model;

	if (model_mode_is_used(cfg->cmp_mode))
		up_model_buf = cfg->icu_new_model_buf;

	stream_pos = decompress_multi_entry_hdr((void **)&data_buf, (void **)&model_buf,
						(void **)&up_model_buf, cfg);

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	if (configure_decoder_setup(&setup_mean, cfg->cmp_par_mean, cfg->spill_mean,
				    cfg->round, max_used_bits.nc_offset_mean, cfg))
		return -1;
	if (configure_decoder_setup(&setup_var, cfg->cmp_par_variance, cfg->spill_variance,
				    cfg->round, max_used_bits.nc_offset_variance, cfg))
		return -1;

	for (i = 0; ; i++) {
		stream_pos = decode_value(&decoded_value, model.mean, stream_pos,
					  &setup_mean);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].mean = decoded_value;

		stream_pos = decode_value(&decoded_value, model.variance, stream_pos,
					  &setup_var);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].variance = decoded_value;

		if (up_model_buf) {
			up_model_buf[i].mean = cmp_up_model(data_buf[i].mean,
				model.mean, cfg->model_value, setup_mean.lossy_par);
			up_model_buf[i].variance = cmp_up_model(data_buf[i].variance,
				model.variance, cfg->model_value, setup_var.lossy_par);
		}

		if (i >= cfg->samples-1)
			break;

		model = next_model_p[i];
	}
	return stream_pos;
}


/**
 * @brief decompress N-CAM background data
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @returns bit position of the last read bit in the bitstream on success;
 *	returns negative on error, returns CMP_ERROR_SMALL_BUF if the bitstream
 *	buffer is too small to read the value from the bitstream
 */

static int decompress_nc_background(const struct cmp_cfg *cfg)
{
	size_t i;
	int stream_pos = 0;
	uint32_t decoded_value;
	struct decoder_setup setup_mean, setup_var, setup_pix;
	struct nc_background *data_buf = cfg->input_buf;
	struct nc_background *model_buf = cfg->model_buf;
	struct nc_background *up_model_buf = NULL;
	struct nc_background *next_model_p;
	struct nc_background model;

	if (model_mode_is_used(cfg->cmp_mode))
		up_model_buf = cfg->icu_new_model_buf;

	stream_pos = decompress_multi_entry_hdr((void **)&data_buf, (void **)&model_buf,
						(void **)&up_model_buf, cfg);

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	if (configure_decoder_setup(&setup_mean, cfg->cmp_par_mean, cfg->spill_mean,
				    cfg->round, max_used_bits.nc_background_mean, cfg))
		return -1;
	if (configure_decoder_setup(&setup_var, cfg->cmp_par_variance, cfg->spill_variance,
				    cfg->round, max_used_bits.nc_background_variance, cfg))
		return -1;
	if (configure_decoder_setup(&setup_pix, cfg->cmp_par_pixels_error, cfg->spill_pixels_error,
				    cfg->round, max_used_bits.nc_background_outlier_pixels, cfg))
		return -1;

	for (i = 0; ; i++) {
		stream_pos = decode_value(&decoded_value, model.mean, stream_pos,
					  &setup_mean);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].mean = decoded_value;

		stream_pos = decode_value(&decoded_value, model.variance, stream_pos,
					  &setup_var);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].variance = decoded_value;

		stream_pos = decode_value(&decoded_value, model.outlier_pixels, stream_pos,
					  &setup_pix);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].outlier_pixels = decoded_value;

		if (up_model_buf) {
			up_model_buf[i].mean = cmp_up_model(data_buf[i].mean,
				model.mean, cfg->model_value, setup_mean.lossy_par);
			up_model_buf[i].variance = cmp_up_model(data_buf[i].variance,
				model.variance, cfg->model_value, setup_var.lossy_par);
			up_model_buf[i].outlier_pixels = cmp_up_model(data_buf[i].outlier_pixels,
				model.outlier_pixels, cfg->model_value, setup_pix.lossy_par);
		}

		if (i >= cfg->samples-1)
			break;

		model = next_model_p[i];
	}
	return stream_pos;
}


/**
 * @brief decompress N-CAM smearing data
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @returns bit position of the last read bit in the bitstream on success;
 *	returns negative on error, returns CMP_ERROR_SMALL_BUF if the bitstream
 *	buffer is too small to read the value from the bitstream
 */

static int decompress_smearing(const struct cmp_cfg *cfg)
{
	size_t i;
	int stream_pos = 0;
	uint32_t decoded_value;
	struct decoder_setup setup_mean, setup_var, setup_pix;
	struct smearing *data_buf = cfg->input_buf;
	struct smearing *model_buf = cfg->model_buf;
	struct smearing *up_model_buf = NULL;
	struct smearing *next_model_p;
	struct smearing model;

	if (model_mode_is_used(cfg->cmp_mode))
		up_model_buf = cfg->icu_new_model_buf;

	stream_pos = decompress_multi_entry_hdr((void **)&data_buf, (void **)&model_buf,
						(void **)&up_model_buf, cfg);

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	if (configure_decoder_setup(&setup_mean, cfg->cmp_par_mean, cfg->spill_mean,
				    cfg->round, max_used_bits.smearing_mean, cfg))
		return -1;
	if (configure_decoder_setup(&setup_var, cfg->cmp_par_variance, cfg->spill_variance,
				    cfg->round, max_used_bits.smearing_variance_mean, cfg))
		return -1;
	if (configure_decoder_setup(&setup_pix, cfg->cmp_par_pixels_error, cfg->spill_pixels_error,
				    cfg->round, max_used_bits.smearing_outlier_pixels, cfg))
		return -1;

	for (i = 0; ; i++) {
		stream_pos = decode_value(&decoded_value, model.mean, stream_pos,
					  &setup_mean);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].mean = decoded_value;

		stream_pos = decode_value(&decoded_value, model.variance_mean, stream_pos,
					  &setup_var);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].variance_mean = decoded_value;

		stream_pos = decode_value(&decoded_value, model.outlier_pixels, stream_pos,
					  &setup_pix);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].outlier_pixels = decoded_value;

		if (up_model_buf) {
			up_model_buf[i].mean = cmp_up_model(data_buf[i].mean,
				model.mean, cfg->model_value, setup_mean.lossy_par);
			up_model_buf[i].variance_mean = cmp_up_model(data_buf[i].variance_mean,
				model.variance_mean, cfg->model_value, setup_var.lossy_par);
			up_model_buf[i].outlier_pixels = cmp_up_model(data_buf[i].outlier_pixels,
				model.outlier_pixels, cfg->model_value, setup_pix.lossy_par);
		}

		if (i >= cfg->samples-1)
			break;

		model = next_model_p[i];
	}
	return stream_pos;
}


/**
 * @brief decompress the data based on a compression configuration
 *
 * @param cfg	pointer to a compression configuration
 *
 * @note cfg->buffer_length is measured in bytes (instead of samples as by the
 *	compression)
 *
 * @returns the size of the decompressed data on success; returns negative on failure
 * TODO: change return type to int32_t
 */

static int decompressed_data_internal(struct cmp_cfg *cfg)
{
	int data_size, strem_len_bit = -1;

	if (!cfg)
		return -1;

	if (!cfg->icu_output_buf)
		return -1;

	data_size = cmp_cal_size_of_data(cfg->samples, cfg->data_type);
	if (!cfg->input_buf || !data_size)
		return data_size;

	if (model_mode_is_used(cfg->cmp_mode))
		if (!cfg->model_buf)
			return -1;

	if (cfg->cmp_mode == CMP_MODE_RAW) {

		if ((unsigned int)data_size < cfg->buffer_length/CHAR_BIT)
			return -1;

		if (cfg->input_buf) {
			memcpy(cfg->input_buf, cfg->icu_output_buf, data_size);
			if (cmp_input_big_to_cpu_endianness(cfg->input_buf, data_size, cfg->data_type))
				return -1;
			strem_len_bit = data_size * CHAR_BIT;
		}

	} else {
		switch (cfg->data_type) {
		case DATA_TYPE_IMAGETTE:
		case DATA_TYPE_IMAGETTE_ADAPTIVE:
		case DATA_TYPE_SAT_IMAGETTE:
		case DATA_TYPE_SAT_IMAGETTE_ADAPTIVE:
		case DATA_TYPE_F_CAM_IMAGETTE:
		case DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE:
			strem_len_bit = decompress_imagette(cfg);
			break;
		case DATA_TYPE_S_FX:
			strem_len_bit = decompress_s_fx(cfg);
			break;
		case DATA_TYPE_S_FX_EFX:
			strem_len_bit = decompress_s_fx_efx(cfg);
			break;
		case DATA_TYPE_S_FX_NCOB:
			strem_len_bit = decompress_s_fx_ncob(cfg);
			break;
		case DATA_TYPE_S_FX_EFX_NCOB_ECOB:
			strem_len_bit = decompress_s_fx_efx_ncob_ecob(cfg);
			break;

		case DATA_TYPE_F_FX:
			strem_len_bit = decompress_f_fx(cfg);
			break;
		case DATA_TYPE_F_FX_EFX:
			strem_len_bit = decompress_f_fx_efx(cfg);
			break;
		case DATA_TYPE_F_FX_NCOB:
			strem_len_bit = decompress_f_fx_ncob(cfg);
			break;
		case DATA_TYPE_F_FX_EFX_NCOB_ECOB:
			strem_len_bit = decompress_f_fx_efx_ncob_ecob(cfg);
			break;

		case DATA_TYPE_L_FX:
			strem_len_bit = decompress_l_fx(cfg);
			break;
		case DATA_TYPE_L_FX_EFX:
			strem_len_bit = decompress_l_fx_efx(cfg);
			break;
		case DATA_TYPE_L_FX_NCOB:
			strem_len_bit = decompress_l_fx_ncob(cfg);
			break;
		case DATA_TYPE_L_FX_EFX_NCOB_ECOB:
			strem_len_bit = decompress_l_fx_efx_ncob_ecob(cfg);
			break;

		case DATA_TYPE_OFFSET:
			strem_len_bit = decompress_nc_offset(cfg);
			break;
		case DATA_TYPE_BACKGROUND:
			strem_len_bit = decompress_nc_background(cfg);
			break;
		case DATA_TYPE_SMEARING:
			strem_len_bit = decompress_smearing(cfg);
			break;

		case DATA_TYPE_F_CAM_OFFSET:
		case DATA_TYPE_F_CAM_BACKGROUND:
		case DATA_TYPE_UNKNOWN:
		default:
			strem_len_bit = -1;
			debug_print("Error: Compressed data type not supported.\n");
			break;
		}
	}
	if (strem_len_bit <= 0)
		return -1;

	return data_size;
}


/**
 * @brief decompress a compression entity
 *
 * @param ent			pointer to the compression entity to be decompressed
 * @param model_of_data		pointer to model data buffer (can be NULL if no
 *				model compression mode is used)
 * @param up_model_buf		pointer to store the updated model for the next model
 *				mode compression (can be the same as the model_of_data
 *				buffer for in-place update or NULL if updated model is not needed)
 * @param decompressed_data	pointer to the decompressed data buffer (can be NULL)
 *
 * @returns the size of the decompressed data on success; returns negative on failure
 */

int decompress_cmp_entiy(struct cmp_entity *ent, void *model_of_data,
			 void *up_model_buf, void *decompressed_data)
{
	int err;
	struct cmp_cfg cfg = {0};

	cfg.model_buf = model_of_data;
	cfg.icu_new_model_buf = up_model_buf;
	cfg.input_buf = decompressed_data;

	if (!ent)
		return -1;

	err = cmp_ent_read_header(ent, &cfg);
	if (err)
		return -1;

	return decompressed_data_internal(&cfg);
}


/**
 * @brief decompress RDCU compressed data without a compression entity header
 *
 * @param compressed_data	pointer to the RDCU compressed data (without a
 *				compression entity header)
 * @param model_of_data		pointer to model data buffer (can be NULL if no
 *				model compression mode is used)
 * @param up_model_buf		pointer to store the updated model for the next model
 *				mode compression (can be the same as the model_of_data
 *				buffer for in-place update or NULL if updated model is not needed)
 * @param decompressed_data	pointer to the decompressed data buffer (can be NULL)
 *
 * @returns the size of the decompressed data on success; returns negative on failure
 */

int decompress_rdcu_data(uint32_t *compressed_data, const struct cmp_info *info,
			 uint16_t *model_of_data, uint16_t *up_model_buf,
			 uint16_t *decompressed_data)

{
	struct cmp_cfg cfg = {0};

	if (!compressed_data)
		return -1;

	if (!info)
		return -1;

	if (info->cmp_err)
		return -1;

	cfg.data_type = DATA_TYPE_IMAGETTE;
	cfg.model_buf = model_of_data;
	cfg.icu_new_model_buf = up_model_buf;
	cfg.input_buf = decompressed_data;

	cfg.cmp_mode = info->cmp_mode_used;
	cfg.model_value = info->model_value_used;
	cfg.round = info->round_used;
	cfg.spill = info->spill_used;
	cfg.golomb_par = info->golomb_par_used;
	cfg.samples = info->samples_used;
	cfg.icu_output_buf = compressed_data;
	cfg.buffer_length = cmp_bit_to_4byte(info->cmp_size);

	return decompressed_data_internal(&cfg);
}
