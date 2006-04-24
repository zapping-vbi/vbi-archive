/*
 *  Copyright (C) 2004 Michael H. Schimek
 *
 *  Based on code from Xine
 *  Copyright (C) 1999-2001 Aaron Holtzman
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

/* $Id: cpu.c,v 1.4 2006-04-24 11:20:26 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <signal.h>
#include <setjmp.h>
#include "avec.h"

static sigjmp_buf		jmpbuf;

static void
sigill_handler			(int			sig)
{
	sig = sig; /* unused */

	siglongjmp (jmpbuf, 1);
}

cpu_feature_set
cpu_detection_altivec		(void)
{
	if (sigsetjmp (jmpbuf, 1)) {
		signal (SIGILL, SIG_DFL);
		return 0;
	}

	signal (SIGILL, sigill_handler);

	__asm__ __volatile__ (" mtspr 256, %0\n"
			      " vand %%v0, %%v0, %%v0\n"
			      :: "r" (-1));

	signal (SIGILL, SIG_DFL);

	return CPU_FEATURE_ALTIVEC;
}
