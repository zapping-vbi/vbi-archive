/*
 *  libzvbi - Miscellaneous types and macros
 *
 *  Copyright (C) 2002-2004 Michael H. Schimek
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

/* $Id: misc.h,v 1.2.2.15 2004-10-14 07:54:01 mschimek Exp $ */

#ifndef MISC_H
#define MISC_H

#include <stddef.h>
#include <string.h>
#include <assert.h>
#include "vbi.h"		/* vbi_log_level */

#define N_ELEMENTS(array) (sizeof (array) / sizeof (*(array)))

#ifdef __GNUC__

#if __GNUC__ < 3
/* Expect expression usually true/false, schedule accordingly. */
#  define __builtin_expect(expr, c) (expr)
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

#define SWAP(x, y)							\
do {									\
	__typeof__ (x) _x = x;						\
	x = y;								\
	y = _x;								\
} while (0)

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

extern void
vbi_log_printf			(vbi_log_level		level,
				 const char *		function,
				 const char *		template,
				 ...)
     __attribute__ ((format (__printf__, 3, 4)));

#else /* !__GNUC__ */

#define __builtin_expect(expr, c) (expr)
#undef __i386__
#undef __i686__
#define __attribute__(x)

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

#define SWAP(x, y)							\
do {									\
	long _x = x;							\
	x = y;								\
	y = _x;								\
} while (0)

#define SATURATE(n, min, max) MIN (MAX (n, min), max)

extern void
vbi_log_printf			(const char *		function,
				 const char *		template,
				 ...);

#endif /* !__GNUC__ */

#define CLAMP(n, min, max) SATURATE (n, min, max)

/* NB gcc inlines and optimizes when size is const. */
#define SET(var) memset (&(var), ~0, sizeof (var))
#define CLEAR(var) memset (&(var), 0, sizeof (var))
#define COPY(d, s) /* useful to copy arrays, otherwise use assignment */ \
	(assert (sizeof (d) == sizeof (s)), memcpy (d, s, sizeof (d)))

/* Use this instead of strncpy(), is a BSD/GNU extension.*/
#ifdef HAVE_STRLCPY
#  define _vbi_strlcpy strlcpy
#else
extern size_t
_vbi_strlcpy			(char *			dst,
				 const char *		src,
				 size_t			len);
#endif

/* strndup is a BSD/GNU extension. */
#ifdef HAVE_STRNDUP
#  define _vbi_strndup strndup
#else
extern char *
_vbi_strndup			(const char *		s,
				 size_t			len);
#endif

#ifdef HAVE_ASPRINTF
#  define _vbi_asprintf asprintf
#else
extern int
_vbi_asprintf			(char **		dstp,
				 const char *		templ,
				 ...);
#endif

#define STRCOPY(d, s) (_vbi_strlcpy (d, s, sizeof (d)) < sizeof (d))

/* Copy bits through mask. */
#define COPY_SET_MASK(to, from, mask)					\
	(to ^= (from) ^ (to & (mask)))
/* Set bits if cond is true, clear if false. */
#define COPY_SET_COND(to, bits, cond)					\
	 ((cond) ? (to |= (bits)) : (to &= ~(bits)))
/* Set and clear bits. */
#define COPY_SET_CLEAR(to, set, clear)					\
	(to = (to & ~(clear)) | (set))

#ifndef __BYTE_ORDER
/* Should be __LITTLE_ENDIAN or __BIG_ENDIAN, but I guess
   that's a GNU/Linuxism. Alternatives? */
#  define __BYTE_ORDER __UNKNOWN_BYTE_ORDER
#endif

#define vbi_malloc malloc
#define vbi_realloc realloc
#define vbi_free free

#endif /* MISC_H */
