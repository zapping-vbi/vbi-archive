/*
 *  Triggers
 *
 *  Copyright (C) 2001 Michael H. Schimek
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

/* $Id: trigger.h,v 1.2 2001-06-23 02:50:44 mschimek Exp $ */

#include "vbi.h"

extern void		vbi_trigger_flush(struct vbi *vbi);
extern void		vbi_deferred_trigger(struct vbi *vbi);
extern void		vbi_eacem_trigger(struct vbi *vbi, unsigned char *s);
extern void		vbi_atvef_trigger(struct vbi *vbi, char *s);
