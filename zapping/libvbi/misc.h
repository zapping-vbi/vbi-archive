/*
 *  libzvbi - Miscellaneous cows and chickens
 *
 *  Copyright (C) 2002-2006 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id: misc.h,v 1.12 2007-08-30 12:27:18 mschimek Exp $ */

#ifndef MISC_H
#define MISC_H

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

#include "macros.h"

#define N_ELEMENTS(array) (sizeof (array) / sizeof (*(array)))

#ifdef __GNUC__

#if __GNUC__ < 3
/* Expect expression usually true/false, schedule accordingly. */
#  define likely(expr) (expr)
#  define unlikely(expr) (expr)
#else
#  define likely(expr) __builtin_expect(expr, 1)
#  define unlikely(expr) __builtin_expect(expr, 0)
#endif

#undef __i386__
#undef __i686__
#if #cpu (i386)
#  define __i386__ 1
#endif
#if #cpu (i686)
#  define __i686__ 1
#endif

/* &x == PARENT (&x.tm_min, struct tm, tm_min),
   safer than &x == (struct tm *) &x.tm_min. A NULL _ptr is safe and
   will return NULL, not -offsetof(_member). */
#undef PARENT
#define PARENT(_ptr, _type, _member) ({					\
	__typeof__ (&((_type *) 0)->_member) _p = (_ptr);		\
	(_p != 0) ? (_type *)(((char *) _p) - offsetof (_type,		\
	  _member)) : (_type *) 0;					\
})

/* Like PARENT(), to be used with const _ptr. */
#define CONST_PARENT(_ptr, _type, _member) ({				\
	__typeof__ (&((const _type *) 0)->_member) _p = (_ptr);		\
	(_p != 0) ? (const _type *)(((const char *) _p) - offsetof	\
	 (const _type, _member)) : (const _type *) 0;			\
})

/* Note the following macros have no side effects only when you
   compile with GCC, so don't expect this. */

/* Absolute value of int, long or long long without a branch.
   Note ABS (INT_MIN) -> INT_MAX + 1. */
#undef ABS
#define ABS(n) ({							\
	register __typeof__ (n) _n = (n), _t = _n;			\
	if (-1 == (-1 >> 1)) { /* do we have signed shifts? */		\
		_t >>= sizeof (_t) * 8 - 1;				\
		_n ^= _t;						\
		_n -= _t;						\
	} else if (_n < 0) { /* also warns if n is unsigned */		\
		_n = -_n;						\
	}								\
	/* return */ _n;						\
})

#undef MIN
#define MIN(x, y) ({							\
	__typeof__ (x) _x = (x);					\
	__typeof__ (y) _y = (y);					\
	(void)(&_x == &_y); /* warn if types do not match */		\
	/* return */ (_x < _y) ? _x : _y;				\
})

#undef MAX
#define MAX(x, y) ({							\
	__typeof__ (x) _x = (x);					\
	__typeof__ (y) _y = (y);					\
	(void)(&_x == &_y); /* warn if types do not match */		\
	/* return */ (_x > _y) ? _x : _y;				\
})

/* Note other compilers may swap only int, long or pointer. */
#undef SWAP
#define SWAP(x, y)							\
do {									\
	__typeof__ (x) _x = x;						\
	x = y;								\
	y = _x;								\
} while (0)

#undef SATURATE
#ifdef __i686__ /* has conditional move */
#define SATURATE(n, min, max) ({					\
	__typeof__ (n) _n = (n);					\
	__typeof__ (n) _min = (min);					\
	__typeof__ (n) _max = (max);					\
	(void)(&_n == &_min); /* warn if types do not match */		\
	(void)(&_n == &_max);						\
	if (_n < _min)							\
		_n = _min;						\
	if (_n > _max)							\
		_n = _max;						\
	/* return */ _n;						\
})
#else
#define SATURATE(n, min, max) ({					\
	__typeof__ (n) _n = (n);					\
	__typeof__ (n) _min = (min);					\
	__typeof__ (n) _max = (max);					\
	(void)(&_n == &_min); /* warn if types do not match */		\
	(void)(&_n == &_max);						\
	if (_n < _min)							\
		_n = _min;						\
	else if (_n > _max)						\
		_n = _max;						\
	/* return */ _n;						\
})
#endif

#else /* !__GNUC__ */

#define likely(expr) (expr)
#define unlikely(expr) (expr)
#undef __i386__
#undef __i686__

static char *
PARENT_HELPER (char *p, unsigned int offset)
{ return (0 == p) ? ((char *) 0) : p - offset; }

static const char *
CONST_PARENT_HELPER (const char *p, unsigned int offset)
{ return (0 == p) ? ((char *) 0) : p - offset; }

#define PARENT(_ptr, _type, _member)					\
	((0 == offsetof (_type, _member)) ? (_type *)(_ptr)		\
	 : (_type *) PARENT_HELPER ((char *)(_ptr), offsetof (_type, _member)))
#define CONST_PARENT(_ptr, _type, _member)				\
	((0 == offsetof (const _type, _member)) ? (const _type *)(_ptr)	\
	 : (const _type *) CONST_PARENT_HELPER ((const char *)(_ptr),	\
	  offsetof (const _type, _member)))

#undef ABS
#define ABS(n) (((n) < 0) ? -(n) : (n))

#undef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#undef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))

#undef SWAP
#define SWAP(x, y)							\
do {									\
	long _x = x;							\
	x = y;								\
	y = _x;								\
} while (0)

#undef SATURATE
#define SATURATE(n, min, max) MIN (MAX (min, n), max)

#endif /* !__GNUC__ */

/* 32 bit constant byte reverse, e.g. 0xAABBCCDD -> 0xDDCCBBAA. */
#define SWAB32(m)							\
	(+ (((m) & 0xFF000000) >> 24)					\
	 + (((m) & 0xFF0000) >> 8)					\
	 + (((m) & 0xFF00) << 8)					\
	 + (((m) & 0xFF) << 24))

#ifdef HAVE_BUILTIN_POPCOUNT
#  define popcnt(x) __builtin_popcount ((uint32_t)(x))
#else
#  define popcnt(x) _vbi_popcnt (x)
#endif

extern unsigned int
_vbi_popcnt			(uint32_t		x);

/* NB GCC inlines and optimizes these functions when size is const. */
#define SET(var) memset (&(var), ~0, sizeof (var))

#define CLEAR(var) memset (&(var), 0, sizeof (var))

#define COPY(d, s) /* useful to copy arrays, otherwise use assignment */ \
	(assert (sizeof (d) == sizeof (s)), memcpy (d, s, sizeof (d)))

/* Copy string const into char array. */
#define STRACPY(array, s)						\
do {									\
	/* Complain if s is no string const or won't fit. */		\
	const char t_[sizeof (array) - 1] __attribute__ ((unused)) = s; \
									\
	memcpy (array, s, sizeof (s));					\
} while (0)

/* Copy bits through mask. */
#define COPY_SET_MASK(dest, from, mask)					\
	(dest ^= (from) ^ (dest & (mask)))

/* Set bits if cond is TRUE, clear if FALSE. */
#define COPY_SET_COND(dest, bits, cond)					\
	 ((cond) ? (dest |= (bits)) : (dest &= ~(bits)))

/* Set and clear bits. */
#define COPY_SET_CLEAR(dest, set, clear)				\
	(dest = (dest & ~(clear)) | (set))

/* For debugging. */
#define vbi3_malloc malloc
#define vbi3_realloc realloc
#define vbi3_strdup strdup
#define vbi3_free free
#define vbi3_cache_malloc malloc
#define vbi3_cache_free free

/* Helper functions. */

vbi3_inline int
_vbi3_to_ascii			(int			c)
{
	if (c < 0)
		return '?';

	c &= 0x7F;

	if (c < 0x20 || c >= 0x7F)
		return '.';

	return c;
}

typedef struct {
	const char *		key;
	int			value;
} _vbi3_key_value_pair;

extern vbi3_bool
_vbi3_keyword_lookup		(int *			value,
				 const char **		inout_s,
				 const _vbi3_key_value_pair * table,
				 unsigned int		n_pairs);

/* Logging stuff. */

extern _vbi3_log_hook		_vbi3_global_log;

extern void
_vbi3_log_vprintf		(vbi3_log_fn		log_fn,
				 void *			user_data,
				 vbi3_log_mask		level,
				 const char *		source_file,
				 const char *		context,
				 const char *		templ,
				 va_list		ap);
extern void
_vbi3_log_printf			(vbi3_log_fn		log_fn,
				 void *			user_data,
				 vbi3_log_mask		level,
				 const char *		source_file,
				 const char *		context,
				 const char *		templ,
				 ...);

#define _vbi3_log(hook, level, templ, args...)				\
do {									\
	_vbi3_log_hook *_h = hook;					\
									\
	if ((NULL != _h && 0 != (_h->mask & level))			\
	    || (_h = &_vbi3_global_log, 0 != (_h->mask & level)))	\
		_vbi3_log_printf (_h->fn, _h->user_data,		\
				  level, __FILE__, __FUNCTION__,	\
				  templ , ##args);			\
} while (0)

#define _vbi3_vlog(hook, level, templ, ap)				\
do {									\
	_vbi3_log_hook *_h = hook;					\
									\
	if ((NULL != _h && 0 != (_h->mask & level))			\
	    || (_h = &_vbi3_global_log, 0 != (_h->mask & level)))	\
		_vbi3_log_vprintf (_h->fn, _h->user_data,		\
				  level, __FILE__, __FUNCTION__,	\
				  templ, ap);				\
} while (0)

#define error(hook, templ, args...)					\
	_vbi3_log (hook, VBI3_LOG_ERROR, templ , ##args)
#define warning(hook, templ, args...)					\
	_vbi3_log (hook, VBI3_LOG_ERROR, templ , ##args)
#define notice(hook, templ, args...)					\
	_vbi3_log (hook, VBI3_LOG_NOTICE, templ , ##args)
#define info(hook, templ, args...)					\
	_vbi3_log (hook, VBI3_LOG_INFO, templ , ##args)
#define debug1(hook, templ, args...)					\
	_vbi3_log (hook, VBI3_LOG_DEBUG, templ , ##args)
#define debug2(hook, templ, args...)					\
	_vbi3_log (hook, VBI3_LOG_DEBUG2, templ , ##args)
#define debug3(hook, templ, args...)					\
	_vbi3_log (hook, VBI3_LOG_DEBUG3, templ , ##args)

/* Portability stuff. */

/* These should be defined in inttypes.h. */
#ifndef PRId64
#  define PRId64 "lld"
#endif
#ifndef PRIu64
#  define PRIu64 "llu"
#endif
#ifndef PRIx64
#  define PRIx64 "llx"
#endif

/* Use this instead of strncpy(). strlcpy() is a BSD/GNU extension. */
#ifndef HAVE_STRLCPY
#  define strlcpy _vbi3_strlcpy
#endif
#undef strncpy
#define strncpy use_strlcpy_instead

extern size_t
_vbi3_strlcpy			(char *			dst,
				 const char *		src,
				 size_t			len);

/* strndup() is a BSD/GNU extension. */
#ifndef HAVE_STRNDUP
#  define strndup _vbi3_strndup
#endif

extern char *
_vbi3_strndup			(const char *		s,
				 size_t			len);

/* vasprintf() is a GNU extension. */
#ifndef HAVE_VASPRINTF
#  define vasprintf _vbi3_vasprintf
#endif

extern int
_vbi3_vasprintf			(char **		dstp,
				 const char *		templ,
				 va_list		ap);

/* asprintf() is a GNU extension. */
#ifndef HAVE_ASPRINTF
#  define asprintf _vbi3_asprintf
#endif

extern int
_vbi3_asprintf			(char **		dstp,
				 const char *		templ,
				 ...);

#undef sprintf
#define sprintf use_snprintf_or_asprintf_instead

#endif /* MISC_H */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
