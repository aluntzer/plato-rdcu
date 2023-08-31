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

#include <byteorder.h>
#include <cmp_debug.h>
#include <cmp_data_types.h>
#include <cmp_support.h>
#include <cmp_icu.h>
#include <cmp_entity.h>



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

	if (cmp_cfg_gen_par_is_invalid(&cfg, ICU_CHECK))
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
	if (cfg)
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

	return cmp_cfg_imagette_is_invalid(cfg, ICU_CHECK);
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

	return cmp_cfg_fx_cob_is_invalid(cfg);
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

	cfg->cmp_par_mean = cmp_par_mean;
	cfg->cmp_par_variance = cmp_par_variance;
	cfg->cmp_par_pixels_error = cmp_par_pixels_error;

	cfg->spill_mean = spillover_mean;
	cfg->spill_variance = spillover_variance;
	cfg->spill_pixels_error = spillover_pixels_error;

	return cmp_cfg_aux_is_invalid(cfg);
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
 * @brief put the value of up to 32 bits into a bitstream
 *
 * @param value			the value to put
 * @param n_bits		number of bits to put in the bitstream
 * @param bit_offset		bit index where the bits will be put, seen from
 *				the very beginning of the bitstream
 * @param bitstream_adr		this is the pointer to the beginning of the
 *				bitstream (can be NULL)
 * @param max_stream_len	maximum length of the bitstream in bits; is
 *				ignored if bitstream_adr is NULL
 *
 * @returns length in bits of the generated bitstream on success; returns
 *          negative in case of erroneous input; returns CMP_ERROR_SMALL_BUF if
 *          the bitstream buffer is too small to put the value in the bitstream
 */

static int put_n_bits32(uint32_t value, unsigned int n_bits, int bit_offset,
			uint32_t *bitstream_adr, unsigned int max_stream_len)
{
	uint32_t *local_adr;
	uint32_t mask;
	unsigned int shiftRight, shiftLeft, bitsLeft, bitsRight;
	int stream_len = (int)(n_bits + (unsigned int)bit_offset); /* overflow results in a negative return value */

	/* Leave in case of erroneous input */
	if (bit_offset < 0)
		return -1;

	if (n_bits == 0)
		return stream_len;

	if (n_bits > 32)
		return -1;

	/* Do we need to write data to the bitstream? */
	if (!bitstream_adr)
		return stream_len;

	/* Check if bitstream buffer is large enough */
	if ((unsigned int)stream_len > max_stream_len) {
		debug_print("Error: The buffer for the compressed data is too small to hold the compressed data. Try a larger buffer_length parameter.\n");
		return CMP_ERROR_SMALL_BUF;
	}

	/* (M) is the n_bits parameter large enough to cover all value bits; the
	 * calculations can be re-used in the unsegmented code, so we have no overhead
	 */
	shiftRight = 32 - n_bits;
	mask = 0xFFFFFFFFU >> shiftRight;
	value &= mask;

	/* Separate the bit_offset into word offset (set local_adr pointer) and local bit offset (bitsLeft) */
	local_adr = bitstream_adr + (bit_offset >> 5);
	bitsLeft = bit_offset & 0x1F;

	/* Calculate the bitsRight for the unsegmented case. If bitsRight is
	 * negative we need to split the value over two words
	 */
	bitsRight = shiftRight - bitsLeft;

	if ((int)bitsRight >= 0) {
		/*         UNSEGMENTED
		 *
		 *|-----------|XXXXX|----------------|
		 *   bitsLeft    n       bitsRight
		 *
		 *  -> to get the mask:
		 *  shiftRight = bitsLeft + bitsRight = 32 - n
		 *  shiftLeft = bitsRight = 32 - n - bitsLeft = shiftRight - bitsLeft
		 */

		shiftLeft = bitsRight;

		/* generate the mask, the bits for the values will be true
		 * shiftRight = 32 - n_bits; see (M) above!
		 * mask = (0XFFFFFFFF >> shiftRight) << shiftLeft; see (M) above!
		 */
		mask <<= shiftLeft;
		value <<= shiftLeft;

		/* clear the destination with inverse mask */
		*(local_adr) &= ~mask;

		/* assign the value */
		*(local_adr) |= value;

	} else {
		/*                             SEGMENTED
		 *
		 *|-----------------------------|XXX| |XX|------------------------------|
		 *          bitsLeft              n1   n2          bitsRight
		 *
		 *  -> to get the mask part 1:
		 *  shiftRight = bitsLeft
		 *  n1 = n - (bitsLeft + n - 32) = 32 - bitsLeft
		 *
		 *  -> to get the mask part 2:
		 *  n2 = bitsLeft + n - 32 = -(32 - n - bitsLeft) = -(bitsRight_UNSEGMENTED)
		 *  shiftLeft = 32 - n2 = 32 - (bitsLeft + n - 32) = 64 - bitsLeft - n
		 *
		 */

		unsigned int n2 = -bitsRight;

		/* part 1: */
		shiftRight = bitsLeft;
		mask = 0XFFFFFFFFU >> shiftRight;

		/* clear the destination with inverse mask */
		*(local_adr) &= ~mask;

		/* assign the value part 1 */
		*(local_adr) |= (value >> n2);

		/* part 2: */
		/* adjust address */
		local_adr += 1;
		shiftLeft = 32 - n2;
		mask = 0XFFFFFFFFU >> n2;

		/* clear the destination */
		*(local_adr) &= mask;

		/* assign the value part 2 */
		*(local_adr) |= (value << shiftLeft);
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
	 * bits can only be formed if q = 0 and qc = 0. Any shift with 0 << x always
	 * results in 0, which forms the correct codeword in this case. To prevent
	 * undefined behavior, the right shift operand is masked (& 0x1FU)
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
 * @param data_type	used compression data type
 *
 * @returns buffer size in bits
 *
 */

static uint32_t cmp_buffer_length_to_bits(uint32_t buffer_length, enum cmp_data_type data_type)
{
	return (cmp_cal_size_of_data(buffer_length, data_type) & ~0x3U) * CHAR_BIT;
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
 * @returns 0 on success; otherwise error
 */

static int configure_encoder_setup(struct encoder_setupt *setup,
				   uint32_t cmp_par, uint32_t spillover,
				   uint32_t lossy_par, uint32_t max_data_bits,
				   const struct cmp_cfg *cfg)
{
	if (!setup)
		return -1;

	if (!cfg)
		return -1;

	if (max_data_bits > 32) {
		debug_print("Error: max_data_bits parameter is bigger than 32 bits.\n");
		return -1;
	}

	memset(setup, 0, sizeof(*setup));

	setup->encoder_par1 = cmp_par;
	setup->max_data_bits = max_data_bits;
	setup->lossy_par = lossy_par;
	setup->bitstream_adr = cfg->icu_output_buf;
	setup->max_stream_len = cmp_buffer_length_to_bits(cfg->buffer_length, cfg->data_type);

	if (cfg->cmp_mode != CMP_MODE_STUFF) {
		if (ilog_2(cmp_par) == -1U)
			return -1;
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
	case CMP_MODE_RAW:
	default:
		return -1;
	}


	return 0;
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

static int compress_imagette(const struct cmp_cfg *cfg)
{
	int err;
	int stream_len = 0;
	size_t i;
	struct encoder_setupt setup;

	uint16_t *data_buf = cfg->input_buf;
	uint16_t *model_buf = cfg->model_buf;
	uint16_t model = 0;
	uint16_t *next_model_p = data_buf;
	uint16_t *up_model_buf = NULL;

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
		up_model_buf = cfg->icu_new_model_buf;
	}

	err = configure_encoder_setup(&setup, cfg->golomb_par, cfg->spill,
				      cfg->round, cfg->max_used_bits->nc_imagette, cfg);
	if (err)
		return -1;

	for (i = 0;; i++) {
		stream_len = encode_value(data_buf[i], model, stream_len, &setup);
		if (stream_len <= 0)
			break;

		if (up_model_buf)
			up_model_buf[i] = cmp_up_model(data_buf[i], model, cfg->model_value,
						       setup.lossy_par);
		if (i >= cfg->samples-1)
			break;

		model = next_model_p[i];
	}
	return stream_len;
}


/**
 * @brief compress the multi-entry packet header structure and sets the data,
 *	model and up_model pointers to the data after the header
 *
 * @param data			pointer to a pointer pointing to the data to be compressed
 * @param model			pointer to a pointer pointing to the model of the data
 * @param up_model		pointer to a pointer pointing to the updated model buffer
 * @param compressed_data	pointer to the compressed data buffer
 *
 * @returns the bit length of the bitstream on success; negative on error,
 *
 * @note the (void **) cast relies on all pointer types having the same internal
 *	representation which is common, but not universal; http://www.c-faq.com/ptrs/genericpp.html
 */

static int compress_multi_entry_hdr(void **data, void **model, void **up_model,
				    void *compressed_data)
{
	if (*up_model) {
		if (*data)
			memcpy(*up_model, *data, MULTI_ENTRY_HDR_SIZE);
		*up_model = (uint8_t *)*up_model + MULTI_ENTRY_HDR_SIZE;
	}

	if (*data) {
		if (compressed_data)
			memcpy(compressed_data, *data, MULTI_ENTRY_HDR_SIZE);
		*data = (uint8_t *)*data + MULTI_ENTRY_HDR_SIZE;
	}

	if (*model)
		*model = (uint8_t *)*model + MULTI_ENTRY_HDR_SIZE;

	return MULTI_ENTRY_HDR_SIZE * CHAR_BIT;
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

static int compress_s_fx(const struct cmp_cfg *cfg)
{
	int err;
	int stream_len = 0;
	size_t i;

	struct s_fx *data_buf = cfg->input_buf;
	struct s_fx *model_buf = cfg->model_buf;
	struct s_fx *up_model_buf = NULL;
	struct s_fx *next_model_p;
	struct s_fx model;
	struct encoder_setupt setup_exp_flag, setup_fx;

	if (model_mode_is_used(cfg->cmp_mode))
		up_model_buf = cfg->icu_new_model_buf;

	stream_len = compress_multi_entry_hdr((void **)&data_buf, (void **)&model_buf,
					      (void **)&up_model_buf, cfg->icu_output_buf);

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	err = configure_encoder_setup(&setup_exp_flag, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				      cfg->round, cfg->max_used_bits->s_exp_flags, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				      cfg->round, cfg->max_used_bits->s_fx, cfg);
	if (err)
		return -1;

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

static int compress_s_fx_efx(const struct cmp_cfg *cfg)
{
	int err;
	int stream_len = 0;
	size_t i;

	struct s_fx_efx *data_buf = cfg->input_buf;
	struct s_fx_efx *model_buf = cfg->model_buf;
	struct s_fx_efx *up_model_buf = NULL;
	struct s_fx_efx *next_model_p;
	struct s_fx_efx model;
	struct encoder_setupt setup_exp_flag, setup_fx, setup_efx;

	if (model_mode_is_used(cfg->cmp_mode))
		up_model_buf = cfg->icu_new_model_buf;

	stream_len = compress_multi_entry_hdr((void **)&data_buf, (void **)&model_buf,
					      (void **)&up_model_buf, cfg->icu_output_buf);

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	err = configure_encoder_setup(&setup_exp_flag, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				      cfg->round, cfg->max_used_bits->s_exp_flags, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				      cfg->round, cfg->max_used_bits->s_fx, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_efx, cfg->cmp_par_efx, cfg->spill_efx,
				      cfg->round, cfg->max_used_bits->s_efx, cfg);
	if (err)
		return -1;

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

static int compress_s_fx_ncob(const struct cmp_cfg *cfg)
{
	int err;
	int stream_len = 0;
	size_t i;

	struct s_fx_ncob *data_buf = cfg->input_buf;
	struct s_fx_ncob *model_buf = cfg->model_buf;
	struct s_fx_ncob *up_model_buf = NULL;
	struct s_fx_ncob *next_model_p;
	struct s_fx_ncob model;
	struct encoder_setupt setup_exp_flag, setup_fx, setup_ncob;

	if (model_mode_is_used(cfg->cmp_mode))
		up_model_buf = cfg->icu_new_model_buf;

	stream_len = compress_multi_entry_hdr((void **)&data_buf, (void **)&model_buf,
					      (void **)&up_model_buf, cfg->icu_output_buf);

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	err = configure_encoder_setup(&setup_exp_flag, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				      cfg->round, cfg->max_used_bits->s_exp_flags, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				      cfg->round, cfg->max_used_bits->s_fx, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_ncob, cfg->cmp_par_ncob, cfg->spill_ncob,
				      cfg->round, cfg->max_used_bits->s_ncob, cfg);
	if (err)
		return -1;

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

static int compress_s_fx_efx_ncob_ecob(const struct cmp_cfg *cfg)
{
	int err;
	int stream_len = 0;
	size_t i;

	struct s_fx_efx_ncob_ecob *data_buf = cfg->input_buf;
	struct s_fx_efx_ncob_ecob *model_buf = cfg->model_buf;
	struct s_fx_efx_ncob_ecob *up_model_buf = NULL;
	struct s_fx_efx_ncob_ecob *next_model_p;
	struct s_fx_efx_ncob_ecob model;
	struct encoder_setupt setup_exp_flag, setup_fx, setup_ncob, setup_efx,
			      setup_ecob;

	if (model_mode_is_used(cfg->cmp_mode))
		up_model_buf = cfg->icu_new_model_buf;

	stream_len = compress_multi_entry_hdr((void **)&data_buf, (void **)&model_buf,
					      (void **)&up_model_buf, cfg->icu_output_buf);

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	err = configure_encoder_setup(&setup_exp_flag, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				      cfg->round, cfg->max_used_bits->s_exp_flags, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				      cfg->round, cfg->max_used_bits->s_fx, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_ncob, cfg->cmp_par_ncob, cfg->spill_ncob,
				      cfg->round, cfg->max_used_bits->s_ncob, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_efx, cfg->cmp_par_efx, cfg->spill_efx,
				      cfg->round, cfg->max_used_bits->s_efx, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_ecob, cfg->cmp_par_ecob, cfg->spill_ecob,
				      cfg->round, cfg->max_used_bits->s_ecob, cfg);
	if (err)
		return -1;

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

static int compress_f_fx(const struct cmp_cfg *cfg)
{
	int err;
	int stream_len = 0;
	size_t i;

	struct f_fx *data_buf = cfg->input_buf;
	struct f_fx *model_buf = cfg->model_buf;
	struct f_fx *up_model_buf = NULL;
	struct f_fx *next_model_p;
	struct f_fx model;
	struct encoder_setupt setup_fx;

	if (model_mode_is_used(cfg->cmp_mode))
		up_model_buf = cfg->icu_new_model_buf;

	stream_len = compress_multi_entry_hdr((void **)&data_buf, (void **)&model_buf,
					      (void **)&up_model_buf, cfg->icu_output_buf);

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	err = configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				      cfg->round, cfg->max_used_bits->f_fx, cfg);
	if (err)
		return -1;

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

static int compress_f_fx_efx(const struct cmp_cfg *cfg)
{
	int err;
	int stream_len = 0;
	size_t i;

	struct f_fx_efx *data_buf = cfg->input_buf;
	struct f_fx_efx *model_buf = cfg->model_buf;
	struct f_fx_efx *up_model_buf = NULL;
	struct f_fx_efx *next_model_p;
	struct f_fx_efx model;
	struct encoder_setupt setup_fx, setup_efx;

	if (model_mode_is_used(cfg->cmp_mode))
		up_model_buf = cfg->icu_new_model_buf;

	stream_len = compress_multi_entry_hdr((void **)&data_buf, (void **)&model_buf,
					      (void **)&up_model_buf, cfg->icu_output_buf);

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	err = configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				      cfg->round, cfg->max_used_bits->f_fx, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_efx, cfg->cmp_par_efx, cfg->spill_efx,
				      cfg->round, cfg->max_used_bits->f_efx, cfg);
	if (err)
		return -1;

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

static int compress_f_fx_ncob(const struct cmp_cfg *cfg)
{
	int err;
	int stream_len = 0;
	size_t i;

	struct f_fx_ncob *data_buf = cfg->input_buf;
	struct f_fx_ncob *model_buf = cfg->model_buf;
	struct f_fx_ncob *up_model_buf = NULL;
	struct f_fx_ncob *next_model_p;
	struct f_fx_ncob model;
	struct encoder_setupt setup_fx, setup_ncob;

	if (model_mode_is_used(cfg->cmp_mode))
		up_model_buf = cfg->icu_new_model_buf;

	stream_len = compress_multi_entry_hdr((void **)&data_buf, (void **)&model_buf,
					      (void **)&up_model_buf, cfg->icu_output_buf);

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	err = configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				      cfg->round, cfg->max_used_bits->f_fx, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_ncob, cfg->cmp_par_ncob, cfg->spill_ncob,
				      cfg->round, cfg->max_used_bits->f_ncob, cfg);
	if (err)
		return -1;

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

static int compress_f_fx_efx_ncob_ecob(const struct cmp_cfg *cfg)
{
	int err;
	int stream_len = 0;
	size_t i;

	struct f_fx_efx_ncob_ecob *data_buf = cfg->input_buf;
	struct f_fx_efx_ncob_ecob *model_buf = cfg->model_buf;
	struct f_fx_efx_ncob_ecob *up_model_buf = NULL;
	struct f_fx_efx_ncob_ecob *next_model_p;
	struct f_fx_efx_ncob_ecob model;
	struct encoder_setupt setup_fx, setup_ncob, setup_efx, setup_ecob;

	if (model_mode_is_used(cfg->cmp_mode))
		up_model_buf = cfg->icu_new_model_buf;

	stream_len = compress_multi_entry_hdr((void **)&data_buf, (void **)&model_buf,
					      (void **)&up_model_buf, cfg->icu_output_buf);

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	err = configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				      cfg->round, cfg->max_used_bits->f_fx, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_ncob, cfg->cmp_par_ncob, cfg->spill_ncob,
				      cfg->round, cfg->max_used_bits->f_ncob, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_efx, cfg->cmp_par_efx, cfg->spill_efx,
				      cfg->round, cfg->max_used_bits->f_efx, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_ecob, cfg->cmp_par_ecob, cfg->spill_ecob,
				      cfg->round, cfg->max_used_bits->f_ecob, cfg);
	if (err)
		return -1;

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

static int compress_l_fx(const struct cmp_cfg *cfg)
{
	int err;
	int stream_len = 0;
	size_t i;

	struct l_fx *data_buf = cfg->input_buf;
	struct l_fx *model_buf = cfg->model_buf;
	struct l_fx *up_model_buf = NULL;
	struct l_fx *next_model_p;
	struct l_fx model;
	struct encoder_setupt setup_exp_flag, setup_fx, setup_fx_var;

	if (model_mode_is_used(cfg->cmp_mode))
		up_model_buf = cfg->icu_new_model_buf;

	stream_len = compress_multi_entry_hdr((void **)&data_buf, (void **)&model_buf,
					      (void **)&up_model_buf, cfg->icu_output_buf);

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	err = configure_encoder_setup(&setup_exp_flag, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				      cfg->round, cfg->max_used_bits->l_exp_flags, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				      cfg->round, cfg->max_used_bits->l_fx, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_fx_var, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				      cfg->round, cfg->max_used_bits->l_fx_variance, cfg);
	if (err)
		return -1;

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

static int compress_l_fx_efx(const struct cmp_cfg *cfg)
{
	int err;
	int stream_len = 0;
	size_t i;

	struct l_fx_efx *data_buf = cfg->input_buf;
	struct l_fx_efx *model_buf = cfg->model_buf;
	struct l_fx_efx *up_model_buf = NULL;
	struct l_fx_efx *next_model_p;
	struct l_fx_efx model;
	struct encoder_setupt setup_exp_flag, setup_fx, setup_efx, setup_fx_var;

	if (model_mode_is_used(cfg->cmp_mode))
		up_model_buf = cfg->icu_new_model_buf;

	stream_len = compress_multi_entry_hdr((void **)&data_buf, (void **)&model_buf,
					      (void **)&up_model_buf, cfg->icu_output_buf);

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	err = configure_encoder_setup(&setup_exp_flag, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				      cfg->round, cfg->max_used_bits->l_exp_flags, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				      cfg->round, cfg->max_used_bits->l_fx, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_efx, cfg->cmp_par_efx, cfg->spill_efx,
				      cfg->round, cfg->max_used_bits->l_efx, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_fx_var, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				      cfg->round, cfg->max_used_bits->l_fx_variance, cfg);
	if (err)
		return -1;

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

static int compress_l_fx_ncob(const struct cmp_cfg *cfg)
{
	int err;
	int stream_len = 0;
	size_t i;

	struct l_fx_ncob *data_buf = cfg->input_buf;
	struct l_fx_ncob *model_buf = cfg->model_buf;
	struct l_fx_ncob *up_model_buf = NULL;
	struct l_fx_ncob *next_model_p;
	struct l_fx_ncob model;
	struct encoder_setupt setup_exp_flag, setup_fx, setup_ncob,
			      setup_fx_var, setup_cob_var;

	if (model_mode_is_used(cfg->cmp_mode))
		up_model_buf = cfg->icu_new_model_buf;

	stream_len = compress_multi_entry_hdr((void **)&data_buf, (void **)&model_buf,
					      (void **)&up_model_buf, cfg->icu_output_buf);

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	err = configure_encoder_setup(&setup_exp_flag, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				      cfg->round, cfg->max_used_bits->l_exp_flags, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				      cfg->round, cfg->max_used_bits->l_fx, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_ncob, cfg->cmp_par_ncob, cfg->spill_ncob,
				      cfg->round, cfg->max_used_bits->l_ncob, cfg);
	if (err)
		return -1;
	/* we use compression parameter for both variance data fields */
	err = configure_encoder_setup(&setup_fx_var, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				      cfg->round, cfg->max_used_bits->l_fx_variance, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_cob_var, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				      cfg->round, cfg->max_used_bits->l_cob_variance, cfg);
	if (err)
		return -1;

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

static int compress_l_fx_efx_ncob_ecob(const struct cmp_cfg *cfg)
{
	int err;
	int stream_len = 0;
	size_t i;

	struct l_fx_efx_ncob_ecob *data_buf = cfg->input_buf;
	struct l_fx_efx_ncob_ecob *model_buf = cfg->model_buf;
	struct l_fx_efx_ncob_ecob *up_model_buf = NULL;
	struct l_fx_efx_ncob_ecob *next_model_p;
	struct l_fx_efx_ncob_ecob model;
	struct encoder_setupt setup_exp_flag, setup_fx, setup_ncob, setup_efx,
			      setup_ecob, setup_fx_var, setup_cob_var;

	if (model_mode_is_used(cfg->cmp_mode))
		up_model_buf = cfg->icu_new_model_buf;

	stream_len = compress_multi_entry_hdr((void **)&data_buf, (void **)&model_buf,
					      (void **)&up_model_buf, cfg->icu_output_buf);

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	err = configure_encoder_setup(&setup_exp_flag, cfg->cmp_par_exp_flags, cfg->spill_exp_flags,
				      cfg->round, cfg->max_used_bits->l_exp_flags, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_fx, cfg->cmp_par_fx, cfg->spill_fx,
				      cfg->round, cfg->max_used_bits->l_fx, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_ncob, cfg->cmp_par_ncob, cfg->spill_ncob,
				      cfg->round, cfg->max_used_bits->l_ncob, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_efx, cfg->cmp_par_efx, cfg->spill_efx,
				      cfg->round, cfg->max_used_bits->l_efx, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_ecob, cfg->cmp_par_ecob, cfg->spill_ecob,
				      cfg->round, cfg->max_used_bits->l_ecob, cfg);
	if (err)
		return -1;
	/* we use compression parameter for both variance data fields */
	err = configure_encoder_setup(&setup_fx_var, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				      cfg->round, cfg->max_used_bits->l_fx_variance, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_cob_var, cfg->cmp_par_fx_cob_variance, cfg->spill_fx_cob_variance,
				      cfg->round, cfg->max_used_bits->l_cob_variance, cfg);
	if (err)
		return -1;

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
 * @brief compress offset data from the normal cameras
 *
 * @param cfg	pointer to the compression configuration structure
 * @returns the bit length of the bitstream on success; negative on error,
 *	CMP_ERROR_SMALL_BUF if the bitstream buffer is too small to put the
 *	value in the bitstream
 */

static int compress_nc_offset(const struct cmp_cfg *cfg)
{
	int err;
	int stream_len = 0;
	size_t i;

	struct nc_offset *data_buf = cfg->input_buf;
	struct nc_offset *model_buf = cfg->model_buf;
	struct nc_offset *up_model_buf = NULL;
	struct nc_offset *next_model_p;
	struct nc_offset model;
	struct encoder_setupt setup_mean, setup_var;

	if (model_mode_is_used(cfg->cmp_mode))
		up_model_buf = cfg->icu_new_model_buf;

	stream_len = compress_multi_entry_hdr((void **)&data_buf, (void **)&model_buf,
					      (void **)&up_model_buf, cfg->icu_output_buf);

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	err = configure_encoder_setup(&setup_mean, cfg->cmp_par_mean, cfg->spill_mean,
				      cfg->round, cfg->max_used_bits->nc_offset_mean, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_var, cfg->cmp_par_variance, cfg->spill_variance,
				      cfg->round, cfg->max_used_bits->nc_offset_variance, cfg);
	if (err)
		return -1;

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
 * @brief compress background data from the normal cameras
 *
 * @param cfg	pointer to the compression configuration structure
 * @returns the bit length of the bitstream on success; negative on error,
 *	CMP_ERROR_SMALL_BUF if the bitstream buffer is too small to put the
 *	value in the bitstream
 */

static int compress_nc_background(const struct cmp_cfg *cfg)
{
	int err;
	int stream_len = 0;
	size_t i;

	struct nc_background *data_buf = cfg->input_buf;
	struct nc_background *model_buf = cfg->model_buf;
	struct nc_background *up_model_buf = NULL;
	struct nc_background *next_model_p;
	struct nc_background model;
	struct encoder_setupt setup_mean, setup_var, setup_pix;

	if (model_mode_is_used(cfg->cmp_mode))
		up_model_buf = cfg->icu_new_model_buf;

	stream_len = compress_multi_entry_hdr((void **)&data_buf, (void **)&model_buf,
					      (void **)&up_model_buf, cfg->icu_output_buf);

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	err = configure_encoder_setup(&setup_mean, cfg->cmp_par_mean, cfg->spill_mean,
				      cfg->round, cfg->max_used_bits->nc_background_mean, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_var, cfg->cmp_par_variance, cfg->spill_variance,
				      cfg->round, cfg->max_used_bits->nc_background_variance, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_pix, cfg->cmp_par_pixels_error, cfg->spill_pixels_error,
				      cfg->round, cfg->max_used_bits->nc_background_outlier_pixels, cfg);
	if (err)
		return -1;

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

static int compress_smearing(const struct cmp_cfg *cfg)
{
	int err;
	int stream_len = 0;
	size_t i;

	struct smearing *data_buf = cfg->input_buf;
	struct smearing *model_buf = cfg->model_buf;
	struct smearing *up_model_buf = NULL;
	struct smearing *next_model_p;
	struct smearing model;
	struct encoder_setupt setup_mean, setup_var_mean, setup_pix;

	if (model_mode_is_used(cfg->cmp_mode))
		up_model_buf = cfg->icu_new_model_buf;

	stream_len = compress_multi_entry_hdr((void **)&data_buf, (void **)&model_buf,
					      (void **)&up_model_buf, cfg->icu_output_buf);

	if (model_mode_is_used(cfg->cmp_mode)) {
		model = model_buf[0];
		next_model_p = &model_buf[1];
	} else {
		memset(&model, 0, sizeof(model));
		next_model_p = data_buf;
	}

	err = configure_encoder_setup(&setup_mean, cfg->cmp_par_mean, cfg->spill_mean,
				      cfg->round, cfg->max_used_bits->smearing_mean, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_var_mean, cfg->cmp_par_variance, cfg->spill_variance,
				      cfg->round, cfg->max_used_bits->smearing_variance_mean, cfg);
	if (err)
		return -1;
	err = configure_encoder_setup(&setup_pix, cfg->cmp_par_pixels_error, cfg->spill_pixels_error,
				      cfg->round, cfg->max_used_bits->smearing_outlier_pixels, cfg);
	if (err)
		return -1;

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
	output_buf_len_bits = cmp_buffer_length_to_bits(cfg->buffer_length, cfg->data_type);

	n_pad_bits = 32 - ((unsigned int)cmp_size & 0x1FU);
	if (n_pad_bits < 32) {
		int n_bits = put_n_bits32(0, n_pad_bits, cmp_size, cfg->icu_output_buf,
					  output_buf_len_bits);
		if (n_bits < 0)
			return n_bits;
	}

	return cmp_size;
}


/**
 * @brief change the endianness of the compressed data to big-endian
 *
 * @param cfg		pointer to the compression configuration structure
 * @param cmp_size	length of the bitstream in bits
 *
 * @returns 0 on success; non-zero on failure
 */

static int cmp_data_to_big_endian(const struct cmp_cfg *cfg, int cmp_size)
{
#if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
	size_t i;
	uint32_t *p;
	uint32_t s = (uint32_t)cmp_size;

	if (cmp_size < 0)
		return cmp_size;

	if (!cfg->icu_output_buf)
		return cmp_size;

	if (cfg->cmp_mode == CMP_MODE_RAW) {
		if (s & 0x7) /* size must be a multiple of 8 in RAW mode */
			return -1;
		if (cmp_input_big_to_cpu_endianness(cfg->icu_output_buf,
						    s/CHAR_BIT, cfg->data_type))
			cmp_size = -1;
	} else {
		if (rdcu_supported_data_type_is_used(cfg->data_type)) {
			p = cfg->icu_output_buf;
		} else {
			p = &cfg->icu_output_buf[MULTI_ENTRY_HDR_SIZE/sizeof(uint32_t)];
			s -= MULTI_ENTRY_HDR_SIZE * CHAR_BIT;
		}

		for (i = 0; i < cmp_bit_to_4byte(s)/sizeof(uint32_t); i++)
			cpu_to_be32s(&p[i]);
	}
#else
	/* do nothing data are already in big-endian */
	(void)cfg;
#endif /*__BYTE_ORDER__ */
	return cmp_size;
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
	int bitsize = 0;

	if (!cfg)
		return -1;

	if (cfg->samples == 0) /* nothing to compress we are done*/
		return 0;

	if (raw_mode_is_used(cfg->cmp_mode))
		if (cfg->samples > cfg->buffer_length)
			return CMP_ERROR_SMALL_BUF;

	if (cmp_cfg_icu_is_invalid(cfg))
		return -1;

	if (raw_mode_is_used(cfg->cmp_mode)) {
		uint32_t raw_size = cmp_cal_size_of_data(cfg->samples, cfg->data_type);

		if (cfg->icu_output_buf)
			memcpy(cfg->icu_output_buf, cfg->input_buf, raw_size);
		bitsize = (int)raw_size * CHAR_BIT; /* convert to bits */
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
			bitsize = compress_imagette(cfg);
			break;

		case DATA_TYPE_S_FX:
			bitsize = compress_s_fx(cfg);
			break;
		case DATA_TYPE_S_FX_EFX:
			bitsize = compress_s_fx_efx(cfg);
			break;
		case DATA_TYPE_S_FX_NCOB:
			bitsize = compress_s_fx_ncob(cfg);
			break;
		case DATA_TYPE_S_FX_EFX_NCOB_ECOB:
			bitsize = compress_s_fx_efx_ncob_ecob(cfg);
			break;

		case DATA_TYPE_F_FX:
			bitsize = compress_f_fx(cfg);
			break;
		case DATA_TYPE_F_FX_EFX:
			bitsize = compress_f_fx_efx(cfg);
			break;
		case DATA_TYPE_F_FX_NCOB:
			bitsize = compress_f_fx_ncob(cfg);
			break;
		case DATA_TYPE_F_FX_EFX_NCOB_ECOB:
			bitsize = compress_f_fx_efx_ncob_ecob(cfg);
			break;

		case DATA_TYPE_L_FX:
			bitsize = compress_l_fx(cfg);
			break;
		case DATA_TYPE_L_FX_EFX:
			bitsize = compress_l_fx_efx(cfg);
			break;
		case DATA_TYPE_L_FX_NCOB:
			bitsize = compress_l_fx_ncob(cfg);
			break;
		case DATA_TYPE_L_FX_EFX_NCOB_ECOB:
			bitsize = compress_l_fx_efx_ncob_ecob(cfg);
			break;

		case DATA_TYPE_OFFSET:
			bitsize = compress_nc_offset(cfg);
			break;
		case DATA_TYPE_BACKGROUND:
			bitsize = compress_nc_background(cfg);
			break;
		case DATA_TYPE_SMEARING:
			bitsize = compress_smearing(cfg);
			break;

		case DATA_TYPE_F_CAM_OFFSET:
		case DATA_TYPE_F_CAM_BACKGROUND:
		/* LCOV_EXCL_START */
		case DATA_TYPE_UNKNOWN:
		default:
			debug_print("Error: Data type not supported.\n");
			bitsize = -1;
		}
		/* LCOV_EXCL_STOP */
	}

	bitsize = pad_bitstream(cfg, bitsize);
	bitsize = cmp_data_to_big_endian(cfg, bitsize);

	return bitsize;
}
