/**
 * @file   read_bitstream.h
 * @author Dominik Loidolt (dominik.loidolt@univie.ac.at)
 * @date   2023
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
 * @brief this library handles the reading from an MSB-first bitstream
 *
 * This API consists of small unitary functions, which must be inlined for best performance.
 * Since link-time-optimization is not available for all compilers, these
 * functions are defined into a .h to be included.
 *
 * Start by invoking bit_init_decoder(). A chunk of the bitstream is then stored
 * into a local register. The local register size is 64 bits.  You can then retrieve
 * bit-fields stored into the local register. The local register is explicitly
 * reloaded from the memory with the bit_refill() function.
 * A reload guarantees a minimum of 57 bits in the local register if the
 * returned status is BIT_UNFINISHED.
 * Otherwise, it can be less than that, so proceed accordingly.
 * Checking if bit_decoder has reached its end can be performed with bit_end_of_stream().
 *
 * This is based on the bitstream part of the FiniteStateEntropy library, see:
 * https://github.com/Cyan4973/FiniteStateEntropy/blob/dev/lib/bitstream.h
 * by @author Yann Collet
 * As well as some ideas from this blog post:
 * https://fgiesen.wordpress.com/2018/02/20/reading-bits-in-far-too-many-ways-part-2/
 * by @author Fabian Giesen
 */

#ifndef READ_BITSTREAM_H
#define READ_BITSTREAM_H

#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>

#include "../common/byteorder.h"
#include "../common/compiler.h"



/**
 * @brief bitstream decoder context type
 */

struct bit_decoder {
	uint64_t bit_container;
	unsigned int bits_consumed;
	const uint8_t *cursor;
	const uint8_t *limit_ptr;
};


/**
 * @brief bitstream decoder status, return type of bit_refill()
 */

enum bit_status {BIT_OVERFLOW, BIT_END_OF_BUFFER, BIT_ALL_READ_IN, BIT_UNFINISHED};


/*
 * bitstream decoder API
 */

static __inline size_t bit_init_decoder(struct bit_decoder *dec, const void *buf, size_t buf_size);
static __inline uint64_t bit_peek_bits(const struct bit_decoder *dec, unsigned int nb_bits);
static __inline void bit_consume_bits(struct bit_decoder *dec, unsigned int nb_bits);
static __inline uint64_t bit_read_bits(struct bit_decoder *dec, unsigned int nb_bits);
static __inline uint32_t bit_read_bits32(struct bit_decoder *dec, unsigned int nb_bits);
static __inline uint32_t bit_read_bits32_sub_1(struct bit_decoder *dec, unsigned int nb_bits);
static __inline unsigned int bit_end_of_stream(const struct bit_decoder *dec);
static __inline int bit_refill(struct bit_decoder *dec);


/*
 * internal implementation
 */

static const uint32_t BIT_MASK[] = {
	0,          1,          3,         7,         0xF,       0x1F,
	0x3F,       0x7F,       0xFF,      0x1FF,     0x3FF,     0x7FF,
	0xFFF,      0x1FFF,     0x3FFF,    0x7FFF,    0xFFFF,    0x1FFFF,
	0x3FFFF,    0x7FFFF,    0xFFFFF,   0x1FFFFF,  0x3FFFFF,  0x7FFFFF,
	0xFFFFFF,   0x1FFFFFF,  0x3FFFFFF, 0x7FFFFFF, 0xFFFFFFF, 0x1FFFFFFF,
	0x3FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF}; /* up to 32 bits */
#define BIT_MASK_SIZE ARRAY_SIZE(BIT_mask)


/**
 * @brief read 8 bytes of big-endian data from an unaligned address
 *
 * @param ptr pointer to the data (can be unaligned)
 *
 * @returns 64 bit data at mem_ptr address in big-endian byte order
 */

static __inline uint64_t bit_read_unaligned_64be(const void *ptr)
{
	typedef __attribute__((aligned(1))) uint64_t unalign64;
	return cpu_to_be64(*(const unalign64*)ptr);
}


/**
 * @brief initialize a bit_decoder
 *
 * @param dec		a pointer to an already allocated bit_decoder structure
 * @param buf		start address of the bitstream buffer
 * @param buf_size	size of the bitstream in bytes
 *
 * @returns size of stream (== src_size), or zero if a problem is detected
 */

static __inline size_t bit_init_decoder(struct bit_decoder *dec, const void *buf,
					size_t buf_size)
{
	assert(dec != NULL);
	assert(buf != NULL);

	dec->cursor = (const uint8_t *)buf;

	if (buf_size < 1) {
		dec->bits_consumed = sizeof(dec->bit_container)*8;
		dec->bit_container = 0;
		dec->limit_ptr = (const uint8_t *)buf;
		return 0;
	}

	if (buf_size >= sizeof(dec->bit_container)) {  /* normal case */
		dec->bits_consumed = 0;
		dec->bit_container = bit_read_unaligned_64be(dec->cursor);
		dec->limit_ptr = dec->cursor + buf_size - sizeof(dec->bit_container);
	} else {
		const uint8_t *ui8_p = (const uint8_t *)(buf);

		dec->bits_consumed = (unsigned int)(sizeof(dec->bit_container) - buf_size) * 8;

		dec->bit_container = (uint64_t)ui8_p[0] << 56;
		switch (buf_size) {
		case 7:
			dec->bit_container += (uint64_t)ui8_p[6] <<  8;
			/* fall-through */
		case 6:
			dec->bit_container += (uint64_t)ui8_p[5] << 16;
			/* fall-through */
		case 5:
			dec->bit_container += (uint64_t)ui8_p[4] << 24;
			/* fall-through */
		case 4:
			dec->bit_container += (uint64_t)ui8_p[3] << 32;
			/* fall-through */
		case 3:
			dec->bit_container += (uint64_t)ui8_p[2] << 40;
			/* fall-through */
		case 2:
			dec->bit_container += (uint64_t)ui8_p[1] << 48;
			/* fall-through */
		default:
			break;
		}
		dec->bit_container >>= dec->bits_consumed;

		dec->limit_ptr = dec->cursor;
	}
	return buf_size;
}


/**
 * @brief provides next n bits from local register; local register is not modified
 *
 * @param dec		a pointer to a bit_decoder context
 * @param nb_bits	number of bits to look; only works if 1 <= nb_bits <= 56
 *
 * @returns extracted value
 */

static __inline uint64_t bit_peek_bits(const struct bit_decoder *dec, unsigned int nb_bits)
{
	/* mask for the shift value register to prevent undefined behaviour */
	uint32_t const reg_mask = 0x3F;

	assert(nb_bits >= 1 && nb_bits <= (64 - 7));
	/* why -7: worst case refill can only put 56 bit in the bit_container */

	/* shift out consumed bits; return the top nb_bits bits we want to peek */
	return (dec->bit_container << (dec->bits_consumed & reg_mask)) >> (64-nb_bits);
}


/**
 * @brief count the leading ones in the local register; local register is not modified
 *
 * @param dec	pointer to a bit_decoder context
 *
 * @returns number of leading ones;
 */

static __inline unsigned int bit_peek_leading_ones(const struct bit_decoder *dec)
{
	/* mask for the shift value register to prevent undefined behaviour */
	uint32_t const reg_mask = 0x3F;
	/* shift out the bits we've already consumed */
	uint64_t const remaining_flip = ~(dec->bit_container << (dec->bits_consumed & reg_mask));

	/* clzll(0) is undefined behaviour */
	return remaining_flip ? (unsigned int)__builtin_clzll(remaining_flip) :
		sizeof(dec->bit_container)*8;
}


/**
 * @brief mark next n bits in the local register as consumed
 *
 * @param dec		pointer to a bit_decoder context
 * @param nb_bits	number of bits to skip
 */

static __inline void bit_consume_bits(struct bit_decoder *dec, unsigned int nb_bits)
{
	dec->bits_consumed += nb_bits;
}


/**
 * @brief read and consume next n bits from the local register
 * @warning do not read more bits than the local register has unconsumed bits.
 *	If you do this, the bit_refill function will return the BIT_OVERFLOW
 *	status the next time the register is refilled.
 *
 * @param dec		pointer to a bit_decoder context
 * @param nb_bits	number of bits to look; only works if 1 <= nb_bits <= 56
 *
 * @returns extracted value
 */

static __inline uint64_t bit_read_bits(struct bit_decoder *dec, unsigned int nb_bits)
{
	uint64_t const read_bits = bit_peek_bits(dec, nb_bits);

	bit_consume_bits(dec, nb_bits);
	return read_bits;
}


/**
 * @brief same as bit_read_bits32() but only returns 32 bit
 * @warning do not read more bits than the local register has unconsumed bits.
 *	If you do this, the bit_refill function will return the BIT_OVERFLOW
 *	status the next time the register is refilled.
 *
 * @param dec		pointer to a bit_decoder context
 * @param nb_bits	number of bits to read; only works if 1 <= nb_bits <= 32
 *
 * @returns extracted 32 bit value
 */

static __inline uint32_t bit_read_bits32(struct bit_decoder *dec, unsigned int nb_bits)
{
	assert(nb_bits <= 32);

	return (uint32_t)bit_read_bits(dec, nb_bits);
}


/**
 * @brief same as bit_read_bits32() but subtract 1 from the extracted value
 * @warning do not read more bits than the local register has unconsumed bits.
 *	If you do this, the bit_refill function will return the BIT_OVERFLOW
 *	status the next time the register is refilled.
 *
 * @param dec		pointer to a bit_decoder context
 * @param nb_bits	number of bits to read; only works if nb_bits <= 32
 *
 * @returns extracted 32 bit value minus 1
 *
 * @note The difference to the bit_read_bits32() function with subtraction is
 *	that the subtracted value is masked with nb_bits. E.g. if you read 4
 *	bits from the bitstream and get 0 and then subtract 1, you get 0xFF
 *	instead of 0xFFFFFFFF
 */

static __inline uint32_t bit_read_bits32_sub_1(struct bit_decoder *dec, unsigned int nb_bits)
{
	/* mask for the shift value register to prevent undefined behaviour */
	uint32_t const reg_mask = sizeof(dec->bit_container)*8 - 1;
	unsigned int const shift_bits = (64 - dec->bits_consumed - nb_bits) & reg_mask;
	uint32_t bits_unmask;

	assert(nb_bits <= 32);

	bits_unmask = (uint32_t)(dec->bit_container >> shift_bits);
	bit_consume_bits(dec, nb_bits);
	return (bits_unmask - 1) & BIT_MASK[nb_bits];
}


/**
 * @brief refill the local register from the buffer previously set in
 *	bit_init_decoder()
 *
 * @param dec	a bitstream decoding context
 *
 * @note this function is safe, it guarantees that it does not read beyond
 *	initialize buffer
 *
 * @returns the status of bit_decoder internal register;
 *	BIT_UNFINISHED: internal register is filled with at least _57-bits_
 *	BIT_END_OF_BUFFER: reached the end of the buffer, only some bits are left in the bitstream
 *	BIT_ALL_READ_IN: _all_ bits of the buffer have been consumed
 *	BIT_OVERFLOW: more bits have been consumed than contained in the local register
 */

static __inline int bit_refill(struct bit_decoder *dec)
{
	unsigned int const bytes_consumed = dec->bits_consumed >> 3;

	if (unlikely(dec->bits_consumed > sizeof(dec->bit_container)*8))
		return BIT_OVERFLOW;

	if (dec->cursor + bytes_consumed < dec->limit_ptr) {
		/* Advance the pointer by the number of full bytes we consumed */
		dec->cursor += bytes_consumed;
		/* Refill the bit container */
		dec->bit_container = bit_read_unaligned_64be(dec->cursor);
		/* The number of bits that we have already consumed in the
		 * current byte, excluding the bits that formed a complete byte
		 * and were already processed.
		 */
		dec->bits_consumed &= 0x7;
		return BIT_UNFINISHED;
	}

	if (dec->cursor == dec->limit_ptr) {
		if (dec->bits_consumed == sizeof(dec->bit_container)*8)
			return BIT_ALL_READ_IN;
		return BIT_END_OF_BUFFER;
	}

	/* limit_ptr < (cursor + bytes_consumed) < end */
	dec->bits_consumed -= (dec->limit_ptr - dec->cursor)*8;
	dec->cursor = dec->limit_ptr;
	dec->bit_container = bit_read_unaligned_64be(dec->cursor);

	return BIT_END_OF_BUFFER;
}


/**
 * @brief Check if the end of the bitstream has been reached
 *
 * @param dec	a bitstream decoding context
 *
 * @returns 1 if bit_decoder has _exactly_ reached its end (all bits consumed)
 */

static __inline unsigned int bit_end_of_stream(const struct bit_decoder *dec)
{
	return ((dec->cursor == dec->limit_ptr) &&
		(dec->bits_consumed == sizeof(dec->bit_container)*8));
}

#endif /* READ_BITSTREAM_H */
