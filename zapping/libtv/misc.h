/*
 *  Copyright (C) 2001-2004 Michael H. Schimek
 *  Copyright (C) 2000-2003 Iñaki García Etxebarria
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

/* $Id: misc.h,v 1.13 2007-08-30 14:14:09 mschimek Exp $ */

#ifndef __ZTV_MISC_H__
#define __ZTV_MISC_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stddef.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>
#include "macros.h"

#define N_ELEMENTS(array) (sizeof (array) / sizeof (*(array)))

#ifdef __GNUC__

#undef likely
#undef unlikely
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
#undef CONST_PARENT
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

#define SWAB16(n) ({							\
	__typeof__ (n) _n = (n);					\
	((_n & 0xFF) << 8) | ((_n >> 8) & 0xFF);			\
})

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

#undef PARENT
#define PARENT(_ptr, _type, _member)					\
	((offsetof (_type, _member) == 0) ? (_type *)(_ptr)		\
	 : (_type *) PARENT_HELPER ((char *)(_ptr), offsetof (_type, _member)))
#undef CONST_PARENT
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

#undef SATURATE
#define SATURATE(n, min, max) MIN (MAX (n, min), max)

#define SWAB16(n) ((((n) & 0xFF) << 8) | (((n) >> 8) & 0xFF))

#endif /* !__GNUC__ */

/* Find first set bit in 32 bit constant, see man 3 ffs. */
#define FFS2(m) ((m) & 0x2 ? 2 : 1)
#define FFS4(m) ((m) & 0xC ? 2 + FFS2 ((m) >> 2) : FFS2 (m))
#define FFS8(m) ((m) & 0xF0 ? 4 + FFS4 ((m) >> 4) : FFS4 (m))
#define FFS16(m) ((m) & 0xFF00 ? 8 + FFS8 ((m) >> 8) : FFS8 (m))
#define FFS32(m) ((m) & 0xFFFF0000 ? 16 + FFS16 ((m) >> 16) : FFS16 (m))
#define FFS(m) (0 == (m) ? 0 : FFS32 (m))

/* Masks m bits out of v, then shifts msb to position n (0 ... 31).
   E.g. v=0x1234, m=0x0FF0, n=7 -> 0x0023. */
#undef MASKED_SHIFT
#define MASKED_SHIFT(v, m, n)						\
	((FFS (m) > (int)(n) + 1) ?					\
	 (((v) & (m)) >> (FFS (m) - (n) - 1)) :				\
	 (((v) & (m)) << MIN ((int)(n) + 1 - FFS (m), 31)))

/* E.g. v=0x1234, m=0x0FF0, n=7 -> 0x0340. */
#undef SHIFT_MASKED
#define SHIFT_MASKED(v, m, n)						\
	((FFS (m) > (int)(n) + 1) ?					\
	 (((v) << (FFS (m) - (n) - 1)) & (m)) :				\
	 (((v) >> MIN ((int)(n) + 1 - FFS32 (m), 31) & (m)))

#undef CLAMP
#define CLAMP(n, min, max) SATURATE (n, min, max)

/* NB gcc inlines and optimizes when size is const. */
#define SET(var) memset (&(var), ~0, sizeof (var))
#define CLEAR(var) memset (&(var), 0, sizeof (var))
#define COPY(d, s) /* useful to copy arrays, otherwise use assignment */ \
	(assert (sizeof (d) == sizeof (s)), memcpy (d, s, sizeof (d)))

/* legacy, to be removed/replaced */
#ifndef printv
#include <stdio.h>
extern int debug_msg;
#define printv(format, args...) \
do { \
  if (debug_msg) { \
    fprintf(stderr, format ,##args); \
  fflush(stderr); } \
} while (0)
#endif

#ifdef HAVE_STRLCPY
#  define _tv_strlcpy strlcpy
#else
extern size_t
_tv_strlcpy			(char *			dst,
				 const char *		src,
				 size_t			len);
#endif

#ifdef HAVE_STRNDUP
#  define _tv_strndup strndup
#else
extern char *
_tv_strndup			(const char *		s,
				 size_t			len);
#endif

#ifdef HAVE_ASPRINTF
#  define _tv_asprintf asprintf
#else
int
_tv_asprintf			(char **		dstp,
				 const char *		templ,
				 ...);
#endif

typedef void
clear_plane_fn			(uint8_t *		dst,
				 unsigned int		value,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned long		padding);

typedef void
copy_plane_fn			(uint8_t *		dst,
				 const uint8_t *	src,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned long		dst_padding,
				 unsigned long		src_padding);

#endif /* MISC_H */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
