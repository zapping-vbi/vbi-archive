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

/* $Id: misc.h,v 1.2.2.9 2004-02-25 17:33:54 mschimek Exp $ */

#ifndef MISC_H
#define MISC_H

#include <stddef.h>
#include <string.h>
#include <assert.h>

/* Public */

/**
 * @addtogroup Basic Types
 *
 * Apart of redefining TRUE and FALSE libzvbi reserves all
 * preprocessor symbols and all type, function, variable and
 * constant names starting with vbi_ or VBI_, and the C++
 * namespace vbi.
 */

#ifndef DOXYGEN_SHOULD_IGNORE_THIS
#  ifdef __GNUC__
     /* Doxygen omits static objects. */
#    define static_inline static __inline__
#  else
#    define static_inline static
#    define __inline__
#  endif
#endif

/**
 * @ingroup Basic
 * @name Boolean type
 * @{
 */
#undef TRUE
#undef FALSE
#define TRUE 1
#define FALSE 0

typedef int vbi_bool;
/** @} */

#if __GNUC__ >= 3
#define vbi_attribute_pure __attribute__ ((pure))
#define vbi_attribute_const __attribute__ ((const))
#else
#define vbi_attribute_pure
#define vbi_attribute_const
#endif

/* Private */

#define N_ELEMENTS(array) (sizeof (array) / sizeof (*(array)))

#ifdef __GNUC__

#if __GNUC__ < 3
/* Expect expression usually true/false, schedule accordingly. */
#  define __builtin_expect(exp, c) (exp)
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
   safer than &x == (struct tm *) &x.tm_min */
#define PARENT(_ptr, _type, _member) ({					\
	__typeof__ (&((_type *) 0)->_member) _p = (_ptr);		\
	(_p != 0) ? (_type *)(((char *) _p) - offsetof (_type,		\
	  _member)) : (_type *) 0;					\
})

#define CONST_PARENT(_ptr, _type, _member) ({				\
	__typeof__ (&((const _type *) 0)->_member) _p = (_ptr);		\
	(_p != 0) ? (const _type *)(((const char *) _p) - offsetof	\
	 (const _type, _member)) : (const _type *) 0;			\
})

#undef ABS
#define ABS(n) ({							\
	register int _n = (n), _t = _n;					\
	_t >>= sizeof (_t) * 8 - 1;					\
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

#ifdef __i686__ /* has conditional move */
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
vbi_log_printf			(const char *		function,
				 const char *		template,
				 ...)
     __attribute__ ((format (__printf__, 2, 3)));

#else /* !__GNUC__ */

#define __builtin_expect(exp, c) (exp)
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

/* NB gcc inlines and optimizes when size is const. */
#define SET(var) memset (&(var), ~0, sizeof (var))
#define CLEAR(var) memset (&(var), 0, sizeof (var))
#define MOVE(d, s) memmove (d, s, sizeof (d))

extern size_t
vbi_strlcpy			(char *			d,
				 const char *		s,
				 size_t			size);

#define STRCOPY(d, s) (vbi_strlcpy (d, s, sizeof (d)) < sizeof (d))

#define COPY_SET_MASK(to, from, mask)					\
	(to ^= (from) ^ (to & (mask)))
#define COPY_SET_COND(to, from, cond)					\
	 ((cond) ? (to |= (from)) : (to &= ~(from)))

#ifndef __BYTE_ORDER
/* Should be __LITTLE_ENDIAN or __BIG_ENDIAN, but I guess
   that's a GNU/Linuxism. Alternatives? */
#  define __BYTE_ORDER __UNKNOWN_BYTE_ORDER
#endif

#endif /* MISC_H */
