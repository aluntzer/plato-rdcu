/**
 * @file   compiler.h
 * @author Armin Luntzer (armin.luntzer@univie.ac.at)
 * @date   2015
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
 * @brief a collection of preprocessor macros
 */


#ifndef COMPILER_H
#define COMPILER_H


/* Derived from Linux "Features Test Macro" header Convenience macros to test
 * the versions of gcc (or a compatible compiler).
 * Use them like this:
 *  #if GNUC_PREREQ (2,8)
 *   ... code requiring gcc 2.8 or later ...
 *  #endif
*/

#if defined(__GNUC__) && defined(__GNUC_MINOR__)
# define GNUC_PREREQ(maj, min) \
	((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
#else
 #define GNUC_PREREQ(maj, min) 0
#endif


/**
 * same with the stuff below
 */

#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)

/**
 *
 * ARRAY_SIZE - get the number of elements in a visible array
 * @param x the array whose size you want
 *
 * This does not work on pointers, or arrays declared as [], or
 * function parameters.  With correct compiler support, such usage
 * will cause a build error
 */

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

#define bitsizeof(x)  (CHAR_BIT * sizeof(x))

#define maximum_signed_value_of_type(a) \
    (INTMAX_MAX >> (bitsizeof(intmax_t) - bitsizeof(a)))

#define maximum_unsigned_value_of_type(a) \
    (UINTMAX_MAX >> (bitsizeof(uintmax_t) - bitsizeof(a)))

/*
 * Signed integer overflow is undefined in C, so here's a helper macro
 * to detect if the sum of two integers will overflow.
 *
 * Requires: a >= 0, typeof(a) equals typeof(b)
 */
#define signed_add_overflows(a, b) \
    ((b) > maximum_signed_value_of_type(a) - (a))

#define unsigned_add_overflows(a, b) \
    ((b) > maximum_unsigned_value_of_type(a) - (a))


#define __get_unaligned_t(type, ptr) __extension__ ({						\
	const struct { type x; } __attribute__((packed)) *__pptr = (__typeof__(__pptr))(ptr);	\
	__pptr->x;										\
})

#define __put_unaligned_t(type, val, ptr) do {							\
	struct { type x; } __attribute__((packed)) *__pptr = (__typeof__(__pptr))(ptr);		\
	__pptr->x = (val);									\
} while (0)


/**
 * @brief read from an unaligned address
 *
 * @param ptr	pointer to the address to read from (can be unaligned)
 *
 * @return the value read from the (unaligned) address in system endianness
 * @note the size of the value is determined by the pointer type
 */

#define get_unaligned(ptr) __get_unaligned_t(__typeof__(*(ptr)), (ptr))


/**
 * @brief write to an unaligned address
 *
 * @param val	the value to write
 * @param ptr	pointer to the address to write to (can be unaligned)
 *
 * @note the size of the value is determined by the pointer type
 */

#define put_unaligned(val, ptr) __put_unaligned_t(__typeof__(*(ptr)), (val), (ptr))


/**
 * @brief mark a variable as unused to suppress compiler warnings.
 *
 * This macro is used to indicate that a variable is intentionally left unused
 * in the code. It helps suppress compiler warnings about unused variables.
 * It also tries to prevent the actual use of the "unused" variables.
 */

#if GNUC_PREREQ(4, 5) || defined(__clang__)
#define UNUSED __attribute__((unused)) \
	__attribute__((deprecated ("parameter declared as UNUSED")))
#elif defined(__GNUC__)
#define UNUSED __attribute__((unused)) \
	__attribute__((deprecated))
#else
#define UNUSED
#endif


/**
 * @brief mark a variable as potentially unused to suppress compiler warnings.
 *
 * This macro is used to indicate that a variable may be intentionally left unused
 * in the code. It helps suppress compiler warnings about unused variables.
 * It does *not* protect against the actual use of the "unused" variables.
 */

#if (DEBUGLEVEL == 0)
#define MAYBE_UNUSED __attribute__((unused))
#else
#define MAYBE_UNUSED
#endif


/**
 * Compile time check usable outside of function scope.
 * Stolen from Linux (hpi_internal.h)
 */
#define compile_time_assert(cond, msg) UNUSED typedef char ASSERT_##msg[(cond) ? 1 : -1]


#endif /* COMPILER_H */
