/*
 *  libzvbi - Teletext packet decoder, packet 8/30
 *
 *  Copyright (C) 2003-2004 Michael H. Schimek
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

/* $Id: packet-830.h,v 1.1.2.1 2004-02-25 17:35:29 mschimek Exp $ */

#ifndef PACKET_830_H
#define PACKET_830_H

#include "pdc.h"

/* Public */

/**
 * @addtogroup Packet830
 * @{
 */
extern vbi_bool
vbi_decode_teletext_8301_cni	(unsigned int *		cni,
				 const uint8_t		buffer[42]);
extern vbi_bool
vbi_decode_teletext_8301_local_time
				(time_t *		time,
				 int *			gmtoff,
				 const uint8_t		buffer[42]);
extern vbi_bool
vbi_decode_teletext_8302_cni	(unsigned int *		cni,
				 const uint8_t		buffer[42]);
extern vbi_bool
vbi_decode_teletext_8302_pdc	(vbi_program_id *	pi,
				 const uint8_t		buffer[42]);
/** @} */

/* Private */

#endif /* PACKET_830_H */
