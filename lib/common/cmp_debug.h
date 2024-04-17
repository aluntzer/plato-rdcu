/**
 * @file   cmp_debug.h
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
 * @brief compression/decompression debugging printing functions
 */

#ifndef CMP_DEBUG_H
#define CMP_DEBUG_H


/* DEBUGLEVEL should be externally defined, usually via the compiler command
 * line.  Its value must be a numeric value. */
#ifndef DEBUGLEVEL
#  ifdef DEBUG
#    define DEBUGLEVEL 1
#  else
#    define DEBUGLEVEL 0
#  endif
#endif


#define PRINT_BUFFER_SIZE 256

__extension__
#if (DEBUGLEVEL > 0)
#  define debug_print(...) cmp_debug_print_impl(__VA_ARGS__)
void cmp_debug_print_impl(const char *fmt, ...);
#else
#  define debug_print(...) do {} while (0)
#endif

__extension__
#define debug_print_level(level, ...)			\
	do {						\
		if (level <= DEBUGLEVEL)		\
			debug_print(__VA_ARGS__);	\
	} while (0)



#endif /* CMP_DEBUG_H */
