/*
 *  libzvbi - Network identification
 *
 *  Copyright (C) 2004 Michael H. Schimek
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

/* $Id: network.h,v 1.3 2005-01-31 07:15:20 mschimek Exp $ */

#ifndef __ZVBI3_NETWORK_H__
#define __ZVBI3_NETWORK_H__

#include <stdio.h>		/* FILE */
#include <inttypes.h>		/* int64_t */
#include "macros.h"

VBI3_BEGIN_DECLS

/**
 * @addtogroup Network
 * @{
 */

/**
 * DOCUMENT ME
 */
typedef struct {
	/* Locale encoding, NUL-terminated. Can be NULL. */
	char *			name;

	/* NUL-terminated ASCII string, can be empty if unknown.
	   Only call_sign, cni_vps, cni_8301, cni_8302 and user_data
	   will be used by libzvbi for channel identification,
	   whichever is non-zero. */
	char			call_sign[16];

	/* NUL-terminated RFC 1766 / ISO 3166 ASCII string,
	   e.g. "GB", "FR", "DE". Can be empty if unknown. */
	char			country_code[4];

	/* XDS Info */

	unsigned int		tape_delay;

	/* VPS Info */

	unsigned int		cni_vps;

	/* Teletext Info */

	unsigned int		cni_8301;
	unsigned int		cni_8302;
	unsigned int		cni_pdc_a;
	unsigned int		cni_pdc_b;

	/* Other */

	void *			user_data;

	/* More? */

} vbi3_network;

/**
 * The European Broadcasting Union (EBU) maintains several tables
 * of Country and Network Identification codes for different
 * purposes, presumably the result of independent development.
 */
typedef enum {
	VBI3_CNI_TYPE_NONE,
	VBI3_CNI_TYPE_UNKNOWN = VBI3_CNI_TYPE_NONE,
	/** VPS format, for example from vbi3_decode_vps_cni(). */
	VBI3_CNI_TYPE_VPS,
	/**
	 * Teletext packet 8/30 format 1, for example
	 * from vbi3_decode_teletext_8301_cni().
	 */
	VBI3_CNI_TYPE_8301,
	/**
	 * Teletext packet 8/30 format 2, for example
	 * from vbi3_decode_teletext_8302_cni().
	 */
	VBI3_CNI_TYPE_8302,
	/**
	 * 5 digit PDC Preselection method "A" format
	 * encoded on Teletext pages.
	 */
	VBI3_CNI_TYPE_PDC_A,
	/**
	 * 4 digit (0x3nnn) PDC Preselection method "B" format
	 * encoded in Teletext packet X/26 local enhancement data.
	 */
	VBI3_CNI_TYPE_PDC_B,
} vbi3_cni_type;

extern const char *
vbi3_cni_type_name		(vbi3_cni_type		type)
  __attribute__ ((const));
extern unsigned int
vbi3_convert_cni			(vbi3_cni_type		to_type,
				 vbi3_cni_type		from_type,
				 unsigned int		cni)
  __attribute__ ((const));
extern vbi3_bool
vbi3_network_is_anonymous	(const vbi3_network *	nk)
  __attribute__ ((_vbi3_nonnull (1)));
extern vbi3_bool
vbi3_network_equal		(const vbi3_network *	nk1,
				 const vbi3_network *	nk2)
  __attribute__ ((_vbi3_nonnull (1, 2)));
extern vbi3_bool
vbi3_network_weak_equal		(const vbi3_network *	nk1,
				 const vbi3_network *	nk2)
  __attribute__ ((_vbi3_nonnull (1, 2)));
extern char *
vbi3_network_id_string		(const vbi3_network *	nk)
  __attribute__ ((_vbi3_nonnull (1)));
extern vbi3_bool
vbi3_network_set_name		(vbi3_network *		nk,
				 const char *		name)
  __attribute__ ((_vbi3_nonnull (1)));
extern vbi3_bool
vbi3_network_set_call_sign	(vbi3_network *		nk,
				 const char *		call_sign)
  __attribute__ ((_vbi3_nonnull (1)));
extern vbi3_bool
vbi3_network_set_cni		(vbi3_network *		nk,
				 vbi3_cni_type		type,
				 unsigned int		cni)
  __attribute__ ((_vbi3_nonnull (1)));
extern void
vbi3_network_reset		(vbi3_network *		nk)
  __attribute__ ((_vbi3_nonnull (1)));
extern void
vbi3_network_destroy		(vbi3_network *		nk)
  __attribute__ ((_vbi3_nonnull (1)));
extern vbi3_bool
vbi3_network_set			(vbi3_network *		dst,
				 const vbi3_network *	src)
  __attribute__ ((_vbi3_nonnull (1)));
extern vbi3_bool
vbi3_network_copy		(vbi3_network *		dst,
				 const vbi3_network *	src)
  __attribute__ ((_vbi3_nonnull (1)));
extern vbi3_bool
vbi3_network_init		(vbi3_network *		nk)
  __attribute__ ((_vbi3_nonnull (1)));
extern void
vbi3_network_array_delete	(vbi3_network *		nk,
				 unsigned int		n_elements);

/* Private */

extern void
_vbi3_network_dump		(const vbi3_network *	nk,
				 FILE *			fp)
  __attribute__ ((_vbi3_nonnull (1, 2)));
extern vbi3_bool
_vbi3_network_set_name_from_ttx_header
				(vbi3_network *		nk,
				 const uint8_t		buffer[40])
  __attribute__ ((_vbi3_nonnull (1, 2)));

/** @} */

VBI3_END_DECLS

#endif /* __ZVBI3_NETWORK_H__ */
