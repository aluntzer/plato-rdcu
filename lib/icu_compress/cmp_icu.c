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
 *
 * To compress data, first create a compression configuration with the
 * cmp_cfg_icu_create() function.
 * Then set the different data buffers with the data to compressed, the model
 * data and the compressed data with the cmp_cfg_icu_buffers() function.
 * Then set the compression data type specific compression parameters. For
 * imagette data use the cmp_cfg_icu_imagette() function. For flux and center of
 * brightness data use the cmp_cfg_fx_cob() function. And for background, offset
 * and smearing data use the cmp_cfg_aux() function.
 * Finally, you can compress the data with the icu_compress_data() function.
 */


#include <stdint.h>
#include <string.h>
#include <limits.h>

#include "../common/byteorder.h"
#include "../common/cmp_debug.h"
#include "../common/cmp_data_types.h"
#include "../common/cmp_support.h"
#include "../common/cmp_entity.h"

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
 * initialised with the compress_chunk_init() function
 */

static uint64_t (*get_timestamp)(void) = default_get_timestamp;


/**
 * @brief holding the version_identifier for the compression header
 * initialised with the compress_chunk_init() function
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

struct encoder_setupt {
	generate_cw_f_pt generate_cw_f; /**< function pointer to a code word encoder */
	int (*encode_method_f)(uint32_t data, uint32_t model, int stream_len,
			       const struct encoder_setupt *setup); /**< pointer to the encoding function */
	uint32_t *bitstream_adr; /**< start address of the compressed data bitstream */
	uint32_t max_stream_len; /**< maximum length of the bitstream/icu_output_buf in bits */
	uint32_t encoder_par1;   /**< encoding parameter 1 */
	uint32_t encoder_par2;   /**< encoding parameter 2 */
	uint32_t spillover_par;  /**< outlier parameter */
	uint32_t lossy_par;      /**< lossy compression parameter */
	uint32_t max_data_bits;  /**< how many bits are needed to represent the highest possible value */
};


/* TODO: doc string */

enum chunk_type {
	CHUNK_TYPE_UNKNOWN,
	CHUNK_TYPE_NCAM_IMGAETTE,
	CHUNK_TYPE_SHORT_CADENCE,
	CHUNK_TYPE_LONG_CADENCE,
	CHUNK_TYPE_SAT_IMGAETTE,
	CHUNK_TYPE_OFFSET_BACKGROUND, /* N-CAM */
	CHUNK_TYPE_SMEARING,
	CHUNK_TYPE_F_CHAIN,
};


/**
 * @brief create an ICU compression configuration
 *
 * @param data_type	compression data product type
 * @param cmp_mode	compression mode
 * @param model_value	model weighting parameter (only needed for model compression mode)
 * @param lossy_par	lossy rounding parameter (use CMP_LOSSLESS for lossless compression)
 *
 * @returns a compression configuration containing the chosen parameters;
 *	on error the data_type record is set to DATA_TYPE_UNKNOWN
 */

struct cmp_cfg cmp_cfg_icu_create(enum cmp_data_type data_type, enum cmp_mode cmp_mode,
				  uint32_t model_value, uint32_t lossy_par)
{
	struct cmp_cfg cfg;

	memset(&cfg, 0, sizeof(cfg));

	cfg.data_type = data_type;
	cfg.cmp_mode = cmp_mode;
	cfg.model_value = model_value;
	cfg.round = lossy_par;
	cfg.max_used_bits = &MAX_USED_BITS_SAFE;

	if (cmp_cfg_gen_par_is_invalid(&cfg, ICU_CHECK) || data_type == DATA_TYPE_CHUNK)
		cfg.data_type = DATA_TYPE_UNKNOWN;

	return cfg;
}


/**
 * @brief set up the different data buffers for an ICU compression
 *
 * @param cfg			pointer to a compression configuration (created
 *				with the cmp_cfg_icu_create() function)
 * @param data_to_compress	pointer to the data to be compressed
 * @param data_samples		length of the data to be compressed measured in
 *				data samples/entities (collection header not
 *				included by imagette data)
 * @param model_of_data		pointer to model data buffer (can be NULL if no
 *				model compression mode is used)
 * @param updated_model		pointer to store the updated model for the next
 *				model mode compression (can be the same as the model_of_data
 *				buffer for in-place update or NULL if updated model is not needed)
 * @param compressed_data	pointer to the compressed data buffer (can be NULL)
 * @param compressed_data_len_samples	length of the compressed_data buffer in
 *					measured in the same units as the data_samples
 *
 * @returns the size of the compressed_data buffer on success; 0 if the
 *	parameters are invalid
 *
 * @note There is a difference in the data_samples parameter when compressing
 * imagette data compared to compressing non-imagette data!
 * When compressing non-imagette data, the compressor expects that the
 * collection header will always prefix the non-imagette data. Therefore, the
 * data_samples parameter is simply the number of entries in the collection. It
 * is not intended to join multiple non-imagette collections and compress them
 * together.
 * When compressing imagette data, the length of the entire data to be
 * compressed, including the collection header, is measured in 16-bit samples.
 * The compressor makes in this case no distinction between header and imagette
 * data. Therefore, the data_samples parameter is the number of 16-bit imagette
 * pixels plus the length of the collection header, measured in 16-bit units.
 * The compression of multiple joined collections is possible.
 */

uint32_t cmp_cfg_icu_buffers(struct cmp_cfg *cfg, void *data_to_compress,
			     uint32_t data_samples, void *model_of_data,
			     void *updated_model, uint32_t *compressed_data,
			     uint32_t compressed_data_len_samples)
{
	uint32_t cmp_data_size, hdr_size;

	if (!cfg) {
		debug_print("Error: pointer to the compression configuration structure is NULL.\n");
		return 0;
	}

	cfg->input_buf = data_to_compress;
	cfg->model_buf = model_of_data;
	cfg->samples = data_samples;
	cfg->icu_new_model_buf = updated_model;
	cfg->icu_output_buf = compressed_data;
	/* cfg->buffer_length = cmp_data_size; */
	cfg->buffer_length = compressed_data_len_samples;

	if (cmp_cfg_icu_buffers_is_invalid(cfg))
		return 0;

	cmp_data_size = cmp_cal_size_of_data(compressed_data_len_samples, cfg->data_type);
	hdr_size = cmp_ent_cal_hdr_size(cfg->data_type, cfg->cmp_mode == CMP_MODE_RAW);

	if ((cmp_data_size + hdr_size) > CMP_ENTITY_MAX_SIZE || cmp_data_size > CMP_ENTITY_MAX_SIZE) {
		debug_print("Error: The buffer for the compressed data is too large to fit in a compression entity.\n");
		return 0;
	}

	return cmp_data_size;
}


/**
 * @brief set up the maximum length of the different data products types for
 *	an ICU compression
 *
 * @param cfg		pointer to a compression configuration (created with the
 *			cmp_cfg_icu_create() function)
 * @param max_used_bits	pointer to a max_used_bits struct containing the maximum
 *			length of the different data products types
 *
 * @returns 0 if all max_used_bits parameters are valid, non-zero if parameters are invalid
 *
 * @note the cmp_cfg_icu_create() function set the max_used_bits configuration to
 *	MAX_USED_BITS_SAFE
 */

int cmp_cfg_icu_max_used_bits(struct cmp_cfg *cfg, const struct cmp_max_used_bits *max_used_bits)
{
	if (!cfg)
		return -1;

	cfg->max_used_bits = max_used_bits;

	if (cmp_cfg_icu_max_used_bits_out_of_limit(max_used_bits))
		return -1;

	return 0;
}


/**
 * @brief set up the configuration parameters for an ICU imagette compression
 *
 * @param cfg			pointer to a compression configuration (created
 *				by the cmp_cfg_icu_create() function)
 * @param cmp_par		imagette compression parameter (Golomb parameter)
 * @param spillover_par		imagette spillover threshold parameter
 *
 * @returns 0 if parameters are valid, non-zero if parameters are invalid
 */

int cmp_cfg_icu_imagette(struct cmp_cfg *cfg, uint32_t cmp_par,
			 uint32_t spillover_par)
{
	if (!cfg)
		return -1;

	cfg->golomb_par = cmp_par;
	cfg->spill = spillover_par;

	if (cmp_cfg_imagette_is_invalid(cfg, ICU_CHECK))
		return -1;

	return 0;
}


/**
 * @brief set up the configuration parameters for a flux/COB compression
 * @note not all parameters are needed for every flux/COB compression data type
 *
 * @param cfg			pointer to a compression configuration (created
 *				by the cmp_cfg_icu_create() function)
 * @param cmp_par_exp_flags	exposure flags compression parameter
 * @param spillover_exp_flags	exposure flags spillover threshold parameter
 * @param cmp_par_fx		normal flux compression parameter
 * @param spillover_fx		normal flux spillover threshold parameter
 * @param cmp_par_ncob		normal center of brightness compression parameter
 * @param spillover_ncob	normal center of brightness spillover threshold parameter
 * @param cmp_par_efx		extended flux compression parameter
 * @param spillover_efx		extended flux spillover threshold parameter
 * @param cmp_par_ecob		extended center of brightness compression parameter
 * @param spillover_ecob	extended center of brightness spillover threshold parameter
 * @param cmp_par_fx_cob_variance	flux/COB variance compression parameter
 * @param spillover_fx_cob_variance	flux/COB variance spillover threshold parameter
 *
 * @returns 0 if parameters are valid, non-zero if parameters are invalid
 */

int cmp_cfg_fx_cob(struct cmp_cfg *cfg,
		   uint32_t cmp_par_exp_flags, uint32_t spillover_exp_flags,
		   uint32_t cmp_par_fx, uint32_t spillover_fx,
		   uint32_t cmp_par_ncob, uint32_t spillover_ncob,
		   uint32_t cmp_par_efx, uint32_t spillover_efx,
		   uint32_t cmp_par_ecob, uint32_t spillover_ecob,
		   uint32_t cmp_par_fx_cob_variance, uint32_t spillover_fx_cob_variance)
{
	if (!cfg)
		return -1;

	cfg->cmp_par_exp_flags = cmp_par_exp_flags;
	cfg->cmp_par_fx = cmp_par_fx;
	cfg->cmp_par_ncob = cmp_par_ncob;
	cfg->cmp_par_efx = cmp_par_efx;
	cfg->cmp_par_ecob = cmp_par_ecob;
	cfg->cmp_par_fx_cob_variance = cmp_par_fx_cob_variance;

	cfg->spill_exp_flags = spillover_exp_flags;
	cfg->spill_fx = spillover_fx;
	cfg->spill_ncob = spillover_ncob;
	cfg->spill_efx = spillover_efx;
	cfg->spill_ecob = spillover_ecob;
	cfg->spill_fx_cob_variance = spillover_fx_cob_variance;

	if (cmp_cfg_fx_cob_is_invalid(cfg))
		return -1;

	return 0;
}


/**
 * @brief set up the configuration parameters for an auxiliary science data compression
 * @note auxiliary compression data types are: DATA_TYPE_OFFSET, DATA_TYPE_BACKGROUND,
	DATA_TYPE_SMEARING, DATA_TYPE_F_CAM_OFFSET, DATA_TYPE_F_CAM_BACKGROUND
 * @note not all parameters are needed for the every auxiliary compression data type
 *
 * @param cfg				pointer to a compression configuration (created
 *					with the cmp_cfg_icu_create() function)
 * @param cmp_par_mean			mean compression parameter
 * @param spillover_mean		mean spillover threshold parameter
 * @param cmp_par_variance		variance compression parameter
 * @param spillover_variance		variance spillover threshold parameter
 * @param cmp_par_pixels_error		outlier pixels number compression parameter
 * @param spillover_pixels_error	outlier pixels number spillover threshold parameter
 *
 * @returns 0 if parameters are valid, non-zero if parameters are invalid
 */

int cmp_cfg_aux(struct cmp_cfg *cfg,
		uint32_t cmp_par_mean, uint32_t spillover_mean,
		uint32_t cmp_par_variance, uint32_t spillover_variance,
		uint32_t cmp_par_pixels_error, uint32_t spillover_pixels_error)
{
	if (!cfg)
		return -1;

	switch (cfg->data_type) {
	case DATA_TYPE_OFFSET:
	case DATA_TYPE_F_CAM_OFFSET:
		cfg->cmp_par_offset_mean = cmp_par_mean;
		cfg->spill_offset_mean = spillover_mean;
		cfg->cmp_par_offset_variance = cmp_par_variance;
		cfg->spill_offset_variance = spillover_variance;
		break;
	case DATA_TYPE_BACKGROUND:
	case DATA_TYPE_F_CAM_BACKGROUND:
		cfg->cmp_par_background_mean = cmp_par_mean;
		cfg->spill_background_mean = spillover_mean;
		cfg->cmp_par_background_variance = cmp_par_variance;
		cfg->spill_background_variance = spillover_variance;
		cfg->cmp_par_background_pixels_error = cmp_par_pixels_error;
		cfg->spill_background_pixels_error = spillover_pixels_error;
		break;
	case DATA_TYPE_SMEARING:
		cfg->cmp_par_smearing_mean = cmp_par_mean;
		cfg->spill_smearing_mean = spillover_mean;
		cfg->cmp_par_smearing_variance = cmp_par_variance;
		cfg->spill_smearing_variance = spillover_variance;
		cfg->cmp_par_smearing_pixels_error = cmp_par_pixels_error;
		cfg->spill_smearing_pixels_error = spillover_pixels_error;
		break;
	default:
		debug_print("Error: The compression data type is not an auxiliary science compression data type.\n");
		return -1;
	}

	if (cmp_cfg_aux_is_invalid(cfg))
		return -1;

	return 0;
}


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
 * @brief put the value of up to 32 bits into a big endian bitstream
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
 * @returns length in bits of the generated bitstream on success; returns
 *          negative in case of erroneous input; returns CMP_ERROR_SMALL_BUF if
 *          the bitstream buffer is too small to put the value in the bitstream
 */

static int put_n_bits32(uint32_t value, unsigned int n_bits, int bit_offset,
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
	unsigned int bits_left = bit_offset & 0x1F;
	unsigned int bits_right = 64 - bits_left - n_bits;
	unsigned int shift_left = 32 - n_bits;
	int stream_len = (int)(n_bits + (unsigned int)bit_offset);
	uint32_t *local_adr;
	uint32_t mask, tmp;

	/* Leave in case of erroneous input */
	if (bit_offset < 0)
		return -1;

	if (n_bits == 0)
		return stream_len;

	if ((int)shift_left < 0)  /* check n_bits <= 32 */
		return -1;

	if (!bitstream_adr)  /* Do we need to write data to the bitstream? */
		return stream_len;

	/* Check if bitstream buffer is large enough */
	if ((unsigned int)stream_len > max_stream_len)
		return CMP_ERROR_SMALL_BUF;

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
 * @param m		Golomb parameter, only m's which are power of 2 are allowed
 *			maximum allowed Golomb parameter is 0x80000000
 * @param log2_m	Rice parameter, is ilog_2(m) calculate outside function
 *			for better performance
 * @param cw		address where the code word is stored
 *
 * @warning no check of the validity of the input parameters!
 * @returns the length of the formed code word in bits; code word is invalid if
 *	the return value is greater than 32
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
	 * bits can only be formed if q = 0 and qc = 0. To prevent undefined b
	 * ehavior, the right shift operand is masked (& 0x1FU)
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
 * @returns the length of the formed code word in bits; code word is invalid if
 *	the return value is greater than 32
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
 * @brief generate a code word without an outlier mechanism and put in the
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

static int encode_normal(uint32_t value, int stream_len,
			 const struct encoder_setupt *setup)
{
	uint32_t code_word, cw_len;

	cw_len = setup->generate_cw_f(value, setup->encoder_par1,
				      setup->encoder_par2, &code_word);

	return put_n_bits32(code_word, cw_len, stream_len, setup->bitstream_adr,
			    setup->max_stream_len);
}


/**
 * @brief subtract the model from the data, encode the result and put it into
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

static int encode_value_zero(uint32_t data, uint32_t model, int stream_len,
			     const struct encoder_setupt *setup)
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
		data++; /* add 1 to every value so we can use 0 as escape symbol */
		return encode_normal(data, stream_len, setup);
	}

	data++; /* add 1 to every value so we can use 0 as escape symbol */

	/* use zero as escape symbol */
	stream_len = encode_normal(0, stream_len, setup);
	if (stream_len <= 0)
		return stream_len;

	/* put the data unencoded in the bitstream */
	stream_len = put_n_bits32(data, setup->max_data_bits, stream_len,
				  setup->bitstream_adr, setup->max_stream_len);

	return stream_len;
}


/**
 * @brief subtract the model from the data, encode the result and put it into
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

static int encode_value_multi(uint32_t data, uint32_t model, int stream_len,
			      const struct encoder_setupt *setup)
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
	 * indicate that the next codeword is unencoded.
	 * We use different escape symbol depended on the size the needed bit of
	 * unencoded data:
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
	if (stream_len <= 0)
		return stream_len;

	/* put the unencoded data in the bitstream */
	stream_len = put_n_bits32(unencoded_data, unencoded_data_len, stream_len,
				  setup->bitstream_adr, setup->max_stream_len);

	return stream_len;
}


/**
 * @brief put the value unencoded with setup->cmp_par_1 bits without any changes
 *	in the bitstream
 *
 * @param value		value to put unchanged in the bitstream
 *	(setup->cmp_par_1 how many bits of the value are used)
 * @param unused	this parameter is ignored
 * @param stream_len	length of the bitstream in bits
 * @param setup		pointer to the encoder setup
 *
 * @returns the bit length of the bitstream with the added unencoded value on
 *	success; negative on error, CMP_ERROR_SMALL_BUF if the bitstream buffer
 *	is too small to put the value in the bitstream
 *
 */

static int encode_value_none(uint32_t value, uint32_t unused, int stream_len,
			     const struct encoder_setupt *setup)
{
	(void)(unused);

	return put_n_bits32(value, setup->encoder_par1, stream_len,
			    setup->bitstream_adr, setup->max_stream_len);
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

static int encode_value(uint32_t data, uint32_t model, int stream_len,
			const struct encoder_setupt *setup)
{
	uint32_t mask = ~(0xFFFFFFFFU >> (32-setup->max_data_bits));

	/* lossy rounding of the data if lossy_par > 0 */
	data = round_fwd(data, setup->lossy_par);
	model = round_fwd(model, setup->lossy_par);

	if (data & mask || model & mask) {
		debug_print("Error: The data or the model of the data are bigger than expected.\n");
		return CMP_ERROR_HIGH_VALUE;
	}

	return setup->encode_method_f(data, model, stream_len, setup);
}


/**
 * @brief calculate the maximum length of the bitstream/icu_output_buf in bits
 * @note we round down to the next 4-byte allied address because we access the
 *	cmp_buffer in uint32_t words
 *
 * @param buffer_length	length of the icu_output_buf in samples
 *
 * @returns buffer size in bits
 *
 */

static uint32_t cmp_buffer_length_to_bits(uint32_t buffer_length)
{
	return (buffer_length & ~0x3U) * 8;
}


/**
 * @brief configure an encoder setup structure to have a setup to encode a vale
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

static void configure_encoder_setup(struct encoder_setupt *setup,
				    uint32_t cmp_par, uint32_t spillover,
				    uint32_t lossy_par, uint32_t max_data_bits,
				    const struct cmp_cfg *cfg)
{
	memset(setup, 0, sizeof(struct encoder_setupt));

	setup->encoder_par1 = cmp_par;
	setup->max_data_bits = max_data_bits;
	setup->lossy_par = lossy_par;
	setup->bitstream_adr = cfg->icu_output_buf;
	setup->max_stream_len = cmp_buffer_length_to_bits(cfg->buffer_length);

	if (cfg->cmp_mode != CMP_MODE_STUFF) {
		setup->encoder_par2 = ilog_2(cmp_par);
		setup->spillover_par = spillover;

		/* for encoder_par1 which are a power of two we can use the faster rice_encoder */
		if (is_a_pow_of_2(setup->encoder_par1))
			setup->generate_cw_f = &rice_encoder;
		else
			setup->generate_cw_f = &golomb_encoder;
	}

	switch (cfg->cmp_mode) {
	case CMP_MODE_MODEL_ZERO:
	case CMP_MODE_DIFF_ZERO:
		setup->encode_method_f = &encode_value_zero;
		break;
	case CMP_MODE_MODEL_MULTI:
	case CMP_MODE_DIFF_MULTI:
		setup->encode_method_f = &encode_value_multi;
		break;
	case CMP_MODE_STUFF:
		setup->encode_method_f = &encode_value_none;
		setup->max_data_bits = cmp_par;
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

static int compress_imagette(const struct cmp_cfg *cfg, int stream_len)
{
	size_t i;
	struct encoder_setupt setup;
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

	configure_encoder_setup(&setup, cfg->golomb_par, cfg->spill, cfg->round,
				max_data_bits, cfg);

	for (i = 0;; i++) {
		stream_len = encode_value(get_unaligned(&data_buf[i]),
					  model, stream_len, &setup);
		if (stream_len <= 0)
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

static int compress_s_fx(const struct cmp_cfg *cfg, int stream_len)
{
	size_t i;

	struct s_fx *data_buf = cfg->input_buf;
	struct s_fx *model_buf = cfg->model_buf;
	struct s_fx *up_model_buf = NULL;
	struct s_fx *next_model_p;
	struct s_fx model;
	struct encoder_setupt setup_exp_flag, setup_fx;

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
		up_model_buf = cfg->icu_new_model_buf;
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_encoder_setup(&setup_exp_flag, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				cfg->round, cfg->max_used_bits->s_exp_flags, cfg);
	configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				cfg->round, cfg->max_used_bits->s_fx, cfg);

	for (i = 0;; i++) {
		stream_len = encode_value(data_buf[i].exp_flags, model.exp_flags,
					  stream_len, &setup_exp_flag);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].fx, model.fx, stream_len,
					  &setup_fx);
		if (stream_len <= 0)
			return stream_len;

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

static int compress_s_fx_efx(const struct cmp_cfg *cfg, int stream_len)
{
	size_t i;

	struct s_fx_efx *data_buf = cfg->input_buf;
	struct s_fx_efx *model_buf = cfg->model_buf;
	struct s_fx_efx *up_model_buf = NULL;
	struct s_fx_efx *next_model_p;
	struct s_fx_efx model;
	struct encoder_setupt setup_exp_flag, setup_fx, setup_efx;

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
		up_model_buf = cfg->icu_new_model_buf;
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_encoder_setup(&setup_exp_flag, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				cfg->round, cfg->max_used_bits->s_exp_flags, cfg);
	configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				cfg->round, cfg->max_used_bits->s_fx, cfg);
	configure_encoder_setup(&setup_efx, cfg->cmp_par_efx, cfg->spill_efx,
				cfg->round, cfg->max_used_bits->s_efx, cfg);

	for (i = 0;; i++) {
		stream_len = encode_value(data_buf[i].exp_flags, model.exp_flags,
					  stream_len, &setup_exp_flag);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].fx, model.fx, stream_len,
					  &setup_fx);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].efx, model.efx,
					  stream_len, &setup_efx);
		if (stream_len <= 0)
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

static int compress_s_fx_ncob(const struct cmp_cfg *cfg, int stream_len)
{
	size_t i;

	struct s_fx_ncob *data_buf = cfg->input_buf;
	struct s_fx_ncob *model_buf = cfg->model_buf;
	struct s_fx_ncob *up_model_buf = NULL;
	struct s_fx_ncob *next_model_p;
	struct s_fx_ncob model;
	struct encoder_setupt setup_exp_flag, setup_fx, setup_ncob;

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
		up_model_buf = cfg->icu_new_model_buf;
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_encoder_setup(&setup_exp_flag, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				cfg->round, cfg->max_used_bits->s_exp_flags, cfg);
	configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				cfg->round, cfg->max_used_bits->s_fx, cfg);
	configure_encoder_setup(&setup_ncob, cfg->cmp_par_ncob, cfg->spill_ncob,
				cfg->round, cfg->max_used_bits->s_ncob, cfg);

	for (i = 0;; i++) {
		stream_len = encode_value(data_buf[i].exp_flags, model.exp_flags,
					  stream_len, &setup_exp_flag);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].fx, model.fx, stream_len,
					  &setup_fx);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].ncob_x, model.ncob_x,
					  stream_len, &setup_ncob);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].ncob_y, model.ncob_y,
					  stream_len, &setup_ncob);
		if (stream_len <= 0)
			return stream_len;

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

static int compress_s_fx_efx_ncob_ecob(const struct cmp_cfg *cfg, int stream_len)
{
	size_t i;

	struct s_fx_efx_ncob_ecob *data_buf = cfg->input_buf;
	struct s_fx_efx_ncob_ecob *model_buf = cfg->model_buf;
	struct s_fx_efx_ncob_ecob *up_model_buf = NULL;
	struct s_fx_efx_ncob_ecob *next_model_p;
	struct s_fx_efx_ncob_ecob model;
	struct encoder_setupt setup_exp_flag, setup_fx, setup_ncob, setup_efx,
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
				cfg->round, cfg->max_used_bits->s_exp_flags, cfg);
	configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				cfg->round, cfg->max_used_bits->s_fx, cfg);
	configure_encoder_setup(&setup_ncob, cfg->cmp_par_ncob, cfg->spill_ncob,
				cfg->round, cfg->max_used_bits->s_ncob, cfg);
	configure_encoder_setup(&setup_efx, cfg->cmp_par_efx, cfg->spill_efx,
				cfg->round, cfg->max_used_bits->s_efx, cfg);
	configure_encoder_setup(&setup_ecob, cfg->cmp_par_ecob, cfg->spill_ecob,
				cfg->round, cfg->max_used_bits->s_ecob, cfg);

	for (i = 0;; i++) {
		stream_len = encode_value(data_buf[i].exp_flags, model.exp_flags,
					  stream_len, &setup_exp_flag);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].fx, model.fx, stream_len,
					  &setup_fx);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].ncob_x, model.ncob_x,
					  stream_len, &setup_ncob);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].ncob_y, model.ncob_y,
					  stream_len, &setup_ncob);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].efx, model.efx,
					  stream_len, &setup_efx);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].ecob_x, model.ecob_x,
					  stream_len, &setup_ecob);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].ecob_y, model.ecob_y,
					  stream_len, &setup_ecob);
		if (stream_len <= 0)
			return stream_len;

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
 * @brief compress F_FX data
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @returns the bit length of the bitstream on success; negative on error,
 *	CMP_ERROR_SMALL_BUF if the bitstream buffer is too small to put the
 *	value in the bitstream
 */

static int compress_f_fx(const struct cmp_cfg *cfg, int stream_len)
{
	size_t i;

	struct f_fx *data_buf = cfg->input_buf;
	struct f_fx *model_buf = cfg->model_buf;
	struct f_fx *up_model_buf = NULL;
	struct f_fx *next_model_p;
	struct f_fx model;
	struct encoder_setupt setup_fx;

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
		up_model_buf = cfg->icu_new_model_buf;
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				cfg->round, cfg->max_used_bits->f_fx, cfg);

	for (i = 0;; i++) {
		stream_len = encode_value(data_buf[i].fx, model.fx, stream_len,
					  &setup_fx);
		if (stream_len <= 0)
			return stream_len;

		if (up_model_buf) {
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
 * @brief compress F_FX_EFX data
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @returns the bit length of the bitstream on success; negative on error,
 *	CMP_ERROR_SMALL_BUF if the bitstream buffer is too small to put the
 *	value in the bitstream
 */

static int compress_f_fx_efx(const struct cmp_cfg *cfg, int stream_len)
{
	size_t i;

	struct f_fx_efx *data_buf = cfg->input_buf;
	struct f_fx_efx *model_buf = cfg->model_buf;
	struct f_fx_efx *up_model_buf = NULL;
	struct f_fx_efx *next_model_p;
	struct f_fx_efx model;
	struct encoder_setupt setup_fx, setup_efx;

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
		up_model_buf = cfg->icu_new_model_buf;
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				cfg->round, cfg->max_used_bits->f_fx, cfg);
	configure_encoder_setup(&setup_efx, cfg->cmp_par_efx, cfg->spill_efx,
				cfg->round, cfg->max_used_bits->f_efx, cfg);

	for (i = 0;; i++) {
		stream_len = encode_value(data_buf[i].fx, model.fx, stream_len,
					  &setup_fx);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].efx, model.efx,
					  stream_len, &setup_efx);
		if (stream_len <= 0)
			return stream_len;

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
	return stream_len;
}


/**
 * @brief compress F_FX_NCOB data
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @returns the bit length of the bitstream on success; negative on error,
 *	CMP_ERROR_SMALL_BUF if the bitstream buffer is too small to put the
 *	value in the bitstream
 */

static int compress_f_fx_ncob(const struct cmp_cfg *cfg, int stream_len)
{
	size_t i;

	struct f_fx_ncob *data_buf = cfg->input_buf;
	struct f_fx_ncob *model_buf = cfg->model_buf;
	struct f_fx_ncob *up_model_buf = NULL;
	struct f_fx_ncob *next_model_p;
	struct f_fx_ncob model;
	struct encoder_setupt setup_fx, setup_ncob;

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
		up_model_buf = cfg->icu_new_model_buf;
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				cfg->round, cfg->max_used_bits->f_fx, cfg);
	configure_encoder_setup(&setup_ncob, cfg->cmp_par_ncob, cfg->spill_ncob,
				cfg->round, cfg->max_used_bits->f_ncob, cfg);

	for (i = 0;; i++) {
		stream_len = encode_value(data_buf[i].fx, model.fx, stream_len,
					  &setup_fx);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].ncob_x, model.ncob_x,
					  stream_len, &setup_ncob);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].ncob_y, model.ncob_y,
					  stream_len, &setup_ncob);
		if (stream_len <= 0)
			return stream_len;

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
	return stream_len;
}


/**
 * @brief compress F_FX_EFX_NCOB_ECOB data
 *
 * @param cfg	pointer to the compression configuration structure
 *
 * @returns the bit length of the bitstream on success; negative on error,
 *	CMP_ERROR_SMALL_BUF if the bitstream buffer is too small to put the
 *	value in the bitstream
 */

static int compress_f_fx_efx_ncob_ecob(const struct cmp_cfg *cfg, int stream_len)
{
	size_t i;

	struct f_fx_efx_ncob_ecob *data_buf = cfg->input_buf;
	struct f_fx_efx_ncob_ecob *model_buf = cfg->model_buf;
	struct f_fx_efx_ncob_ecob *up_model_buf = NULL;
	struct f_fx_efx_ncob_ecob *next_model_p;
	struct f_fx_efx_ncob_ecob model;
	struct encoder_setupt setup_fx, setup_ncob, setup_efx, setup_ecob;

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
		up_model_buf = cfg->icu_new_model_buf;
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				cfg->round, cfg->max_used_bits->f_fx, cfg);
	configure_encoder_setup(&setup_ncob, cfg->cmp_par_ncob, cfg->spill_ncob,
				cfg->round, cfg->max_used_bits->f_ncob, cfg);
	configure_encoder_setup(&setup_efx, cfg->cmp_par_efx, cfg->spill_efx,
				cfg->round, cfg->max_used_bits->f_efx, cfg);
	configure_encoder_setup(&setup_ecob, cfg->cmp_par_ecob, cfg->spill_ecob,
				cfg->round, cfg->max_used_bits->f_ecob, cfg);

	for (i = 0;; i++) {
		stream_len = encode_value(data_buf[i].fx, model.fx, stream_len,
					  &setup_fx);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].ncob_x, model.ncob_x,
					  stream_len, &setup_ncob);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].ncob_y, model.ncob_y,
					  stream_len, &setup_ncob);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].efx, model.efx,
					  stream_len, &setup_efx);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].ecob_x, model.ecob_x,
					  stream_len, &setup_ecob);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].ecob_y, model.ecob_y,
					  stream_len, &setup_ecob);
		if (stream_len <= 0)
			return stream_len;

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

static int compress_l_fx(const struct cmp_cfg *cfg, int stream_len)
{
	size_t i;

	struct l_fx *data_buf = cfg->input_buf;
	struct l_fx *model_buf = cfg->model_buf;
	struct l_fx *up_model_buf = NULL;
	struct l_fx *next_model_p;
	struct l_fx model;
	struct encoder_setupt setup_exp_flag, setup_fx, setup_fx_var;

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
		up_model_buf = cfg->icu_new_model_buf;
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_encoder_setup(&setup_exp_flag, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				cfg->round, cfg->max_used_bits->l_exp_flags, cfg);
	configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				cfg->round, cfg->max_used_bits->l_fx, cfg);
	configure_encoder_setup(&setup_fx_var, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				cfg->round, cfg->max_used_bits->l_fx_variance, cfg);

	for (i = 0;; i++) {
		stream_len = encode_value(data_buf[i].exp_flags, model.exp_flags,
					  stream_len, &setup_exp_flag);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].fx, model.fx, stream_len,
					  &setup_fx);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].fx_variance, model.fx_variance,
					  stream_len, &setup_fx_var);
		if (stream_len <= 0)
			return stream_len;

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

static int compress_l_fx_efx(const struct cmp_cfg *cfg, int stream_len)
{
	size_t i;

	struct l_fx_efx *data_buf = cfg->input_buf;
	struct l_fx_efx *model_buf = cfg->model_buf;
	struct l_fx_efx *up_model_buf = NULL;
	struct l_fx_efx *next_model_p;
	struct l_fx_efx model;
	struct encoder_setupt setup_exp_flag, setup_fx, setup_efx, setup_fx_var;

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
		up_model_buf = cfg->icu_new_model_buf;
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_encoder_setup(&setup_exp_flag, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				cfg->round, cfg->max_used_bits->l_exp_flags, cfg);
	configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				cfg->round, cfg->max_used_bits->l_fx, cfg);
	configure_encoder_setup(&setup_efx, cfg->cmp_par_efx, cfg->spill_efx,
				cfg->round, cfg->max_used_bits->l_efx, cfg);
	configure_encoder_setup(&setup_fx_var, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				cfg->round, cfg->max_used_bits->l_fx_variance, cfg);

	for (i = 0;; i++) {
		stream_len = encode_value(data_buf[i].exp_flags, model.exp_flags,
					  stream_len, &setup_exp_flag);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].fx, model.fx, stream_len,
					  &setup_fx);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].efx, model.efx,
					  stream_len, &setup_efx);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].fx_variance, model.fx_variance,
					  stream_len, &setup_fx_var);
		if (stream_len <= 0)
			return stream_len;

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

static int compress_l_fx_ncob(const struct cmp_cfg *cfg, int stream_len)
{
	size_t i;

	struct l_fx_ncob *data_buf = cfg->input_buf;
	struct l_fx_ncob *model_buf = cfg->model_buf;
	struct l_fx_ncob *up_model_buf = NULL;
	struct l_fx_ncob *next_model_p;
	struct l_fx_ncob model;
	struct encoder_setupt setup_exp_flag, setup_fx, setup_ncob,
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
				cfg->round, cfg->max_used_bits->l_exp_flags, cfg);
	configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				cfg->round, cfg->max_used_bits->l_fx, cfg);
	configure_encoder_setup(&setup_ncob, cfg->cmp_par_ncob, cfg->spill_ncob,
				cfg->round, cfg->max_used_bits->l_ncob, cfg);
	/* we use compression parameter for both variance data fields */
	configure_encoder_setup(&setup_fx_var, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				cfg->round, cfg->max_used_bits->l_fx_variance, cfg);
	configure_encoder_setup(&setup_cob_var, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				cfg->round, cfg->max_used_bits->l_cob_variance, cfg);

	for (i = 0;; i++) {
		stream_len = encode_value(data_buf[i].exp_flags, model.exp_flags,
					  stream_len, &setup_exp_flag);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].fx, model.fx, stream_len,
					  &setup_fx);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].ncob_x, model.ncob_x,
					  stream_len, &setup_ncob);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].ncob_y, model.ncob_y,
					  stream_len, &setup_ncob);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].fx_variance, model.fx_variance,
					  stream_len, &setup_fx_var);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].cob_x_variance, model.cob_x_variance,
					  stream_len, &setup_cob_var);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].cob_y_variance, model.cob_y_variance,
					  stream_len, &setup_cob_var);
		if (stream_len <= 0)
			return stream_len;

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

static int compress_l_fx_efx_ncob_ecob(const struct cmp_cfg *cfg, int stream_len)
{
	size_t i;

	struct l_fx_efx_ncob_ecob *data_buf = cfg->input_buf;
	struct l_fx_efx_ncob_ecob *model_buf = cfg->model_buf;
	struct l_fx_efx_ncob_ecob *up_model_buf = NULL;
	struct l_fx_efx_ncob_ecob *next_model_p;
	struct l_fx_efx_ncob_ecob model;
	struct encoder_setupt setup_exp_flag, setup_fx, setup_ncob, setup_efx,
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
				cfg->round, cfg->max_used_bits->l_exp_flags, cfg);
	configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				cfg->round, cfg->max_used_bits->l_fx, cfg);
	configure_encoder_setup(&setup_ncob, cfg->cmp_par_ncob, cfg->spill_ncob,
				cfg->round, cfg->max_used_bits->l_ncob, cfg);
	configure_encoder_setup(&setup_efx, cfg->cmp_par_efx, cfg->spill_efx,
				cfg->round, cfg->max_used_bits->l_efx, cfg);
	configure_encoder_setup(&setup_ecob, cfg->cmp_par_ecob, cfg->spill_ecob,
				cfg->round, cfg->max_used_bits->l_ecob, cfg);
	/* we use compression parameter for both variance data fields */
	configure_encoder_setup(&setup_fx_var, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				cfg->round, cfg->max_used_bits->l_fx_variance, cfg);
	configure_encoder_setup(&setup_cob_var, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				cfg->round, cfg->max_used_bits->l_cob_variance, cfg);

	for (i = 0;; i++) {
		stream_len = encode_value(data_buf[i].exp_flags, model.exp_flags,
					  stream_len, &setup_exp_flag);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].fx, model.fx, stream_len,
					  &setup_fx);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].ncob_x, model.ncob_x,
					  stream_len, &setup_ncob);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].ncob_y, model.ncob_y,
					  stream_len, &setup_ncob);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].efx, model.efx,
					  stream_len, &setup_efx);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].ecob_x, model.ecob_x,
					  stream_len, &setup_ecob);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].ecob_y, model.ecob_y,
					  stream_len, &setup_ecob);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].fx_variance, model.fx_variance,
					  stream_len, &setup_fx_var);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].cob_x_variance, model.cob_x_variance,
					  stream_len, &setup_cob_var);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].cob_y_variance, model.cob_y_variance,
					  stream_len, &setup_cob_var);
		if (stream_len <= 0)
			return stream_len;

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

static int compress_offset(const struct cmp_cfg *cfg, int stream_len)
{
	size_t i;

	struct offset *data_buf = cfg->input_buf;
	struct offset *model_buf = cfg->model_buf;
	struct offset *up_model_buf = NULL;
	struct offset *next_model_p;
	struct offset model;
	struct encoder_setupt setup_mean, setup_var;

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
			mean_bits_used = cfg->max_used_bits->fc_offset_mean;
			variance_bits_used = cfg->max_used_bits->fc_offset_variance;
			break;
		case DATA_TYPE_OFFSET:
		default:
			mean_bits_used = cfg->max_used_bits->nc_offset_mean;
			variance_bits_used = cfg->max_used_bits->nc_offset_variance;
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
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].variance, model.variance,
					  stream_len, &setup_var);
		if (stream_len <= 0)
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

static int compress_background(const struct cmp_cfg *cfg, int stream_len)
{
	size_t i;

	struct background *data_buf = cfg->input_buf;
	struct background *model_buf = cfg->model_buf;
	struct background *up_model_buf = NULL;
	struct background *next_model_p;
	struct background model;
	struct encoder_setupt setup_mean, setup_var, setup_pix;

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
			mean_used_bits = cfg->max_used_bits->fc_background_mean;
			varinace_used_bits = cfg->max_used_bits->fc_background_variance;
			pixels_error_used_bits = cfg->max_used_bits->fc_background_outlier_pixels;
			break;
		case DATA_TYPE_BACKGROUND:
		default:
			mean_used_bits = cfg->max_used_bits->nc_background_mean;
			varinace_used_bits = cfg->max_used_bits->nc_background_variance;
			pixels_error_used_bits = cfg->max_used_bits->nc_background_outlier_pixels;
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
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].variance, model.variance,
					  stream_len, &setup_var);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].outlier_pixels, model.outlier_pixels,
					  stream_len, &setup_pix);
		if (stream_len <= 0)
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

static int compress_smearing(const struct cmp_cfg *cfg, int stream_len)
{
	size_t i;

	struct smearing *data_buf = cfg->input_buf;
	struct smearing *model_buf = cfg->model_buf;
	struct smearing *up_model_buf = NULL;
	struct smearing *next_model_p;
	struct smearing model;
	struct encoder_setupt setup_mean, setup_var_mean, setup_pix;

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
		up_model_buf = cfg->icu_new_model_buf;
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	configure_encoder_setup(&setup_mean, cfg->cmp_par_smearing_mean, cfg->spill_smearing_mean,
				cfg->round, cfg->max_used_bits->smearing_mean, cfg);
	configure_encoder_setup(&setup_var_mean, cfg->cmp_par_smearing_variance, cfg->spill_smearing_variance,
				cfg->round, cfg->max_used_bits->smearing_variance_mean, cfg);
	configure_encoder_setup(&setup_pix, cfg->cmp_par_smearing_pixels_error, cfg->spill_smearing_pixels_error,
				cfg->round, cfg->max_used_bits->smearing_outlier_pixels, cfg);

	for (i = 0;; i++) {
		stream_len = encode_value(data_buf[i].mean, model.mean,
					  stream_len, &setup_mean);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].variance_mean, model.variance_mean,
					  stream_len, &setup_var_mean);
		if (stream_len <= 0)
			return stream_len;
		stream_len = encode_value(data_buf[i].outlier_pixels, model.outlier_pixels,
					  stream_len, &setup_pix);
		if (stream_len <= 0)
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
 * @brief fill the last part of the bitstream with zeros
 *
 * @param cfg		pointer to the compression configuration structure
 * @param cmp_size	length of the bitstream in bits
 *
 * @returns the bit length of the bitstream on success; negative on error,
 *	CMP_ERROR_SMALL_BUF if the bitstream buffer is too small to put the
 *	value in the bitstream
 */

static int pad_bitstream(const struct cmp_cfg *cfg, int cmp_size)
{
	unsigned int output_buf_len_bits, n_pad_bits;

	if (cmp_size < 0)
		return cmp_size;

	if (!cfg->icu_output_buf)
		return cmp_size;

	/* no padding in RAW mode; ALWAYS BIG-ENDIAN */
	if (cfg->cmp_mode == CMP_MODE_RAW)
		return cmp_size;

	/* maximum length of the bitstream/icu_output_buf in bits */
	output_buf_len_bits = cmp_buffer_length_to_bits(cfg->buffer_length);

	n_pad_bits = 32 - ((unsigned int)cmp_size & 0x1FU);
	if (n_pad_bits < 32) {
		int n_bits = put_n_bits32(0, n_pad_bits, cmp_size, cfg->icu_output_buf,
					  output_buf_len_bits);
		if (n_bits < 0)
			return n_bits;
	}

	return cmp_size;
}


/* TODO: doc string */

static int compress_data_internal(const struct cmp_cfg *cfg, int stream_len)
{
	int bitsize = 0;

	if (!cfg)
		return -1;

	if (stream_len < 0)
		return stream_len;

	if (cfg->samples == 0) /* nothing to compress we are done*/
		return 0;

	if (stream_len & 0x7) {
		debug_print("Error: The stream_len parameter must be a multiple of 8.\n");
		return -1;
	}

	if (raw_mode_is_used(cfg->cmp_mode) && cfg->icu_output_buf)
		if (((uint32_t)stream_len >> 3) + cfg->samples * size_of_a_sample(cfg->data_type) > cfg->buffer_length)
			return CMP_ERROR_SMALL_BUF;

	if (cmp_cfg_icu_is_invalid(cfg))
		return -1;

	if (raw_mode_is_used(cfg->cmp_mode)) {
		uint32_t raw_size = cfg->samples * (uint32_t)size_of_a_sample(cfg->data_type);

		if (cfg->icu_output_buf) {
			uint8_t *p = (uint8_t *)cfg->icu_output_buf + (stream_len >> 3);

			memcpy(p, cfg->input_buf, raw_size);
			if (cpu_to_be_data_type(p, raw_size, cfg->data_type))
				return -1;
		}
		bitsize += stream_len + (int)raw_size*8; /* convert to bits */
	} else {
		if (cfg->icu_output_buf && cfg->samples/3 > cfg->buffer_length)
			debug_print("Warning: The size of the compressed_data buffer is 3 times smaller than the data_to_compress. This is probably unintended.\n");

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

		case DATA_TYPE_F_FX:
			bitsize = compress_f_fx(cfg, stream_len);
			break;
		case DATA_TYPE_F_FX_EFX:
			bitsize = compress_f_fx_efx(cfg, stream_len);
			break;
		case DATA_TYPE_F_FX_NCOB:
			bitsize = compress_f_fx_ncob(cfg, stream_len);
			break;
		case DATA_TYPE_F_FX_EFX_NCOB_ECOB:
			bitsize = compress_f_fx_efx_ncob_ecob(cfg, stream_len);
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

		/* LCOV_EXCL_START */
		case DATA_TYPE_UNKNOWN:
		default:
			debug_print("Error: Data type not supported.\n");
			bitsize = -1;
		}
		/* LCOV_EXCL_STOP */
	}

	bitsize = pad_bitstream(cfg, bitsize);

	return bitsize;
}


/**
 * @brief compress data on the ICU in software
 *
 * @param cfg	pointer to a compression configuration (created with the
 *		cmp_cfg_icu_create() function, setup with the cmp_cfg_xxx() functions)
 *
 * @note the validity of the cfg structure is checked before the compression is
 *	 started
 *
 * @returns the bit length of the bitstream on success; negative on error,
 *	CMP_ERROR_SMALL_BUF (-2) if the compressed data buffer is too small to
 *	hold the whole compressed data, CMP_ERROR_HIGH_VALUE (-3) if a data or
 *	model value is bigger than the max_used_bits parameter allows (set with
 *	the cmp_set_max_used_bits() function)
 */

int icu_compress_data(const struct cmp_cfg *cfg)
{
	struct cmp_cfg cfg_cpy;
	int dst_capacity_used = 0;

	if (cfg) {
		if (cfg->samples == 0)
			return 0;
		cfg_cpy = *cfg;
		cfg_cpy.buffer_length = cmp_cal_size_of_data(cfg->buffer_length, cfg->data_type);
		if (!cfg_cpy.buffer_length)
			return -1;

		if (!rdcu_supported_data_type_is_used(cfg->data_type) && !cmp_data_type_is_invalid(cfg->data_type)) {
			if (cfg->icu_new_model_buf) {
				if (cfg->input_buf)
					memcpy(cfg->icu_new_model_buf, cfg->input_buf, COLLECTION_HDR_SIZE);
				cfg_cpy.icu_new_model_buf = (uint8_t *)cfg_cpy.icu_new_model_buf + COLLECTION_HDR_SIZE;
			}
			if (cfg->icu_output_buf && cfg->input_buf && cfg->buffer_length)
				memcpy(cfg->icu_output_buf, cfg->input_buf, COLLECTION_HDR_SIZE);
			if (cfg->input_buf)
				cfg_cpy.input_buf =  (uint8_t *)cfg->input_buf + COLLECTION_HDR_SIZE;
			if (cfg->model_buf)
				cfg_cpy.model_buf = (uint8_t *)cfg->model_buf + COLLECTION_HDR_SIZE;
			dst_capacity_used = COLLECTION_HDR_SIZE*8;
		}
		return compress_data_internal(&cfg_cpy, dst_capacity_used);
	}
	return compress_data_internal(NULL, dst_capacity_used);
}


/* TODO: doc string */

static uint32_t cmp_guess_good_spill(uint32_t golomb_par)
{
	if (!golomb_par)
		return 0;
	return cmp_icu_max_spill(golomb_par);
}


/* TODO: doc string */

static int set_cmp_col_size(uint8_t *p, int cmp_col_size)
{
	uint16_t v = (uint16_t)cmp_col_size;

	if (cmp_col_size > UINT16_MAX)
		return -1;

	v -= COLLECTION_HDR_SIZE+2;
	if (p) {
		memset(p, v >> 8, 1);
		memset(p+1, v & 0xFF, 1);
	}

	return 0;
}


/* TODO: doc string */

static int32_t cmp_collection(uint8_t *col, uint8_t *model, uint8_t *updated_model,
			      uint32_t *dst, uint32_t dst_capacity,
			      struct cmp_cfg *cfg, int32_t dst_size)
{
	int32_t dst_size_begin = dst_size;
	int dst_size_bits;

	if (dst_size < 0)
		return dst_size;
	if (!col)
		return -1;

	if (cfg->cmp_mode != CMP_MODE_RAW) {
		/* we put the compressed data size be */
		dst_size += CMP_COLLECTION_FILD_SIZE;
	}

	/* we do not compress the collection header, we simply copy the header
	 * into the compressed data
	 */
	if (dst) {
		if ((uint32_t)dst_size + COLLECTION_HDR_SIZE > dst_capacity)
			return CMP_ERROR_SMALL_BUF;
		memcpy((uint8_t *)dst + dst_size, col, COLLECTION_HDR_SIZE);
		if (model_mode_is_used(cfg->cmp_mode) && cfg->icu_new_model_buf)
			memcpy(cfg->icu_new_model_buf, col, COLLECTION_HDR_SIZE);
	}
	dst_size += COLLECTION_HDR_SIZE;

	/* prepare the different buffers */
	cfg->icu_output_buf = dst;
	cfg->input_buf = col + COLLECTION_HDR_SIZE;
	if (model)
		cfg->model_buf = model + COLLECTION_HDR_SIZE;
	if (updated_model)
		cfg->icu_new_model_buf = updated_model + COLLECTION_HDR_SIZE;

	{
		struct collection_hdr *col_hdr = (struct collection_hdr *)col;
		uint8_t subservice = cmp_col_get_subservice(col_hdr);
		uint16_t col_data_length = cmp_col_get_data_length(col_hdr);

		cfg->data_type = convert_subservice_to_cmp_data_type(subservice);

		if (!size_of_a_sample(cfg->data_type))
			return -1;
		if (col_data_length % size_of_a_sample(cfg->data_type))
			return -1;
		cfg->samples = col_data_length/size_of_a_sample(cfg->data_type);
		if ((dst && (uint32_t)dst_size + col_data_length > dst_capacity) ||
		    cfg->cmp_mode == CMP_MODE_RAW) {
			cfg->buffer_length = dst_capacity;
			dst_size_bits = compress_data_internal(cfg, dst_size<<3);
			if (dst_size < 0)
				return dst_size_bits;
		} else {
			/* we limit the size of the compressed data buffer  */
			cfg->buffer_length = (uint32_t)dst_size + col_data_length-1;
			dst_size_bits = compress_data_internal(cfg, dst_size<<3);
			if (dst_size_bits == CMP_ERROR_SMALL_BUF ||
			    (!dst && (int)cmp_bit_to_byte((unsigned int)dst_size_bits)-dst_size > col_data_length)) { /* if dst == NULL icu_compress_data_2 will not return a CMP_ERROR_SMALL_BUF */
				enum cmp_mode cmp_mode_cpy = cfg->cmp_mode;

				cfg->buffer_length = (uint32_t)dst_size + col_data_length;
				cfg->cmp_mode = CMP_MODE_RAW;
				dst_size_bits = compress_data_internal(cfg, dst_size<<3);
				cfg->cmp_mode = cmp_mode_cpy;
			}
		}
		if (dst_size_bits < 0)
			return dst_size_bits;
	}

	dst_size = (int32_t)cmp_bit_to_byte((unsigned int)dst_size_bits); /*TODO: fix casts */
	if (dst && cfg->cmp_mode != CMP_MODE_RAW)
		if (set_cmp_col_size((uint8_t *)dst+dst_size_begin, dst_size-dst_size_begin))
			return -1;

	return dst_size;
}


static int cmp_ent_build_chunk_header(struct cmp_entity *ent, uint32_t chunk_size,
				      const struct cmp_cfg *cfg, uint64_t start_timestamp,
				      int32_t cmp_ent_size_byte)
{
	if (ent) { /* setup the compressed entity header */
		int err = 0;

		if (cmp_ent_size_byte < 0)
			cmp_ent_size_byte = 0;
		err |= cmp_ent_set_version_id(ent, version_identifier);
		err |= cmp_ent_set_size(ent, (uint32_t)cmp_ent_size_byte);
		if (cmp_ent_set_original_size(ent, chunk_size)) {
			debug_print("Error: The size of the chunk is too.\n");
			return -1;
		}
		err |= cmp_ent_set_start_timestamp(ent, start_timestamp);
		err |= cmp_ent_set_data_type(ent, DATA_TYPE_CHUNK, cfg->cmp_mode == CMP_MODE_RAW);
		err |= cmp_ent_set_cmp_mode(ent, cfg->cmp_mode);
		err |= cmp_ent_set_model_value(ent, cfg->model_value);
		err |= cmp_ent_set_model_id(ent, 0);
		err |= cmp_ent_set_model_counter(ent, 0);
		if (cfg->max_used_bits)
			err |= cmp_ent_set_max_used_bits_version(ent, cfg->max_used_bits->version);
		else
			err |= cmp_ent_set_max_used_bits_version(ent, 0);
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
		err |= cmp_ent_set_end_timestamp(ent, get_timestamp());
		if (err)
			return -1;
	}
	return NON_IMAGETTE_HEADER_SIZE;
}


/* TODO: doc string; ref document */

static enum chunk_type get_chunk_type(uint16_t subservice)
{
	enum chunk_type chunk_type = CHUNK_TYPE_UNKNOWN;

	switch (subservice) {
	case SST_NCxx_S_SCIENCE_IMAGETTE:
		chunk_type = CHUNK_TYPE_NCAM_IMGAETTE;
		break;
	case SST_NCxx_S_SCIENCE_SAT_IMAGETTE:
		chunk_type = CHUNK_TYPE_SAT_IMGAETTE;
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
	case SST_NCxx_S_SCIENCE_F_FX:
	case SST_NCxx_S_SCIENCE_F_FX_EFX:
	case SST_NCxx_S_SCIENCE_F_FX_NCOB:
	case SST_NCxx_S_SCIENCE_F_FX_EFX_NCOB_ECOB:
		debug_print("Error: No chunk is defined for fast cadence subservices\n");
		chunk_type = CHUNK_TYPE_UNKNOWN;
		break;
	default:
		debug_print("Error: Unknown subservice: %i.\n", subservice);
		chunk_type = CHUNK_TYPE_UNKNOWN;
		break;
	};

	return chunk_type;
}


static enum chunk_type cmp_col_get_chunk_type(const struct collection_hdr *col)
{
	return get_chunk_type(cmp_col_get_subservice(col));
}


static int read_in_cmp_par(const struct cmp_par *par, enum chunk_type chunk_type,
			   struct cmp_cfg *cfg)
{
	memset(cfg, 0, sizeof(struct cmp_cfg));

	cfg->cmp_mode = par->cmp_mode;
	cfg->model_value = par->model_value;
	cfg->round = par->lossy_par;
	cfg->max_used_bits = &MAX_USED_BITS_SAFE;

	switch (chunk_type) {
	case CHUNK_TYPE_NCAM_IMGAETTE:
		cfg->cmp_par_imagette = par->nc_imagette;
		break;
	case CHUNK_TYPE_SAT_IMGAETTE:
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
	default:
		return -1;
	};

	cfg->spill_par_1 = cmp_guess_good_spill(cfg->cmp_par_1);
	cfg->spill_par_2 = cmp_guess_good_spill(cfg->cmp_par_2);
	cfg->spill_par_3 = cmp_guess_good_spill(cfg->cmp_par_3);
	cfg->spill_par_4 = cmp_guess_good_spill(cfg->cmp_par_4);
	cfg->spill_par_5 = cmp_guess_good_spill(cfg->cmp_par_5);
	cfg->spill_par_6 = cmp_guess_good_spill(cfg->cmp_par_6);
	return 0;
}


/**
 * @brief initialise the compress_chunk() function
 *
 * If not initialised the compress_chunk() function sets the timestamps and
 * version_id in the compression entity header to zero
 *
 * @param return_timestamp	pointer to a function returning a current 48-bit
 *				timestamp
 * @param version_id		version identifier of the applications software
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
 *				buffer; has to be 4-byte aligned (can be NULL)
 * @param dst_capacity		capacity of the dst buffer;  it's recommended to
 *				provide a dst_capacity >=
 *				compress_chunk_cmp_size_bound(chunk, chunk_size)
 *				as it eliminates one potential failure scenario:
 *				not enough space in the dst buffer to write the
 *				compressed data; size is round down to a multiple
 *				of 4
 * @returns the byte size of the compressed_data buffer on success; negative on
 *	error, CMP_ERROR_SMALL_BUF (-2) if the compressed data buffer is too
 *	small to hold the whole compressed data
 */

int32_t compress_chunk(void *chunk, uint32_t chunk_size,
		       void *chunk_model, void *updated_chunk_model,
		       uint32_t *dst, uint32_t dst_capacity,
		       const struct cmp_par *cmp_par)
{
	uint64_t start_timestamp = get_timestamp();
	size_t read_bytes;
	struct collection_hdr *col = (struct collection_hdr *)chunk;
	int32_t cmp_size_byte; /* size of the compressed data in bytes */
	enum chunk_type chunk_type;
	struct cmp_cfg cfg;
	int err;

	if (!chunk) {
		debug_print("Error: Pointer to the data chunk is NULL. No data no compression.\n");
		return -1;
	}
	if (chunk_size < COLLECTION_HDR_SIZE) {
		debug_print("Error: The chunk size is smaller than the minimum size.\n");
		return -1;
	}
	if (chunk_size > CMP_ENTITY_MAX_ORIGINAL_SIZE) {
		debug_print("Error: The chunk size is bigger than the maximum allowed chunk size.\n");
		return -1;
	}

	/* we will build the compression header after the compression of the chunk */
	if (cmp_par->cmp_mode == CMP_MODE_RAW)
		cmp_size_byte = GENERIC_HEADER_SIZE;
	else
		cmp_size_byte = NON_IMAGETTE_HEADER_SIZE;
	if (dst) {
		if (dst_capacity < (uint32_t)cmp_size_byte) {
			debug_print("Error: The destination capacity is smaller than the minimum compression entity size.\n");
			return CMP_ERROR_SMALL_BUF;
		}
		memset(dst, 0, (uint32_t)cmp_size_byte);
	}

	chunk_type = cmp_col_get_chunk_type(col);
	if (read_in_cmp_par(cmp_par, chunk_type, &cfg))
		return -1;

	for (read_bytes = 0;
	     read_bytes < chunk_size - COLLECTION_HDR_SIZE;
	     read_bytes += cmp_col_get_size(col)) {
		uint8_t *col_model = NULL;
		uint8_t *col_up_model = NULL;
		/* setup pointers for the next collection we want to compress */
		col = (struct collection_hdr *)((uint8_t *)chunk + read_bytes);
		if (chunk_model)
			col_model = ((uint8_t *)chunk_model + read_bytes);
		if (updated_chunk_model)
			col_up_model = ((uint8_t *)updated_chunk_model + read_bytes);

		if (cmp_col_get_chunk_type(col) != chunk_type) {
			debug_print("Error: The chunk contains collections with an incompatible mix of subservices.\n");
			return -1;
		}
		if (read_bytes + cmp_col_get_size(col) > chunk_size)
			break;

		cmp_size_byte = cmp_collection((uint8_t *)col, col_model, col_up_model,
					       dst, dst_capacity, &cfg, cmp_size_byte);
	}

	if (read_bytes != chunk_size) {
		debug_print("Error: The sum of the compressed collections does not match the size of the data in the compression header.\n");
		return -1;
	}

	err = cmp_ent_build_chunk_header((struct cmp_entity *)dst, chunk_size, &cfg,
				      start_timestamp, cmp_size_byte);
	if (err < 0)
		return err;

	return cmp_size_byte;
}


/**
 * @brief returns the maximum compressed size in a worst case scenario
 *
 * In case the input data is not compressible
 * This function is primarily useful for memory allocation purposes
 * (destination buffer size).
 *
 * @note if the number of collections is known you can use the
 *	COMPRESS_CHUNK_BOUND macro for compilation-time evaluation
 *	(stack memory allocation for example)
 *
 * @param chunk		pointer to the chunk you want compress
 * @param chunk_size	size of the chunk in bytes
 *
 * @returns maximum compressed size for a chunk compression; 0 on error
 */

uint32_t compress_chunk_cmp_size_bound(const void *chunk, size_t chunk_size)
{
	int32_t read_bytes;
	uint32_t num_col = 0;

	if (chunk_size > CMP_ENTITY_MAX_ORIGINAL_SIZE-NON_IMAGETTE_HEADER_SIZE-CMP_COLLECTION_FILD_SIZE) {
		debug_print("Error: The chunk size is bigger than the maximum allowed chunk size.\n");
		return 0;
	}

	for (read_bytes = 0;
	     read_bytes < (int32_t)chunk_size-COLLECTION_HDR_SIZE;
	     read_bytes += cmp_col_get_size((const struct collection_hdr *)((const uint8_t *)chunk + read_bytes)))
		num_col++;


	if ((uint32_t)read_bytes != chunk_size) {
		debug_print("Error: The sum of the compressed collections does not match the size of the data in the compression header.\n");
		return 0;
	}

	return COMPRESS_CHUNK_BOUND((uint32_t)chunk_size, num_col);
}


/**
 * @brief set the model id and model counter in the compression entity header
 *
 * @param dst		pointer to the compressed data starting with a
 *			compression entity header
 * @param dst_size	byte size of the dst buffer
 * @param model_id	model identifier; for identifying entities that originate
 *			from the same starting model
 * @param model_counter	model_counter; counts how many times the model was
 *			updated; for non model mode compression use 0
 *
 * @returns 0 on success, otherwise error
 */

int compress_chunk_set_model_id_and_counter(uint32_t *dst, int dst_size,
					    uint16_t model_id, uint8_t model_counter)
{
	if (dst_size < NON_IMAGETTE_HEADER_SIZE)
		return 1;

	return cmp_ent_set_model_id((struct cmp_entity *)dst, model_id) ||
		cmp_ent_set_model_counter((struct cmp_entity *)dst, model_counter);

}
