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

/* $Id: macros.h,v 1.5 2007-08-30 14:14:09 mschimek Exp $ */

#ifndef __ZTV_MACROS_H__
#define __ZTV_MACROS_H__

#ifdef __cplusplus
#  define TV_BEGIN_DECLS extern "C" {
#  define TV_END_DECLS }
#else
#  define TV_BEGIN_DECLS
#  define TV_END_DECLS
#endif

TV_BEGIN_DECLS

#if __GNUC__ >= 4
#  define _tv_sentinel sentinel(0)
#else
#  define _tv_sentinel
#  define __restrict__
#endif

#if (__GNUC__ == 3 && __GNUC_MINOR__ >= 3) || __GNUC__ >= 4
#  define _tv_nonnull(args...) nonnull(args)
#else
#  define _tv_nonnull(args...)
#endif

#if __GNUC__ >= 2
#  define tv_inline static __inline__
#else
#  define tv_inline static
#  define __attribute__(args...)
#endif

#ifndef TRUE
#  define TRUE 1
#endif
#ifndef FALSE
#  define FALSE 0
#endif

typedef int tv_bool;

#ifndef NULL
#  ifdef __cplusplus
#    define NULL (0L)
#  else
#    define NULL ((void*)0)
#  endif
#endif

TV_END_DECLS

#endif /* __ZTV_MACROS_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
