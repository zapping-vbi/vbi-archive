/*
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

/* $Id: errstr.h,v 1.1 2001-08-20 00:53:23 mschimek Exp $ */

#ifndef ERRSTR_H
#define ERRSTR_H

#define errstr (get_errstr())

extern void		reset_errstr(void);
extern void		set_errstr(char *, void (*)(void *));
extern char *		get_errstr(void);

extern void		set_errstr_printf(char *, ...);

#endif /* ERRSTR_H */
