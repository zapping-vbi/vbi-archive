/*
 *  libzvbi - Video Programming System
 *
 *  Copyright (C) 2000-2004 Michael H. Schimek
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

/* $Id: vps.h,v 1.1.2.2 2004-02-25 17:35:29 mschimek Exp $ */

#ifndef VPS_H
#define VPS_H

#include "misc.h"
#include "pdc.h"
#include "vbi.h"

/* Public */

/**
 * @addtogroup VPS
 * @{
 */
extern vbi_bool
vbi_decode_vps_cni		(unsigned int *		cni,
				 const uint8_t		buffer[13]);
extern vbi_bool
vbi_decode_vps_pdc		(vbi_program_id *	pi,
				 const uint8_t		buffer[13]);
/** @} */

/* Private */

extern void
vbi_decode_vps			(vbi_decoder *		vbi,
				 const uint8_t		buffer[13]);

#endif /* VPS_H */
