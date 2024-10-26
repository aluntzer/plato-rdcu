/**
 * @file   cmp_debug.c
 * @author Dominik Loidolt (dominik.loidolt@univie.ac.at)
 * @date   2024
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

#if (DEBUGLEVEL > 0)

#ifndef ICU_ASW
#  include <stdio.h>
#endif
#include <stddef.h>
#include <string.h>
#include <stdarg.h>

#include "cmp_debug.h"
#include "compiler.h"
#include "vsnprintf.h"


/**
 * @brief outputs a debug string
 *
 * This function outputs a string for debugging purposes
 *
 * @param str The string to output
 */

static void cmp_debug_puts(const char *str)
{
#ifdef ICU_ASW
	/* XXX adapt it to your needs */
	/* asw_puts(str); */
	(void)str;
#else
	(void)fputs(str, stderr);
	(void)fputs("\n", stderr);
#endif
}


/**
 * @brief implements debug printing
 *
 * This function formats a string and prints it for debugging. It uses a static
 * buffer to format the string
 *
 * @param fmt	pointer to a null-terminated byte string specifying how to
 *		interpret the data
 * @param ...	arguments specifying data to print
 */

void cmp_debug_print_impl(const char *fmt, ...)
{
	static char print_buffer[PRINT_BUFFER_SIZE];
	int len;
	va_list args;

	va_start(args, fmt);
	len = my_vsnprintf(print_buffer, sizeof(print_buffer)-1, fmt, args);
	va_end(args);

	if (len < 0) {
		const char str[] = "my_snprintf is broken";

		compile_time_assert(sizeof(str) <= sizeof(print_buffer), CMP_DEBUG_PRINT_BUFFER_SIZE_TO_SMALL);
		memcpy(print_buffer, str, sizeof(str));
	}
	if ((size_t)len >= sizeof(print_buffer)-1) {
		const char str[] =  "cmp_debug print_buffer too small";

		compile_time_assert(sizeof(str) <= sizeof(print_buffer), CMP_DEBUG_PRINT_BUFFER_SIZE_TO_SMALL);
		memcpy(print_buffer, str, sizeof(str));
	}

	cmp_debug_puts(print_buffer);
}

#else
/* prevent warning: ISO C requires a translation unit to contain at least one declaration [-Wempty-translation-unit] */
extern int make_iso_compilers_happy;
#endif /* (DEBUGLEVEL > 0) */
