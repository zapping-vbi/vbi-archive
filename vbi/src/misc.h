/*
 *  libzvbi - Miscellaneous types and macros
 *
 *  Copyright (C) 2002-2003 Michael H. Schimek
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

/* $Id: misc.h,v 1.2.2.4 2003-09-24 18:49:56 mschimek Exp $ */

#ifndef MISC_H
#define MISC_H

#include <stddef.h>
#include <string.h>
#include <assert.h>

/* Public */

/**
 * @addtogroup Basic Types
 *
 * Apart of redefining TRUE and FALSE libzvbi reserves all type,
 * function, variable and constant names, and preprocessor symbols
 * starting with vbi_ or VBI_. In the future it may also reserve
 * the C++ namespace vbi.
 */

#ifndef DOXYGEN_SHOULD_IGNORE_THIS
/* doxygen omits static objects */
#define static_inline static __inline__
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

/**
 * @ingroup Event
 * @brief Unique network id (a libzvbi thing).
 *
 * 0 = unknown network, bit 31 reserved for preliminary nuids.
 * Other network codes are arbitrary.
 *
 * XXX fixme
a) custom, defined by the client (0<<63 + some)
b) temporary, created by libzvbi (7<<61 + time_t, you get the idea)
c) received call sign (6<<61 + 7 chars)
d) received cni (5<<61 + table index) (cannot use cni directly, blah blah)
Idea: Each channel cached by libzvbi has two nuids, one internal and
one client visible. When libzvbi detects a channel switch it sets
internal and visible to one new temporary nuid. When the client
announces a channel switch it can name the channel in advance by
assigning any nuid to visible, e.g. custom from a channel table
index or a received nuid known from previous sessions. Likewise
the client can rename a channel any time after a switch, e.g. when
a nuid was actually received. An id received by libzvbi is assigned
to internal and the client is notified about it, with value.
 */
typedef unsigned int vbi_nuid;

/* preliminary */
#define NUID0 0

#ifndef __GNUC__
#define __inline__
#endif

/* Private */

#define N_ELEMENTS(array) (sizeof (array) / sizeof (*(array)))

#ifdef __GNUC__

#if __GNUC__ < 3
#define __builtin_expect(exp, c) (exp)
#endif

#undef __i386__
#undef __i686__
#if #cpu (i386)
#define __i386__ 1
#endif
#if #cpu (i686)
#define __i686__ 1
#endif

#define PACKED __attribute__ ((packed))

/* &x == PARENT (&x.tm_min, struct tm, tm_min),
   safer than &x == (struct tm *) &x.tm_min */
#undef PARENT
#define PARENT(_ptr, _type, _member) ({					\
	__typeof__ (&((const _type *) 0)->_member) _p = (_ptr);		\
	(_p != 0) ? (_type *)(((char *) _p) - offsetof (_type,		\
	  _member)) : (_type *) 0;					\
})

#undef ABS
#define ABS(n) ({							\
	register int _n = n, _t = _n;					\
	_t >>= sizeof (_t) * 8 - 1;					\
	_n ^= _t;							\
	_n -= _t;							\
})

#undef MIN
#define MIN(x, y) ({							\
	__typeof__ (x) _x = x;						\
	__typeof__ (y) _y = y;						\
	(void)(&_x == &_y); /* alert when type mismatch */		\
	(_x < _y) ? _x : _y;						\
})

#undef MAX
#define MAX(x, y) ({							\
	__typeof__ (x) _x = x;						\
	__typeof__ (y) _y = y;						\
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
#ifdef __i686__ /* conditional move */
#define SATURATE(n, min, max) ({					\
	__typeof__ (n) _n = n;						\
	__typeof__ (n) _min = min;					\
	__typeof__ (n) _max = max;					\
	if (_n < _min)							\
		_n = _min;						\
	if (_n > _max)							\
		_n = _max;						\
	_n;								\
})
#else
#define SATURATE(n, min, max) ({					\
	__typeof__ (n) _n = n;						\
	__typeof__ (n) _min = min;					\
	__typeof__ (n) _max = max;					\
	if (_n < _min)							\
		_n = _min;						\
	else if (_n > _max)						\
		_n = _max;						\
	_n;								\
})
#endif

#else /* !__GNUC__ */

#define __builtin_expect(exp, c) (exp)
#undef __i386__
#undef __i686__
#define PACKED

static char *
PARENT_HELPER (char *p, unsigned int offset)
{ return (p == 0) ? 0 : p - offset; }

#undef PARENT
#define PARENT(_ptr, _type, _member)					\
	((offsetof (_type, _member) == 0) ? (_type *)(_ptr)		\
	 : (_type *) PARENT_HELPER ((char *)(_ptr), offsetof (_type, _member)))

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


extern const char _zvbi_intl_domainname[];

#ifndef _
#  ifdef ENABLE_NLS
#    include <libintl.h>
#    define _(String) dgettext (_zvbi_intl_domainname, String)
#    ifdef gettext_noop
#      define N_(String) gettext_noop (String)
#    else
#      define N_(String) (String)
#    endif
#  else /* Stubs that do something close enough.  */
#    define gettext(Msgid) ((const char *) (Msgid))
#    define dgettext(Domainname, Msgid) ((const char *) (Msgid))
#    define dcgettext(Domainname, Msgid, Category) ((const char *) (Msgid))
#    define ngettext(Msgid1, Msgid2, N) \
       ((N) == 1 ? (const char *) (Msgid1) : (const char *) (Msgid2))
#    define dngettext(Domainname, Msgid1, Msgid2, N) \
       ((N) == 1 ? (const char *) (Msgid1) : (const char *) (Msgid2))
#    define dcngettext(Domainname, Msgid1, Msgid2, N, Category) \
       ((N) == 1 ? (const char *) (Msgid1) : (const char *) (Msgid2))
#    define textdomain(Domainname) ((const char *) (Domainname))
#    define bindtextdomain(Domainname, Dirname) ((const char *) (Dirname))
#    define bind_textdomain_codeset(Domainname, Codeset) ((const char *) (Codeset))
#    define _(String) (String)
#    define N_(String) (String)
#  endif
#endif

#endif /* MISC_H */
