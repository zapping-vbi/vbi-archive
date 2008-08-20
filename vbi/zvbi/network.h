/*
 *  libzvbi - Network identification
 *
 *  Copyright (C) 2004-2006 Michael H. Schimek
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the 
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 *  Boston, MA  02110-1301  USA.
 */

/* $Id: network.h,v 1.1.2.2 2008-08-20 12:34:37 mschimek Exp $ */

#ifndef __ZVBI_NETWORK_H__
#define __ZVBI_NETWORK_H__

#include <stdio.h>		/* FILE */
#include <inttypes.h>		/* int64_t */

#include "zvbi/macros.h"

VBI_BEGIN_DECLS

/**
 * @addtogroup Network
 * @{
 */

/**
 * This structure contains information identifying a TV network.
 */
typedef struct {
	/**
	 * Name of the network, either transmitted by the network
	 * itself or from a CNI table lookup. This is a NUL-terminated
	 * string in locale encoding. Can be @c NULL if unknown.
	 *
	 * Use vbi_network_set_name() to change this string.
	 */
	char *			name;

	/**
	 * The call sign of the received station, from XDS data.
	 * A NUL-terminated ASCII string, can be empty if unknown.
	 *
	 * Use vbi_network_set_call_sign() to change this string.
	 *
	 * Libzvbi examines only the call_sign, cni_vps, cni_8301,
	 * cni_8302 and user_data field to distinguish networks.
	 */
	char			call_sign[16];

	char			_reserved3[4];
	unsigned int		_reserved4;

	/**
	 * CNI of the network from VPS data, or from a CNI table lookup.
	 * See @c VBI_CNI_TYPE_VPS for details. Can be zero if unknown
	 * or not applicable.
	 *
	 * Use vbi_network_set_cni() to change this or other CNI fields,
	 * and to fill in CNI fields from a CNI table lookup.
	 */
	unsigned int		cni_vps;

	/**
	 * CNI of the network from Teletext packet 8/30 format 1,
	 * or from a CNI table lookup. See @c VBI_CNI_TYPE_8301 for
	 * details. Can be zero if unknown or not applicable.
	 */
	unsigned int		cni_8301;

	/**
	 * CNI of the network from Teletext packet 8/30 format 2,
	 * or from a CNI table lookup. See @c VBI_CNI_TYPE_8302 for
	 * details. Can be zero if unknown or not applicable.
	 */
	unsigned int		cni_8302;

	/**
	 * CNI of the network used for PDC Preselection method "A",
	 * from a CNI table lookup. See @c VBI_CNI_TYPE_PDC_A for
	 * details. Can be zero if unknown or not applicable.
	 */
	unsigned int		cni_pdc_a;

	/**
	 * CNI of the network used for PDC Preselection method "B",
	 * from a CNI table lookup. See @c VBI_CNI_TYPE_PDC_B for
	 * details. Can be zero if unknown or not applicable.
	 */
	unsigned int		cni_pdc_b;

	/**
	 * User pointer, for example a pointer into a channel table.
	 * Libzvbi compares, but does not dereference, this pointer
	 * to distinguish networks.
	 *
	 * See also vbi_network_equal().
	 */
	void *			user_data;

	void *			_reserved1[4];
	int			_reserved2[4];
} vbi_network;

/**
 */
typedef enum {
	VBI_CNI_TYPE_NONE,
	VBI_CNI_TYPE_UNKNOWN = VBI_CNI_TYPE_NONE,

	/**
	 * Video Programming System (VPS) format, a PDC CNI, for
	 * example from vbi_decode_vps_cni(). Note VPS transmits only
	 * the 4 lsb of the country code (0xcnn).
	 *
	 * Example ZDF: 0xDC2.
	 */
	VBI_CNI_TYPE_VPS,

	/**
	 * Teletext packet 8/30 format 1, for example from
	 * vbi_decode_teletext_8301_cni(). The country code is stored
	 * in the MSB, the network code in the LSB (0xccnn).  Note
	 * these CNIs may use different country and network codes than
	 * the PDC CNIs.
	 *
	 * Example BBC 1: 0x447F, ZDF: 0x4902.
	 */
	VBI_CNI_TYPE_8301,

	/**
	 * Teletext packet 8/30 format 2 (PDC), for example from
	 * vbi_decode_teletext_8302_cni(). The country code is stored
	 * in the MSB, the network code in the LSB (0xccnn).
	 *
	 * Example BBC 1: 0x2C7F, ZDF: 0x1DC2.
	 */
	VBI_CNI_TYPE_8302,

	/**
	 * PDC Preselection method "A" format encoded on Teletext
	 * pages. This number consists of 2 hex digits for the
	 * country code and 3 bcd digits for the network code.
	 *
	 * Example ZDF: 0x1D102. (German PDC-A network codes 101 ... 163
	 * correspond to 8/30-2 codes 0xC1 ... 0xFF. Other countries may
	 * use different schemes.)
	 */
	VBI_CNI_TYPE_PDC_A,

	/**
	 * PDC Preselection method "B" format encoded in Teletext
	 * packet X/26 local enhancement data (0x3cnn). X/26 transmits
	 * only the 4 lsb of the country code and the 7 lsb of
	 * the network code. To avoid ambiguity these CNIs may not
	 * use the same country and network codes as other PDC CNIs.
	 *
	 * Example BBC 1: 0x3C7F.
	 */
	VBI_CNI_TYPE_PDC_B
} vbi_cni_type;

extern unsigned int
vbi_convert_cni			(vbi_cni_type		to_type,
				 vbi_cni_type		from_type,
				 unsigned int		cni)
  _vbi_const;
extern vbi_bool
vbi_network_anonymous		(const vbi_network *	nk)
  _vbi_nonnull ((1));
extern vbi_bool
vbi_network_weakly_equal	(const vbi_network *	nk1,
				 const vbi_network *	nk2)
  _vbi_nonnull ((1, 2));
extern vbi_bool
vbi_network_equal		(const vbi_network *	nk1,
				 const vbi_network *	nk2)
  _vbi_nonnull ((1, 2));
extern vbi_bool
vbi_network_set_name		(vbi_network *		nk,
				 const char *		name)
  _vbi_nonnull ((1));
extern vbi_bool
vbi_network_set_call_sign	(vbi_network *		nk,
				 const char *		call_sign)
  _vbi_nonnull ((1));
extern vbi_bool
vbi_network_set_cni		(vbi_network *		nk,
				 vbi_cni_type		type,
				 unsigned int		cni)
  _vbi_nonnull ((1));
extern vbi_bool
vbi_network_set			(vbi_network *		dst,
				 const vbi_network *	src)
  _vbi_nonnull ((1));
extern vbi_bool
vbi_network_reset		(vbi_network *		nk)
  _vbi_nonnull ((1));
extern vbi_bool
vbi_network_copy		(vbi_network *		dst,
				 const vbi_network *	src)
  _vbi_nonnull ((1));
extern vbi_bool
vbi_network_destroy		(vbi_network *		nk)
  _vbi_nonnull ((1));
extern vbi_bool
vbi_network_init		(vbi_network *		nk)
  _vbi_nonnull ((1));
extern vbi_network *
vbi_network_dup			(const vbi_network *	nk)
  _vbi_alloc;
extern void
vbi_network_delete		(vbi_network *		nk)
  _vbi_nonnull ((1));
extern vbi_network *
vbi_network_new			(void)
  _vbi_alloc;

/** @} */

/* Private */

extern const char *
_vbi_cni_type_name		(vbi_cni_type		type)
  _vbi_const;
extern vbi_bool
_vbi_network_dump		(const vbi_network *	nk,
				 FILE *			fp)
  _vbi_nonnull ((1, 2));

VBI_END_DECLS

#endif /* __ZVBI_NETWORK_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
