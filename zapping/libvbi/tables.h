/*
 *  Zapzilla - Teletext / PDC tables
 *
 *  PDC and VPS CNI codes based on TR 101 231 EBU (2000-07)
 *  Programme type tables (PDC/EPG)
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek
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

/* $Id: tables.h,v 1.3 2001-02-26 05:56:59 mschimek Exp $ */

#ifndef TABLES_H
#define TABLES_H

extern char *		country_names_en[];

extern struct pdc_vps_entry {
	int			country;
	char *			short_name;	/* 8 chars */
	char *			long_name;
	unsigned short		cni1;		/* Packet 8/30 format 1 */
	unsigned short		cni2;		/* Packet 8/30 format 2 */
	unsigned short		cni3;		/* Packet X/26 */
	unsigned short		cni4;		/* VPS */
} PDC_VPS_CNI[];

extern char *		program_class[16];
extern char *		program_type[8][16];

extern char *		eia608_program_type[96];

#endif /* TABLES_H */
