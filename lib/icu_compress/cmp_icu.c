/**
 * @file   cmp_icu.c
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
 * @brief software compression library
 * @see Data Compression User Manual PLATO-UVIE-PL-UM-0001
 */


#include <stdint.h>
#include <string.h>
#include <limits.h>

#include "../common/byteorder.h"
#include "../common/cmp_debug.h"
#include "../common/cmp_data_types.h"
#include "../common/cmp_support.h"
#include "../common/cmp_max_used_bits.h"
#include "../common/cmp_entity.h"
#include "../common/cmp_error.h"
#include "../common/leon_inttypes.h"

#include "../cmp_icu.h"
#include "../cmp_chunk.h"


/**
 * @brief default implementation of the get_timestamp() function
 *
 * @returns 0
 */

static uint64_t default_get_timestamp(void)
{
	return 0;
}


/**
 * @brief function pointer to a function returning a current PLATO timestamp
 *	initialised with the compress_chunk_init() function
 */

static uint64_t (*get_timestamp)(void) = default_get_timestamp;


/**
 * @brief holding the version_identifier for the compression header
 *	initialised with the compress_chunk_init() function
 */

static uint32_t version_identifier;


/**
 * @brief pointer to a code word generation function
 */

typedef uint32_t (*generate_cw_f_pt)(uint32_t value, uint32_t encoder_par1,
				     uint32_t encoder_par2, uint32_t *cw);


/**
 * @brief structure to hold a setup to encode a value
 */

struct encoder_setup {
	generate_cw_f_pt generate_cw_f; /**< function pointer to a code word encoder */
	uint32_t (*encode_method_f)(uint32_t data, uint32_t model, uint32_t stream_len,
			       const struct encoder_setup *setup); /**< pointer to the encoding function */
	uint32_t *bitstream_adr; /**< start address of the compressed data bitstream */
	uint32_t max_stream_len; /**< maximum length of the bitstream/icu_output_buf in bits */
	uint32_t encoder_par1;   /**< encoding parameter 1 */
	uint32_t encoder_par2;   /**< encoding parameter 2 */
	uint32_t spillover_par;  /**< outlier parameter */
	uint32_t lossy_par;      /**< lossy compression parameter */
	uint32_t max_data_bits;  /**< how many bits are needed to represent the highest possible value */
};


/**
 * @brief map a signed value into a positive value range
 *
 * @param value_to_map	signed value to map
 * @param max_data_bits	how many bits are needed to represent the
 *			highest possible value
 *
 * @returns the positive mapped value
 */

static uint32_t map_to_pos(uint32_t value_to_map, unsigned int max_data_bits)
{
	uint32_t result;
	uint32_t mask = (~0U >> (32 - max_data_bits)); /* mask the used bits */

	value_to_map &= mask;
	if (value_to_map >> (max_data_bits - 1)) { /* check the leading signed bit */
		value_to_map |= ~mask; /* convert to 32-bit signed integer */
		/* map negative values to uneven numbers */
		result = (-value_to_map) * 2 - 1; /* possible integer overflow is intended */
	} else {
		/* map positive values to even numbers */
		result = value_to_map * 2; /* possible integer overflow is intended */
	}

	return result;
}


/**
 * @brief put the value of up to 32 bits into a big-endian bitstream
 *
 * @param value			the value to put into the bitstream
 * @param n_bits		number of bits to put into the bitstream
 * @param bit_offset		bit index where the bits will be put, seen from
 *				the very beginning of the bitstream
 * @param bitstream_adr		this is the pointer to the beginning of the
 *				bitstream (can be NULL)
 * @param max_stream_len	maximum length of the bitstream in *bits*; is
 *				ignored if bitstream_adr is NULL
 *
 * @returns the length of the generated bitstream in bits on success or an error
 *          code (which can be tested with cmp_is_error()) in the event of an
 *          incorrect input or if the bitstream buffer is too small to put the
 *          value in the bitstream.
 */

static uint32_t put_n_bits32(uint32_t value, unsigned int n_bits, uint32_t bit_offset,
			uint32_t *bitstream_adr, unsigned int max_stream_len)
{
	/*
	 *                               UNSEGMENTED
	 * |-----------|XXXXXX|---------------|--------------------------------|
	 * |-bits_left-|n_bits|-------------------bits_right-------------------|
	 * ^
	 * local_adr
	 *                               SEGMENTED
	 * |-----------------------------|XXX|XXX|-----------------------------|
	 * |----------bits_left----------|n_bits-|---------bits_right----------|
	 */
	uint32_t bits_left = bit_offset & 0x1F;
	uint32_t bits_right = 64 - bits_left - n_bits;
	uint32_t shift_left = 32 - n_bits;
	uint32_t stream_len = n_bits + bit_offset; /* no check for overflow */
	uint32_t *local_adr;
	uint32_t mask, tmp;

	/* Leave in case of erroneous input */
	RETURN_ERROR_IF((int)shift_left < 0, INT_DECODER, "cannot insert more than 32 bits into the bit stream");  /* check n_bits <= 32 */

	if (n_bits == 0)
		return stream_len;

	if (!bitstream_adr)  /* Do we need to write data to the bitstream? */
		return stream_len;

	/* Check if the bitstream buffer is large enough */
	if (stream_len > max_stream_len)
		return CMP_ERROR(SMALL_BUF_);

	local_adr = bitstream_adr + (bit_offset >> 5);

	/* clear the destination with inverse mask */
	mask = (0XFFFFFFFFU << shift_left) >> bits_left;
	tmp = be32_to_cpu(*local_adr) & ~mask;

	/* put (the first part of) the value into the bitstream */
	tmp |= (value << shift_left) >> bits_left;
	*local_adr = cpu_to_be32(tmp);

	/* Do we need to split the value over two words (SEGMENTED case) */
	if (bits_right < 32) {
		local_adr++;  /* adjust address */

		/* clear the destination */
		mask = 0XFFFFFFFFU << bits_right;
		tmp = be32_to_cpu(*local_adr) & ~mask;

		/* put the 2nd part of the value into the bitstream */
		tmp |= value << bits_right;
		*local_adr = cpu_to_be32(tmp);
	}
	return stream_len;
}


/**
 * @brief forms the codeword according to the Rice code
 *
 * @param value		value to be encoded (must be smaller or equal than cmp_ima_max_spill(m))
 * @param m		Golomb parameter, only m's which are a power of 2 are allowed
 *			maximum allowed Golomb parameter is 0x80000000
 * @param log2_m	Rice parameter, is ilog_2(m) calculate outside function
 *			for better performance
 * @param cw		address where the code word is stored
 *
 * @warning no check of the validity of the input parameters!
 * @returns the length of the formed code word in bits; the code word is invalid
 *	if the return value is greater than 32
 */

static uint32_t rice_encoder(uint32_t value, uint32_t m, uint32_t log2_m,
			     uint32_t *cw)
{
	uint32_t q = value >> log2_m;   /* quotient of value/m */
	uint32_t qc = (1U << q) - 1;    /* quotient code without ending zero */

	uint32_t r = value & (m-1);     /* remainder of value/m */
	uint32_t rl = log2_m + 1;       /* length of the remainder (+1 for the 0 in the quotient code) */

	*cw = (qc << (rl & 0x1FU)) | r; /* put the quotient and remainder code together */
	/*
	 * NOTE: If log2_m = 31 -> rl = 32, (q << rl) leads to an undefined
	 * behavior. However, in this case, a valid code with a maximum of 32
	 * bits can only be formed if q = 0 and qc = 0. To prevent undefined
	 * behavior, the right shift operand is masked (& 0x1FU)
	 */

	return rl + q;  /* calculate the length of the code word */
}


/**
 * @brief forms a codeword according to the Golomb code
 *
 * @param value		value to be encoded (must be smaller or equal than cmp_ima_max_spill(m))
 * @param m		Golomb parameter (have to be bigger than 0)
 * @param log2_m	is ilog_2(m) calculate outside function for better performance
 * @param cw		address where the code word is stored
 *
 * @warning no check of the validity of the input parameters!
 * @returns the length of the formed code word in bits; the code word is invalid
 *	if the return value is greater than 32
 */

static uint32_t golomb_encoder(uint32_t value, uint32_t m, uint32_t log2_m,
			       uint32_t *cw)
{
	uint32_t len = log2_m + 1;               /* codeword length in group 0 */
	uint32_t cutoff = (0x2U << log2_m) - m;  /* members in group 0 */

	if (value < cutoff) { /* group 0 */
		*cw = value;
	} else { /* other groups */
		uint32_t const reg_mask = 0x1FU;  /* mask for the right shift operand to prevent undefined behavior */
		uint32_t g = (value-cutoff) / m;  /* group number of same cw length */
		uint32_t r = (value-cutoff) - g * m; /* member in the group */
		uint32_t gc = (1U << (g & reg_mask)) - 1; /* prepare the left side in unary */
		uint32_t b = cutoff << 1;         /* form the base codeword */

		*cw = gc << ((len+1) & reg_mask);  /* composed codeword part 1 */
		*cw += b + r;                      /* composed codeword part 2 */
		len += 1 + g;                      /* length of the codeword */
	}
	return len;
}


/**
 * @brief generate a code word without an outlier mechanism and put it in the
 *	bitstream
 *
 * @param value		value to encode in the bitstream
 * @param stream_len	length of the bitstream in bits
 * @param setup		pointer to the encoder setup
 *
 * @returns the bit length of the bitstream with the added encoded value on
 *	success; negative on error, CMP_ERROR_SMALL_BUF if the bitstream buffer
 *	is too small to put the value in the bitstream
 */

static uint32_t encode_normal(uint32_t value, uint32_t stream_len,
			 const struct encoder_setup *setup)
{
	uint32_t code_word, cw_len;

	cw_len = setup->generate_cw_f(value, setup->encoder_par1,
				      setup->encoder_par2, &code_word);

	return put_n_bits32(code_word, cw_len, stream_len, setup->bitstream_adr,
			    setup->max_stream_len);
}


/**
 * @brief subtracts the model from the data, encodes the result and puts it into
 *	bitstream, for encoding outlier use the zero escape symbol mechanism
 *
 * @param data		data to encode
 * @param model		model of the data (0 if not used)
 * @param stream_len	length of the bitstream in bits
 * @param setup		pointer to the encoder setup
 *
 * @returns the bit length of the bitstream with the added encoded value on
 *	success; negative on error, CMP_ERROR_SMALL_BUF if the bitstream buffer
 *	is too small to put the value in the bitstream
 *
 * @note no check if the data or model are in the allowed range
 * @note no check if the setup->spillover_par is in the allowed range
 */

static uint32_t encode_value_zero(uint32_t data, uint32_t model, uint32_t stream_len,
				  const struct encoder_setup *setup)
{
	data -= model; /* possible underflow is intended */

	data = map_to_pos(data, setup->max_data_bits);

	/* For performance reasons, we check to see if there is an outlier
	 * before adding one, rather than the other way around:
	 * data++;
	 * if (data < setup->spillover_par && data != 0)
	 *	return ...
	 */
	if (data < (setup->spillover_par - 1)) { /* detect non-outlier */
		data++; /* add 1 to every value so we can use 0 as the escape symbol */
		return encode_normal(data, stream_len, setup);
	}

	data++; /* add 1 to every value so we can use 0 as the escape symbol */

	/* use zero as escape symbol */
	stream_len = encode_normal(0, stream_len, setup);
	if (cmp_is_error(stream_len))
		return stream_len;

	/* put the data unencoded in the bitstream */
	stream_len = put_n_bits32(data, setup->max_data_bits, stream_len,
				  setup->bitstream_adr, setup->max_stream_len);
	return stream_len;
}


/**
 * @brief subtract the model from the data, encode the result and puts it into
 *	bitstream, for encoding outlier use the multi escape symbol mechanism
 *
 * @param data		data to encode
 * @param model		model of the data (0 if not used)
 * @param stream_len	length of the bitstream in bits
 * @param setup		pointer to the encoder setup
 *
 * @returns the bit length of the bitstream with the added encoded value on
 *	success; negative on error, CMP_ERROR_SMALL_BUF if the bitstream buffer
 *	is too small to put the value in the bitstream
 *
 * @note no check if the data or model are in the allowed range
 * @note no check if the setup->spillover_par is in the allowed range
 */

static uint32_t encode_value_multi(uint32_t data, uint32_t model, uint32_t stream_len,
				   const struct encoder_setup *setup)
{
	uint32_t unencoded_data;
	unsigned int unencoded_data_len;
	uint32_t escape_sym, escape_sym_offset;

	data -= model; /* possible underflow is intended */

	data = map_to_pos(data, setup->max_data_bits);

	if (data < setup->spillover_par) /* detect non-outlier */
		return  encode_normal(data, stream_len, setup);

	/*
	 * In this mode we put the difference between the data and the spillover
	 * threshold value (unencoded_data) after an encoded escape symbol, which
	 * indicates that the next codeword is unencoded.
	 * We use different escape symbols depending on the size of the needed
	 * bit of unencoded data:
	 * 0, 1, 2 bits needed for unencoded data -> escape symbol is spillover_par + 0
	 * 3, 4 bits needed for unencoded data -> escape symbol is spillover_par + 1
	 * 5, 6 bits needed for unencoded data -> escape symbol is spillover_par + 2
	 * and so on
	 */
	unencoded_data = data - setup->spillover_par;

	if (!unencoded_data) /* catch __builtin_clz(0) because the result is undefined.*/
		escape_sym_offset = 0;
	else
		escape_sym_offset = (31U - (uint32_t)__builtin_clz(unencoded_data)) >> 1;

	escape_sym = setup->spillover_par + escape_sym_offset;
	unencoded_data_len = (escape_sym_offset + 1U) << 1;

	/* put the escape symbol in the bitstream */
	stream_len = encode_normal(escape_sym, stream_len, setup);
	if (cmp_is_error(stream_len))
		return stream_len;

	/* put the unencoded data in the bitstream */
	stream_len = put_n_bits32(unencoded_data, unencoded_data_len, stream_len,
				  setup->bitstream_adr, setup->max_stream_len);
	return stream_len;
}


/**
 * @brief encodes the data with the model and the given setup and put it into
 *	the bitstream
 *
 * @param data		data to encode
 * @param model		model of the data (0 if not used)
 * @param stream_len	length of the bitstream in bits
 * @param setup		pointer to the encoder setup
 *
 * @returns the bit length of the bitstream with the added encoded value on
 *	success; negative on error, CMP_ERROR_SMALL_BUF if the bitstream buffer
 *	is too small to put the value in the bitstream, CMP_ERROR_HIGH_VALUE if
 *	the value or the model is bigger than the max_used_bits parameter allows
 */

static uint32_t encode_value(uint32_t data, uint32_t model, uint32_t stream_len,
			const struct encoder_setup *setup)
{
	uint32_t mask = ~(0xFFFFFFFFU >> (32-setup->max_data_bits));

	/* lossy rounding of the data if lossy_par > 0 */
	data = round_fwd(data, setup->lossy_par);
	model = round_fwd(model, setup->lossy_par);

	RETURN_ERROR_IF(data & mask || model & mask, DATA_VALUE_TOO_LARGE, "");

	return setup->encode_method_f(data, model, stream_len, setup);
}


/**
 * @brief calculate the maximum length of the bitstream/icu_output_buf in bits
 * @note we round down to the next 4-byte allied address because we access the
 *	cmp_buffer in uint32_t words
 *
 * @param buffer_length	length of the icu_output_buf in bytes
 *
 * @returns buffer size in bits
 */

static uint32_t cmp_buffer_length_to_bits(uint32_t buffer_length)
{
	return (buffer_length & ~0x3U) * 8;
}


/**
 * @brief configure an encoder setup structure to have a setup to encode a value
 *
 * @param setup		pointer to the encoder setup
 * @param cmp_par	compression parameter
 * @param spillover	spillover_par parameter
 * @param lossy_par	lossy compression parameter
 * @param max_data_bits	how many bits are needed to represent the highest possible value
 * @param cfg		pointer to the compression configuration structure
 *
 * @warning input parameters are not checked for validity
 */

static void configure_encoder_setup(struct encoder_setup *setup,
				    uint32_t cmp_par, uint32_t spillover,
				    uint32_t lossy_par, uint32_t max_data_bits,
				    const struct cmp_cfg *cfg)
{
	memset(setup, 0, sizeof(struct encoder_setup));

	setup->encoder_par1 = cmp_par;
	setup->max_data_bits = max_data_bits;
	setup->lossy_par = lossy_par;
	setup->bitstream_adr = cfg->icu_output_buf;
	setup->max_stream_len = cmp_buffer_length_to_bits(cfg->buffer_length);
	setup->encoder_par2 = ilog_2(cmp_par);
	setup->spillover_par = spillover;

	/* for encoder_par1 which is a power of two we can use the faster rice_encoder */
	if (is_a_pow_of_2(setup->encoder_par1))
		setup->generate_cw_f = &rice_encoder;
	else
		setup->generate_cw_f = &golomb_encoder;

	switch (cfg->cmp_mode) {
	case CMP_MODE_MODEL_ZERO:
	case CMP_MODE_DIFF_ZERO:
		setup->encode_method_f = &encode_value_zero;
		break;
	case CMP_MODE_MODEL_MULTI:
	case CMP_MODE_DIFF_MULTI:
		setup->encode_method_f = &encode_value_multi;
		break;
	/* LCOV_EXCL_START */
	case CMP_MODE_RAW:
		/* CMP_MODE_RAW is already handled before; nothing to do here */
		break;
	/* LCOV_EXCL_STOP */
	}
}


/**
 * @brief compress imagette data
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @returns the bit length of the bitstream on success; negative on error,
 *	CMP_ERROR_SMALL_BUF if the bitstream buffer is too small to put the
 *	value in the bitstream, CMP_ERROR_HIGH_VALUE if the value or the model is
 *	bigger than the max_used_bits parameter allows
 */

static uint32_t compress_imagette(const struct cmp_cfg *cfg, uint32_t stream_len)
{
	size_t i;
	struct encoder_setup setup;
	uint32_t max_data_bits;

	uint16_t *data_buf = cfg->input_buf;
	uint16_t *model_buf = cfg->model_buf;
	uint16_t model = 0;
	uint16_t *next_model_p = data_buf;
	uint16_t *up_model_buf = NULL;

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = get_unaligned(&model_buf[0]);
		next_model_p = &model_buf[1];
		up_model_buf = cfg->icu_new_model_buf;
	}

	switch (cfg->data_type) {
	case DATA_TYPE_IMAGETTE:
	case DATA_TYPE_IMAGETTE_ADAPTIVE:
		max_data_bits = MAX_USED_BITS.nc_imagette;
		break;
	case DATA_TYPE_SAT_IMAGETTE:
	case DATA_TYPE_SAT_IMAGETTE_ADAPTIVE:
		max_data_bits = MAX_USED_BITS.saturated_imagette;
		break;
	default:
	case DATA_TYPE_F_CAM_IMAGETTE:
	case DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE:
		max_data_bits = MAX_USED_BITS.fc_imagette;
		break;
	}

	configure_encoder_setup(&setup, cfg->cmp_par_imagette,
				cfg->spill_imagette, cfg->round, max_data_bits, cfg);

	for (i = 0;; i++) {
		stream_len = encode_value(get_unaligned(&data_buf[i]),
					  model, stream_len, &setup);
		if (cmp_is_error(stream_len))
			break;

		if (up_model_buf) {
			uint16_t data = get_unaligned(&data_buf[i]);
			up_model_buf[i] = cmp_up_model(data, model, cfg->model_value,
						       setup.lossy_par);
		}
		if (i >= cfg->samples-1)
			break;

		model = get_unaligned(&next_model_p[i]);
	}
	return stream_len;
}


/**
 * @brief compress short normal light flux (S_FX) data
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @returns the bit length of the bitstream on success; negative on error,
 *	CMP_ERROR_SMALL_BUF if the bitstream buffer is too small to put the
 *	value in the bitstream
 */

static uint32_t compress_s_fx(const struct cmp_cfg *cfg, uint32_t stream_len)
{
	size_t i;

	struct s_fx *data_buf = cfg->input_buf;
	struct s_fx *model_buf = cfg->model_buf;
	struct s_fx *up_model_buf = NULL;
	struct s_fx *next_model_p;
	struct s_fx model;
	struct encoder_setup setup_exp_flag, setup_fx;

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
		up_model_buf = cfg->icu_new_model_buf;
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_encoder_setup(&setup_exp_flag, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				cfg->round, MAX_USED_BITS.s_exp_flags, cfg);
	configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				cfg->round, MAX_USED_BITS.s_fx, cfg);

	for (i = 0;; i++) {
		stream_len = encode_value(data_buf[i].exp_flags, model.exp_flags,
					  stream_len, &setup_exp_flag);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].fx, model.fx, stream_len,
					  &setup_fx);
		if (cmp_is_error(stream_len))
			break;

		if (up_model_buf) {
			up_model_buf[i].exp_flags = cmp_up_model(data_buf[i].exp_flags, model.exp_flags,
								 cfg->model_value, setup_exp_flag.lossy_par);
			up_model_buf[i].fx = cmp_up_model(data_buf[i].fx, model.fx,
							  cfg->model_value, setup_fx.lossy_par);
		}

		if (i >= cfg->samples-1)
			break;

		model = next_model_p[i];
	}
	return stream_len;
}


/**
 * @brief compress S_FX_EFX data
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @returns the bit length of the bitstream on success; negative on error,
 *	CMP_ERROR_SMALL_BUF if the bitstream buffer is too small to put the
 *	value in the bitstream
 */

static uint32_t compress_s_fx_efx(const struct cmp_cfg *cfg, uint32_t stream_len)
{
	size_t i;

	struct s_fx_efx *data_buf = cfg->input_buf;
	struct s_fx_efx *model_buf = cfg->model_buf;
	struct s_fx_efx *up_model_buf = NULL;
	struct s_fx_efx *next_model_p;
	struct s_fx_efx model;
	struct encoder_setup setup_exp_flag, setup_fx, setup_efx;

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
		up_model_buf = cfg->icu_new_model_buf;
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_encoder_setup(&setup_exp_flag, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				cfg->round, MAX_USED_BITS.s_exp_flags, cfg);
	configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				cfg->round, MAX_USED_BITS.s_fx, cfg);
	configure_encoder_setup(&setup_efx, cfg->cmp_par_efx, cfg->spill_efx,
				cfg->round, MAX_USED_BITS.s_efx, cfg);

	for (i = 0;; i++) {
		stream_len = encode_value(data_buf[i].exp_flags, model.exp_flags,
					  stream_len, &setup_exp_flag);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].fx, model.fx, stream_len,
					  &setup_fx);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].efx, model.efx,
					  stream_len, &setup_efx);
		if (cmp_is_error(stream_len))
			return stream_len;

		if (up_model_buf) {
			up_model_buf[i].exp_flags = cmp_up_model(data_buf[i].exp_flags, model.exp_flags,
				cfg->model_value, setup_exp_flag.lossy_par);
			up_model_buf[i].fx = cmp_up_model(data_buf[i].fx, model.fx,
				cfg->model_value, setup_fx.lossy_par);
			up_model_buf[i].efx = cmp_up_model(data_buf[i].efx, model.efx,
				cfg->model_value, setup_efx.lossy_par);
		}

		if (i >= cfg->samples-1)
			break;

		model = next_model_p[i];
	}
	return stream_len;
}


/**
 * @brief compress S_FX_NCOB data
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @returns the bit length of the bitstream on success; negative on error,
 *	CMP_ERROR_SMALL_BUF if the bitstream buffer is too small to put the
 *	value in the bitstream
 */

static uint32_t compress_s_fx_ncob(const struct cmp_cfg *cfg, uint32_t stream_len)
{
	size_t i;

	struct s_fx_ncob *data_buf = cfg->input_buf;
	struct s_fx_ncob *model_buf = cfg->model_buf;
	struct s_fx_ncob *up_model_buf = NULL;
	struct s_fx_ncob *next_model_p;
	struct s_fx_ncob model;
	struct encoder_setup setup_exp_flag, setup_fx, setup_ncob;

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
		up_model_buf = cfg->icu_new_model_buf;
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_encoder_setup(&setup_exp_flag, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				cfg->round, MAX_USED_BITS.s_exp_flags, cfg);
	configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				cfg->round, MAX_USED_BITS.s_fx, cfg);
	configure_encoder_setup(&setup_ncob, cfg->cmp_par_ncob, cfg->spill_ncob,
				cfg->round, MAX_USED_BITS.s_ncob, cfg);

	for (i = 0;; i++) {
		stream_len = encode_value(data_buf[i].exp_flags, model.exp_flags,
					  stream_len, &setup_exp_flag);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].fx, model.fx, stream_len,
					  &setup_fx);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].ncob_x, model.ncob_x,
					  stream_len, &setup_ncob);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].ncob_y, model.ncob_y,
					  stream_len, &setup_ncob);
		if (cmp_is_error(stream_len))
			break;

		if (up_model_buf) {
			up_model_buf[i].exp_flags = cmp_up_model(data_buf[i].exp_flags, model.exp_flags,
				cfg->model_value, setup_exp_flag.lossy_par);
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
	return stream_len;
}


/**
 * @brief compress S_FX_EFX_NCOB_ECOB data
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @returns the bit length of the bitstream on success; negative on error,
 *	CMP_ERROR_SMALL_BUF if the bitstream buffer is too small to put the
 *	value in the bitstream
 */

static uint32_t compress_s_fx_efx_ncob_ecob(const struct cmp_cfg *cfg, uint32_t stream_len)
{
	size_t i;

	struct s_fx_efx_ncob_ecob *data_buf = cfg->input_buf;
	struct s_fx_efx_ncob_ecob *model_buf = cfg->model_buf;
	struct s_fx_efx_ncob_ecob *up_model_buf = NULL;
	struct s_fx_efx_ncob_ecob *next_model_p;
	struct s_fx_efx_ncob_ecob model;
	struct encoder_setup setup_exp_flag, setup_fx, setup_ncob, setup_efx,
			      setup_ecob;

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
		up_model_buf = cfg->icu_new_model_buf;
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_encoder_setup(&setup_exp_flag, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				cfg->round, MAX_USED_BITS.s_exp_flags, cfg);
	configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				cfg->round, MAX_USED_BITS.s_fx, cfg);
	configure_encoder_setup(&setup_ncob, cfg->cmp_par_ncob, cfg->spill_ncob,
				cfg->round, MAX_USED_BITS.s_ncob, cfg);
	configure_encoder_setup(&setup_efx, cfg->cmp_par_efx, cfg->spill_efx,
				cfg->round, MAX_USED_BITS.s_efx, cfg);
	configure_encoder_setup(&setup_ecob, cfg->cmp_par_ecob, cfg->spill_ecob,
				cfg->round, MAX_USED_BITS.s_ecob, cfg);

	for (i = 0;; i++) {
		stream_len = encode_value(data_buf[i].exp_flags, model.exp_flags,
					  stream_len, &setup_exp_flag);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].fx, model.fx, stream_len,
					  &setup_fx);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].ncob_x, model.ncob_x,
					  stream_len, &setup_ncob);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].ncob_y, model.ncob_y,
					  stream_len, &setup_ncob);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].efx, model.efx,
					  stream_len, &setup_efx);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].ecob_x, model.ecob_x,
					  stream_len, &setup_ecob);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].ecob_y, model.ecob_y,
					  stream_len, &setup_ecob);
		if (cmp_is_error(stream_len))
			break;

		if (up_model_buf) {
			up_model_buf[i].exp_flags = cmp_up_model(data_buf[i].exp_flags, model.exp_flags,
				cfg->model_value, setup_exp_flag.lossy_par);
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
	return stream_len;
}


/**
 * @brief compress L_FX data
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @returns the bit length of the bitstream on success; negative on error,
 *	CMP_ERROR_SMALL_BUF if the bitstream buffer is too small to put the
 *	value in the bitstream
 */

static uint32_t compress_l_fx(const struct cmp_cfg *cfg, uint32_t stream_len)
{
	size_t i;

	struct l_fx *data_buf = cfg->input_buf;
	struct l_fx *model_buf = cfg->model_buf;
	struct l_fx *up_model_buf = NULL;
	struct l_fx *next_model_p;
	struct l_fx model;
	struct encoder_setup setup_exp_flag, setup_fx, setup_fx_var;

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
		up_model_buf = cfg->icu_new_model_buf;
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_encoder_setup(&setup_exp_flag, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				cfg->round, MAX_USED_BITS.l_exp_flags, cfg);
	configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				cfg->round, MAX_USED_BITS.l_fx, cfg);
	configure_encoder_setup(&setup_fx_var, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				cfg->round, MAX_USED_BITS.l_fx_variance, cfg);

	for (i = 0;; i++) {
		stream_len = encode_value(data_buf[i].exp_flags, model.exp_flags,
					  stream_len, &setup_exp_flag);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].fx, model.fx, stream_len,
					  &setup_fx);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].fx_variance, model.fx_variance,
					  stream_len, &setup_fx_var);
		if (cmp_is_error(stream_len))
			break;

		if (up_model_buf) {
			up_model_buf[i].exp_flags = cmp_up_model32(data_buf[i].exp_flags, model.exp_flags,
				cfg->model_value, setup_exp_flag.lossy_par);
			up_model_buf[i].fx = cmp_up_model(data_buf[i].fx, model.fx,
				cfg->model_value, setup_fx.lossy_par);
			up_model_buf[i].fx_variance = cmp_up_model(data_buf[i].fx_variance, model.fx_variance,
				cfg->model_value, setup_fx_var.lossy_par);
		}

		if (i >= cfg->samples-1)
			break;

		model = next_model_p[i];
	}
	return stream_len;
}


/**
 * @brief compress L_FX_EFX data
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @returns the bit length of the bitstream on success; negative on error,
 *	CMP_ERROR_SMALL_BUF if the bitstream buffer is too small to put the
 *	value in the bitstream
 */

static uint32_t compress_l_fx_efx(const struct cmp_cfg *cfg, uint32_t stream_len)
{
	size_t i;

	struct l_fx_efx *data_buf = cfg->input_buf;
	struct l_fx_efx *model_buf = cfg->model_buf;
	struct l_fx_efx *up_model_buf = NULL;
	struct l_fx_efx *next_model_p;
	struct l_fx_efx model;
	struct encoder_setup setup_exp_flag, setup_fx, setup_efx, setup_fx_var;

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
		up_model_buf = cfg->icu_new_model_buf;
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_encoder_setup(&setup_exp_flag, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				cfg->round, MAX_USED_BITS.l_exp_flags, cfg);
	configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				cfg->round, MAX_USED_BITS.l_fx, cfg);
	configure_encoder_setup(&setup_efx, cfg->cmp_par_efx, cfg->spill_efx,
				cfg->round, MAX_USED_BITS.l_efx, cfg);
	configure_encoder_setup(&setup_fx_var, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				cfg->round, MAX_USED_BITS.l_fx_variance, cfg);

	for (i = 0;; i++) {
		stream_len = encode_value(data_buf[i].exp_flags, model.exp_flags,
					  stream_len, &setup_exp_flag);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].fx, model.fx, stream_len,
					  &setup_fx);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].efx, model.efx,
					  stream_len, &setup_efx);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].fx_variance, model.fx_variance,
					  stream_len, &setup_fx_var);
		if (cmp_is_error(stream_len))
			break;

		if (up_model_buf) {
			up_model_buf[i].exp_flags = cmp_up_model32(data_buf[i].exp_flags, model.exp_flags,
				cfg->model_value, setup_exp_flag.lossy_par);
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
	return stream_len;
}


/**
 * @brief compress L_FX_NCOB data
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @returns the bit length of the bitstream on success; negative on error,
 *	CMP_ERROR_SMALL_BUF if the bitstream buffer is too small to put the
 *	value in the bitstream
 */

static uint32_t compress_l_fx_ncob(const struct cmp_cfg *cfg, uint32_t stream_len)
{
	size_t i;

	struct l_fx_ncob *data_buf = cfg->input_buf;
	struct l_fx_ncob *model_buf = cfg->model_buf;
	struct l_fx_ncob *up_model_buf = NULL;
	struct l_fx_ncob *next_model_p;
	struct l_fx_ncob model;
	struct encoder_setup setup_exp_flag, setup_fx, setup_ncob,
			      setup_fx_var, setup_cob_var;

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
		up_model_buf = cfg->icu_new_model_buf;
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_encoder_setup(&setup_exp_flag, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				cfg->round, MAX_USED_BITS.l_exp_flags, cfg);
	configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				cfg->round, MAX_USED_BITS.l_fx, cfg);
	configure_encoder_setup(&setup_ncob, cfg->cmp_par_ncob, cfg->spill_ncob,
				cfg->round, MAX_USED_BITS.l_ncob, cfg);
	/* we use the cmp_par_fx_cob_variance parameter for fx and cob variance data */
	configure_encoder_setup(&setup_fx_var, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				cfg->round, MAX_USED_BITS.l_fx_variance, cfg);
	configure_encoder_setup(&setup_cob_var, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				cfg->round, MAX_USED_BITS.l_cob_variance, cfg);

	for (i = 0;; i++) {
		stream_len = encode_value(data_buf[i].exp_flags, model.exp_flags,
					  stream_len, &setup_exp_flag);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].fx, model.fx, stream_len,
					  &setup_fx);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].ncob_x, model.ncob_x,
					  stream_len, &setup_ncob);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].ncob_y, model.ncob_y,
					  stream_len, &setup_ncob);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].fx_variance, model.fx_variance,
					  stream_len, &setup_fx_var);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].cob_x_variance, model.cob_x_variance,
					  stream_len, &setup_cob_var);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].cob_y_variance, model.cob_y_variance,
					  stream_len, &setup_cob_var);
		if (cmp_is_error(stream_len))
			break;

		if (up_model_buf) {
			up_model_buf[i].exp_flags = cmp_up_model32(data_buf[i].exp_flags, model.exp_flags,
				cfg->model_value, setup_exp_flag.lossy_par);
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
	return stream_len;
}


/**
 * @brief compress L_FX_EFX_NCOB_ECOB data
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @returns the bit length of the bitstream on success; negative on error,
 *	CMP_ERROR_SMALL_BUF if the bitstream buffer is too small to put the
 *	value in the bitstream
 */

static uint32_t compress_l_fx_efx_ncob_ecob(const struct cmp_cfg *cfg, uint32_t stream_len)
{
	size_t i;

	struct l_fx_efx_ncob_ecob *data_buf = cfg->input_buf;
	struct l_fx_efx_ncob_ecob *model_buf = cfg->model_buf;
	struct l_fx_efx_ncob_ecob *up_model_buf = NULL;
	struct l_fx_efx_ncob_ecob *next_model_p;
	struct l_fx_efx_ncob_ecob model;
	struct encoder_setup setup_exp_flag, setup_fx, setup_ncob, setup_efx,
			      setup_ecob, setup_fx_var, setup_cob_var;

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
		up_model_buf = cfg->icu_new_model_buf;
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_encoder_setup(&setup_exp_flag, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				cfg->round, MAX_USED_BITS.l_exp_flags, cfg);
	configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				cfg->round, MAX_USED_BITS.l_fx, cfg);
	configure_encoder_setup(&setup_ncob, cfg->cmp_par_ncob, cfg->spill_ncob,
				cfg->round, MAX_USED_BITS.l_ncob, cfg);
	configure_encoder_setup(&setup_efx, cfg->cmp_par_efx, cfg->spill_efx,
				cfg->round, MAX_USED_BITS.l_efx, cfg);
	configure_encoder_setup(&setup_ecob, cfg->cmp_par_ecob, cfg->spill_ecob,
				cfg->round, MAX_USED_BITS.l_ecob, cfg);
	/* we use compression parameters for both variance data fields */
	configure_encoder_setup(&setup_fx_var, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				cfg->round, MAX_USED_BITS.l_fx_variance, cfg);
	configure_encoder_setup(&setup_cob_var, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				cfg->round, MAX_USED_BITS.l_cob_variance, cfg);

	for (i = 0;; i++) {
		stream_len = encode_value(data_buf[i].exp_flags, model.exp_flags,
					  stream_len, &setup_exp_flag);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].fx, model.fx, stream_len,
					  &setup_fx);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].ncob_x, model.ncob_x,
					  stream_len, &setup_ncob);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].ncob_y, model.ncob_y,
					  stream_len, &setup_ncob);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].efx, model.efx,
					  stream_len, &setup_efx);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].ecob_x, model.ecob_x,
					  stream_len, &setup_ecob);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].ecob_y, model.ecob_y,
					  stream_len, &setup_ecob);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].fx_variance, model.fx_variance,
					  stream_len, &setup_fx_var);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].cob_x_variance, model.cob_x_variance,
					  stream_len, &setup_cob_var);
		if (cmp_is_error(stream_len))
			break;
		stream_len = encode_value(data_buf[i].cob_y_variance, model.cob_y_variance,
					  stream_len, &setup_cob_var);
		if (cmp_is_error(stream_len))
			break;

		if (up_model_buf) {
			up_model_buf[i].exp_flags = cmp_up_model32(data_buf[i].exp_flags, model.exp_flags,
				cfg->model_value, setup_exp_flag.lossy_par);
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
	return stream_len;
}


/**
 * @brief compress offset data from the normal and fast cameras
 *
 * @param cfg	pointer to the compression configuration structure
 * @returns the bit length of the bitstream on success; negative on error,
 *	CMP_ERROR_SMALL_BUF if the bitstream buffer is too small to put the
 *	value in the bitstream
 */

static uint32_t compress_offset(const struct cmp_cfg *cfg, uint32_t stream_len)
{
	size_t i;

	struct offset *data_buf = cfg->input_buf;
	struct offset *model_buf = cfg->model_buf;
	struct offset *up_model_buf = NULL;
	struct offset *next_model_p;
	struct offset model;
	struct encoder_setup setup_mean, setup_var;

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
		up_model_buf = cfg->icu_new_model_buf;
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	{
		unsigned int mean_bits_used, variance_bits_used;

		switch (cfg->data_type) {
		case DATA_TYPE_F_CAM_OFFSET:
			mean_bits_used = MAX_USED_BITS.fc_offset_mean;
			variance_bits_used = MAX_USED_BITS.fc_offset_variance;
			break;
		case DATA_TYPE_OFFSET:
		default:
			mean_bits_used = MAX_USED_BITS.nc_offset_mean;
			variance_bits_used = MAX_USED_BITS.nc_offset_variance;
			break;
		}
		configure_encoder_setup(&setup_mean, cfg->cmp_par_offset_mean, cfg->spill_offset_mean,
					cfg->round, mean_bits_used, cfg);
		configure_encoder_setup(&setup_var, cfg->cmp_par_offset_variance, cfg->spill_offset_variance,
					cfg->round, variance_bits_used, cfg);
	}

	for (i = 0;; i++) {
		stream_len = encode_value(data_buf[i].mean, model.mean,
					  stream_len, &setup_mean);
		if (cmp_is_error(stream_len))
			return stream_len;
		stream_len = encode_value(data_buf[i].variance, model.variance,
					  stream_len, &setup_var);
		if (cmp_is_error(stream_len))
			return stream_len;

		if (up_model_buf) {
			up_model_buf[i].mean = cmp_up_model(data_buf[i].mean, model.mean,
				cfg->model_value, setup_mean.lossy_par);
			up_model_buf[i].variance = cmp_up_model(data_buf[i].variance, model.variance,
				cfg->model_value, setup_var.lossy_par);
		}

		if (i >= cfg->samples-1)
			break;

		model = next_model_p[i];
	}
	return stream_len;
}


/**
 * @brief compress background data from the normal and fast cameras
 *
 * @param cfg	pointer to the compression configuration structure
 * @returns the bit length of the bitstream on success; negative on error,
 *	CMP_ERROR_SMALL_BUF if the bitstream buffer is too small to put the
 *	value in the bitstream
 */

static uint32_t compress_background(const struct cmp_cfg *cfg, uint32_t stream_len)
{
	size_t i;

	struct background *data_buf = cfg->input_buf;
	struct background *model_buf = cfg->model_buf;
	struct background *up_model_buf = NULL;
	struct background *next_model_p;
	struct background model;
	struct encoder_setup setup_mean, setup_var, setup_pix;

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
		up_model_buf = cfg->icu_new_model_buf;
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	{
		unsigned int mean_used_bits, varinace_used_bits, pixels_error_used_bits;

		switch (cfg->data_type) {
		case DATA_TYPE_F_CAM_BACKGROUND:
			mean_used_bits = MAX_USED_BITS.fc_background_mean;
			varinace_used_bits = MAX_USED_BITS.fc_background_variance;
			pixels_error_used_bits = MAX_USED_BITS.fc_background_outlier_pixels;
			break;
		case DATA_TYPE_BACKGROUND:
		default:
			mean_used_bits = MAX_USED_BITS.nc_background_mean;
			varinace_used_bits = MAX_USED_BITS.nc_background_variance;
			pixels_error_used_bits = MAX_USED_BITS.nc_background_outlier_pixels;
			break;
		}
		configure_encoder_setup(&setup_mean, cfg->cmp_par_background_mean, cfg->spill_background_mean,
					cfg->round, mean_used_bits, cfg);
		configure_encoder_setup(&setup_var, cfg->cmp_par_background_variance, cfg->spill_background_variance,
					cfg->round, varinace_used_bits, cfg);
		configure_encoder_setup(&setup_pix, cfg->cmp_par_background_pixels_error, cfg->spill_background_pixels_error,
					cfg->round, pixels_error_used_bits, cfg);
	}

	for (i = 0;; i++) {
		stream_len = encode_value(data_buf[i].mean, model.mean,
					  stream_len, &setup_mean);
		if (cmp_is_error(stream_len))
			return stream_len;
		stream_len = encode_value(data_buf[i].variance, model.variance,
					  stream_len, &setup_var);
		if (cmp_is_error(stream_len))
			return stream_len;
		stream_len = encode_value(data_buf[i].outlier_pixels, model.outlier_pixels,
					  stream_len, &setup_pix);
		if (cmp_is_error(stream_len))
			return stream_len;

		if (up_model_buf) {
			up_model_buf[i].mean = cmp_up_model(data_buf[i].mean, model.mean,
				cfg->model_value, setup_mean.lossy_par);
			up_model_buf[i].variance = cmp_up_model(data_buf[i].variance, model.variance,
				cfg->model_value, setup_var.lossy_par);
			up_model_buf[i].outlier_pixels = cmp_up_model(data_buf[i].outlier_pixels, model.outlier_pixels,
				cfg->model_value, setup_pix.lossy_par);
		}

		if (i >= cfg->samples-1)
			break;

		model = next_model_p[i];
	}
	return stream_len;
}


/**
 * @brief compress smearing data from the normal cameras
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @returns the bit length of the bitstream on success; negative on error,
 *	CMP_ERROR_SMALL_BUF if the bitstream buffer is too small to put the
 *	value in the bitstream
 */

static uint32_t compress_smearing(const struct cmp_cfg *cfg, uint32_t stream_len)
{
	size_t i;

	struct smearing *data_buf = cfg->input_buf;
	struct smearing *model_buf = cfg->model_buf;
	struct smearing *up_model_buf = NULL;
	struct smearing *next_model_p;
	struct smearing model;
	struct encoder_setup setup_mean, setup_var_mean, setup_pix;

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
		up_model_buf = cfg->icu_new_model_buf;
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_encoder_setup(&setup_mean, cfg->cmp_par_smearing_mean, cfg->spill_smearing_mean,
				cfg->round, MAX_USED_BITS.smearing_mean, cfg);
	configure_encoder_setup(&setup_var_mean, cfg->cmp_par_smearing_variance, cfg->spill_smearing_variance,
				cfg->round, MAX_USED_BITS.smearing_variance_mean, cfg);
	configure_encoder_setup(&setup_pix, cfg->cmp_par_smearing_pixels_error, cfg->spill_smearing_pixels_error,
				cfg->round, MAX_USED_BITS.smearing_outlier_pixels, cfg);

	for (i = 0;; i++) {
		stream_len = encode_value(data_buf[i].mean, model.mean,
					  stream_len, &setup_mean);
		if (cmp_is_error(stream_len))
			return stream_len;
		stream_len = encode_value(data_buf[i].variance_mean, model.variance_mean,
					  stream_len, &setup_var_mean);
		if (cmp_is_error(stream_len))
			return stream_len;
		stream_len = encode_value(data_buf[i].outlier_pixels, model.outlier_pixels,
					  stream_len, &setup_pix);
		if (cmp_is_error(stream_len))
			return stream_len;

		if (up_model_buf) {
			up_model_buf[i].mean = cmp_up_model(data_buf[i].mean, model.mean,
				cfg->model_value, setup_mean.lossy_par);
			up_model_buf[i].variance_mean = cmp_up_model(data_buf[i].variance_mean, model.variance_mean,
				cfg->model_value, setup_var_mean.lossy_par);
			up_model_buf[i].outlier_pixels = cmp_up_model(data_buf[i].outlier_pixels, model.outlier_pixels,
				cfg->model_value, setup_pix.lossy_par);
		}

		if (i >= cfg->samples-1)
			break;

		model = next_model_p[i];
	}
	return stream_len;
}


/**
 * @brief checks if the ICU compression configuration is valid
 *
 * @param cfg	pointer to the cmp_cfg structure to be validated
 *
 * @returns an error code if any of the configuration parameters are invalid,
 *	otherwise returns CMP_ERROR_NO_ERROR on valid configuration
 */

static uint32_t cmp_cfg_icu_is_invalid_error_code(const struct cmp_cfg *cfg)
{

	RETURN_ERROR_IF(cmp_cfg_gen_par_is_invalid(cfg) , PAR_GENERIC, "");
	RETURN_ERROR_IF(cmp_cfg_icu_buffers_is_invalid(cfg), PAR_BUFFERS, "");

	if (cmp_imagette_data_type_is_used(cfg->data_type)) {
		RETURN_ERROR_IF(cmp_cfg_imagette_is_invalid(cfg), PAR_SPECIFIC, "");
	} else if (cmp_fx_cob_data_type_is_used(cfg->data_type)) {
		RETURN_ERROR_IF(cmp_cfg_fx_cob_is_invalid(cfg), PAR_SPECIFIC, "");
	} else if (cmp_aux_data_type_is_used(cfg->data_type)) {
		RETURN_ERROR_IF(cmp_cfg_aux_is_invalid(cfg), PAR_SPECIFIC, "");
	} else {
		RETURN_ERROR(INT_DATA_TYPE_UNSUPPORTED, "");
	}

	return CMP_ERROR(NO_ERROR);
}


/**
 * @brief fill the last part of the bitstream with zeros
 *
 * @param cfg		pointer to the compression configuration structure
 * @param cmp_size	length of the bitstream in bits
 *
 * @returns the bit length of the bitstream on success; negative on error,
 *	CMP_ERROR_SMALL_BUF if the bitstream buffer is too small to put the
 *	value in the bitstream
 */

static uint32_t pad_bitstream(const struct cmp_cfg *cfg, uint32_t cmp_size)
{
	unsigned int output_buf_len_bits, n_pad_bits;

	if (!cfg->icu_output_buf)
		return cmp_size;

	/* no padding in RAW mode; ALWAYS BIG-ENDIAN */
	if (cfg->cmp_mode == CMP_MODE_RAW)
		return cmp_size;

	/* maximum length of the bitstream/icu_output_buf in bits */
	output_buf_len_bits = cmp_buffer_length_to_bits(cfg->buffer_length);

	n_pad_bits = 32 - (cmp_size & 0x1FU);
	if (n_pad_bits < 32) {
		FORWARD_IF_ERROR(put_n_bits32(0, n_pad_bits, cmp_size,
				 cfg->icu_output_buf, output_buf_len_bits), "");
	}

	return cmp_size;
}


/**
 * @brief internal data compression function
 * This function can compress all types of collection data (one at a time).
 * This function does not take the header of a collection into account.
 *
 * @param cfg	pointer to a compression configuration
 *
 * @note the validity of the cfg structure is checked before the compression is
 *	 started
 *
 * @returns the bit length of the bitstream on success or an error code if it
 *	fails (which can be tested with cmp_is_error())
 */

static uint32_t compress_data_internal(const struct cmp_cfg *cfg, uint32_t stream_len)
{
	uint32_t bitsize = 0;

	FORWARD_IF_ERROR(stream_len, "");
	RETURN_ERROR_IF(cfg == NULL, GENERIC, "");
	RETURN_ERROR_IF(stream_len & 0x7, GENERIC, "The stream_len parameter must be a multiple of 8.");

	if (raw_mode_is_used(cfg->cmp_mode) && cfg->icu_output_buf) {
		uint32_t raw_stream_size = (stream_len >> 3)
			+ cfg->samples * (uint32_t)size_of_a_sample(cfg->data_type);

		/* TODO: move this check to the memcpy */
		RETURN_ERROR_IF(raw_stream_size > cfg->buffer_length, SMALL_BUF_, "");
	}
	if (cfg->samples == 0) /* nothing to compress we are done */
		return stream_len;

	FORWARD_IF_ERROR(cmp_cfg_icu_is_invalid_error_code(cfg), "");


	if (raw_mode_is_used(cfg->cmp_mode)) {
		uint32_t raw_size = cfg->samples * (uint32_t)size_of_a_sample(cfg->data_type);

		if (cfg->icu_output_buf) {
			uint8_t *p = (uint8_t *)cfg->icu_output_buf + (stream_len >> 3);

			memcpy(p, cfg->input_buf, raw_size);
			RETURN_ERROR_IF(cpu_to_be_data_type(p, raw_size, cfg->data_type), GENERIC, "");
		}
		bitsize += stream_len + raw_size*8; /* convert to bits */
	} else {
		switch (cfg->data_type) {
		case DATA_TYPE_IMAGETTE:
		case DATA_TYPE_IMAGETTE_ADAPTIVE:
		case DATA_TYPE_SAT_IMAGETTE:
		case DATA_TYPE_SAT_IMAGETTE_ADAPTIVE:
		case DATA_TYPE_F_CAM_IMAGETTE:
		case DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE:
			bitsize = compress_imagette(cfg, stream_len);
			break;

		case DATA_TYPE_S_FX:
			bitsize = compress_s_fx(cfg, stream_len);
			break;
		case DATA_TYPE_S_FX_EFX:
			bitsize = compress_s_fx_efx(cfg, stream_len);
			break;
		case DATA_TYPE_S_FX_NCOB:
			bitsize = compress_s_fx_ncob(cfg, stream_len);
			break;
		case DATA_TYPE_S_FX_EFX_NCOB_ECOB:
			bitsize = compress_s_fx_efx_ncob_ecob(cfg, stream_len);
			break;


		case DATA_TYPE_L_FX:
			bitsize = compress_l_fx(cfg, stream_len);
			break;
		case DATA_TYPE_L_FX_EFX:
			bitsize = compress_l_fx_efx(cfg, stream_len);
			break;
		case DATA_TYPE_L_FX_NCOB:
			bitsize = compress_l_fx_ncob(cfg, stream_len);
			break;
		case DATA_TYPE_L_FX_EFX_NCOB_ECOB:
			bitsize = compress_l_fx_efx_ncob_ecob(cfg, stream_len);
			break;

		case DATA_TYPE_OFFSET:
		case DATA_TYPE_F_CAM_OFFSET:
			bitsize = compress_offset(cfg, stream_len);
			break;
		case DATA_TYPE_BACKGROUND:
		case DATA_TYPE_F_CAM_BACKGROUND:
			bitsize = compress_background(cfg, stream_len);
			break;
		case DATA_TYPE_SMEARING:
			bitsize = compress_smearing(cfg, stream_len);
			break;
		case DATA_TYPE_F_FX:
		case DATA_TYPE_F_FX_EFX:
		case DATA_TYPE_F_FX_NCOB:
		case DATA_TYPE_F_FX_EFX_NCOB_ECOB:
		case DATA_TYPE_UNKNOWN:
		default:
			RETURN_ERROR(INT_DATA_TYPE_UNSUPPORTED, "");
		}
	}

	if (cmp_is_error(bitsize))
		return bitsize;

	bitsize = pad_bitstream(cfg, bitsize);

	return bitsize;
}


/**
 * @brief estimate a "good" spillover threshold parameter
 *
 * @param golomb_par	Golomb parameter
 *
 * @returns a spill over threshold parameter
 * TODO: tune this calculation for multi escape symbol mechanism
 */

static uint32_t cmp_guess_good_spill(uint32_t golomb_par)
{
	if (!golomb_par)
		return 0;
	return cmp_icu_max_spill(golomb_par);
}


/**
 * @brief set the compressed collection size field
 *
 * @param cmp_col_size_field	pointer to the compressed collection size field
 * @param cmp_col_size		size of the compressed collection (not including
 *				the compressed collection header size and the
 *				size of the compressed collection size field
 *				itself)
 *
 * @returns error code
 */

static uint32_t set_cmp_col_size(uint8_t *cmp_col_size_field, uint32_t cmp_col_size)
{
	uint16_t const v = cpu_to_be16((uint16_t)cmp_col_size);

	RETURN_ERROR_IF(cmp_col_size > UINT16_MAX, INT_CMP_COL_TOO_LARGE,
			"%"PRIu32" is bigger than the maximum allowed compression collection size", cmp_col_size_field);

	memcpy(cmp_col_size_field, &v, CMP_COLLECTION_FILD_SIZE);

	return 0;
}


/**
 * @brief compresses a collection (with a collection header followed by data)
 *
 * @param col		pointer to a collection header
 * @param model		pointer to the model to be used for compression, or NULL
 *			if not applicable
 * @param updated_model	pointer to the buffer where the updated model will be
 *			stored, or NULL if not applicable
 * @param dst		pointer to the buffer where the compressed data will be
 *			stored, or NULL to only get the compressed data size
 * @param dst_capacity	the size of the dst buffer in bytes
 * @param cfg		pointer to a compression configuration
 * @param dst_size	the already used size of the dst buffer in bytes
 *
 * @returns the size of the compressed data in bytes (new dst_size) on
 *	success or an error code if it fails (which can be tested with
 *	cmp_is_error())
 */

static uint32_t cmp_collection(uint8_t *col, uint8_t *model, uint8_t *updated_model,
			       uint32_t *dst, uint32_t dst_capacity,
			       struct cmp_cfg *cfg, uint32_t dst_size)
{
	uint32_t dst_size_begin = dst_size;
	uint32_t dst_size_bits;
	struct collection_hdr *col_hdr = (struct collection_hdr *)col;
	uint16_t col_data_length = cmp_col_get_data_length(col_hdr);
	uint16_t sample_size;

	/* sanity check of the collection header */
	cfg->data_type = convert_subservice_to_cmp_data_type(cmp_col_get_subservice(col_hdr));
	sample_size = (uint16_t)size_of_a_sample(cfg->data_type);
	RETURN_ERROR_IF(sample_size == 0, COL_SUBSERVICE_UNSUPPORTED,
			"unsupported subservice: %u", cmp_col_get_subservice(col_hdr));
	RETURN_ERROR_IF(col_data_length % sample_size, COL_SIZE_INCONSISTENT,
			"col_data_length: %u %% sample_size: %u != 0", col_data_length, sample_size);
	cfg->samples = col_data_length/sample_size;


	if (cfg->cmp_mode != CMP_MODE_RAW) {
		/* hear we reserve space for the compressed data size field */
		dst_size += CMP_COLLECTION_FILD_SIZE;
	}

	/* we do not compress the collection header, we simply copy the header
	 * into the compressed data
	 */
	if (dst) {
		RETURN_ERROR_IF(dst_size + COLLECTION_HDR_SIZE > dst_capacity,
				SMALL_BUF_, "");
		memcpy((uint8_t *)dst + dst_size, col, COLLECTION_HDR_SIZE);
	}
	dst_size += COLLECTION_HDR_SIZE;
	if (model_mode_is_used(cfg->cmp_mode) && updated_model)
		memcpy(updated_model, col, COLLECTION_HDR_SIZE);

	/* prepare the different buffers */
	cfg->icu_output_buf = dst;
	cfg->input_buf = col + COLLECTION_HDR_SIZE;
	if (model)
		cfg->model_buf = model + COLLECTION_HDR_SIZE;
	if (updated_model)
		cfg->icu_new_model_buf = updated_model + COLLECTION_HDR_SIZE;

	/* is enough capacity in the dst buffer to store the data uncompressed */
	if ((!dst || dst_capacity >= dst_size + col_data_length) &&
	    cfg->cmp_mode != CMP_MODE_RAW) {
		/* we set the compressed buffer size to the data size -1 to provoke
		 * a CMP_ERROR_SMALL_BUF_ error if the data are not compressible
		 */
		cfg->buffer_length = dst_size + col_data_length - 1;
		dst_size_bits = compress_data_internal(cfg, dst_size<<3);

		if (cmp_get_error_code(dst_size_bits) == CMP_ERROR_SMALL_BUF_ ||
		    (!dst && cmp_bit_to_byte(dst_size_bits)-dst_size > col_data_length)) { /* if dst == NULL compress_data_internal will not return a CMP_ERROR_SMALL_BUF */
			/* can not compress the data with the given parameters;
			 * put them uncompressed (raw) into the dst buffer */
			enum cmp_mode cmp_mode_cpy = cfg->cmp_mode;

			cfg->buffer_length = dst_size + col_data_length;
			cfg->cmp_mode = CMP_MODE_RAW;
			dst_size_bits = compress_data_internal(cfg, dst_size<<3);
			cfg->cmp_mode = cmp_mode_cpy;
			/* updated model is in this case a copy of the data to compress */
			if (model_mode_is_used(cfg->cmp_mode) && cfg->icu_new_model_buf)
				memcpy(cfg->icu_new_model_buf, cfg->input_buf, col_data_length);
		}
	} else {
		cfg->buffer_length = dst_capacity;
		dst_size_bits = compress_data_internal(cfg, dst_size<<3);
	}
	FORWARD_IF_ERROR(dst_size_bits, "compression failed");

	dst_size = cmp_bit_to_byte(dst_size_bits);
	if (cfg->cmp_mode != CMP_MODE_RAW && dst) {
		uint8_t *cmp_col_size_field = (uint8_t *)dst+dst_size_begin;
		uint32_t cmp_col_size = dst_size - dst_size_begin -
			COLLECTION_HDR_SIZE - CMP_COLLECTION_FILD_SIZE;

		FORWARD_IF_ERROR(set_cmp_col_size(cmp_col_size_field, cmp_col_size), "");
	}

	return dst_size;
}


/**
 * @brief builds a compressed entity header for a compressed chunk
 *
 * @param entity		start address of the compression entity header
 *				(can be NULL if you only want the entity header
 *				size)
 * @param chunk_size		the original size of the chunk in bytes
 * @param cfg			pointer to the compression configuration used to
 *				compress the chunk
 * @param start_timestamp	the start timestamp of the chunk compression
 * @param cmp_ent_size_byte	the size of the compression entity (entity
 *				header plus compressed data)
 *
 * @return the size of the compressed entity header in bytes or an error code
 *	if it fails (which can be tested with cmp_is_error())
 */

static uint32_t cmp_ent_build_chunk_header(uint32_t *entity, uint32_t chunk_size,
					   const struct cmp_cfg *cfg, uint64_t start_timestamp,
					   uint32_t cmp_ent_size_byte)
{
	if (entity) { /* setup the compressed entity header */
		struct cmp_entity *ent = (struct cmp_entity *)entity;
		int err = 0;

		err |= cmp_ent_set_version_id(ent, version_identifier); /* set by compress_chunk_init */
		err |= cmp_ent_set_size(ent, cmp_ent_size_byte);
		err |= cmp_ent_set_original_size(ent, chunk_size);
		err |= cmp_ent_set_data_type(ent, DATA_TYPE_CHUNK, cfg->cmp_mode == CMP_MODE_RAW);
		err |= cmp_ent_set_cmp_mode(ent, cfg->cmp_mode);
		err |= cmp_ent_set_model_value(ent, cfg->model_value);
		/* model id/counter are set by the user with the compress_chunk_set_model_id_and_counter() */
		err |= cmp_ent_set_model_id(ent, 0);
		err |= cmp_ent_set_model_counter(ent, 0);
		err |= cmp_ent_set_reserved(ent, 0);
		err |= cmp_ent_set_lossy_cmp_par(ent, cfg->round);
		if (cfg->cmp_mode != CMP_MODE_RAW) {
			err |= cmp_ent_set_non_ima_spill1(ent, cfg->spill_par_1);
			err |= cmp_ent_set_non_ima_cmp_par1(ent, cfg->cmp_par_1);
			err |= cmp_ent_set_non_ima_spill2(ent, cfg->spill_par_2);
			err |= cmp_ent_set_non_ima_cmp_par2(ent, cfg->cmp_par_2);
			err |= cmp_ent_set_non_ima_spill3(ent, cfg->spill_par_3);
			err |= cmp_ent_set_non_ima_cmp_par3(ent, cfg->cmp_par_3);
			err |= cmp_ent_set_non_ima_spill4(ent, cfg->spill_par_4);
			err |= cmp_ent_set_non_ima_cmp_par4(ent, cfg->cmp_par_4);
			err |= cmp_ent_set_non_ima_spill5(ent, cfg->spill_par_5);
			err |= cmp_ent_set_non_ima_cmp_par5(ent, cfg->cmp_par_5);
			err |= cmp_ent_set_non_ima_spill6(ent, cfg->spill_par_6);
			err |= cmp_ent_set_non_ima_cmp_par6(ent, cfg->cmp_par_6);
		}
		RETURN_ERROR_IF(err, ENTITY_HEADER, "");
		RETURN_ERROR_IF(cmp_ent_set_start_timestamp(ent, start_timestamp),
				ENTITY_TIMESTAMP, "");
		RETURN_ERROR_IF(cmp_ent_set_end_timestamp(ent, get_timestamp()),
				ENTITY_TIMESTAMP, "");
	}

	if (cfg->cmp_mode == CMP_MODE_RAW)
		return GENERIC_HEADER_SIZE;
	else
		return NON_IMAGETTE_HEADER_SIZE;
}


/**
 * @brief types of chunks containing different types of collections
 *	according to DetailedBudgetWorking_2023-10-11
 */

enum chunk_type {
	CHUNK_TYPE_UNKNOWN,
	CHUNK_TYPE_NCAM_IMAGETTE,
	CHUNK_TYPE_SHORT_CADENCE,
	CHUNK_TYPE_LONG_CADENCE,
	CHUNK_TYPE_SAT_IMAGETTE,
	CHUNK_TYPE_OFFSET_BACKGROUND, /* N-CAM */
	CHUNK_TYPE_SMEARING,
	CHUNK_TYPE_F_CHAIN
};


/**
 * @brief get the chunk_type of a collection
 * @details map a sub-service to a chunk service according to
 *	DetailedBudgetWorking_2023-10-11
 *
 * @param col	pointer to a collection header
 *
 * @returns chunk type of the collection, CHUNK_TYPE_UNKNOWN on
 *	failure
 */

static enum chunk_type cmp_col_get_chunk_type(const struct collection_hdr *col)
{
	enum chunk_type chunk_type;

	switch (cmp_col_get_subservice(col)) {
	case SST_NCxx_S_SCIENCE_IMAGETTE:
		chunk_type = CHUNK_TYPE_NCAM_IMAGETTE;
		break;
	case SST_NCxx_S_SCIENCE_SAT_IMAGETTE:
		chunk_type = CHUNK_TYPE_SAT_IMAGETTE;
		break;
	case SST_NCxx_S_SCIENCE_OFFSET:
	case SST_NCxx_S_SCIENCE_BACKGROUND:
		chunk_type = CHUNK_TYPE_OFFSET_BACKGROUND;
		break;
	case SST_NCxx_S_SCIENCE_SMEARING:
		chunk_type = CHUNK_TYPE_SMEARING;
		break;
	case SST_NCxx_S_SCIENCE_S_FX:
	case SST_NCxx_S_SCIENCE_S_FX_EFX:
	case SST_NCxx_S_SCIENCE_S_FX_NCOB:
	case SST_NCxx_S_SCIENCE_S_FX_EFX_NCOB_ECOB:
		chunk_type = CHUNK_TYPE_SHORT_CADENCE;
		break;
	case SST_NCxx_S_SCIENCE_L_FX:
	case SST_NCxx_S_SCIENCE_L_FX_EFX:
	case SST_NCxx_S_SCIENCE_L_FX_NCOB:
	case SST_NCxx_S_SCIENCE_L_FX_EFX_NCOB_ECOB:
		chunk_type = CHUNK_TYPE_LONG_CADENCE;
		break;
	case SST_FCx_S_SCIENCE_IMAGETTE:
	case SST_FCx_S_SCIENCE_OFFSET_VALUES:
	case SST_FCx_S_BACKGROUND_VALUES:
		chunk_type = CHUNK_TYPE_F_CHAIN;
		break;
	case SST_NCxx_S_SCIENCE_F_FX:
	case SST_NCxx_S_SCIENCE_F_FX_EFX:
	case SST_NCxx_S_SCIENCE_F_FX_NCOB:
	case SST_NCxx_S_SCIENCE_F_FX_EFX_NCOB_ECOB:
		debug_print("Error: No chunk is defined for fast cadence subservices");
		/* fall through */
	default:
		chunk_type = CHUNK_TYPE_UNKNOWN;
		break;
	}

	return chunk_type;
}


/**
 * @brief Set the compression configuration from the compression parameters
 *	based on the chunk type
 *
 * @param[in] par		pointer to a compression parameters struct
 * @param[in] chunk_type	the type of the chunk to be compressed
 * @param[out] cfg		pointer to a compression configuration
 */

static void init_cmp_cfg_from_cmp_par(const struct cmp_par *par, enum chunk_type chunk_type,
				     struct cmp_cfg *cfg)
{
	memset(cfg, 0, sizeof(struct cmp_cfg));

	/* the ranges of the parameters are checked in cmp_cfg_icu_is_invalid() */
	cfg->cmp_mode = par->cmp_mode;
	cfg->model_value = par->model_value;
	cfg->round = par->lossy_par;

	switch (chunk_type) {
	case CHUNK_TYPE_NCAM_IMAGETTE:
		cfg->cmp_par_imagette = par->nc_imagette;
		break;
	case CHUNK_TYPE_SAT_IMAGETTE:
		cfg->cmp_par_imagette = par->saturated_imagette;
		break;
	case CHUNK_TYPE_SHORT_CADENCE:
		cfg->cmp_par_exp_flags = par->s_exp_flags;
		cfg->cmp_par_fx = par->s_fx;
		cfg->cmp_par_ncob = par->s_ncob;
		cfg->cmp_par_efx = par->s_efx;
		cfg->cmp_par_ecob = par->s_ecob;
		break;
	case CHUNK_TYPE_LONG_CADENCE:
		cfg->cmp_par_exp_flags = par->l_exp_flags;
		cfg->cmp_par_fx = par->l_fx;
		cfg->cmp_par_ncob = par->l_ncob;
		cfg->cmp_par_efx = par->l_efx;
		cfg->cmp_par_ecob = par->l_ecob;
		cfg->cmp_par_fx_cob_variance = par->l_fx_cob_variance;
		break;
	case CHUNK_TYPE_OFFSET_BACKGROUND:
		cfg->cmp_par_offset_mean = par->nc_offset_mean;
		cfg->cmp_par_offset_variance = par->nc_offset_variance;

		cfg->cmp_par_background_mean = par->nc_background_mean;
		cfg->cmp_par_background_variance = par->nc_background_variance;
		cfg->cmp_par_background_pixels_error = par->nc_background_outlier_pixels;
		break;

	case CHUNK_TYPE_SMEARING:
		cfg->cmp_par_smearing_mean = par->smearing_mean;
		cfg->cmp_par_smearing_variance = par->smearing_variance_mean;
		cfg->cmp_par_smearing_pixels_error = par->smearing_outlier_pixels;
		break;

	case CHUNK_TYPE_F_CHAIN:
		cfg->cmp_par_imagette = par->fc_imagette;

		cfg->cmp_par_offset_mean = par->fc_offset_mean;
		cfg->cmp_par_offset_variance = par->fc_offset_variance;

		cfg->cmp_par_background_mean = par->fc_background_mean;
		cfg->cmp_par_background_variance = par->fc_background_variance;
		cfg->cmp_par_background_pixels_error = par->fc_background_outlier_pixels;
		break;
	case CHUNK_TYPE_UNKNOWN:
		break;
	}

	cfg->spill_par_1 = cmp_guess_good_spill(cfg->cmp_par_1);
	cfg->spill_par_2 = cmp_guess_good_spill(cfg->cmp_par_2);
	cfg->spill_par_3 = cmp_guess_good_spill(cfg->cmp_par_3);
	cfg->spill_par_4 = cmp_guess_good_spill(cfg->cmp_par_4);
	cfg->spill_par_5 = cmp_guess_good_spill(cfg->cmp_par_5);
	cfg->spill_par_6 = cmp_guess_good_spill(cfg->cmp_par_6);
}


/**
 * @brief initialise the compress_chunk() function
 *
 * If not initialised the compress_chunk() function sets the timestamps and
 * version_id in the compression entity header to zero
 *
 * @param return_timestamp	pointer to a function returning a current 48-bit
 *				timestamp
 * @param version_id		application software version identifier
 */

void compress_chunk_init(uint64_t(return_timestamp)(void), uint32_t version_id)
{
	if (return_timestamp)
		get_timestamp = return_timestamp;

	version_identifier = version_id;
}


/**
 * @brief compress a data chunk consisting of put together data collections
 *
 * @param chunk			pointer to the chunk to be compressed
 * @param chunk_size		byte size of the chunk
 * @param chunk_model		pointer to a model of a chunk; has the same size
 *				as the chunk (can be NULL if no model compression
 *				mode is used)
 * @param updated_chunk_model	pointer to store the updated model for the next
 *				model mode compression; has the same size as the
 *				chunk (can be the same as the model_of_data
 *				buffer for in-place update or NULL if updated
 *				model is not needed)
 * @param dst			destination pointer to the compressed data
 *				buffer; has to be 4-byte aligned; can be NULL to
 *				only get the compressed data size
 * @param dst_capacity		capacity of the dst buffer;  it's recommended to
 *				provide a dst_capacity >=
 *				compress_chunk_cmp_size_bound(chunk, chunk_size)
 *				as it eliminates one potential failure scenario:
 *				not enough space in the dst buffer to write the
 *				compressed data; size is internally rounded down
 *				to a multiple of 4
 * @returns the byte size of the compressed data or an error code if it
 *	fails (which can be tested with cmp_is_error())
 */

uint32_t compress_chunk(void *chunk, uint32_t chunk_size,
			void *chunk_model, void *updated_chunk_model,
			uint32_t *dst, uint32_t dst_capacity,
			const struct cmp_par *cmp_par)
{
	uint64_t start_timestamp = get_timestamp();
	struct collection_hdr *col = (struct collection_hdr *)chunk;
	enum chunk_type chunk_type;
	struct cmp_cfg cfg;
	uint32_t cmp_size_byte; /* size of the compressed data in bytes */
	size_t read_bytes;

	RETURN_ERROR_IF(chunk == NULL, CHUNK_NULL, "");
	RETURN_ERROR_IF(cmp_par == NULL, PAR_NULL, "");
	RETURN_ERROR_IF(chunk_size < COLLECTION_HDR_SIZE, CHUNK_SIZE_INCONSISTENT,
			"chunk_size: %"PRIu32"", chunk_size);
	RETURN_ERROR_IF(chunk_size > CMP_ENTITY_MAX_ORIGINAL_SIZE, CHUNK_TOO_LARGE,
			"chunk_size: %"PRIu32"", chunk_size);

	chunk_type = cmp_col_get_chunk_type(col);
	RETURN_ERROR_IF(chunk_type == CHUNK_TYPE_UNKNOWN, COL_SUBSERVICE_UNSUPPORTED,
			"unsupported subservice: %u", cmp_col_get_subservice(col));

	init_cmp_cfg_from_cmp_par(cmp_par, chunk_type, &cfg);

	/* reserve space for the compression entity header, we will build the
	 * header after the compression of the chunk
	 */
	cmp_size_byte = cmp_ent_build_chunk_header(NULL, chunk_size, &cfg, start_timestamp, 0);
	if (dst) {
		RETURN_ERROR_IF(dst_capacity < cmp_size_byte, SMALL_BUF_,
				"dst_capacity must be at least as large as the minimum size of the compression unit.");
		memset(dst, 0, cmp_size_byte);
	}

	/* compress one collection after another */
	for (read_bytes = 0;
	     read_bytes <= chunk_size - COLLECTION_HDR_SIZE;
	     read_bytes += cmp_col_get_size(col))
	{
		uint8_t *col_model = NULL;
		uint8_t *col_up_model = NULL;

		/* setup pointers for the next collection we want to compress */
		col = (struct collection_hdr *)((uint8_t *)chunk + read_bytes);
		if (chunk_model)
			col_model = ((uint8_t *)chunk_model + read_bytes);
		if (updated_chunk_model)
			col_up_model = ((uint8_t *)updated_chunk_model + read_bytes);

		RETURN_ERROR_IF(cmp_col_get_chunk_type(col) != chunk_type, CHUNK_SUBSERVICE_INCONSISTENT, "");

		/* chunk size is inconsistent with the sum of sizes in the collection headers */
		if (read_bytes + cmp_col_get_size(col) > chunk_size)
			break;

		cmp_size_byte = cmp_collection((uint8_t *)col, col_model, col_up_model,
					       dst, dst_capacity, &cfg, cmp_size_byte);
		FORWARD_IF_ERROR(cmp_size_byte, "error occurred when compressing the collection with offset %u", read_bytes);
	}

	RETURN_ERROR_IF(read_bytes != chunk_size, CHUNK_SIZE_INCONSISTENT, "");

	FORWARD_IF_ERROR(cmp_ent_build_chunk_header(dst, chunk_size, &cfg,
					    start_timestamp, cmp_size_byte), "");

	return cmp_size_byte;
}


/**
 * @brief returns the maximum compressed size in a worst-case scenario
 * In case the input data is not compressible
 * This function is primarily useful for memory allocation purposes
 * (destination buffer size).
 *
 * @note if the number of collections is known you can use the
 *	COMPRESS_CHUNK_BOUND macro for compilation-time evaluation
 *	(stack memory allocation for example)
 *
 * @param chunk		pointer to the chunk you want to compress
 * @param chunk_size	size of the chunk in bytes
 *
 * @returns maximum compressed size for a chunk compression on success or an
 *	error code if it fails (which can be tested with cmp_is_error())
 */

uint32_t compress_chunk_cmp_size_bound(const void *chunk, size_t chunk_size)
{
	int32_t read_bytes;
	uint32_t num_col = 0;
	size_t bound;
	size_t const max_chunk_size = CMP_ENTITY_MAX_ORIGINAL_SIZE
		- NON_IMAGETTE_HEADER_SIZE - CMP_COLLECTION_FILD_SIZE;

	RETURN_ERROR_IF(chunk == NULL, CHUNK_NULL, "");
	RETURN_ERROR_IF(chunk_size < COLLECTION_HDR_SIZE, CHUNK_SIZE_INCONSISTENT, "");
	RETURN_ERROR_IF(chunk_size > max_chunk_size, CHUNK_TOO_LARGE,
			"chunk_size: %"PRIu32" > max_chunk_size: %"PRIu32"",
			chunk_size, max_chunk_size);

	/* count the number of collections in the chunk */
	for (read_bytes = 0;
	     read_bytes <= (int32_t)(chunk_size-COLLECTION_HDR_SIZE);
	     read_bytes += cmp_col_get_size((const struct collection_hdr *)((const uint8_t *)chunk + read_bytes)))
		num_col++;

	RETURN_ERROR_IF((uint32_t)read_bytes != chunk_size, CHUNK_SIZE_INCONSISTENT, "");

	bound = COMPRESS_CHUNK_BOUND_UNSAFE(chunk_size, num_col);
	RETURN_ERROR_IF(bound > CMP_ENTITY_MAX_SIZE, CHUNK_TOO_LARGE, "bound: %lu", bound);

	return (uint32_t)bound;
}


/**
 * @brief set the model id and model counter in the compression entity header
 *
 * @param dst		pointer to the compressed data (starting with a
 *			compression entity header)
 * @param dst_size	byte size of the dst buffer
 * @param model_id	model identifier; for identifying entities that originate
 *			from the same starting model
 * @param model_counter	model_counter; counts how many times the model was
 *			updated; for non model mode compression use 0
 *
 * @returns the byte size of the dst buffer (= dst_size) on success or an error
 *	code if it fails (which can be tested with cmp_is_error())
 */

uint32_t compress_chunk_set_model_id_and_counter(void *dst, uint32_t dst_size,
						 uint16_t model_id, uint8_t model_counter)
{
	RETURN_ERROR_IF(dst == NULL, ENTITY_NULL, "");
	FORWARD_IF_ERROR(dst_size, "");
	RETURN_ERROR_IF(dst_size < GENERIC_HEADER_SIZE, ENTITY_TOO_SMALL,
			"dst_size: %"PRIu32"", dst_size);

	cmp_ent_set_model_id(dst, model_id);
	cmp_ent_set_model_counter(dst, model_counter);

	return dst_size;
}


/**
 * @brief compress data the same way as the RDCU HW compressor
 *
 * @param rcfg	pointer to a RDCU compression configuration (created with the
 *		rdcu_cfg_create() function, set up with the rdcu_cfg_buffers()
 *		and rdcu_cfg_imagette() functions)
 * @param info	pointer to a compression information structure contains the
 *		metadata of a compression (can be NULL)
 *
 * @returns the bit length of the bitstream on success; negative on error,
 *	CMP_ERROR_SMALL_BUF (-2) if the compressed data buffer is too small to
 *	hold the whole compressed data
 *
 * @warning only the small buffer error in the info.cmp_err field is implemented
 */

int32_t compress_like_rdcu(const struct rdcu_cfg *rcfg, struct cmp_info *info)
{
	struct cmp_cfg cfg = {0};
	uint32_t cmp_size_bit;

	if (info)
		memset(info, 0, sizeof(*info));

	if (!rcfg)
		return (int32_t)compress_data_internal(NULL, 0);

	cfg.data_type = DATA_TYPE_IMAGETTE;

	cfg.input_buf = rcfg->input_buf;
	cfg.model_buf = rcfg->model_buf;
	cfg.samples = rcfg->samples;
	cfg.buffer_length = (rcfg->buffer_length * sizeof(uint16_t));
	cfg.cmp_mode = rcfg->cmp_mode;
	cfg.model_value = rcfg->model_value;
	cfg.round = rcfg->round;

	if (info) {
		info->cmp_err = 0;
		info->cmp_mode_used = (uint8_t)rcfg->cmp_mode;
		info->model_value_used = (uint8_t)rcfg->model_value;
		info->round_used = (uint8_t)rcfg->round;
		info->spill_used = rcfg->spill;
		info->golomb_par_used = rcfg->golomb_par;
		info->samples_used = rcfg->samples;
		info->rdcu_new_model_adr_used = rcfg->rdcu_new_model_adr;
		info->rdcu_cmp_adr_used = rcfg->rdcu_buffer_adr;

		if (rcfg->ap1_golomb_par && rcfg->ap1_golomb_par) {
			uint32_t ap1_cmp_size, ap2_cmp_size;

			cfg.cmp_par_imagette = rcfg->ap1_golomb_par;
			cfg.spill_imagette = rcfg->ap1_spill;
			ap1_cmp_size = compress_data_internal(&cfg, 0);
			if (cmp_is_error(ap1_cmp_size) || ap1_cmp_size > INT32_MAX)
				ap1_cmp_size = 0;

			cfg.cmp_par_imagette = rcfg->ap2_golomb_par;
			cfg.spill_imagette = rcfg->ap2_spill;
			ap2_cmp_size = compress_data_internal(&cfg, 0);
			if (cmp_is_error(ap2_cmp_size) || ap2_cmp_size > INT32_MAX)
				ap2_cmp_size = 0;

			info->ap1_cmp_size = ap1_cmp_size;
			info->ap2_cmp_size = ap2_cmp_size;
		}
	}

	cfg.cmp_par_imagette = rcfg->golomb_par;
	cfg.spill_imagette = rcfg->spill;
	cfg.icu_new_model_buf = rcfg->icu_new_model_buf;
	cfg.icu_output_buf = rcfg->icu_output_buf;

	cmp_size_bit = compress_data_internal(&cfg, 0);

	if (info) {
		if (cmp_get_error_code(cmp_size_bit) == CMP_ERROR_SMALL_BUF_)
			info->cmp_err |= 1UL << 0;/* SMALL_BUFFER_ERR_BIT;*/ /* set small buffer error */
		if (cmp_is_error(cmp_size_bit))
			info->cmp_size = 0;
		else
			info->cmp_size = cmp_size_bit;
	}

	return (int32_t)cmp_size_bit;
}
