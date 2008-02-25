/*
 *  libzvbi -- Useful macros
 *
 *  Copyright (C) 2002, 2003, 2004, 2007 Michael H. Schimek
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the 
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 *  Boston, MA  02110-1301  USA.
 */

/* $Id: macros.h,v 1.1.2.11 2008-02-25 21:01:41 mschimek Exp $ */

#ifndef __ZVBI3_MACROS_H__
#define __ZVBI3_MACROS_H__

#ifdef __cplusplus
#  define VBI3_BEGIN_DECLS extern "C" {
#  define VBI3_END_DECLS }
#else
#  define VBI3_BEGIN_DECLS
#  define VBI3_END_DECLS
#endif

VBI3_BEGIN_DECLS

/* Public */

#if __GNUC__ >= 4
#  define _vbi3_sentinel __attribute__ ((__sentinel__(0)))
#  define _vbi3_deprecated __attribute__ ((__deprecated__))
#else
#  define _vbi3_sentinel
#  define _vbi3_deprecated
#  define __restrict__
#endif

#if (__GNUC__ == 3 && __GNUC_MINOR__ >= 3) || __GNUC__ >= 4
#  define _vbi3_nonnull(params) __attribute__ ((__nonnull__ params))
#  define _vbi3_format(params) __attribute__ ((__format__ params))
#else
#  define _vbi3_nonnull(params)
#  define _vbi3_format(params)
#endif

#if __GNUC__ >= 3
#  define _vbi3_pure __attribute__ ((__pure__))
#  define _vbi3_alloc __attribute__ ((__malloc__))
#else
#  define _vbi3_pure
#  define _vbi3_alloc
#endif

#if __GNUC__ >= 2
#  define _vbi3_unused __attribute__ ((__unused__))
#  define _vbi3_const __attribute__ ((__const__))
#  define _vbi3_inline static __inline__
#else
#  define _vbi3_unused
#  define _vbi3_const
#  define _vbi3_inline static
#endif

/**
 * @ingroup Basic
 * @name Boolean type
 * @{
 */
#ifndef TRUE
#  define TRUE 1
#endif
#ifndef FALSE
#  define FALSE 0
#endif

typedef int vbi3_bool;
/** @} */

#ifndef NULL
#  ifdef __cplusplus
#    define NULL (0L)
#  else
#    define NULL ((void *) 0)
#  endif
#endif

/* XXX Document me - for variadic funcs. */
#define VBI3_END ((void *) 0)

#if 0
typedef void
vbi3_lock_fn			(void *			user_data);
typedef void
vbi3_unlock_fn			(void *			user_data);
#endif

/**
 * @ingroup Basic
 * @{
 */
typedef enum {
	/** External error causes, for example lack of memory. */
	VBI3_LOG_ERROR		= 1 << 3,

	/**
	 * Invalid parameters and similar problems which suggest
	 * a bug in the caller.
	 */
	VBI3_LOG_WARNING		= 1 << 4,

	/**
	 * Causes of possibly undesired results, for example when a
	 * data service cannot be decoded with the current video
	 * standard.
	 */
	VBI3_LOG_NOTICE		= 1 << 5,

	/** Progress messages. */
	VBI3_LOG_INFO		= 1 << 6,

	/** Information useful to debug the library. */
	VBI3_LOG_DEBUG		= 1 << 7,

	/** Driver responses (strace). Not implemented yet. */
	VBI3_LOG_DRIVER		= 1 << 8,

	/** More detailed debugging information. */
	VBI3_LOG_DEBUG2		= 1 << 9,
	VBI3_LOG_DEBUG3		= 1 << 10
} vbi3_log_mask;

typedef void
vbi3_log_fn			(vbi3_log_mask		level,
				 const char *		context,
				 const char *		message,
				 void *			user_data);

extern vbi3_log_fn		vbi3_log_on_stderr;
/** @} */

/* Private */

typedef struct {
	vbi3_log_fn *		fn;
	void *			user_data;
	vbi3_log_mask		mask;
} _vbi3_log_hook;

VBI3_END_DECLS

#endif /* __ZVBI3_MACROS_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
