/*
 *  libzvbi - Miscellaneous types and macros
 *
 *  Copyright (C) 2002, 2003, 2004 Michael H. Schimek
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

/* $Id: misc.h,v 1.11 2005-09-01 01:40:52 mschimek Exp $ */

#ifndef MISC_H
#define MISC_H

#include <stddef.h>
#include <string.h>
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

/* Absolute value of int without a branch.
   Note ABS (INT_MIN) == INT_MAX + 1. */
#undef ABS
#define ABS(n) ({							\
	register int _n = (n), _t = _n;					\
	_t >>= sizeof (_t) * 8 - 1; /* assumes signed shift, safe? */	\
	_n ^= _t;							\
	_n -= _t;							\
})

#undef MIN
#define MIN(x, y) ({							\
	__typeof__ (x) _x = (x);					\
	__typeof__ (y) _y = (y);					\
	(void)(&_x == &_y); /* alert when type mismatch */		\
	(_x < _y) ? _x : _y;						\
})

#undef MAX
#define MAX(x, y) ({							\
	__typeof__ (x) _x = (x);					\
	__typeof__ (y) _y = (y);					\
	(void)(&_x == &_y); /* alert when type mismatch */		\
	(_x > _y) ? _x : _y;						\
})

#undef SWAP
#define SWAP(x, y)							\
do {									\
	__typeof__ (x) _x = x;						\
	x = y;								\
	y = _x;								\
} while (0)

#undef SATURATE
#ifdef __i686__ /* has conditional move. */
#define SATURATE(n, min, max) ({					\
	__typeof__ (n) _n = (n);					\
	__typeof__ (n) _min = (min);					\
	__typeof__ (n) _max = (max);					\
	(void)(&_n == &_min); /* alert when type mismatch */		\
	(void)(&_n == &_max);						\
	if (_n < _min)							\
		_n = _min;						\
	if (_n > _max)							\
		_n = _max;						\
	_n;								\
})
#else
#define SATURATE(n, min, max) ({					\
	__typeof__ (n) _n = (n);					\
	__typeof__ (n) _min = (min);					\
	__typeof__ (n) _max = (max);					\
	(void)(&_n == &_min); /* alert when type mismatch */		\
	(void)(&_n == &_max);						\
	if (_n < _min)							\
		_n = _min;						\
	else if (_n > _max)						\
		_n = _max;						\
	_n;								\
})
#endif

#else /* !__GNUC__ */

#define likely(expr) (expr)
#define unlikely(expr) (expr)
#undef __i386__
#undef __i686__
#define __attribute__(args...)

static char *
PARENT_HELPER (char *p, unsigned int offset)
{ return (p == 0) ? 0 : p - offset; }

static const char *
CONST_PARENT_HELPER (const char *p, unsigned int offset)
{ return (p == 0) ? 0 : p - offset; }

#define PARENT(_ptr, _type, _member)					\
	((offsetof (_type, _member) == 0) ? (_type *)(_ptr)		\
	 : (_type *) PARENT_HELPER ((char *)(_ptr), offsetof (_type, _member)))
#define CONST_PARENT(_ptr, _type, _member)				\
	((offsetof (const _type, _member) == 0) ? (const _type *)(_ptr)	\
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
#define SATURATE(n, min, max) MIN (MAX (n, min), max)

#ifndef ZAPPING8
extern void
vbi3_log_printf			(const char *		function,
				 const char *		template,
				 ...);
#endif

#endif /* !__GNUC__ */

/* 32 bit constant byte reverse, e.g. 0xAABBCCDD -> 0xDDCCBBAA */
#define SWAB32(m)							\
	(+ (((m) & 0xFF000000) >> 24)					\
	 + (((m) & 0xFF0000) >> 8)					\
	 + (((m) & 0xFF00) << 16)					\
	 + (((m) & 0xFF) << 8))

#undef CLAMP
#define CLAMP(n, min, max) SATURATE (n, min, max)

/* NB gcc inlines and optimizes when size is const. */
#define SET(var) memset (&(var), ~0, sizeof (var))
#define CLEAR(var) memset (&(var), 0, sizeof (var))
#define COPY(d, s) /* useful to copy arrays, otherwise use assignment */ \
	(assert (sizeof (d) == sizeof (s)), memcpy (d, s, sizeof (d)))

/* Use this instead of strncpy(), is a BSD/GNU extension.*/
#ifdef HAVE_STRLCPY
#  define _vbi3_strlcpy strlcpy
#else
extern size_t
_vbi3_strlcpy			(char *			dst,
				 const char *		src,
				 size_t			len);
#endif

/* strndup is a BSD/GNU extension. */
#ifdef HAVE_STRNDUP
#  define _vbi3_strndup strndup
#else
extern char *
_vbi3_strndup			(const char *		s,
				 size_t			len);
#endif

#ifdef HAVE_ASPRINTF
#  define _vbi3_asprintf asprintf
#else
extern int
_vbi3_asprintf			(char **		dstp,
				 const char *		templ,
				 ...);
#endif

#define STRCOPY(d, s) (_vbi3_strlcpy (d, s, sizeof (d)) < sizeof (d))

/* Copy bits through mask. */
#define COPY_SET_MASK(to, from, mask)					\
	(to ^= (from) ^ (to & (mask)))
/* Set bits if cond is true, clear if false. */
#define COPY_SET_COND(to, bits, cond)					\
	 ((cond) ? (to |= (bits)) : (to &= ~(bits)))
/* Set and clear bits. */
#define COPY_SET_CLEAR(to, set, clear)					\
	(to = (to & ~(clear)) | (set))

vbi3_inline int
vbi3_printable			(int			c)
{
	if (c < 0)
		return '?';

	c &= 0x7F;

	if (c < 0x20 || c >= 0x7F)
		return '.';

	return c;
}

/* TODO */
#define vbi3_malloc malloc
#define vbi3_realloc realloc
#define vbi3_free free
#define vbi3_cache_malloc malloc
#define vbi3_cache_free free

#ifdef ZAPPING8

#define vbi3_log_printf(level, func, templ, args...)			\
do {									\
	fprintf (stderr, "%s: " templ, func , ##args);			\
} while (0)

#endif

#define debug(templ, args...)						\
do {									\
	fprintf (stderr, "%s:%u: %s: " templ ".\n",			\
		 __FILE__, __LINE__, __PRETTY_FUNCTION__ , ##args);	\
} while (0)

#define error(templ, args...)						\
do {									\
	fprintf (stderr, "%s:%u: %s: " templ ".\n",			\
		 __FILE__, __LINE__, __PRETTY_FUNCTION__ , ##args);	\
} while (0)

#endif /* MISC_H */
