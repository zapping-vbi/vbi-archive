/*
 *  Zapping TV viewer
 *
 *  Copyright (C) 2005 Michael H. Schimek
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

/* $Id: cpudt.c,v 1.1.2.2 2005-06-17 02:54:20 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "libtv/cpu.h"

int
main				(int			argc,
				 char **		argv)
{
	cpu_feature_set required_features;
	cpu_feature_set actual_features;

	assert (2 == argc);

	required_features = cpu_feature_set_from_string (argv[1]);
	actual_features = cpu_detection ();

	if (required_features == (actual_features & required_features))
		exit (EXIT_SUCCESS);
	else
		exit (EXIT_FAILURE);
}
