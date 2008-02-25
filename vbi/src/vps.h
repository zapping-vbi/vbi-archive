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

/* $Id: vps.h,v 1.1.2.8 2008-02-25 20:59:00 mschimek Exp $ */

#ifndef __ZVBI3_VPS_H__
#define __ZVBI3_VPS_H__

#include <inttypes.h>		/* uint8_t */
#include "macros.h"
#ifndef ZAPPING8
#  include "version.h"
#endif
#include "pdc.h"		/* vbi3_program_id */

VBI3_BEGIN_DECLS

/* Public */

/**
 * @addtogroup VPS
 * @{
 */
extern vbi3_bool
vbi3_decode_vps_cni		(unsigned int *		cni,
				 const uint8_t		buffer[13])
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  _vbi3_nonnull ((1, 2))
#endif
  ;
extern vbi3_bool
vbi3_encode_vps_cni		(uint8_t		buffer[13],
				 unsigned int		cni)
  _vbi3_nonnull ((1));

/* Private */

#if defined ZAPPING8 || 3 == VBI_VERSION_MINOR
extern vbi3_bool
vbi3_decode_vps_pdc		(vbi3_program_id *	pid,
				 const uint8_t		buffer[13])
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  _vbi3_nonnull ((1, 2))
#endif
  ;
extern vbi3_bool
vbi3_encode_vps_pdc		(uint8_t		buffer[13],
				 const vbi3_program_id *pid)
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  _vbi3_nonnull ((1, 2))
#endif
  ;
vbi3_bool
vbi3_decode_dvb_pdc_descriptor	(vbi3_program_id *	pid,
				 const uint8_t		buffer[5])
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  _vbi3_nonnull ((1, 2))
#endif
  ;
vbi3_bool
vbi3_encode_dvb_pdc_descriptor	(uint8_t		buffer[5],
				 const vbi3_program_id *pid)
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  _vbi3_nonnull ((1, 2))
#endif
  ;
#endif
/** @} */

VBI3_END_DECLS

#endif /* __ZVBI3_VPS_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
