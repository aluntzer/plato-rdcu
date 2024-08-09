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

#include "../common/byteorder.h"
#include "../common/compiler.h"

#include "read_bitstream.h"
#include "cmp_max_used_bits_list.h"
#include "../decmp.h"
#include "../common/cmp_debug.h"
#include "../common/cmp_support.h"
#include "../common/cmp_entity.h"
#include "../common/cmp_max_used_bits.h"


#define CORRUPTION_DETECTED -1


MAYBE_UNUSED static const char *please_check_str =
	"Please check that the compression parameters match those used to compress the data and that the compressed data are not corrupted.";


/**
 * @brief function pointer to a code word decoder function
 */

typedef uint32_t(*decoder_ptr)(struct bit_decoder *, uint32_t, uint32_t);


/**
 * @brief structure to hold all parameters to decode a value
 */

struct decoder_setup {
	int (*decode_method_f)(const struct decoder_setup *setup,
			       uint32_t *decoded_value); /* pointer to the decoding function with escape mechanism */
	decoder_ptr decode_cw_f; /* pointer to the code word decoder function (Golomb/Rice/unary) */
	struct bit_decoder *dec; /* pointer to a bit_decoder context */
	uint32_t encoder_par1;   /* encoding parameter 1 */
	uint32_t encoder_par2;   /* encoding parameter 2 */
	uint32_t outlier_par;    /* outlier parameter */
	uint32_t lossy_par;      /* lossy compression parameter */
	uint32_t max_data_bits;  /* bit length of the decoded value */
};


enum decmp_type {ICU_DECOMRESSION, RDCU_DECOMPRESSION};


/**
 * @brief decode the next unary code word in the bitstream
 *
 * @param dec		a pointer to a bit_decoder context
 * @param m		this parameter is not used
 * @param log2_m	this parameter is not used
 * @note: Can be used to decode a code word with compression parameter m = 1 (log2_m = 0)
 *
 * @returns the decoded value
 */

static __inline uint32_t unary_decoder(struct bit_decoder *dec, uint32_t m UNUSED,
				       uint32_t log2_m UNUSED)
{
	uint32_t const decoded_cw = bit_peek_leading_ones(dec); /* decode unary coding */
	uint32_t const cw_len = decoded_cw + 1; /* Number of 1's + following 0 */

	bit_consume_bits(dec, cw_len);

	return decoded_cw;
}


/**
 * @brief decode the next Rice code word in the bitstream
 *
 * @param dec		a pointer to a bit_decoder context
 * @param m		Golomb parameter, must be the same used for encoding
 * @param log2_m	Rice parameter, is ilog_2(m), must be larger than 0
 * @note the Golomb parameter (m) must be a power of 2
 * @warning the Rice parameter (log2_m) must be greater than 0! If you want to
 *	use a Rice parameter equal to 0, use the unary_decoder instead.
 *
 * @returns the decoded value
 */

static uint32_t rice_decoder(struct bit_decoder *dec, uint32_t m, uint32_t log2_m)
{
	uint32_t q;  /* quotient */
	uint32_t r;  /* remainder */

	assert(log2_m > 0 && log2_m < 32);

	q = unary_decoder(dec, m, log2_m); /* decode quotient unary code part */
	r = bit_read_bits32(dec, log2_m); /* get remainder */

	return (q << log2_m) + r; /* calculate decoded value (q*m+r) */
}


/**
 * @brief decode the next Golomb code word in the bitstream
 *
 * @param dec		a pointer to a bit_decoder context
 * @param m		Golomb parameter (has to be bigger than 0)
 * @param log2_m	is ilog_2(m) calculate outside function for better
 *			performance
 *
 * @returns the decoded value
 */

static uint32_t golomb_decoder(struct bit_decoder *dec, uint32_t m, uint32_t log2_m)
{
	uint32_t q;  /* quotient */
	uint32_t r1; /* remainder case 1 */
	uint32_t r2; /* remainder case 2 */
	uint32_t r;  /* remainder */
	uint32_t cutoff; /* cutoff between group 1 and 2 */

	assert(m > 0);
	assert(log2_m == ilog_2(m));

	/* decode quotient unary code part */
	q = unary_decoder(dec, m, log2_m);

	/* get the remainder code for both cases */
	r2 = (uint32_t)bit_peek_bits(dec, log2_m+1);
	r1 = r2 >> 1;

	/* calculate cutoff between case 1 and 2 */
	cutoff = (0x2U << log2_m) - m; /* = 2^(log2_m+1)-m */

	if (r1 < cutoff) { /* remainder case 1: remainder length=log2_m */
		bit_consume_bits(dec, log2_m);
		r = r1;
	} else { /* remainder case 2: remainder length = log2_m+1 */
		bit_consume_bits(dec, log2_m+1);
		r = r2 - cutoff;
	}

	return q*m + r;
}


/**
 * @brief select the decoder based on the used Golomb parameter
 *
 * @param golomb_par	Golomb parameter, has to be bigger than 0
 *
 * @note if the Golomb parameter is a power of 2 we can use the faster Rice decoder
 * @note if the Golomb parameter is 1 we can use the even faster unary decoder
 *
 * @returns function pointer to the select code word decoder function
 */

static decoder_ptr select_decoder(uint32_t golomb_par)
{
	assert(golomb_par > 0);

	if (golomb_par == 1)
		return &unary_decoder;

	if (is_a_pow_of_2(golomb_par))
		return &rice_decoder;
	else
		return &golomb_decoder;
}


/**
 * @brief decode the next code word with zero escape system mechanism from the bitstream
 *
 * @param setup		pointer to the decoder setup
 * @param decoded_value	points to the location where the decoded value is stored
 *
 * @returns 0 on success; otherwise error
 */

static int decode_zero(const struct decoder_setup *setup, uint32_t *decoded_value)
{
	/* Decode the next value in the bitstream with the Golomb/Rice/unary decoder */
	*decoded_value = setup->decode_cw_f(setup->dec, setup->encoder_par1, setup->encoder_par2);

	if (*decoded_value != 0) { /* no escape symbol detected */
		if (*decoded_value >= setup->outlier_par) {
			debug_print("Error: Data consistency check failed. Non-outlier decoded value greater or equal than the outlier parameter. %s", please_check_str);
			return CORRUPTION_DETECTED;
		}
		*decoded_value -= 1;
	} else {
		/* the zero escape symbol mechanism was used; read unencoded value */
		bit_refill(setup->dec);
		*decoded_value = bit_read_bits32_sub_1(setup->dec, setup->max_data_bits);

		if (*decoded_value < setup->outlier_par - 1) { /* -1 because we subtract -1 from the *decoded_value */
			if (bit_refill(setup->dec) != BIT_OVERFLOW)
				debug_print("Error: Data consistency check failed. Outlier small than the outlier parameter. %s", please_check_str);
			return CORRUPTION_DETECTED;
		}
	}
	return bit_refill(setup->dec) == BIT_OVERFLOW;
}


/**
 * @brief decode the next code word with the multi escape mechanism from the bitstream
 *
 * @param setup		pointer to the decoder setup
 * @param decoded_value	points to the location where the decoded value is stored
 *
 * @returns 0 on success; otherwise error
 */

static int decode_multi(const struct decoder_setup *setup, uint32_t *decoded_value)
{
	/* Decode the next value in the bitstream with the Golomb/Rice/unary decoder */
	*decoded_value = setup->decode_cw_f(setup->dec, setup->encoder_par1, setup->encoder_par2);

	if (*decoded_value >= setup->outlier_par) { /* escape symbol mechanism detected */
		uint32_t const unencoded_len = (*decoded_value - setup->outlier_par + 1) << 1;

		if (unencoded_len > ((setup->max_data_bits+1) & -2U)) { /* round up max_data_bits to the nearest multiple of 2 */
			debug_print("Error: Data consistency check failed. Multi escape symbol higher than expected. %s", please_check_str);
			return CORRUPTION_DETECTED;
		}

		/* read unencoded value */
		bit_refill(setup->dec);
		*decoded_value = bit_read_bits32(setup->dec, unencoded_len);

		if (*decoded_value >> (unencoded_len-2) == 0) { /* check if at least one bit of the two highest is set. */
			if (unencoded_len > 2) { /* Exception: if we code outlier_par as outlier, no set bit is expected */
				if (bit_refill(setup->dec) != BIT_OVERFLOW)
					debug_print("Error: Data consistency check failed. Unencoded data after multi escape symbol to small. %s", please_check_str);
				return CORRUPTION_DETECTED;
			}
		}

		*decoded_value += setup->outlier_par;

		if ((*decoded_value & BIT_MASK[setup->max_data_bits]) < setup->outlier_par) { /* check for overflow in addition */
			if (bit_refill(setup->dec) != BIT_OVERFLOW)
				debug_print("Error: Data consistency check failed. Outlier small than the outlier parameter. %s", please_check_str);
			return CORRUPTION_DETECTED;
		}
	}
	return bit_refill(setup->dec) == BIT_OVERFLOW;
}


/**
 * @brief remap an unsigned value back to a signed value
 * @note this is the reverse function of map_to_pos()
 *
 * @param value_to_unmap	unsigned value to remap
 *
 * @returns the signed remapped value
 */

static __inline uint32_t re_map_to_pos(uint32_t value_to_unmap)
{
	if (value_to_unmap & 0x1) { /* if uneven */
		/* uint64_t to prevent overflow if value_to_unmap == 0xFFFFFFFF */
		uint64_t tmp64 = value_to_unmap;

		return (uint32_t)(-((tmp64 + 1) / 2));
	} else {
		return value_to_unmap / 2;
	}
}


/**
 * @brief decompress the next code word in the bitstream and decorrelate it with
 *	the model
 *
 * @param setup		pointer to the decoder setup
 * @param decoded_value	points to the location where the decoded value is stored
 * @param model		model of the decoded_value (0 if not used)
 *
 * @returns 0 on success; otherwise error
 */

static int decode_value(const struct decoder_setup *setup, uint32_t *decoded_value,
			uint32_t model)
{
	/* decode the next value from the bitstream */
	int err = setup->decode_method_f(setup, decoded_value);

	/* map the unsigned decode value back to a signed value */
	*decoded_value = re_map_to_pos(*decoded_value);

	/* decorrelate data the data with the model */
	*decoded_value += round_fwd(model, setup->lossy_par);

	/* we mask only the used bits in case there is an overflow when adding the model */
	*decoded_value &= BIT_MASK[setup->max_data_bits];

	/* inverse step of the lossy compression */
	*decoded_value = round_inv(*decoded_value, setup->lossy_par);

	return err;
}


/**
 * @brief configure a decoder setup structure to have a setup to decode a value
 *
 * @param setup		pointer to the decoder setup
 * @param dec		pointer to a bit_decoder context
 * @param cmp_mode	compression mode
 * @param cmp_par	compression parameter
 * @param spillover	spillover_par parameter
 * @param lossy_par	lossy compression parameter
 * @param max_data_bits	how many bits are needed to represent the highest possible value
 */

static void configure_decoder_setup(struct decoder_setup *setup, struct bit_decoder *dec,
				    enum cmp_mode cmp_mode, uint32_t cmp_par,
				    uint32_t spillover, uint32_t lossy_par,
				    uint32_t max_data_bits)
{
	assert(setup != NULL);
	assert(dec != NULL);
	assert(cmp_par != 0);
	assert(max_data_bits > 0 && max_data_bits <= 32);

	if (multi_escape_mech_is_used(cmp_mode))
		setup->decode_method_f = &decode_multi;
	else if (zero_escape_mech_is_used(cmp_mode))
		setup->decode_method_f = &decode_zero;
	else {
		debug_print("Error: Compression mode not supported.");
		assert(0);
	}
	setup->decode_cw_f = select_decoder(cmp_par);
	setup->dec = dec;
	setup->encoder_par1 = cmp_par; /* encoding parameter 1 */
	setup->encoder_par2 = ilog_2(cmp_par); /* encoding parameter 2 */
	setup->outlier_par = spillover; /* outlier parameter */
	setup->lossy_par = lossy_par; /* lossy compression parameter */
	setup->max_data_bits = max_data_bits; /* how many bits are needed to represent the highest possible value */
}


/**
 * @brief return a pointer of the data of a collection
 *
 * @param col	pointer to a collection header (can be NULL)
 *
 * @returns pointer to the collection data; NULL if col is NULL
 */

static void *get_collection_data(void *col)
{
	if (col)
		col = (uint8_t *)col + COLLECTION_HDR_SIZE;
	return col;
}


/**
 * @brief decompress imagette data
 *
 * @param cfg	pointer to the compression configuration structure
 * @param dec	a pointer to a bit_decoder context
 *
 * @returns 0 on success; otherwise error
 */

static int decompress_imagette(const struct cmp_cfg *cfg, struct bit_decoder *dec, enum decmp_type decmp_type)
{
	size_t i;
	int err;
	uint32_t decoded_value;
	uint32_t max_data_bits;
	struct decoder_setup setup;
	uint16_t *data_buf;
	uint16_t *model_buf;
	uint16_t *up_model_buf;
	uint16_t *next_model_p;
	uint16_t model;

	switch (decmp_type) {
	case RDCU_DECOMPRESSION: /* RDCU compresses the header like data */
		data_buf = cfg->input_buf;
		model_buf = cfg->model_buf;
		up_model_buf = cfg->icu_new_model_buf;
		break;
	case ICU_DECOMRESSION:
		data_buf = get_collection_data(cfg->input_buf);
		model_buf = get_collection_data(cfg->model_buf);
		up_model_buf = get_collection_data(cfg->icu_new_model_buf);
		break;
	}

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		up_model_buf = NULL;
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	switch (cfg->data_type) {
	case DATA_TYPE_IMAGETTE:
	case DATA_TYPE_IMAGETTE_ADAPTIVE:
		max_data_bits = cfg->max_used_bits->nc_imagette;
		break;
	case DATA_TYPE_SAT_IMAGETTE:
	case DATA_TYPE_SAT_IMAGETTE_ADAPTIVE:
		max_data_bits = cfg->max_used_bits->saturated_imagette;
		break;
	default:
	case DATA_TYPE_F_CAM_IMAGETTE:
	case DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE:
		max_data_bits = cfg->max_used_bits->fc_imagette;
		break;
	}

	configure_decoder_setup(&setup, dec, cfg->cmp_mode, cfg->golomb_par,
				cfg->spill, cfg->round, max_data_bits);

	for (i = 0; ; i++) {
		err = decode_value(&setup, &decoded_value, model);
		if (err)
			break;
		data_buf[i] = (__typeof__(data_buf[i]))decoded_value;

		if (up_model_buf)
			up_model_buf[i] = cmp_up_model(data_buf[i], model, cfg->model_value,
						       setup.lossy_par);

		if (i >= cfg->samples-1)
			break;

		model = next_model_p[i];
	}
	return err;
}


#if 0
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
	if (cfg->buffer_length < COLLECTION_HDR_SIZE)
		return -1;

	if (*data) {
		if (cfg->icu_output_buf)
			memcpy(*data, cfg->icu_output_buf, COLLECTION_HDR_SIZE);
		*data = (uint8_t *)*data + COLLECTION_HDR_SIZE;
	}

	if (*model) {
		if (cfg->icu_output_buf)
			memcpy(*model, cfg->icu_output_buf, COLLECTION_HDR_SIZE);
		*model = (uint8_t *)*model + COLLECTION_HDR_SIZE;
	}

	if (*up_model) {
		if (cfg->icu_output_buf)
			memcpy(*up_model, cfg->icu_output_buf, COLLECTION_HDR_SIZE);
		*up_model = (uint8_t *)*up_model + COLLECTION_HDR_SIZE;
	}

	return COLLECTION_HDR_SIZE * CHAR_BIT;
}
#endif


/**
 * @brief decompress short normal light flux (S_FX) data
 *
 * @param cfg	pointer to the compression configuration structure
 * @param dec	a pointer to a bit_decoder context
 *
 * @returns 0 on success; otherwise error
 */

static int decompress_s_fx(const struct cmp_cfg *cfg, struct bit_decoder *dec)
{
	size_t i;
	int err;
	uint32_t decoded_value;
	struct decoder_setup setup_exp_flags, setup_fx;
	struct s_fx *data_buf = get_collection_data(cfg->input_buf);
	struct s_fx *model_buf = get_collection_data(cfg->model_buf);
	struct s_fx *up_model_buf;
	struct s_fx *next_model_p;
	struct s_fx model;

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
		up_model_buf = get_collection_data(cfg->icu_new_model_buf);
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
		up_model_buf = NULL;
	}

	configure_decoder_setup(&setup_exp_flags, dec, cfg->cmp_mode, cfg->cmp_par_exp_flags,
				cfg->spill_exp_flags, cfg->round, cfg->max_used_bits->s_exp_flags);
	configure_decoder_setup(&setup_fx, dec, cfg->cmp_mode, cfg->cmp_par_fx,
				cfg->spill_fx, cfg->round, cfg->max_used_bits->s_fx);

	for (i = 0; ; i++) {
		err = decode_value(&setup_exp_flags, &decoded_value, model.exp_flags);
		if (err)
			break;
		data_buf[i].exp_flags = (__typeof__(data_buf[i].exp_flags))decoded_value;

		err = decode_value(&setup_fx, &decoded_value, model.fx);
		if (err)
			break;
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
	return err;
}


/**
 * @brief decompress S_FX_EFX data
 *
 * @param cfg	pointer to the compression configuration structure
 * @param dec	a pointer to a bit_decoder context
 *
 * @returns 0 on success; otherwise error
 */

static int decompress_s_fx_efx(const struct cmp_cfg *cfg, struct bit_decoder *dec)
{
	size_t i;
	int err;
	uint32_t decoded_value;
	struct decoder_setup setup_exp_flags, setup_fx, setup_efx;
	struct s_fx_efx *data_buf = get_collection_data(cfg->input_buf);
	struct s_fx_efx *model_buf = get_collection_data(cfg->model_buf);
	struct s_fx_efx *up_model_buf;
	struct s_fx_efx *next_model_p;
	struct s_fx_efx model;

	if (model_mode_is_used(cfg->cmp_mode)) {
		up_model_buf = get_collection_data(cfg->icu_new_model_buf);
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		up_model_buf = NULL;
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_decoder_setup(&setup_exp_flags, dec, cfg->cmp_mode, cfg->cmp_par_exp_flags,
				cfg->spill_exp_flags, cfg->round, cfg->max_used_bits->s_exp_flags);
	configure_decoder_setup(&setup_fx, dec, cfg->cmp_mode, cfg->cmp_par_fx,
				cfg->spill_fx, cfg->round, cfg->max_used_bits->s_fx);
	configure_decoder_setup(&setup_efx, dec, cfg->cmp_mode, cfg->cmp_par_efx,
				cfg->spill_efx, cfg->round, cfg->max_used_bits->s_efx);

	for (i = 0; ; i++) {
		err = decode_value(&setup_exp_flags, &decoded_value, model.exp_flags);
		if (err)
			break;
		data_buf[i].exp_flags = (__typeof__(data_buf[i].exp_flags)) decoded_value;

		err = decode_value(&setup_fx, &decoded_value, model.fx);
		if (err)
			break;
		data_buf[i].fx = decoded_value;

		err = decode_value(&setup_efx, &decoded_value, model.efx);
		if (err)
			break;
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
	return err;
}


/**
 * @brief decompress short S_FX_NCOB data
 *
 * @param cfg	pointer to the compression configuration structure
 * @param dec	a pointer to a bit_decoder context
 *
 * @returns 0 on success; otherwise error
 */

static int decompress_s_fx_ncob(const struct cmp_cfg *cfg, struct bit_decoder *dec)
{
	size_t i;
	int err;
	uint32_t decoded_value;
	struct decoder_setup setup_exp_flags, setup_fx, setup_ncob;
	struct s_fx_ncob *data_buf = get_collection_data(cfg->input_buf);
	struct s_fx_ncob *model_buf = get_collection_data(cfg->model_buf);
	struct s_fx_ncob *up_model_buf;
	struct s_fx_ncob *next_model_p;
	struct s_fx_ncob model;

	if (model_mode_is_used(cfg->cmp_mode)) {
		up_model_buf = get_collection_data(cfg->icu_new_model_buf);
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		up_model_buf = NULL;
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_decoder_setup(&setup_exp_flags, dec, cfg->cmp_mode, cfg->cmp_par_exp_flags,
				cfg->spill_exp_flags, cfg->round, cfg->max_used_bits->s_exp_flags);
	configure_decoder_setup(&setup_fx, dec, cfg->cmp_mode, cfg->cmp_par_fx,
				cfg->spill_fx, cfg->round, cfg->max_used_bits->s_fx);
	configure_decoder_setup(&setup_ncob, dec, cfg->cmp_mode, cfg->cmp_par_ncob,
				cfg->spill_ncob, cfg->round, cfg->max_used_bits->s_ncob);

	for (i = 0; ; i++) {
		err = decode_value(&setup_exp_flags, &decoded_value, model.exp_flags);
		if (err)
			break;
		data_buf[i].exp_flags = (__typeof__(data_buf[i].exp_flags)) decoded_value;

		err = decode_value(&setup_fx, &decoded_value, model.fx);
		if (err)
			break;
		data_buf[i].fx = decoded_value;

		err = decode_value(&setup_ncob, &decoded_value, model.ncob_x);
		if (err)
			break;
		data_buf[i].ncob_x = decoded_value;

		err = decode_value(&setup_ncob, &decoded_value, model.ncob_y);
		if (err)
			break;
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
	return err;
}


/**
 * @brief decompress short S_FX_NCOB_ECOB data
 *
 * @param cfg	pointer to the compression configuration structure
 * @param dec	a pointer to a bit_decoder context
 *
 * @returns 0 on success; otherwise error
 */

static int decompress_s_fx_efx_ncob_ecob(const struct cmp_cfg *cfg, struct bit_decoder *dec)
{
	size_t i;
	int err;
	uint32_t decoded_value;
	struct decoder_setup setup_exp_flags, setup_fx, setup_ncob, setup_efx, setup_ecob;
	struct s_fx_efx_ncob_ecob *data_buf = get_collection_data(cfg->input_buf);
	struct s_fx_efx_ncob_ecob *model_buf = get_collection_data(cfg->model_buf);
	struct s_fx_efx_ncob_ecob *up_model_buf;
	struct s_fx_efx_ncob_ecob *next_model_p;
	struct s_fx_efx_ncob_ecob model;

	if (model_mode_is_used(cfg->cmp_mode)) {
		up_model_buf = get_collection_data(cfg->icu_new_model_buf);
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		up_model_buf = NULL;
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_decoder_setup(&setup_exp_flags, dec, cfg->cmp_mode, cfg->cmp_par_exp_flags,
				cfg->spill_exp_flags, cfg->round, cfg->max_used_bits->s_exp_flags);
	configure_decoder_setup(&setup_fx, dec, cfg->cmp_mode, cfg->cmp_par_fx, cfg->spill_fx,
				cfg->round, cfg->max_used_bits->s_fx);
	configure_decoder_setup(&setup_ncob, dec, cfg->cmp_mode, cfg->cmp_par_ncob, cfg->spill_ncob,
				cfg->round, cfg->max_used_bits->s_ncob);
	configure_decoder_setup(&setup_efx, dec, cfg->cmp_mode, cfg->cmp_par_efx, cfg->spill_efx,
				cfg->round, cfg->max_used_bits->s_efx);
	configure_decoder_setup(&setup_ecob, dec, cfg->cmp_mode, cfg->cmp_par_ecob, cfg->spill_ecob,
				cfg->round, cfg->max_used_bits->s_ecob);

	for (i = 0; ; i++) {
		err = decode_value(&setup_exp_flags, &decoded_value, model.exp_flags);
		if (err)
			break;
		data_buf[i].exp_flags = (__typeof__(data_buf[i].exp_flags)) decoded_value;

		err = decode_value(&setup_fx, &decoded_value, model.fx);
		if (err)
			break;
		data_buf[i].fx = decoded_value;

		err = decode_value(&setup_ncob, &decoded_value, model.ncob_x);
		if (err)
			break;
		data_buf[i].ncob_x = decoded_value;

		err = decode_value(&setup_ncob, &decoded_value, model.ncob_y);
		if (err)
			break;
		data_buf[i].ncob_y = decoded_value;

		err = decode_value(&setup_efx, &decoded_value, model.efx);
		if (err)
			break;
		data_buf[i].efx = decoded_value;

		err = decode_value(&setup_ecob, &decoded_value, model.ecob_x);
		if (err)
			break;
		data_buf[i].ecob_x = decoded_value;

		err = decode_value(&setup_ecob, &decoded_value, model.ecob_y);
		if (err)
			break;
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
	return err;
}


/**
 * @brief decompress fast normal light flux (F_FX) data
 *
 * @param cfg	pointer to the compression configuration structure
 * @param dec	a pointer to a bit_decoder context
 *
 * @returns 0 on success; otherwise error
 */

static int decompress_f_fx(const struct cmp_cfg *cfg, struct bit_decoder *dec)
{
	size_t i;
	int err;
	uint32_t decoded_value;
	struct decoder_setup setup_fx;
	struct f_fx *data_buf = get_collection_data(cfg->input_buf);
	struct f_fx *model_buf = get_collection_data(cfg->model_buf);
	struct f_fx *up_model_buf;
	struct f_fx *next_model_p;
	struct f_fx model;

	if (model_mode_is_used(cfg->cmp_mode)) {
		up_model_buf = get_collection_data(cfg->icu_new_model_buf);
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		up_model_buf = NULL;
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_decoder_setup(&setup_fx, dec, cfg->cmp_mode, cfg->cmp_par_fx, cfg->spill_fx,
				cfg->round, cfg->max_used_bits->f_fx);

	for (i = 0; ; i++) {
		err = decode_value(&setup_fx, &decoded_value, model.fx);
		if (err)
			break;
		data_buf[i].fx = decoded_value;

		if (up_model_buf)
			up_model_buf[i].fx = cmp_up_model(data_buf[i].fx, model.fx,
							  cfg->model_value, setup_fx.lossy_par);

		if (i >= cfg->samples-1)
			break;

		model = next_model_p[i];
	}
	return err;
}


/**
 * @brief decompress F_FX_EFX data
 *
 * @param cfg	pointer to the compression configuration structure
 * @param dec	a pointer to a bit_decoder context
 *
 * @returns 0 on success; otherwise error
 */

static int decompress_f_fx_efx(const struct cmp_cfg *cfg, struct bit_decoder *dec)
{
	size_t i;
	int err;
	uint32_t decoded_value;
	struct decoder_setup setup_fx, setup_efx;
	struct f_fx_efx *data_buf = get_collection_data(cfg->input_buf);
	struct f_fx_efx *model_buf = get_collection_data(cfg->model_buf);
	struct f_fx_efx *up_model_buf;
	struct f_fx_efx *next_model_p;
	struct f_fx_efx model;

	if (model_mode_is_used(cfg->cmp_mode)) {
		up_model_buf = get_collection_data(cfg->icu_new_model_buf);
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		up_model_buf = NULL;
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_decoder_setup(&setup_fx, dec, cfg->cmp_mode, cfg->cmp_par_fx, cfg->spill_fx,
				cfg->round, cfg->max_used_bits->f_fx);
	configure_decoder_setup(&setup_efx, dec, cfg->cmp_mode, cfg->cmp_par_efx, cfg->spill_efx,
				cfg->round, cfg->max_used_bits->f_efx);

	for (i = 0; ; i++) {
		err = decode_value(&setup_fx, &decoded_value, model.fx);
		if (err)
			break;
		data_buf[i].fx = decoded_value;

		err = decode_value(&setup_efx, &decoded_value, model.efx);
		if (err)
			break;
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
	return err;
}


/**
 * @brief decompress short F_FX_NCOB data
 *
 * @param cfg	pointer to the compression configuration structure
 * @param dec	a pointer to a bit_decoder context
 *
 * @returns 0 on success; otherwise error
 */

static int decompress_f_fx_ncob(const struct cmp_cfg *cfg, struct bit_decoder *dec)
{
	size_t i;
	int err;
	uint32_t decoded_value;
	struct decoder_setup setup_fx, setup_ncob;
	struct f_fx_ncob *data_buf = get_collection_data(cfg->input_buf);
	struct f_fx_ncob *model_buf = get_collection_data(cfg->model_buf);
	struct f_fx_ncob *up_model_buf;
	struct f_fx_ncob *next_model_p;
	struct f_fx_ncob model;

	if (model_mode_is_used(cfg->cmp_mode)) {
		up_model_buf = get_collection_data(cfg->icu_new_model_buf);
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		up_model_buf = NULL;
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_decoder_setup(&setup_fx, dec, cfg->cmp_mode, cfg->cmp_par_fx, cfg->spill_fx,
				cfg->round, cfg->max_used_bits->f_fx);
	configure_decoder_setup(&setup_ncob, dec, cfg->cmp_mode, cfg->cmp_par_ncob, cfg->spill_ncob,
				cfg->round, cfg->max_used_bits->f_ncob);

	for (i = 0; ; i++) {
		err = decode_value(&setup_fx, &decoded_value, model.fx);
		if (err)
			break;
		data_buf[i].fx = decoded_value;

		err = decode_value(&setup_ncob, &decoded_value, model.ncob_x);
		if (err)
			break;
		data_buf[i].ncob_x = decoded_value;

		err = decode_value(&setup_ncob, &decoded_value, model.ncob_y);
		if (err)
			break;
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
	return err;
}


/**
 * @brief decompress short F_FX_NCOB_ECOB data
 *
 * @param cfg	pointer to the compression configuration structure
 * @param dec	a pointer to a bit_decoder context
 *
 * @returns 0 on success; otherwise error
 */

static int decompress_f_fx_efx_ncob_ecob(const struct cmp_cfg *cfg, struct bit_decoder *dec)
{
	size_t i;
	int err;
	uint32_t decoded_value;
	struct decoder_setup setup_fx, setup_ncob, setup_efx, setup_ecob;
	struct f_fx_efx_ncob_ecob *data_buf = get_collection_data(cfg->input_buf);
	struct f_fx_efx_ncob_ecob *model_buf = get_collection_data(cfg->model_buf);
	struct f_fx_efx_ncob_ecob *up_model_buf;
	struct f_fx_efx_ncob_ecob *next_model_p;
	struct f_fx_efx_ncob_ecob model;

	if (model_mode_is_used(cfg->cmp_mode)) {
		up_model_buf = get_collection_data(cfg->icu_new_model_buf);
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		up_model_buf = NULL;
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_decoder_setup(&setup_fx, dec, cfg->cmp_mode, cfg->cmp_par_fx, cfg->spill_fx,
				cfg->round, cfg->max_used_bits->f_fx);
	configure_decoder_setup(&setup_ncob, dec, cfg->cmp_mode, cfg->cmp_par_ncob, cfg->spill_ncob,
				cfg->round, cfg->max_used_bits->f_ncob);
	configure_decoder_setup(&setup_efx, dec, cfg->cmp_mode, cfg->cmp_par_efx, cfg->spill_efx,
				cfg->round, cfg->max_used_bits->f_efx);
	configure_decoder_setup(&setup_ecob, dec, cfg->cmp_mode, cfg->cmp_par_ecob, cfg->spill_ecob,
				cfg->round, cfg->max_used_bits->f_ecob);

	for (i = 0; ; i++) {
		err = decode_value(&setup_fx, &decoded_value, model.fx);
		if (err)
			break;
		data_buf[i].fx = decoded_value;

		err = decode_value(&setup_ncob, &decoded_value, model.ncob_x);
		if (err)
			break;
		data_buf[i].ncob_x = decoded_value;

		err = decode_value(&setup_ncob, &decoded_value, model.ncob_y);
		if (err)
			break;
		data_buf[i].ncob_y = decoded_value;

		err = decode_value(&setup_efx, &decoded_value, model.efx);
		if (err)
			break;
		data_buf[i].efx = decoded_value;

		err = decode_value(&setup_ecob, &decoded_value, model.ecob_x);
		if (err)
			break;
		data_buf[i].ecob_x = decoded_value;

		err = decode_value(&setup_ecob, &decoded_value, model.ecob_y);
		if (err)
			break;
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
	return err;
}


/**
 * @brief decompress long normal light flux (L_FX) data
 *
 * @param cfg	pointer to the compression configuration structure
 * @param dec	a pointer to a bit_decoder context
 *
 * @returns 0 on success; otherwise error
 */

static int decompress_l_fx(const struct cmp_cfg *cfg, struct bit_decoder *dec)
{
	size_t i;
	int err;
	uint32_t decoded_value;
	struct decoder_setup setup_exp_flags, setup_fx, setup_fx_var;
	struct l_fx *data_buf = get_collection_data(cfg->input_buf);
	struct l_fx *model_buf = get_collection_data(cfg->model_buf);
	struct l_fx *up_model_buf;
	struct l_fx *next_model_p;
	struct l_fx model;

	if (model_mode_is_used(cfg->cmp_mode)) {
		up_model_buf = get_collection_data(cfg->icu_new_model_buf);
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		up_model_buf = NULL;
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_decoder_setup(&setup_exp_flags, dec, cfg->cmp_mode, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				cfg->round, cfg->max_used_bits->l_exp_flags);
	configure_decoder_setup(&setup_fx, dec, cfg->cmp_mode, cfg->cmp_par_fx, cfg->spill_fx,
				cfg->round, cfg->max_used_bits->l_fx);
	configure_decoder_setup(&setup_fx_var, dec, cfg->cmp_mode, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				cfg->round, cfg->max_used_bits->l_fx_variance);

	for (i = 0; ; i++) {
		err = decode_value(&setup_exp_flags, &decoded_value, model.exp_flags);
		if (err)
			break;
		data_buf[i].exp_flags = decoded_value;

		err = decode_value(&setup_fx, &decoded_value, model.fx);
		if (err)
			break;
		data_buf[i].fx = decoded_value;

		err = decode_value(&setup_fx_var, &decoded_value, model.fx_variance);
		if (err)
			break;
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
	return err;
}


/**
 * @brief decompress L_FX_EFX data
 *
 * @param cfg	pointer to the compression configuration structure
 * @param dec	a pointer to a bit_decoder context
 *
 * @returns 0 on success; otherwise error
 */

static int decompress_l_fx_efx(const struct cmp_cfg *cfg, struct bit_decoder *dec)
{
	size_t i;
	int err;
	uint32_t decoded_value;
	struct decoder_setup setup_exp_flags, setup_fx, setup_efx, setup_fx_var;
	struct l_fx_efx *data_buf = get_collection_data(cfg->input_buf);
	struct l_fx_efx *model_buf = get_collection_data(cfg->model_buf);
	struct l_fx_efx *up_model_buf;
	struct l_fx_efx *next_model_p;
	struct l_fx_efx model;

	if (model_mode_is_used(cfg->cmp_mode)) {
		up_model_buf = get_collection_data(cfg->icu_new_model_buf);
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		up_model_buf = NULL;
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_decoder_setup(&setup_exp_flags, dec, cfg->cmp_mode, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				cfg->round, cfg->max_used_bits->l_exp_flags);
	configure_decoder_setup(&setup_fx, dec, cfg->cmp_mode, cfg->cmp_par_fx, cfg->spill_fx,
				cfg->round, cfg->max_used_bits->l_fx);
	configure_decoder_setup(&setup_efx, dec, cfg->cmp_mode, cfg->cmp_par_efx, cfg->spill_efx,
				cfg->round, cfg->max_used_bits->l_efx);
	configure_decoder_setup(&setup_fx_var, dec, cfg->cmp_mode, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				cfg->round, cfg->max_used_bits->l_fx_variance);

	for (i = 0; ; i++) {
		err = decode_value(&setup_exp_flags, &decoded_value, model.exp_flags);
		if (err)
			break;
		data_buf[i].exp_flags = decoded_value;

		err = decode_value(&setup_fx, &decoded_value, model.fx);
		if (err)
			break;
		data_buf[i].fx = decoded_value;

		err = decode_value(&setup_efx, &decoded_value, model.efx);
		if (err)
			break;
		data_buf[i].efx = decoded_value;

		err = decode_value(&setup_fx_var, &decoded_value, model.fx_variance);
		if (err)
			break;
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
	return err;
}


/**
 * @brief decompress L_FX_NCOB data
 *
 * @param cfg	pointer to the compression configuration structure
 * @param dec	a pointer to a bit_decoder context
 *
 * @returns 0 on success; otherwise error
 */

static int decompress_l_fx_ncob(const struct cmp_cfg *cfg, struct bit_decoder *dec)
{
	size_t i;
	int err;
	uint32_t decoded_value;
	struct decoder_setup setup_exp_flags, setup_fx, setup_ncob,
			     setup_fx_var, setup_cob_var;
	struct l_fx_ncob *data_buf = get_collection_data(cfg->input_buf);
	struct l_fx_ncob *model_buf = get_collection_data(cfg->model_buf);
	struct l_fx_ncob *up_model_buf;
	struct l_fx_ncob *next_model_p;
	struct l_fx_ncob model;

	if (model_mode_is_used(cfg->cmp_mode)) {
		up_model_buf = get_collection_data(cfg->icu_new_model_buf);
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		up_model_buf = NULL;
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_decoder_setup(&setup_exp_flags, dec, cfg->cmp_mode, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				cfg->round, cfg->max_used_bits->l_exp_flags);
	configure_decoder_setup(&setup_fx, dec, cfg->cmp_mode, cfg->cmp_par_fx, cfg->spill_fx,
				cfg->round, cfg->max_used_bits->l_fx);
	configure_decoder_setup(&setup_ncob, dec, cfg->cmp_mode, cfg->cmp_par_ncob, cfg->spill_ncob,
				cfg->round, cfg->max_used_bits->l_ncob);
	configure_decoder_setup(&setup_fx_var, dec, cfg->cmp_mode, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				cfg->round, cfg->max_used_bits->l_fx_variance);
	configure_decoder_setup(&setup_cob_var, dec, cfg->cmp_mode, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				cfg->round, cfg->max_used_bits->l_cob_variance);

	for (i = 0; ; i++) {
		err = decode_value(&setup_exp_flags, &decoded_value, model.exp_flags);
		if (err)
			break;
		data_buf[i].exp_flags = decoded_value;

		err = decode_value(&setup_fx, &decoded_value, model.fx);
		if (err)
			break;
		data_buf[i].fx = decoded_value;

		err = decode_value(&setup_ncob, &decoded_value, model.ncob_x);
		if (err)
			break;
		data_buf[i].ncob_x = decoded_value;

		err = decode_value(&setup_ncob, &decoded_value, model.ncob_y);
		if (err)
			break;
		data_buf[i].ncob_y = decoded_value;

		err = decode_value(&setup_fx_var, &decoded_value, model.fx_variance);
		if (err)
			break;
		data_buf[i].fx_variance = decoded_value;

		err = decode_value(&setup_cob_var, &decoded_value, model.cob_x_variance);
		if (err)
			break;
		data_buf[i].cob_x_variance = decoded_value;

		err = decode_value(&setup_cob_var, &decoded_value, model.cob_y_variance);
		if (err)
			break;
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
	return err;
}


/**
 * @brief decompress L_FX_EFX_NCOB_ECOB data
 *
 * @param cfg	pointer to the compression configuration structure
 * @param dec	a pointer to a bit_decoder context
 *
 * @returns 0 on success; otherwise error
 */

static int decompress_l_fx_efx_ncob_ecob(const struct cmp_cfg *cfg, struct bit_decoder *dec)
{
	size_t i;
	int err;
	uint32_t decoded_value;
	struct decoder_setup setup_exp_flags, setup_fx, setup_ncob, setup_efx,
			     setup_ecob, setup_fx_var, setup_cob_var;
	struct l_fx_efx_ncob_ecob *data_buf = get_collection_data(cfg->input_buf);
	struct l_fx_efx_ncob_ecob *model_buf = get_collection_data(cfg->model_buf);
	struct l_fx_efx_ncob_ecob *up_model_buf;
	struct l_fx_efx_ncob_ecob *next_model_p;
	struct l_fx_efx_ncob_ecob model;

	if (model_mode_is_used(cfg->cmp_mode)) {
		up_model_buf = get_collection_data(cfg->icu_new_model_buf);
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		up_model_buf = NULL;
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_decoder_setup(&setup_exp_flags, dec, cfg->cmp_mode, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				cfg->round, cfg->max_used_bits->l_exp_flags);
	configure_decoder_setup(&setup_fx, dec, cfg->cmp_mode, cfg->cmp_par_fx, cfg->spill_fx,
				cfg->round, cfg->max_used_bits->l_fx);
	configure_decoder_setup(&setup_ncob, dec, cfg->cmp_mode, cfg->cmp_par_ncob, cfg->spill_ncob,
				cfg->round, cfg->max_used_bits->l_ncob);
	configure_decoder_setup(&setup_efx, dec, cfg->cmp_mode, cfg->cmp_par_efx, cfg->spill_efx,
				cfg->round, cfg->max_used_bits->l_efx);
	configure_decoder_setup(&setup_ecob, dec, cfg->cmp_mode, cfg->cmp_par_ecob, cfg->spill_ecob,
				cfg->round, cfg->max_used_bits->l_ecob);
	configure_decoder_setup(&setup_fx_var, dec, cfg->cmp_mode, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				cfg->round, cfg->max_used_bits->l_fx_variance);
	configure_decoder_setup(&setup_cob_var, dec, cfg->cmp_mode, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				cfg->round, cfg->max_used_bits->l_cob_variance);

	for (i = 0; ; i++) {
		err = decode_value(&setup_exp_flags, &decoded_value, model.exp_flags);
		if (err)
			break;
		data_buf[i].exp_flags = decoded_value;

		err = decode_value(&setup_fx, &decoded_value, model.fx);
		if (err)
			break;
		data_buf[i].fx = decoded_value;

		err = decode_value(&setup_ncob, &decoded_value, model.ncob_x);
		if (err)
			break;
		data_buf[i].ncob_x = decoded_value;

		err = decode_value(&setup_ncob, &decoded_value, model.ncob_y);
		if (err)
			break;
		data_buf[i].ncob_y = decoded_value;

		err = decode_value(&setup_efx, &decoded_value, model.efx);
		if (err)
			break;
		data_buf[i].efx = decoded_value;

		err = decode_value(&setup_ecob, &decoded_value, model.ecob_x);
		if (err)
			break;
		data_buf[i].ecob_x = decoded_value;

		err = decode_value(&setup_ecob, &decoded_value, model.ecob_y);
		if (err)
			break;
		data_buf[i].ecob_y = decoded_value;

		err = decode_value(&setup_fx_var, &decoded_value, model.fx_variance);
		if (err)
			break;
		data_buf[i].fx_variance = decoded_value;

		err = decode_value(&setup_cob_var, &decoded_value, model.cob_x_variance);
		if (err)
			break;
		data_buf[i].cob_x_variance = decoded_value;

		err = decode_value(&setup_cob_var, &decoded_value, model.cob_y_variance);
		if (err)
			break;
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
	return err;
}


/**
 * @brief decompress N-CAM and F-CAM offset data
 *
 * @param cfg	pointer to the compression configuration structure
 * @param dec	a pointer to a bit_decoder context
 *
 * @returns 0 on success; otherwise error
 */

static int decompress_offset(const struct cmp_cfg *cfg, struct bit_decoder *dec)
{
	size_t i;
	int err;
	uint32_t decoded_value;
	struct decoder_setup setup_mean, setup_var;
	struct offset *data_buf = get_collection_data(cfg->input_buf);
	struct offset *model_buf = get_collection_data(cfg->model_buf);
	struct offset *up_model_buf;
	struct offset *next_model_p;
	struct offset model;

	if (model_mode_is_used(cfg->cmp_mode)) {
		up_model_buf = get_collection_data(cfg->icu_new_model_buf);
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		up_model_buf = NULL;
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	{
		unsigned int mean_bits_used, variance_bits_used;

		switch (cfg->data_type) {
		case DATA_TYPE_F_CAM_OFFSET:
			mean_bits_used = cfg->max_used_bits->fc_offset_mean;
			variance_bits_used = cfg->max_used_bits->fc_offset_variance;
			break;
		case DATA_TYPE_OFFSET:
		default:
			mean_bits_used = cfg->max_used_bits->nc_offset_mean;
			variance_bits_used = cfg->max_used_bits->nc_offset_variance;
			break;
		}
		configure_decoder_setup(&setup_mean, dec, cfg->cmp_mode, cfg->cmp_par_offset_mean, cfg->spill_offset_mean,
					cfg->round, mean_bits_used);

		configure_decoder_setup(&setup_var, dec, cfg->cmp_mode, cfg->cmp_par_offset_variance, cfg->spill_offset_variance,
					cfg->round, variance_bits_used);

	}

	for (i = 0; ; i++) {
		err = decode_value(&setup_mean, &decoded_value, model.mean);
		if (err)
			break;
		data_buf[i].mean = decoded_value;

		err = decode_value(&setup_var, &decoded_value, model.variance);
		if (err)
			break;
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
	return err;
}


/**
 * @brief decompress N-CAM background data
 *
 * @param cfg	pointer to the compression configuration structure
 * @param dec	a pointer to a bit_decoder context
 *
 * @returns 0 on success; otherwise error
 */

static int decompress_background(const struct cmp_cfg *cfg, struct bit_decoder *dec)
{
	size_t i;
	int err;
	uint32_t decoded_value;
	struct decoder_setup setup_mean, setup_var, setup_pix;
	struct background *data_buf = get_collection_data(cfg->input_buf);
	struct background *model_buf = get_collection_data(cfg->model_buf);
	struct background *up_model_buf;
	struct background *next_model_p;
	struct background model;

	if (model_mode_is_used(cfg->cmp_mode)) {
		up_model_buf = get_collection_data(cfg->icu_new_model_buf);
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		up_model_buf = NULL;
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}
	{
		unsigned int mean_used_bits, variance_used_bits, outlier_pixels_used_bits;

		switch (cfg->data_type) {
		case DATA_TYPE_F_CAM_BACKGROUND:
			mean_used_bits = cfg->max_used_bits->fc_background_mean;
			variance_used_bits = cfg->max_used_bits->fc_background_variance;
			outlier_pixels_used_bits = cfg->max_used_bits->fc_background_outlier_pixels;
			break;
		case DATA_TYPE_BACKGROUND:
		default:
			mean_used_bits = cfg->max_used_bits->nc_background_mean;
			variance_used_bits = cfg->max_used_bits->nc_background_variance;
			outlier_pixels_used_bits = cfg->max_used_bits->nc_background_outlier_pixels;
			break;
		}

		configure_decoder_setup(&setup_mean, dec, cfg->cmp_mode, cfg->cmp_par_background_mean, cfg->spill_background_mean,
					cfg->round, mean_used_bits);

		configure_decoder_setup(&setup_var, dec, cfg->cmp_mode, cfg->cmp_par_background_variance, cfg->spill_background_variance,
					cfg->round, variance_used_bits);

		configure_decoder_setup(&setup_pix, dec, cfg->cmp_mode, cfg->cmp_par_background_pixels_error, cfg->spill_background_pixels_error,
					cfg->round, outlier_pixels_used_bits);

	}

	for (i = 0; ; i++) {
		err = decode_value(&setup_mean, &decoded_value, model.mean);
		if (err)
			break;
		data_buf[i].mean = decoded_value;

		err = decode_value(&setup_var, &decoded_value, model.variance);
		if (err)
			break;
		data_buf[i].variance = decoded_value;

		err = decode_value(&setup_pix, &decoded_value, model.outlier_pixels);
		if (err)
			break;
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
	return err;
}


/**
 * @brief decompress N-CAM smearing data
 *
 * @param cfg	pointer to the compression configuration structure
 * @param dec	a pointer to a bit_decoder context
 *
 * @returns 0 on success; otherwise error
 */

static int decompress_smearing(const struct cmp_cfg *cfg, struct bit_decoder *dec)
{
	size_t i;
	int err;
	uint32_t decoded_value;
	struct decoder_setup setup_mean, setup_var, setup_pix;
	struct smearing *data_buf = get_collection_data(cfg->input_buf);
	struct smearing *model_buf = get_collection_data(cfg->model_buf);
	struct smearing *up_model_buf;
	struct smearing *next_model_p;
	struct smearing model;

	if (model_mode_is_used(cfg->cmp_mode)) {
		up_model_buf = get_collection_data(cfg->icu_new_model_buf);
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		up_model_buf = NULL;
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_decoder_setup(&setup_mean, dec, cfg->cmp_mode, cfg->cmp_par_smearing_mean, cfg->spill_smearing_mean,
				cfg->round, cfg->max_used_bits->smearing_mean);
	configure_decoder_setup(&setup_var, dec, cfg->cmp_mode, cfg->cmp_par_smearing_variance, cfg->spill_smearing_variance,
				cfg->round, cfg->max_used_bits->smearing_variance_mean);
	configure_decoder_setup(&setup_pix, dec, cfg->cmp_mode, cfg->cmp_par_smearing_pixels_error, cfg->spill_smearing_pixels_error,
				cfg->round, cfg->max_used_bits->smearing_outlier_pixels);

	for (i = 0; ; i++) {
		err = decode_value(&setup_mean, &decoded_value, model.mean);
		if (err)
			break;
		data_buf[i].mean = decoded_value;

		err = decode_value(&setup_var, &decoded_value, model.variance_mean);
		if (err)
			break;
		data_buf[i].variance_mean = (__typeof__(data_buf[i].variance_mean))decoded_value;

		err = decode_value(&setup_pix, &decoded_value, model.outlier_pixels);
		if (err)
			break;
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
	return err;
}


/**
 * @brief Decompresses the collection header.
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @note the collection header is not truly compressed; it is simply copied into
 *	the compressed data.
 *
 * @return The size of the decompressed collection header on success,
 *         or -1 if the buffer length is insufficient
 */

static int decompress_collection_hdr(const struct cmp_cfg *cfg)
{
	if (cfg->buffer_length < COLLECTION_HDR_SIZE)
		return -1;

	if (cfg->icu_output_buf) {
		if (cfg->input_buf)
			memcpy(cfg->input_buf, cfg->icu_output_buf, COLLECTION_HDR_SIZE);

		if (model_mode_is_used(cfg->cmp_mode) && cfg->icu_new_model_buf)
			memcpy(cfg->icu_new_model_buf, cfg->icu_output_buf, COLLECTION_HDR_SIZE);
	}
	return COLLECTION_HDR_SIZE;
}


/**
 * @brief decompress the data based on a compression configuration
 *
 * @param cfg		pointer to a compression configuration
 * @param decmp_type	type of decompression: ICU chunk or RDCU decompression
 *
 * @note cfg->buffer_length is measured in bytes
 *
 * @returns the size of the decompressed data on success; returns negative on failure
 */

static int decompressed_data_internal(const struct cmp_cfg *cfg, enum decmp_type decmp_type)
{
	int err;
	uint32_t data_size;

	assert(decmp_type == ICU_DECOMRESSION || decmp_type == RDCU_DECOMPRESSION);

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
	} else if (cmp_aux_data_type_is_used(cfg->data_type)) {
		if (cmp_cfg_aux_is_invalid(cfg))
			return -1;
	} else {
		return -1;
	}

	if (model_mode_is_used(cfg->cmp_mode))
		if (!cfg->model_buf) /* we need a model for model compression */
			return -1;

	data_size = cfg->samples * (uint32_t)size_of_a_sample(cfg->data_type);
	if (decmp_type == ICU_DECOMRESSION)
		data_size += COLLECTION_HDR_SIZE;
	/* data_size = cmp_cal_size_of_data(cfg->samples, cfg->data_type); */
	/* if (!cfg->input_buf || !data_size) */
	/* 	return (int)data_size; */

	if (cfg->cmp_mode == CMP_MODE_RAW) {
		/* if (data_size < cfg->buffer_length/CHAR_BIT) */
		/* 	return -1; */

		if (cfg->input_buf) {
			/* uint32_t s = cmp_col_get_size(cfg->icu_output_buf); */
			/* assert(s==data_size); */
			memcpy(cfg->input_buf, cfg->icu_output_buf, data_size);
			switch (decmp_type) {
			case ICU_DECOMRESSION:
				if (be_to_cpu_chunk(cfg->input_buf, data_size))
					return -1;
				break;
			case RDCU_DECOMPRESSION:
				if (be_to_cpu_data_type(cfg->input_buf, data_size,
							cfg->data_type))
					return -1;
				break;
			}
		}
		err = 0;

	} else {
		struct bit_decoder dec;
		int hdr_size = 0;

		if (!cfg->input_buf)
			return (int)data_size;

		if (decmp_type == ICU_DECOMRESSION) {
			hdr_size = decompress_collection_hdr(cfg);
			if (hdr_size < 0)
				return -1;
		}

		bit_init_decoder(&dec, (uint8_t *)cfg->icu_output_buf+hdr_size,
				 cfg->buffer_length-(uint32_t)hdr_size);

		switch (cfg->data_type) {
		case DATA_TYPE_IMAGETTE:
		case DATA_TYPE_IMAGETTE_ADAPTIVE:
		case DATA_TYPE_SAT_IMAGETTE:
		case DATA_TYPE_SAT_IMAGETTE_ADAPTIVE:
		case DATA_TYPE_F_CAM_IMAGETTE:
		case DATA_TYPE_F_CAM_IMAGETTE_ADAPTIVE:
			err = decompress_imagette(cfg, &dec, decmp_type);
			break;
		case DATA_TYPE_S_FX:
			err = decompress_s_fx(cfg, &dec);
			break;
		case DATA_TYPE_S_FX_EFX:
			err = decompress_s_fx_efx(cfg, &dec);
			break;
		case DATA_TYPE_S_FX_NCOB:
			err = decompress_s_fx_ncob(cfg, &dec);
			break;
		case DATA_TYPE_S_FX_EFX_NCOB_ECOB:
			err = decompress_s_fx_efx_ncob_ecob(cfg, &dec);
			break;

		case DATA_TYPE_F_FX:
			err = decompress_f_fx(cfg, &dec);
			break;
		case DATA_TYPE_F_FX_EFX:
			err = decompress_f_fx_efx(cfg, &dec);
			break;
		case DATA_TYPE_F_FX_NCOB:
			err = decompress_f_fx_ncob(cfg, &dec);
			break;
		case DATA_TYPE_F_FX_EFX_NCOB_ECOB:
			err = decompress_f_fx_efx_ncob_ecob(cfg, &dec);
			break;

		case DATA_TYPE_L_FX:
			err = decompress_l_fx(cfg, &dec);
			break;
		case DATA_TYPE_L_FX_EFX:
			err = decompress_l_fx_efx(cfg, &dec);
			break;
		case DATA_TYPE_L_FX_NCOB:
			err = decompress_l_fx_ncob(cfg, &dec);
			break;
		case DATA_TYPE_L_FX_EFX_NCOB_ECOB:
			err = decompress_l_fx_efx_ncob_ecob(cfg, &dec);
			break;

		case DATA_TYPE_OFFSET:
		case DATA_TYPE_F_CAM_OFFSET:
			err = decompress_offset(cfg, &dec);
			break;
		case DATA_TYPE_BACKGROUND:
		case DATA_TYPE_F_CAM_BACKGROUND:
			err = decompress_background(cfg, &dec);
			break;
		case DATA_TYPE_SMEARING:
			err = decompress_smearing(cfg, &dec);
			break;

		case DATA_TYPE_UNKNOWN:
		default:
			err = -1;
			debug_print("Error: Compressed data type not supported.");
			break;
		}

		switch (bit_refill(&dec)) {
		case BIT_OVERFLOW:
			if (dec.cursor == dec.limit_ptr)
				debug_print("Error: The end of the compressed bit stream has been exceeded. Please check that the compression parameters match those used to compress the data and that the compressed data are not corrupted.");
			else
				debug_print("Error: Data consistency check failed. %s", please_check_str);
			break;
		case BIT_END_OF_BUFFER:
			/* check if non consumed bits are zero */
			if (bit_read_bits(&dec, sizeof(dec.bit_container)*8 - dec.bits_consumed) == 0)
				break;
			/* fall through */
		case BIT_UNFINISHED:
			debug_print("Warning: Not all compressed data are processed.");
			break;
		}
	}
	if (err)
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
	if (!cfg)
		return -1;

	cfg->data_type = cmp_ent_get_data_type(ent);
	/* the compression entity data type field only supports imagette or chunk data types */
	if (cfg->data_type != DATA_TYPE_CHUNK && !rdcu_supported_data_type_is_used(cfg->data_type)) {
		debug_print("Error: Compression entity data type not supported.");
		return -1;
	}

	cfg->cmp_mode = cmp_ent_get_cmp_mode(ent);
	if (cmp_ent_get_data_type_raw_bit(ent) != (cfg->cmp_mode == CMP_MODE_RAW)) {
		debug_print("Error: The entity's raw data bit does not match up with the compression mode.");
		return -1;
	}
	cfg->model_value = cmp_ent_get_model_value(ent);
	cfg->round = cmp_ent_get_lossy_cmp_par(ent);
	cfg->buffer_length = cmp_ent_get_cmp_data_size(ent);

	if (cfg->data_type == DATA_TYPE_CHUNK) {
		cfg->samples = 0;
		if ((cfg->buffer_length < (COLLECTION_HDR_SIZE + CMP_COLLECTION_FILD_SIZE) && (cfg->cmp_mode != CMP_MODE_RAW)) ||
		    (cfg->buffer_length < COLLECTION_HDR_SIZE && (cfg->cmp_mode == CMP_MODE_RAW))) {
			debug_print("Error: The compressed data size in the compression header is smaller than a collection header.");
			return -1;
		}
	} else {
		uint32_t org_size = cmp_ent_get_original_size(ent);

		if (org_size % sizeof(uint16_t)) {
			debug_print("Error: The original size of an imagette product type in the compression header must be a multiple of 2.");
			cfg->samples = 0;
			return -1;
		}
		cfg->samples = org_size/sizeof(uint16_t);
	}

	cfg->icu_output_buf = cmp_ent_get_data_buf(ent);

	cfg->max_used_bits = cmp_max_used_bits_list_get(cmp_ent_get_max_used_bits_version(ent));
	if (!cfg->max_used_bits) {
		debug_print("Error: The Max. Used Bits Registry Version in the compression header is unknown.");
		return -1;
	}

	if (cfg->cmp_mode == CMP_MODE_RAW) {
		if (cmp_ent_get_original_size(ent) != cmp_ent_get_cmp_data_size(ent)) {
			debug_print("Error: The compressed data size and the decompressed original data size in the compression header should be the same in raw mode.");
			return -1;
		}
		/* no specific header is used for raw data we are done */
		return 0;
	}

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
	case DATA_TYPE_F_CAM_OFFSET:
	case DATA_TYPE_BACKGROUND:
	case DATA_TYPE_F_CAM_BACKGROUND:
	case DATA_TYPE_SMEARING:
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
	case DATA_TYPE_CHUNK:
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
	/* LCOV_EXCL_START */
	case DATA_TYPE_UNKNOWN:
	default:
		return -1;
	/* LCOV_EXCL_STOP */
	}

	return 0;
}


/**
 * @brief Get the size of the compressed collection data
 *
 * @param cmp_col	pointer to a compressed collection
 *
 * @return The size of the compressed collection data in bytes
 */

static uint16_t get_cmp_collection_data_length(uint8_t *cmp_col)
{
	uint16_t cmp_data_size;
	/* If a non-raw mode is used to compress all collections, a
	 * 2-byte big endian field with the size of the compressed data
	 * is prefixed (without the size of the file itself and without
	 * the size of the collection header). This is followed by a
	 * collection header, followed by the compressed data.
	 * |---------------------| - cmp_col
	 * |compressed collection|
	 * |      data size      | 2 bytes
	 * |---------------------|-
	 * |   COLLECTION HDR    |
	 * |                     | 12 bytes
	 * |---------------------|-
	 * |    compressed data  | (variable) data size
	 * |         *-*-*       |
	 * |         *-*-*       |
	 * |---------------------|- next cmp_col
	 * Fields not scaled correctly
	 */

	memcpy(&cmp_data_size, cmp_col, sizeof(cmp_data_size));
	be16_to_cpus(&cmp_data_size);

	return cmp_data_size;
}


/**
 * @brief get the total size of the compressed collection
 * j
 * This function returns the total size of the compressed collection in bytes,
 * including the size of the compressed size field itself, the collection header,
 * and the compressed collection data.
 *
 * @param cmp_col	pointer to a compressed collection
 *
 * @return The total size of the compressed collection in bytes
 */

static uint32_t get_cmp_collection_size(uint8_t *cmp_col)
{
	return CMP_COLLECTION_FILD_SIZE + COLLECTION_HDR_SIZE
		+ get_cmp_collection_data_length(cmp_col);
}


/**
 * @brief get the number of compressed collections in a compression entity
 *
 * This function returns the number of compressed collections in a compression
 * entity, by iterating over the compressed collection data
 *
 * @param ent	 pointer to the compression entity
 *
 * @return the number of compressed collections in the compressed entity, or -1
 *	on error
 */

static int get_num_of_chunks(struct cmp_entity *ent)
{
	uint8_t *cmp_data_p = cmp_ent_get_data_buf(ent);
	long cmp_data_size = cmp_ent_get_cmp_data_size(ent);
	int n = 0;
	uint8_t *p = cmp_data_p;
	/* highest plausible address of compressed collection */
	const uint8_t *limit_ptr = cmp_data_p + cmp_data_size - COLLECTION_HDR_SIZE;

	while (p < limit_ptr) {
		p += get_cmp_collection_size(p);
		n++;
	}

	if (p-cmp_data_p != cmp_data_size) {
		debug_print("Error: The sum of the compressed collection does not match the size of the data in the compression header.");
		return -1;
	}
	return n;
}


/**
 * @brief Parse n'th compressed collection and set configuration parameters
 *	for decompression it
 *
 * @param cmp_col		pointer to a compressed collection to parse
 * @param n			the number of the compressed collection to
 *				parse, starting from 1
 * @param cfg			pointer to the configuration structure
 * @param coll_uncompressed	pointer to store whether the collection is
 *				uncompressed or not
 *
 * @return the byte offset where to put the uncompressed result in the
 *	decompressed data, or -1 on error.
 */

static long parse_cmp_collection(uint8_t *cmp_col, int n, struct cmp_cfg *cfg, int *coll_uncompressed)
{
	int i;
	long decmp_pos = 0; /* position where to put the uncompressed result */
	/* pointer to the collection header */
	struct collection_hdr *col_hdr = (struct collection_hdr *)(cmp_col + CMP_COLLECTION_FILD_SIZE);
	uint32_t cmp_data_size; /* size of the compressed data in the collection (not including the header) */
	uint16_t original_col_size; /* size of the decompressed collection data (not including the header) */
	size_t sample_size;
	void *void_poiter; /* used to suppress unaligned cast warning */

	/* get to the collection we want to decompress */
	for (i = 0; i < n; i++) {
		decmp_pos += cmp_col_get_size(col_hdr);
		cmp_col += get_cmp_collection_size(cmp_col);
		col_hdr = (struct collection_hdr *)(cmp_col + CMP_COLLECTION_FILD_SIZE);
	}

	cmp_data_size = get_cmp_collection_data_length(cmp_col);
	original_col_size = cmp_col_get_data_length(col_hdr);

	if (cmp_data_size > original_col_size) {
		debug_print("Error: Collection %i, the size of the compressed collection is larger than that of the uncompressed collection.", i);
		return -1;
	}

	/* if the compressed data size == original_col_size the collection data
	 * was put uncompressed into the bitstream */
	if (cmp_data_size == original_col_size)
		*coll_uncompressed = 1;
	else
		*coll_uncompressed = 0;

	void_poiter = (void *)(col_hdr); /* unaligned cast -> reading compressed data as uint8_t * */
	cfg->icu_output_buf = void_poiter;
	cfg->buffer_length = cmp_data_size + COLLECTION_HDR_SIZE;

	cfg->data_type = convert_subservice_to_cmp_data_type(cmp_col_get_subservice(col_hdr));
	sample_size = size_of_a_sample(cfg->data_type);
	if (!sample_size)
		return -1;

	if (original_col_size % sample_size) {
		debug_print("Error: The size of the collection is not a multiple of a collection entry.");
		return -1;
	}
	cfg->samples = original_col_size / sample_size;

	return decmp_pos;
}


/**
 * @brief decompress a compression entity
 *
 * @note this function assumes that the entity size in the ent header is correct
 * @param ent			pointer to the compression entity to be decompressed
 * @param model_of_data		pointer to model data buffer (can be NULL if no
 *				model compression mode is used)
 * @param up_model_buf		pointer to store the updated model for the next model
 *				mode compression (can be the same as the model_of_data
 *				buffer for an in-place update or NULL if the updated model is not needed)
 * @param decompressed_data	pointer to the decompressed data buffer (can be NULL)
 *
 * @returns the size of the decompressed data on success; returns negative on failure
 */

int decompress_cmp_entiy(struct cmp_entity *ent, void *model_of_data,
			 void *up_model_buf, void *decompressed_data)
{
	struct cmp_cfg cfg;
	int decmp_size;
	int i, n_chunks;

	memset(&cfg, 0, sizeof(struct cmp_cfg));

	if (!ent)
		return -1;

	decmp_size = (int)cmp_ent_get_original_size(ent);
	if (decmp_size < 0)
		return -1;
	if (decmp_size == 0)
		return 0;

	if (cmp_ent_read_header(ent, &cfg))
		return -1;

	if (cfg.data_type != DATA_TYPE_CHUNK) { /* perform a non-chunk decompression */
		if (cfg.cmp_mode == CMP_MODE_RAW) {
			uint32_t data_size = cfg.samples * sizeof(uint16_t);

			if (decompressed_data) {
				memcpy(decompressed_data, cmp_ent_get_data_buf(ent), data_size);
				if (cmp_input_big_to_cpu_endianness(decompressed_data, data_size, cfg.data_type))
					return -1;
			}
			return (int)data_size;
		}

		cfg.model_buf = model_of_data;
		cfg.icu_new_model_buf = up_model_buf;
		cfg.input_buf = decompressed_data;

		return decompressed_data_internal(&cfg, RDCU_DECOMPRESSION);
	}

	/* perform a chunk decompression */

	if (cfg.cmp_mode == CMP_MODE_RAW) {
		if (decompressed_data) {
			memcpy(decompressed_data, cfg.icu_output_buf, cfg.buffer_length);
			cpu_to_be_chunk(decompressed_data, cfg.buffer_length);
		}
		/* if (up_model_buf) { /1* TODO: if diff non model mode? *1/ */
		/* 	memcpy(up_model_buf, cfg.icu_output_buf, cfg.buffer_length); */
		/* 	cpu_to_be_chunk(up_model_buf, cfg.buffer_length); */
		/* } */
		return (int)cfg.buffer_length;
	}

	n_chunks = get_num_of_chunks(ent);
	if (n_chunks <= 0)
		return -1;

	for (i = 0; i < n_chunks; i++) {
		int decmp_chunk_size;
		int col_uncompressed;
		struct cmp_cfg cmp_cpy = cfg;
		long offset = parse_cmp_collection(cmp_ent_get_data_buf(ent), i,
						   &cmp_cpy, &col_uncompressed);
		if (offset < 0)
			return -1;

		if (decompressed_data)
			cmp_cpy.input_buf = (uint8_t *)decompressed_data + offset;
		if (model_of_data)
			cmp_cpy.model_buf = (uint8_t *)model_of_data + offset;
		if (up_model_buf)
			cmp_cpy.icu_new_model_buf = (uint8_t *)up_model_buf + offset;

		if (col_uncompressed) {
			if (cmp_cpy.icu_new_model_buf && model_mode_is_used(cmp_cpy.cmp_mode)) {
				uint32_t s = cmp_cpy.buffer_length;
				memcpy(cmp_cpy.icu_new_model_buf, cmp_cpy.icu_output_buf, s);
				if (be_to_cpu_chunk(cmp_cpy.icu_new_model_buf, s))
					return -1;
			}
			cmp_cpy.cmp_mode = CMP_MODE_RAW;
		}

		decmp_chunk_size = decompressed_data_internal(&cmp_cpy, ICU_DECOMRESSION);
		if (decmp_chunk_size < 0)
			return decmp_chunk_size;
	}
	return decmp_size;
}


/**
 * @brief decompress RDCU compressed data without a compression entity header
 *
 * @param compressed_data	pointer to the RDCU compressed data (without a
 *				compression entity header)
 * @param info			pointer to a decompression information structure
 *				consisting of the metadata of the compression
 * @param model_of_data		pointer to model data buffer (can be NULL if no
 *				model compression mode is used)
 * @param up_model_buf		pointer to store the updated model for the next model
 *				mode compression (can be the same as the model_of_data
 *				buffer for an in-place update or NULL if the
 *				updated model is not needed)
 * @param decompressed_data	pointer to the decompressed data buffer (can be NULL)
 *
 * @returns the size of the decompressed data on success; returns negative on failure
 */

int decompress_rdcu_data(uint32_t *compressed_data, const struct cmp_info *info,
			 uint16_t *model_of_data, uint16_t *up_model_buf,
			 uint16_t *decompressed_data)

{
	struct cmp_cfg cfg;

	if (!compressed_data)
		return -1;

	if (!info)
		return -1;

	if (info->cmp_err)
		return -1;

	memset(&cfg, 0, sizeof(struct cmp_cfg));

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
	cfg.buffer_length = (info->cmp_size+7)/8;
	cfg.max_used_bits = &MAX_USED_BITS_SAFE;

	return decompressed_data_internal(&cfg, RDCU_DECOMPRESSION);
}
