/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2001 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
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

/* $Id: vbi.c,v 1.12 2002-10-02 02:13:48 mschimek Exp $ */

#include "site_def.h"

#include "../common/fifo.h"
#include "../systems/mpeg.h"
#include "../systems/systems.h"
#include "../common/alloc.h"
#include "../common/log.h"
#include "../common/sync.h"
#include "../options.h"
#include "vbi.h"

/* TODO. Dummies. */
void * vbi_thread(void *F);
void vbi_init(fifo *f, multiplexer *mux);

void *
vbi_thread(void *F)
{
	return NULL;
}

void
vbi_init(fifo *f, multiplexer *mux)
{
	FAIL("vbi broken, sorry.");
}
