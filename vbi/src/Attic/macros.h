/*
 *  libzvbi - Macros
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

/* $Id: macros.h,v 1.1.2.1 2004-03-31 00:41:34 mschimek Exp $ */

#ifndef __ZVBI_MACROS_H__
#define __ZVBI_MACROS_H__

/* Public */

#ifdef __cplusplus
#  define VBI_BEGIN_DECLS extern "C" {
#  define VBI_END_DECLS }
#else
#  define VBI_BEGIN_DECLS
#  define VBI_END_DECLS
#endif

VBI_BEGIN_DECLS

#if __GNUC__ >= 2
   /* Inline this function at -O2 and higher. */
#  define vbi_inline static __inline__
#else
#  define vbi_inline static
#endif

#if __GNUC__ >= 3
   /* Function has no side effects and return value depends
      only on parameters and non-volatile globals or
      memory pointed to by parameters. */
#  define vbi_pure __attribute__ ((pure))
   /* Function has no side effects and return value depends
      only on parameters. */
#  define vbi_const __attribute__ ((const))
   /* Function returns pointer which does not alias anything. */
#  define vbi_alloc __attribute__ ((malloc))
#else
#  define vbi_pure
#  define vbi_const
#  define vbi_alloc
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

typedef int vbi_bool;
/** @} */

#ifndef NULL
#  ifdef __cplusplus
#    define NULL (0L)
#  else
#    define NULL ((void*)0)
#  endif
#endif

VBI_END_DECLS

/* Private */

#endif /* __ZVBI_MACROS_H__ */
