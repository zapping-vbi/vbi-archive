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

/* $Id: main.h,v 1.1 2005-01-08 14:54:22 mschimek Exp $ */

#ifndef MAIN_H
#define MAIN_H

#include "windows.h"
#include "DS_Deinterlace.h"

extern DEINTERLACE_METHOD *	deinterlace_methods[30];

extern DEINTERLACE_METHOD *
deinterlace_find_method		(const gchar *		name);

#endif /* MAIN_H */
