/*
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

/* $Id: cpu.c,v 1.1 2005-02-25 18:15:54 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "cpu.h"
#include "mmx/mmx.h"
#include "avec/avec.h"

cpu_feature_set			cpu_features;

cpu_feature_set
cpu_detection			(void)
{
	cpu_features = 0;

#if defined (HAVE_MMX)
	cpu_features = cpu_detection_mmx ();
#elif defined (HAVE_ALTIVEC)
	cpu_features = cpu_detection_altivec ();
#endif

	return cpu_features;
}
