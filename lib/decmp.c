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

#define CMP_ERROR_SAMLL_BUF -2


/* structure to hold a setup to encode a value */
typedef unsigned int (*decoder_ptr)(unsigned int, unsigned int, unsigned int, unsigned int *);
struct decoder_setup {
	/* generate_cw_f_pt generate_cw_f; /1* pointer to the code word generation function *1/ */
	decoder_ptr decode_cw_f;
	int (*encode_method_f)(uint32_t *decoded_value, int stream_pos,
			       const struct decoder_setup *setup); /* pointer to the decoding function */
	uint32_t *bitstream_adr; /* start address of the compressed data bitstream */
	uint32_t max_stream_len; /* maximum length of the bitstream/icu_output_buf in bits */
	uint32_t max_cw_len;
	uint32_t encoder_par1; /* encoding parameter 1 */
	uint32_t encoder_par2; /* encoding parameter 2 */
	uint32_t outlier_par; /* outlier parameter */
	uint32_t lossy_par; /* lossy compression parameter */
	uint32_t model_value; /* model value parameter */
	uint32_t max_data_bits; /* how many bits are needed to represent the highest possible value */
};


double get_compression_ratio(uint32_t samples, uint32_t cmp_size_bits,
			     enum cmp_data_type data_type)
{
	double orign_len_bits = (double)cmp_cal_size_of_data(samples, data_type) * CHAR_BIT;

	return orign_len_bits/(double)cmp_size_bits;
}


static unsigned int count_leading_ones(unsigned int value)
{
	unsigned int n_ones = 0; /* number of leading 1s */

	while (1) {
		unsigned int leading_bit;

		leading_bit = value & 0x80000000;
		if (!leading_bit)
			break;

		n_ones++;
		value <<= 1;
	}
	return n_ones;
}


static unsigned int rice_decoder(uint32_t code_word, unsigned int m,
				 unsigned int log2_m, unsigned int *decoded_cw)
{
	unsigned int q; /* quotient code */
	unsigned int ql; /* length of the quotient code */
	unsigned int r; /* remainder code */
	unsigned int rl; /* length of the remainder code */
	unsigned int cw_len; /* length of the decoded code word in bits */

	(void)m;

	q = count_leading_ones(code_word);
	ql = q + 1; /* Number of 1's + following 0 */

	rl = log2_m;

	cw_len = rl + ql;

	if (cw_len > 32) /* can only decode code words with maximum 32 bits */
		return 0;

	code_word = code_word << ql;  /* shift quotient code out */

	/* Right shifting an integer by a number of bits equal or greater than
	 * its size is undefined behavior
	 */
	if (rl == 0)
		r = 0;
	else
		r = code_word >> (32 - rl);

	*decoded_cw = (q << log2_m) + r;

	return cw_len;
}


static unsigned int golomb_decoder(unsigned int code_word, unsigned int m,
				   unsigned int log2_m, unsigned int
				   *decoded_cw)
{
	unsigned int q; /* quotient code */
	unsigned int r1; /* remainder code group 1 */
	unsigned int r2; /* remainder code group 2 */
	unsigned int r; /* remainder code */
	unsigned int rl; /* length of the remainder code */
	unsigned int cutoff; /* cutoff between group 1 and 2 */
	unsigned int cw_len; /* length of the decoded code word in bits */

	q = count_leading_ones(code_word);

	rl = log2_m + 1;
	code_word <<= (q+1);  /* shift quotient code out */

	r2 = code_word >> (32 - rl);
	r1 = r2 >> 1;

	cutoff = (1UL << rl) - m;

	if (r1 < cutoff) {
		cw_len = q + rl;
		r = r1;
	} else {
		cw_len = q + rl + 1;
		r = r2 - cutoff;
	}

	if (cw_len > 32)
		return 0;

	*decoded_cw = q*m + r;
	return cw_len;
}


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
 * @param p_value		pointer to the read value
 * @param n_bits		number of bits to read from the bitstream
 * @param bit_offset		bit index where the bits will be read, seen from
 *				the very beginning of the bitstream
 * @param bitstream_adr		this is the pointer to the beginning of the
 *				bitstream (can be NULL)
 * @param max_stream_len	maximum length of the bitstream in bits; is
 *				ignored if bitstream_adr is NULL
 *
 * @returns length in bits of the generated bitstream on success; returns
 *          negative in case of erroneous input; returns CMP_ERROR_SAMLL_BUF if
 *          the bitstream buffer is too small to read the value from the bitstream
 */

static int get_n_bits32(uint32_t *p_value, unsigned int n_bits, int bit_offset,
			uint32_t *bitstream_adr, unsigned int max_stream_len)
{
	const unsigned int *local_adr;
	unsigned int bitsLeft, bitsRight, localEndPos;
	unsigned int mask;
	int stream_len = (int)(n_bits + (unsigned int)bit_offset); /* overflow results in a negative return value */

	/*leave in case of erroneous input */
	if (bit_offset < 0)
		return -1;

	if (n_bits == 0)
		return stream_len;

	if (n_bits > 32)
		return -1;

	if (!bitstream_adr)
		return stream_len;

	if (!p_value)
		return stream_len;

	/* Check if bitstream buffer is large enough */
	if ((unsigned int)stream_len > max_stream_len) {
		debug_print("Error: Buffer overflow detected.\n");
		return CMP_ERROR_SAMLL_BUF;

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


static int decode_normal(uint32_t *decoded_value, int stream_pos, const struct decoder_setup *setup)
{
	uint32_t read_val = ~0U;
	int n_read_bits, cw_len, n_bits;

	if (stream_pos + setup->max_cw_len > setup->max_stream_len)   /* check buffer overflow */
		n_read_bits = setup->max_stream_len - stream_pos;
	else
		n_read_bits = setup->max_cw_len;
	if (n_read_bits >= 32 || n_read_bits == 0)
		return -1;

	n_bits = get_n_bits32(&read_val, n_read_bits, stream_pos,
				  setup->bitstream_adr, setup->max_stream_len);
	if (n_bits <= 0)
		return -1;

	read_val = read_val << (32 - n_read_bits);

	cw_len = setup->decode_cw_f(read_val, setup->encoder_par1, setup->encoder_par2, decoded_value);
	if (cw_len < 0)
		return -1;

	return stream_pos + cw_len;
}


static int decode_multi(uint32_t *decoded_value, int stream_pos,
			    const struct decoder_setup *setup)
{
	stream_pos = decode_normal(decoded_value, stream_pos, setup);
	if (stream_pos < 0)
		return stream_pos;

	if (*decoded_value >= setup->outlier_par) {
		/* escape symbol mechanism was used; read unencoded value */
		int n_bits;
		uint32_t unencoded_val = 0;
		unsigned int unencoded_len;

		unencoded_len = (*decoded_value - setup->outlier_par + 1) * 2;

		n_bits = get_n_bits32(&unencoded_val, unencoded_len, stream_pos,
				  setup->bitstream_adr, setup->max_stream_len);
		if (n_bits <= 0)
			return -1;

		*decoded_value = unencoded_val + setup->outlier_par;
		stream_pos += unencoded_len;
	}
	return stream_pos;
}


static int decode_zero(uint32_t *decoded_value, int stream_pos,
			   const struct decoder_setup *setup)
{
	stream_pos = decode_normal(decoded_value, stream_pos, setup);

	if (stream_pos <= 0)
		return stream_pos;

	if (*decoded_value > setup->outlier_par) /* consistency check */
		return -1;

	if (*decoded_value == 0) {/* escape symbol mechanism was used; read unencoded value */
		int n_bits;
		uint32_t unencoded_val = 0;

		n_bits = get_n_bits32(&unencoded_val, setup->max_data_bits, stream_pos,
				  setup->bitstream_adr, setup->max_stream_len);
		if (n_bits <= 0)
			return -1;
		if (unencoded_val < setup->outlier_par && unencoded_val != 0) /* consistency check */
			return -1;

		*decoded_value = unencoded_val;
		stream_pos += setup->max_data_bits;
	}
	(*decoded_value)--;
	if (*decoded_value == 0xFFFFFFFF) /* catch underflow */
		(*decoded_value) = (*decoded_value) >> (32-setup->max_data_bits);
	return stream_pos;
}


/**
 * @brief remap a unsigned value back to a signed value
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
	} else
		return value_to_unmap / 2;
}


static int decode_value(uint32_t *decoded_value, uint32_t model,
			int stream_pos, const struct decoder_setup *setup)
{
	uint32_t mask = (~0U >> (32 - setup->max_data_bits)); /* mask the used bits */

	stream_pos = setup->encode_method_f(decoded_value, stream_pos, setup);
	if (stream_pos <= 0)
		return stream_pos;

	*decoded_value = re_map_to_pos(*decoded_value); //, setup->max_used_bits);

	*decoded_value += round_fwd(model, setup->lossy_par);

	*decoded_value &= mask;

	*decoded_value = round_inv(*decoded_value, setup->lossy_par);

	return stream_pos;
}


static int configure_decoder_setup(struct decoder_setup *setup,
				   uint32_t cmp_par, uint32_t spill,
				   uint32_t lossy_par, uint32_t max_data_bits,
				   const struct cmp_cfg *cfg)
{
	if (multi_escape_mech_is_used(cfg->cmp_mode))
		setup->encode_method_f = &decode_multi;
	else if (zero_escape_mech_is_used(cfg->cmp_mode))
		setup->encode_method_f = &decode_zero;
	else {
		debug_print("Error: Compression mode not supported.\n");
		return -1;
	}

	setup->bitstream_adr = cfg->icu_output_buf; /* start address of the compressed data bitstream */
	if (cfg->buffer_length & 0x3) {
		debug_print("Error: The length of the compressed data is not a multiple of 4 bytes.");
		return -1;
	}
	setup->max_stream_len = (cfg->buffer_length) * CHAR_BIT;  /* maximum length of the bitstream/icu_output_buf in bits */
	if (rdcu_supported_data_type_is_used(cfg->data_type))
		setup->max_cw_len = 16;
	else
		setup->max_cw_len = 32;
	setup->encoder_par1 = cmp_par; /* encoding parameter 1 */
	setup->encoder_par2 = ilog_2(cmp_par); /* encoding parameter 2 */
	setup->outlier_par = spill; /* outlier parameter */
	setup->lossy_par = lossy_par; /* lossy compression parameter */
	setup->model_value = cfg->model_value; /* model value parameter */
	setup->max_data_bits = max_data_bits; /* how many bits are needed to represent the highest possible value */
	setup->decode_cw_f = select_decoder(cmp_par);

	return 0;
}


static int decompress_imagette(struct cmp_cfg *cfg)
{
	size_t i;
	int stream_pos = 0;
	struct decoder_setup setup;
	uint16_t *decompressed_data = cfg->input_buf;
	uint16_t *model_buf = cfg->model_buf;
	uint16_t *up_model_buf = cfg->icu_new_model_buf;
	uint32_t decoded_value = 0;
	uint16_t model;
	int err;

	err = configure_decoder_setup(&setup, cfg->golomb_par, cfg->spill,
				      cfg->round, 16, cfg);
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

static int decompressed_data_internal(struct cmp_cfg *cfg)
{
	int data_size, strem_len_bit = -1;

	if (!cfg)
		return 0; /* or -1? */
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
			strem_len_bit = decompress_imagette(cfg);
			break;
		default:
			strem_len_bit = -1;
			debug_print("Error: Compressed data type not supported.\n");
			break;
		}

	}
	/* TODO: is this usefull? if (strem_len_bit != data_size * CHAR_BIT) { */
	if (strem_len_bit <= 0)
		return -1;


	return data_size;
}


int decompress_cmp_entiy(struct cmp_entity *ent, void *model_buf,
			 void *up_model_buf, void *decompressed_data)
{
	int err;
	struct cmp_cfg cfg = {0};

	cfg.model_buf = model_buf;
	cfg.icu_new_model_buf = up_model_buf;
	cfg.input_buf = decompressed_data;

	if (!ent)
		return -1;

	err = cmp_ent_read_header(ent, &cfg);
	if (err)
		return -1;

	return decompressed_data_internal(&cfg);
}


/* model buffer is overwritten with updated model */

int decompress_data(uint32_t *compressed_data, void *de_model_buf,
		    const struct cmp_info *info, void *decompressed_data)
{
	int size_decomp_data;
	struct cmp_cfg cfg = {0};

	if (!compressed_data)
		return -1;

	if (!info)
		return -1;

	if (info->cmp_err)
		return -1;

	if (model_mode_is_used(info->cmp_mode_used))
		if (!de_model_buf)
			return -1;
	/* TODO: add ohter modes */

	if (!decompressed_data)
		return -1;

	/* cfg.data_type = info->data_type_used; */
	cfg.cmp_mode = info->cmp_mode_used;
	cfg.model_value = info->model_value_used;
	cfg.round = info->round_used;
	cfg.spill = info->spill_used;
	cfg.golomb_par = info->golomb_par_used;
	cfg.samples = info->samples_used;
	cfg.icu_output_buf = compressed_data;
	cfg.buffer_length = cmp_bit_to_4byte(info->cmp_size);
	cfg.input_buf = decompressed_data;
	cfg.model_buf = de_model_buf;
	size_decomp_data = decompressed_data_internal(&cfg);
	if (size_decomp_data <= 0)
		return -1;
	else
		return 0;


}
