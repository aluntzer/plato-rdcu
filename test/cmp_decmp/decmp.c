/**
 * @file   decmp.c
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
 * @brief software decompression library
 * @see Data Compression User Manual PLATO-UVIE-PL-UM-0001
 *
 * To decompress a compression entity (consisting of a compression entity header
 * and the compressed data) use the decompress_cmp_entiy() function.
 *
 * @warning not intended for use with the flight software
 */


#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <assert.h>

#include <byteorder.h>
#include <compiler.h>

#include <cmp_debug.h>
#include <cmp_support.h>
#include <cmp_entity.h>
#include <cmp_max_used_bits.h>
#include <cmp_max_used_bits_list.h>


#define MAX_CW_LEN_RDCU 16 /* maximum RDCU Golomb code word bit length */
#define MAX_CW_LEN_ICU 32 /* maximum ICU Golomb code word bit length */


const char *please_check_str = "Please check that the compression parameters match those used to compress the data and that the compressed data are not corrupted.\n";


/**
 * @brief function pointer to a code word decoder function
 */

typedef unsigned int(*decoder_ptr)(uint32_t, uint32_t, uint32_t, uint32_t *);


/**
 * @brief structure to hold a setup to encode a value
 */

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
	uint32_t max_data_bits; /* bit length of the decoded value */
	uint32_t max_cw_len; /* bit length of the longest possible code word */
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
	if (unlikely(~value == 0))  /* __builtin_clz(0) is undefined. */
		return 32;
	return (unsigned int)__builtin_clz(~value);
}


/**
 * @brief decode a Rice code word
 *
 * @param code_word	Rice code word bitstream starting at the MSb
 * @param m		Golomb parameter (not used)
 * @param log2_m	Rice parameter, must be the same used for encoding; is ilog_2(m)
 * @param decoded_cw	pointer where decoded value is written
 *
 * @returns the length of the decoded code word in bits (NOT the decoded value);
 *	failure if the return value is larger than 32
 */

static unsigned int rice_decoder(uint32_t code_word, uint32_t m, uint32_t log2_m,
				 uint32_t *decoded_cw)
{
	uint32_t q; /* quotient code */
	uint32_t r; /* remainder code */
	uint32_t rl = log2_m; /* length of the remainder code */
	uint32_t cw_len; /* length of the decoded code word in bits */

	(void)m; /* we don't need the Golomb parameter */

	assert(log2_m < 32);
	assert(decoded_cw != NULL);

	/* decode quotient unary code part */
	q = count_leading_ones(code_word);

	cw_len = q + 1 + rl; /* Number of 1's + following 0 + remainder length */

	/* get remainder code  */
	/* mask shift to prevented undefined behaviour in error case cw_len > 32 */
	code_word >>= (32 - cw_len) & 0x1FU;
	r = code_word & ((1U << rl) - 1);

	*decoded_cw = (q << rl) + r;

	return cw_len;
}


/**
 * @brief decode a Golomb code word
 *
 * @param code_word	Golomb code word bitstream starting at the MSb
 * @param m		Golomb parameter (have to be bigger than 0)
 * @param log2_m	is ilog_2(m) calculate outside function for better
 *			performance
 * @param decoded_cw	pointer where decoded value is written
 *
 * @returns the length of the decoded code word in bits (NOT the decoded value);
 *	failure if the return value is larger than 32
 */

static unsigned int golomb_decoder(uint32_t code_word, uint32_t m,
				   uint32_t log2_m, uint32_t *decoded_cw)
{
	uint32_t q;  /* quotient code */
	uint32_t r1; /* remainder code case 1 */
	uint32_t r2; /* remainder code case 2 */
	uint32_t r;  /* remainder code */
	uint32_t cutoff; /* cutoff between group 1 and 2 */
	uint32_t cw_len; /* length of the decoded code word in bits */

	assert(m > 0);
	assert(log2_m == ilog_2(m) && log2_m < 32);
	assert(decoded_cw != NULL);

	q = count_leading_ones(code_word); /* decode quotient unary code part */

	/* The behaviour is undefined if the right shift operand is greater than
	 * or equal to the length in bits of the shifted left operand, so we mask
	 * the right operand to avoid this case. (q = 32)
	 */
	code_word <<= (q & 0x1FU); /* shift out leading ones */
	code_word <<= 1; /* shift out zero in the quotient unary code */

	/* get the remainder code for both cases */
	r2 = code_word >> (32 - (log2_m + 1));
	r1 = r2 >> 1;

	cutoff = (0x2U << log2_m) - m; /* = 2^(log2_m+1)-m */

	if (r1 < cutoff) { /* remainder case 1: remainder length=log2_m */
		cw_len = q + 1 + log2_m;
		r = r1;
	} else { /* remainder case 2: remainder length = log2_m+1 */
		cw_len = q + 1 + log2_m + 1;
		r = r2 - cutoff;
	}

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

static decoder_ptr select_decoder(uint32_t golomb_par)
{
	assert(golomb_par > 0);

	if (is_a_pow_of_2(golomb_par))
		return &rice_decoder;
	else
		return &golomb_decoder;
}


/**
 * @brief read a value of up to 32 bits from a big-endian bitstream
 *
 * @param p_value		pointer to the read value, the read value will
 *				be converted to the system endianness
 * @param n_bits		number of bits to read from the bitstream
 * @param bit_offset		bit index where the bits will be read, seen from
 *				the very beginning of the bitstream
 * @param bitstream_adr		pointer to the beginning of the bitstream
 * @param max_stream_len	maximum length of the bitstream in bits
 *
 * @returns bit position of the last read bit in the bitstream on success;
 *	returns negative in case of erroneous input; returns CMP_ERROR_SMALL_BUF
 *	if the bitstream buffer is too small to read the value from the
 *	bitstream
 */

static int get_n_bits32(uint32_t *p_value, unsigned int n_bits, int bit_offset,
			uint32_t *bitstream_adr, unsigned int max_stream_len)
{
	/* separate the bit_offset into word offset (local_adr pointer) and
	 * local bit offset (bits_left)
	 */
	uint32_t *local_adr = bitstream_adr + (bit_offset >> 5);
	unsigned int bits_left = bit_offset & 0x1f;
	unsigned int bits_right = 32 - n_bits;
	unsigned int local_end_pos = bits_left + n_bits;
	int stream_len = (int)(n_bits + (unsigned int)bit_offset); /* overflow results in a negative return value */

	assert(p_value != NULL);
	assert(n_bits > 0 && n_bits <= 32);
	assert(bit_offset >= 0);
	assert(bitstream_adr != NULL);

	/* Check if the bitstream buffer is large enough */
	if (unlikely((unsigned int)stream_len > max_stream_len)) {
			debug_print("Error: The end of the compressed bit stream has been exceeded. %s", please_check_str);
			return CMP_ERROR_SMALL_BUF;
	}

	*p_value = (cpu_to_be32(*local_adr) << bits_left) >> bits_right;

	if (local_end_pos > 32) { /* part 2: */
		local_adr += 1;   /* adjust address */
		bits_right = 64 - local_end_pos;
		*p_value |= cpu_to_be32(*local_adr) >> bits_right;
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
	unsigned int n_read_bits = setup->max_cw_len;
	int stream_pos_read;
	uint32_t read_val;
	unsigned int cw_len;

	/* check if we can read max_cw_len or less; we do not know how long the
	 * code word actually is so we try to read the maximum cw length */
	if (setup->max_cw_len > setup->max_stream_len - (unsigned int)stream_pos) {
		n_read_bits = setup->max_stream_len - (unsigned int)stream_pos;
		if (n_read_bits == 0) {
			debug_print("Error: The end of the compressed bit stream has been exceeded. %s", please_check_str);
			return CMP_ERROR_SMALL_BUF;
		}
	}

	stream_pos_read = get_n_bits32(&read_val, n_read_bits, stream_pos,
				       setup->bitstream_adr, setup->max_stream_len);
	if (stream_pos_read < 0)
		return stream_pos_read;

	/* if we read less than 32, we shift the bitstream so that it starts at the MSb */
	read_val = read_val << (32 - n_read_bits);

	cw_len = setup->decode_cw_f(read_val, setup->encoder_par1,
				    setup->encoder_par2, decoded_value);
	/* consistency check: The bit length of the codeword cannot be greater
	 * than the bits read from the bitstream.  */
	if (cw_len > n_read_bits) {
		debug_print("Error: Data consistency check failed. Unable to decode the codeword. %s", please_check_str);
		return -1;
	}

	return stream_pos + (int)cw_len;
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

	/* consistency check: values larger than the outlier parameter should not
	 * be Golomb/Rice encoded */
	if (*decoded_value > setup->outlier_par) {
		debug_print("Error: Data consistency check failed. Decoded value lager than the outlier parameter. %s", please_check_str);
		return -1;
	}

	if (*decoded_value == 0) {
		/* escape symbol mechanism was used; read unencoded value */
		uint32_t unencoded_val;

		stream_pos = get_n_bits32(&unencoded_val, setup->max_data_bits, stream_pos,
					  setup->bitstream_adr, setup->max_stream_len);
		if (stream_pos < 0)
			return stream_pos;
		/* consistency check: outliers must be bigger than the outlier_par */
		if (unencoded_val < setup->outlier_par && unencoded_val != 0) {
			debug_print("Error: Data consistency check failed. Outlier small than the outlier parameter. %s", please_check_str);
			return -1;
		}

		*decoded_value = unencoded_val;
	}

	(*decoded_value)--;
	if (*decoded_value == 0xFFFFFFFF) /* catch underflow */
		(*decoded_value) >>=  (32 - setup->max_data_bits);

	return stream_pos;
}


/**
 * @brief decode a Golomb/Rice encoded code word with the multi escape mechanism
 *	from the bitstream
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
		unsigned int unencoded_len = (*decoded_value - setup->outlier_par + 1) << 1;
		uint32_t *unencoded_val = decoded_value;

		/* consistency check: length of the unencoded value can not be bigger than the maximum data length */
		if (unencoded_len > ((setup->max_data_bits+1) & -2U)) { /* round up max_data_bits to the nearest multiple of 2 */
			debug_print("Error: Data consistency check failed. Multi escape symbol higher than expected. %s", please_check_str);
			return -1;
		}

		stream_pos = get_n_bits32(unencoded_val, unencoded_len, stream_pos,
					  setup->bitstream_adr, setup->max_stream_len);
		if (stream_pos < 0)
			return stream_pos;

		/* consistency check: check if the unencoded value used the bits expected */
		if (*unencoded_val >> (unencoded_len-2) == 0) { /* check if at least one bit of the two highest is set. */
			if (unencoded_len > 2) { /* Exception: if we code outlier_par, no set bit is expected */
				debug_print("Error: Data consistency check failed. Unencoded value after escape symbol to small. %s", please_check_str);
				return -1;
			}
		}

		*decoded_value = *unencoded_val + setup->outlier_par;
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
		debug_print("Error: The length of the compressed data is not a multiple of 4 bytes.\n");
		return -1;
	}
	setup->max_stream_len = (cfg->buffer_length) * CHAR_BIT;  /* maximum length of the bitstream/icu_output_buf in bits */
	setup->encoder_par1 = cmp_par; /* encoding parameter 1 */
	if (ilog_2(cmp_par) == -1U)
		return -1;
	setup->encoder_par2 = ilog_2(cmp_par); /* encoding parameter 2 */
	setup->outlier_par = spillover; /* outlier parameter */
	setup->lossy_par = lossy_par; /* lossy compression parameter */
	setup->max_data_bits = max_data_bits; /* how many bits are needed to represent the highest possible value */
	setup->decode_cw_f = select_decoder(cmp_par);
	if (rdcu_supported_data_type_is_used(cfg->data_type))
		setup->max_cw_len = MAX_CW_LEN_RDCU;
	else
		setup->max_cw_len = MAX_CW_LEN_ICU;

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
	size_t i;
	int stream_pos = 0;
	uint32_t decoded_value;
	struct decoder_setup setup;
	uint16_t *data_buf = cfg->input_buf;
	uint16_t *model_buf = cfg->model_buf;
	uint16_t *up_model_buf;
	uint16_t *next_model_p;
	uint16_t model;

	if (model_mode_is_used(cfg->cmp_mode)) {
		up_model_buf = cfg->icu_new_model_buf;
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		up_model_buf = NULL;
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	if (configure_decoder_setup(&setup, cfg->golomb_par, cfg->spill,
				    cfg->round, cfg->max_used_bits->nc_imagette, cfg))
		return -1;

	for (i = 0; ; i++) {
		stream_pos = decode_value(&decoded_value, model, stream_pos, &setup);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i] = (__typeof__(data_buf[i]))decoded_value;

		if (up_model_buf)
			up_model_buf[i] = cmp_up_model(data_buf[i], model, cfg->model_value,
						       setup.lossy_par);

		if (i >= cfg->samples-1)
			break;

		model = next_model_p[i];
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
				    cfg->round, cfg->max_used_bits->s_exp_flags, cfg))
		return -1;
	if (configure_decoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				    cfg->round, cfg->max_used_bits->s_fx, cfg))
		return -1;

	for (i = 0; ; i++) {
		stream_pos = decode_value(&decoded_value, model.exp_flags,
					  stream_pos, &setup_exp_flags);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].exp_flags = (__typeof__(data_buf[i].exp_flags))decoded_value;

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
				    cfg->round, cfg->max_used_bits->s_exp_flags, cfg))
		return -1;
	if (configure_decoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				    cfg->round, cfg->max_used_bits->s_fx, cfg))
		return -1;
	if (configure_decoder_setup(&setup_efx, cfg->cmp_par_efx, cfg->spill_efx,
				    cfg->round, cfg->max_used_bits->s_efx, cfg))
		return -1;

	for (i = 0; ; i++) {
		stream_pos = decode_value(&decoded_value, model.exp_flags,
					  stream_pos, &setup_exp_flags);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].exp_flags = (__typeof__(data_buf[i].exp_flags)) decoded_value;

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
				    cfg->round, cfg->max_used_bits->s_exp_flags, cfg))
		return -1;
	if (configure_decoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				    cfg->round, cfg->max_used_bits->s_fx, cfg))
		return -1;
	if (configure_decoder_setup(&setup_ncob, cfg->cmp_par_ncob, cfg->spill_ncob,
				    cfg->round, cfg->max_used_bits->s_ncob, cfg))
		return -1;

	for (i = 0; ; i++) {
		stream_pos = decode_value(&decoded_value, model.exp_flags,
					  stream_pos, &setup_exp_flags);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].exp_flags = (__typeof__(data_buf[i].exp_flags)) decoded_value;

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
				    cfg->round, cfg->max_used_bits->s_exp_flags, cfg))
		return -1;
	if (configure_decoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				    cfg->round, cfg->max_used_bits->s_fx, cfg))
		return -1;
	if (configure_decoder_setup(&setup_ncob, cfg->cmp_par_ncob, cfg->spill_ncob,
				    cfg->round, cfg->max_used_bits->s_ncob, cfg))
		return -1;
	if (configure_decoder_setup(&setup_efx, cfg->cmp_par_efx, cfg->spill_efx,
				    cfg->round, cfg->max_used_bits->s_efx, cfg))
		return -1;
	if (configure_decoder_setup(&setup_ecob, cfg->cmp_par_ecob, cfg->spill_ecob,
				    cfg->round, cfg->max_used_bits->s_ecob, cfg))
		return -1;

	for (i = 0; ; i++) {
		stream_pos = decode_value(&decoded_value, model.exp_flags,
					  stream_pos, &setup_exp_flags);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].exp_flags = (__typeof__(data_buf[i].exp_flags)) decoded_value;

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
				    cfg->round, cfg->max_used_bits->f_fx, cfg))
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
				    cfg->round, cfg->max_used_bits->f_fx, cfg))
		return -1;
	if (configure_decoder_setup(&setup_efx, cfg->cmp_par_efx, cfg->spill_efx,
				    cfg->round, cfg->max_used_bits->f_efx, cfg))
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
				    cfg->round, cfg->max_used_bits->f_fx, cfg))
		return -1;
	if (configure_decoder_setup(&setup_ncob, cfg->cmp_par_ncob, cfg->spill_ncob,
				    cfg->round, cfg->max_used_bits->f_ncob, cfg))
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
				    cfg->round, cfg->max_used_bits->f_fx, cfg))
		return -1;
	if (configure_decoder_setup(&setup_ncob, cfg->cmp_par_ncob, cfg->spill_ncob,
				    cfg->round, cfg->max_used_bits->f_ncob, cfg))
		return -1;
	if (configure_decoder_setup(&setup_efx, cfg->cmp_par_efx, cfg->spill_efx,
				    cfg->round, cfg->max_used_bits->f_efx, cfg))
		return -1;
	if (configure_decoder_setup(&setup_ecob, cfg->cmp_par_ecob, cfg->spill_ecob,
				    cfg->round, cfg->max_used_bits->f_ecob, cfg))
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
				    cfg->round, cfg->max_used_bits->l_exp_flags, cfg))
		return -1;
	if (configure_decoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				    cfg->round, cfg->max_used_bits->l_fx, cfg))
		return -1;
	if (configure_decoder_setup(&setup_fx_var, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				    cfg->round, cfg->max_used_bits->l_fx_variance, cfg))
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
			up_model_buf[i].exp_flags = cmp_up_model32(data_buf[i].exp_flags, model.exp_flags,
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
				    cfg->round, cfg->max_used_bits->l_exp_flags, cfg))
		return -1;
	if (configure_decoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				    cfg->round, cfg->max_used_bits->l_fx, cfg))
		return -1;
	if (configure_decoder_setup(&setup_efx, cfg->cmp_par_efx, cfg->spill_efx,
				    cfg->round, cfg->max_used_bits->l_efx, cfg))
		return -1;
	if (configure_decoder_setup(&setup_fx_var, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				    cfg->round, cfg->max_used_bits->l_fx_variance, cfg))
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
			up_model_buf[i].exp_flags = cmp_up_model32(data_buf[i].exp_flags, model.exp_flags,
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
				    cfg->round, cfg->max_used_bits->l_exp_flags, cfg))
		return -1;
	if (configure_decoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				    cfg->round, cfg->max_used_bits->l_fx, cfg))
		return -1;
	if (configure_decoder_setup(&setup_ncob, cfg->cmp_par_ncob, cfg->spill_ncob,
				    cfg->round, cfg->max_used_bits->l_ncob, cfg))
		return -1;
	if (configure_decoder_setup(&setup_fx_var, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				    cfg->round, cfg->max_used_bits->l_fx_variance, cfg))
		return -1;
	if (configure_decoder_setup(&setup_cob_var, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				    cfg->round, cfg->max_used_bits->l_cob_variance, cfg))
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
			up_model_buf[i].exp_flags = cmp_up_model32(data_buf[i].exp_flags, model.exp_flags,
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
				    cfg->round, cfg->max_used_bits->l_exp_flags, cfg))
		return -1;
	if (configure_decoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				    cfg->round, cfg->max_used_bits->l_fx, cfg))
		return -1;
	if (configure_decoder_setup(&setup_ncob, cfg->cmp_par_ncob, cfg->spill_ncob,
				    cfg->round, cfg->max_used_bits->l_ncob, cfg))
		return -1;
	if (configure_decoder_setup(&setup_efx, cfg->cmp_par_efx, cfg->spill_efx,
				    cfg->round, cfg->max_used_bits->l_efx, cfg))
		return -1;
	if (configure_decoder_setup(&setup_ecob, cfg->cmp_par_ecob, cfg->spill_ecob,
				    cfg->round, cfg->max_used_bits->l_ecob, cfg))
		return -1;
	if (configure_decoder_setup(&setup_fx_var, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				    cfg->round, cfg->max_used_bits->l_fx_variance, cfg))
		return -1;
	if (configure_decoder_setup(&setup_cob_var, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				    cfg->round, cfg->max_used_bits->l_cob_variance, cfg))
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
			up_model_buf[i].exp_flags = cmp_up_model32(data_buf[i].exp_flags, model.exp_flags,
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
				    cfg->round, cfg->max_used_bits->nc_offset_mean, cfg))
		return -1;
	if (configure_decoder_setup(&setup_var, cfg->cmp_par_variance, cfg->spill_variance,
				    cfg->round, cfg->max_used_bits->nc_offset_variance, cfg))
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
				    cfg->round, cfg->max_used_bits->nc_background_mean, cfg))
		return -1;
	if (configure_decoder_setup(&setup_var, cfg->cmp_par_variance, cfg->spill_variance,
				    cfg->round, cfg->max_used_bits->nc_background_variance, cfg))
		return -1;
	if (configure_decoder_setup(&setup_pix, cfg->cmp_par_pixels_error, cfg->spill_pixels_error,
				    cfg->round, cfg->max_used_bits->nc_background_outlier_pixels, cfg))
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
		data_buf[i].outlier_pixels = (__typeof__(data_buf[i].outlier_pixels))decoded_value;

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
				    cfg->round, cfg->max_used_bits->smearing_mean, cfg))
		return -1;
	if (configure_decoder_setup(&setup_var, cfg->cmp_par_variance, cfg->spill_variance,
				    cfg->round, cfg->max_used_bits->smearing_variance_mean, cfg))
		return -1;
	if (configure_decoder_setup(&setup_pix, cfg->cmp_par_pixels_error, cfg->spill_pixels_error,
				    cfg->round, cfg->max_used_bits->smearing_outlier_pixels, cfg))
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
		data_buf[i].variance_mean = (__typeof__(data_buf[i].variance_mean))decoded_value;

		stream_pos = decode_value(&decoded_value, model.outlier_pixels, stream_pos,
					  &setup_pix);
		if (stream_pos <= 0)
			return stream_pos;
		data_buf[i].outlier_pixels = (__typeof__(data_buf[i].outlier_pixels))decoded_value;

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
	uint32_t data_size;
	int strem_len_bit = -1;

	if (!cfg)
		return -1;

	if (!cfg->icu_output_buf)
		return -1;

	if (!cfg->max_used_bits)
		return -1;

	if (cmp_imagette_data_type_is_used(cfg->data_type)) {
		if (cmp_cfg_imagette_is_invalid(cfg, ICU_CHECK))
			return -1;
	} else if (cmp_fx_cob_data_type_is_used(cfg->data_type)) {
		if (cmp_cfg_fx_cob_is_invalid(cfg))
			return -1;
	} else if (cmp_aux_data_type_is_used(cfg->data_type))
		if (cmp_cfg_aux_is_invalid(cfg))
			return -1;

	data_size = cmp_cal_size_of_data(cfg->samples, cfg->data_type);
	if (!cfg->input_buf || !data_size)
		return (int)data_size;

	if (model_mode_is_used(cfg->cmp_mode))
		if (!cfg->model_buf)
			return -1;

	if (cfg->cmp_mode == CMP_MODE_RAW) {

		if (data_size < cfg->buffer_length/CHAR_BIT)
			return -1;

		if (cfg->input_buf) {
			memcpy(cfg->input_buf, cfg->icu_output_buf, data_size);
			if (cmp_input_big_to_cpu_endianness(cfg->input_buf, data_size, cfg->data_type))
				return -1;
			strem_len_bit = (int)data_size * CHAR_BIT;
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

	return (int)data_size;
}


/**
 * @brief read in an imagette compression entity header to a
 *	compression configuration
 *
 * @param ent	pointer to a compression entity
 * @param cfg	pointer to a compression configuration
 *
 * @returns 0 on success; otherwise error
 */

static int cmp_ent_read_header(struct cmp_entity *ent, struct cmp_cfg *cfg)
{
	int32_t samples;

	if (!cfg)
		return -1;

	cfg->data_type = cmp_ent_get_data_type(ent);
	if (cmp_data_type_is_invalid(cfg->data_type)) {
		debug_print("Error: Compression data type not supported.\n");
		return -1;
	}

	cfg->cmp_mode = cmp_ent_get_cmp_mode(ent);
	if (cmp_ent_get_data_type_raw_bit(ent) != (cfg->cmp_mode == CMP_MODE_RAW)) {
		debug_print("Error: The entity's raw data bit does not match up with the compression mode.\n");
		return -1;
	}
	cfg->model_value = cmp_ent_get_model_value(ent);
	cfg->round = cmp_ent_get_lossy_cmp_par(ent);
	cfg->buffer_length = cmp_ent_get_cmp_data_size(ent);

	samples = cmp_input_size_to_samples(cmp_ent_get_original_size(ent), cfg->data_type);
	if (samples < 0) {
		debug_print("Error: original_size and data product type in the compression header are not compatible.\n");
		cfg->samples = 0;
		return -1;
	}

	cfg->samples = (uint32_t)samples;

	cfg->icu_output_buf = cmp_ent_get_data_buf(ent);

	cfg->max_used_bits = cmp_max_used_bits_list_get(cmp_ent_get_max_used_bits_version(ent));
	if (!cfg->max_used_bits) {
		debug_print("Error: The Max. Used Bits Registry Version in the compression header is unknown.\n");
		return -1;
	}

	if (cfg->cmp_mode == CMP_MODE_RAW)
		/* no specific header is used for raw data we are done */
		return 0;

	switch (cfg->data_type) {
	case DATA_TYPE_IMAGETTE_ADAPTIVE:
	case DATA_TYPE_SAT_IMAGETTE_ADAPTIVE:
	case DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE:
		cfg->ap1_golomb_par = cmp_ent_get_ima_ap1_golomb_par(ent);
		cfg->ap1_spill = cmp_ent_get_ima_ap1_spill(ent);
		cfg->ap2_golomb_par = cmp_ent_get_ima_ap2_golomb_par(ent);
		cfg->ap2_spill = cmp_ent_get_ima_ap2_spill(ent);
		/* fall through */
	case DATA_TYPE_IMAGETTE:
	case DATA_TYPE_SAT_IMAGETTE:
	case DATA_TYPE_F_CAM_IMAGETTE:
		cfg->spill = cmp_ent_get_ima_spill(ent);
		cfg->golomb_par = cmp_ent_get_ima_golomb_par(ent);
		break;
	case DATA_TYPE_OFFSET:
	case DATA_TYPE_BACKGROUND:
	case DATA_TYPE_SMEARING:
		cfg->cmp_par_mean = cmp_ent_get_non_ima_cmp_par1(ent);
		cfg->spill_mean = cmp_ent_get_non_ima_spill1(ent);
		cfg->cmp_par_variance = cmp_ent_get_non_ima_cmp_par2(ent);
		cfg->spill_variance = cmp_ent_get_non_ima_spill2(ent);
		cfg->cmp_par_pixels_error = cmp_ent_get_non_ima_cmp_par3(ent);
		cfg->spill_pixels_error = cmp_ent_get_non_ima_spill3(ent);
		break;
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
		cfg->cmp_par_exp_flags = cmp_ent_get_non_ima_cmp_par1(ent);
		cfg->spill_exp_flags = cmp_ent_get_non_ima_spill1(ent);
		cfg->cmp_par_fx = cmp_ent_get_non_ima_cmp_par2(ent);
		cfg->spill_fx = cmp_ent_get_non_ima_spill2(ent);
		cfg->cmp_par_ncob = cmp_ent_get_non_ima_cmp_par3(ent);
		cfg->spill_ncob = cmp_ent_get_non_ima_spill3(ent);
		cfg->cmp_par_efx = cmp_ent_get_non_ima_cmp_par4(ent);
		cfg->spill_efx = cmp_ent_get_non_ima_spill4(ent);
		cfg->cmp_par_ecob = cmp_ent_get_non_ima_cmp_par5(ent);
		cfg->spill_ecob = cmp_ent_get_non_ima_spill5(ent);
		cfg->cmp_par_fx_cob_variance = cmp_ent_get_non_ima_cmp_par6(ent);
		cfg->spill_fx_cob_variance = cmp_ent_get_non_ima_spill6(ent);
		break;
	case DATA_TYPE_F_CAM_OFFSET:
	case DATA_TYPE_F_CAM_BACKGROUND:
	/* LCOV_EXCL_START */
	case DATA_TYPE_UNKNOWN:
	default:
		return -1;
	/* LCOV_EXCL_STOP */
	}

	return 0;
}


/**
 * @brief decompress a compression entity
 *
 * @param ent			pointer to the compression entity to be decompressed
 * @param model_of_data		pointer to model data buffer (can be NULL if no
 *				model compression mode is used)
 * @param up_model_buf		pointer to store the updated model for the next model
 *				mode compression (can be the same as the model_of_data
 *				buffer for an in-place update or NULL if updated model is not needed)
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
 * @param info			pointer to a decompression information structure
 *				consisting the metadata of the compression
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
	cfg.max_used_bits = &MAX_USED_BITS_SAFE;

	return decompressed_data_internal(&cfg);
}
