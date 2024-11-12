/**
 * @file   cmp_error.h
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
 * @brief collection of macros to handling errors
 * This is heavy inspired by the error handling of zstd (lib/common/error_private.h) by
 * @author Yann Collet et al.
 * https://github.com/facebook/zstd/blob/dev/
 */

#ifndef CMP_ERROR_H
#define CMP_ERROR_H


#include <stdint.h>


/**
 * @brief add the CMP_ERROR prefix to the error name
 *
 * @param name the name to concatenate with cmp error prefix
 */

#define PREFIX_CMP_ERRROR(name) CMP_ERROR_##name


/**
 * @brief generate a return value of a error
 *
 * @param name	 error name without CMP_ERROR prefix to create an error code for
 */

#define CMP_ERROR(name) ((uint32_t)-PREFIX_CMP_ERRROR(name))


/**
 * @brief ignore this is an internal helper
 *
 * this is a helper function to help force c99-correctness during compilation
 * under strict compilation modes variadic macro arguments can't be empty
 * however variadic function arguments can be using a function therefore lets
 * us statically check that at least one string argument was passed
 * independent of the compilation flags
 */

static __inline void _force_has_format_string(const char *format, ...) {
	(void)format;
}


/**
 * @brief ignore this is an internal helper
 *
 * we want to force this function invocation to be syntactically correct but
 * we don't want to force runtime evaluation of its arguments
 */

__extension__
#define _FORCE_HAS_FORMAT_STRING(...)				\
	do {							\
		if (0) {					\
			_force_has_format_string(__VA_ARGS__);	\
		}						\
	} while (0)

#define ERR_QUOTE(str) #str


/**
 * @brief return the specified error if the condition evaluates to true
 *
 * @param cond	 the condition to evaluate
 * @param err	the error to return if the condition is true
 * @param ...	 additional arguments for error logging
 *
 * in debug modes (DEBUGLEVEL>=3) prints additional information
 */

__extension__
#define RETURN_ERROR_IF(cond, err, ...)								\
	do {											\
		if (cond) {									\
			debug_print_level(3, "%s:%d: Error: check %s failed, returning %s",	\
					  __FILE__, __LINE__, ERR_QUOTE(cond),			\
					  ERR_QUOTE(CMP_ERROR(err)));				\
			_FORCE_HAS_FORMAT_STRING(__VA_ARGS__);					\
			debug_print_level(3, "-> " __VA_ARGS__);				\
			return CMP_ERROR(err);							\
		}										\
	} while (0)


/**
 * @brief unconditionally return the specified error
 *
 * @param err	the error to return unconditionally
 * @param ...	additional arguments for error logging
 *
 * in debug modes (DEBUGLEVEL>=3) prints additional information
 */

__extension__
#define RETURN_ERROR(err, ...)									\
	do {											\
		debug_print_level(3, "%s:%d: Error: unconditional check failed, returning %s",	\
				  __FILE__, __LINE__, ERR_QUOTE(ERROR(err)));			\
		_FORCE_HAS_FORMAT_STRING(__VA_ARGS__);						\
		debug_print_level(3, "-> " __VA_ARGS__);					\
		return CMP_ERROR(err);								\
	} while(0)


/**
 * @brief if the provided expression evaluates to an error code returns that error code
 *
 * @param err	the error expression to evaluate
 * @param ...	additional arguments for error logging
 *
 * in debug modes (DEBUGLEVEL>=3) prints additional information
 */

__extension__
#define FORWARD_IF_ERROR(err, ...)							\
	do {										\
		uint32_t const err_code = (err);					\
		if (cmp_is_error(err_code)) {						\
			debug_print_level(3, "%s:%d: Error: forwarding error in %s: %s",\
					  __FILE__, __LINE__, ERR_QUOTE(err),		\
					  cmp_get_error_name(err_code));		\
			_FORCE_HAS_FORMAT_STRING(__VA_ARGS__);				\
			debug_print_level(3, "--> " __VA_ARGS__);			\
			return err_code;						\
		}									\
	} while(0)


#endif /* CMP_ERROR_H */
