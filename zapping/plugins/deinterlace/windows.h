/*
 *  Zapping TV viewer
 *
 *  Copyright (C) 2004 Michael H. Schimek
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

/* $Id: windows.h,v 1.1 2005-01-08 14:35:43 mschimek Exp $ */

#ifndef WINDOWS_H
#define WINDOWS_H

#include <inttypes.h>		/* int64_t */
#include "common/intl-priv.h"

#define _cdecl			/* what's that? */
#define __cdecl

/* __declspec(align(n)) -> __attribute__((aligned(n))) */
/* __declspec(dllexport) -> __attribute__(()) */
#define __declspec(x) __attribute__((x))
#define align(x) aligned(x)
#define dllexport

/* __asm emms; or __asm { emms } -> __asm__ (" emms\n") */
#define _asm
#define __asm
#define emms __asm__ __volatile__ (" emms\n");

#define __min(x, y) ({							\
  __typeof__ (x) _x = (x);						\
  __typeof__ (y) _y = (y);						\
  (_x < _y) ? _x : _y;							\
})

#define __max(x, y) ({							\
  __typeof__ (x) _x = (x);						\
  __typeof__ (y) _y = (y);						\
  (_x > _y) ? _x : _y;							\
})

#define WINAPI
#define APIENTRY

typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int UINT;
typedef long LONG;
typedef long DWORD;
typedef unsigned long ULONG;
typedef int32_t __int32;
typedef int64_t __int64;
typedef void *LPVOID;
typedef char *LPCSTR;
typedef void *HWND;
typedef void *HMODULE;
typedef void *HANDLE;
typedef void *HINSTANCE;

#define size_t unsigned int

typedef struct {
} RECT;

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef NULL
#define NULL ((void *) 0)
#endif

#define WM_APP 0

/* DScaler helpids.h */
enum {
  IDH_2FRAME,
  IDH_ADAPTIVE,
  IDH_BOB,
  IDH_BLENDEDCLIP,
  IDH_EVEN,
  IDH_GREEDY,
  IDH_GREEDY2,
  IDH_GREEDYHM,
  IDH_MOCOMP2,
  IDH_ODD,
  IDH_OLD_GAME,
  IDH_SCALER_BOB,
  IDH_TOMSMOCOMP,
  IDH_VIDEOBOB,
  IDH_VIDEOWEAVE,
  IDH_WEAVE,
};

/* For _save(), _restore() below. */
#define _saved_regs unsigned int saved_regs[6]

/* NOTE inline asm shall not use global mutables. Global consts are
   safe, but only by absolute address. %[name] would use ebx (GOT)
   relative addressing and inline asm usually overwrites ebx. Static
   consts must be referenced with _m() though, or they optimize away.
   Locals with _m(), ebp or esp relative, are safe. _m(name) is a
   named asm operand (gcc 3.1+ feature), such that inline asm can
   refer to locals by %[name] instead of numbers. */
#define _m(x) [x] "m" (x)
#define _m_array(x) [x] "m" (x[0])

/* Replaces  mov eax,local+3  by  mov eax,%[local3]  and  _m_nth(local,3) */
#define _m_nth(x, nth) [x##nth] "m" (((char *) &x)[nth])

/* NOTE Intel syntax - dest first. */
#define _save(x) " mov %[saved_" #x "]," #x "\n"
#define _restore(x) " mov " #x ",%[saved_" #x "]\n"

/* We use Intel syntax because the code was written for MSVC, noprefix
   because regs have no % prefix. ebx is the -fPIC GOT pointer. We cannot
   add ebx to the clobber list, must save & restore by hand. */
#define _asm_begin							\
  __asm__ __volatile__ (						\
  ".intel_syntax noprefix\n"						\
  _save(ebx)
#define _asm_end							\
  _restore(ebx)								\
  ".intel_syntax\n"							\
  ::									\
  [saved_eax] "m" (saved_regs[0]),					\
  [saved_ebx] "m" (saved_regs[1]),					\
  [saved_ecx] "m" (saved_regs[2]),					\
  [saved_edx] "m" (saved_regs[3]),					\
  [saved_esi] "m" (saved_regs[4]),					\
  [saved_edi] "m" (saved_regs[5])

/* Stringify _strf(FOO) -> "FOO" */
#define _strf1(x) #x
#define _strf(x) _strf1(x)

#endif /* WINDOWS_H */
