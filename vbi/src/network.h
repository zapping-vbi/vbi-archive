/*
 *  libzvbi - Network identification
 *
 *  Copyright (C) 2004-2006 Michael H. Schimek
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

/* $Id: network.h,v 1.1.2.7 2006-05-07 06:04:58 mschimek Exp $ */

#ifndef __ZVBI3_NETWORK_H__
#define __ZVBI3_NETWORK_H__

#include <stdio.h>		/* FILE */
#include <inttypes.h>		/* int64_t */
#include "macros.h"
#include "bcd.h"		/* vbi3_pgno */

VBI3_BEGIN_DECLS

/**
 * @addtogroup Network
 * @{
 */

/**
 * This structure contains information about a TV network, usually
 * data received by one of the VBI decoders.
 */
typedef struct {
	/**
	 * Name of the network, usually from XDS or a CNI table lookup.
	 * A NUL-terminated string in locale encoding. Can be @c NULL
	 * if unknown.
	 *
	 * Use vbi3_network_set_name() to change this string.
	 */
	char *			name;

	/**
	 * The call sign of the received station, from XDS data.
	 * A NUL-terminated ASCII string, can be empty if unknown.
	 *
	 * Use vbi3_network_set_call_sign() to change this string.
	 *
	 * Libzvbi examines only the call_sign, cni_vps, cni_8301,
	 * cni_8302 and user_data fields to distinguish networks.
	 */
	char			call_sign[16];

	/**
	 * Home country of the network, from a CNI table lookup or
	 * derived from the call sign. A NUL-terminated RFC 1766 / ISO
	 * 3166 ASCII string, e.g. "US", "FR", "DE". Can be empty if
	 * unknown.
	 */
	char			country_code[4];

	/**
	 * Programs on this network are tape-delayed, from XDS data.
	 */
	unsigned int		tape_delay;

	/**
	 * CNI of the network from VPS data, or from a CNI table lookup.
	 * See @c VBI3_CNI_TYPE_VPS for details. Can be zero if unknown
	 * or not applicable.
	 *
	 * Use vbi3_network_set_cni() to change this or other CNIs, and
	 * to fill unset fields from a CNI table lookup.
	 */
	unsigned int		cni_vps;

	/**
	 * CNI of the network from Teletext packet 8/30 format 1,
	 * or from a CNI table lookup. See @c VBI3_CNI_TYPE_8301 for
	 * details. Can be zero if unknown or not applicable.
	 */
	unsigned int		cni_8301;

	/**
	 * CNI of the network from Teletext packet 8/30 format 2,
	 * or from a CNI table lookup. See @c VBI3_CNI_TYPE_8302 for
	 * details. Can be zero if unknown or not applicable.
	 */
	unsigned int		cni_8302;

	/**
	 * CNI of the network used for PDC Preselection method "A",
	 * from a CNI table lookup. See @c VBI3_CNI_TYPE_PDC_A for
	 * details. Can be zero if unknown or not applicable.
	 */
	unsigned int		cni_pdc_a;

	/**
	 * CNI of the network used for PDC Preselection method "B",
	 * from a CNI table lookup. See @c VBI3_CNI_TYPE_PDC_B for
	 * details. Can be zero if unknown or not applicable.
	 */
	unsigned int		cni_pdc_b;

	/**
	 * User data pointer, for example into a frequency table.
	 * Libzvbi compares, but does not dereference, this pointer
	 * to distinguish networks.
	 *
	 * See also vbi3_network_equal().
	 */
	void *			user_data;

	void *			reserved1[4];
	unsigned int		reserved2[4];

} vbi3_network;

/**
 * The European Broadcasting Union (EBU) maintains several tables
 * of Country and Network Identification (CNI) codes. CNIs of type
 * VPS, 8/30-1 and 8/30-2 can be used to identify networks during a channel
 * scan. Programme Delivery Control (PDC) CNIs are additionally used to
 * control video recorders, in case a program is late or runs longer than
 * announced. Preselection allows users to select programs to be recorded
 * directly from Teletext program guide pages, without entering the channel
 * number, date, start and end time.
 *
 * Note the country and/or network code may be meaningless because
 * some CNIs have been arbitrarily assigned. Use vbi3_network_set_cni()
 * to look up a network and country name, or vbi3_convert_cni() to
 * convert between different CNI types.
 */
typedef enum {
	VBI3_CNI_TYPE_NONE,
	VBI3_CNI_TYPE_UNKNOWN = VBI3_CNI_TYPE_NONE,
	/**
	 * Video Programming System (VPS) format, a PDC CNI. For example
	 * from vbi3_decode_vps_cni(). Note VPS has room for only 4 lsb
	 * of the country code (0xcnn).
	 *
	 * Example ZDF: 0xDC2.
	 */
	VBI3_CNI_TYPE_VPS,
	/**
	 * Teletext packet 8/30 format 1, for example
	 * from vbi3_decode_teletext_8301_cni(). The country code is
	 * stored in the MSB, the network code in the LSB (0xccnn).
	 * Note these CNIs may use different country and network codes
	 * than the PDC CNIs.
	 *
	 * Example BBC1: 0x447F, ZDF: 0x4902.
	 */
	VBI3_CNI_TYPE_8301,
	/**
	 * Teletext packet 8/30 format 2 (PDC), for example
	 * from vbi3_decode_teletext_8302_cni(). The country code is
	 * stored in the MSB, the network code in the LSB (0xccnn).
	 *
	 * Example BBC1: 0x2C7F, ZDF: 0x1DC2.
	 */
	VBI3_CNI_TYPE_8302,
	/**
	 * PDC Preselection method "A" format encoded on Teletext
	 * pages. This number consists of 2 hex digits for the
	 * country code and 3 bcd digits for the network code.
	 *
	 * Example ZDF: 0x1D102. (German PDC-A network codes 101 ... 163
	 * correspond to 8/30-2 codes 0xC1 ... 0xFF. Other countries may
	 * use different schemes.)
	 */
	VBI3_CNI_TYPE_PDC_A,
	/**
	 * PDC Preselection method "B" format encoded in Teletext
	 * packet X/26 local enhancement data (0x3cnn). X/26 has
	 * room for only 4 lsb of the country code and 7 lsb of
	 * the network code. To avoid ambiguity these CNIs may not
	 * use the same country and network codes as other PDC CNIs.
	 *
	 * Example BBC1: 0x3C7F.
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

/**
 * @internal
 * Information to extract EPG data from Teletext program guide pages
 * without, or with incomplete, Preselection meta-data.
 */
struct _vbi3_network_pdc {
	/** First page this record refers to. */
	vbi3_pgno		first_pgno;
	/** Last page this record refers to. */
	vbi3_pgno		last_pgno;

	/**
	 * Format of the broadcast date on this page.
         * - Space matches a space character or spacing attribute.
	 * - %% matches the percent sign
	 * - %d day of month (1 ... 9 or 01 ... 31)
	 * - %m month (1 ... 9 or 01 ... 12)
	 * - %y last two digits of year (00 ... 99)
	 */
	const char *		date_format;

	/** Look for the broadcast date on this row (0 ... 24). */
	unsigned int		date_row;

	/**
	 * Format of the broadcast time on this page.
         * - Space matches a space character or spacing attribute.
	 * - %% matches the percent sign
	 * - %H hour (00 ... 23)
	 * - %k hour (1 ... 9 or 10 ... 23)
	 * - %M minute (00 ... 59)
	 */
	const char *		time_format;

	/**
	 * Times on these pages refer to midnight at the broadcast
	 * date plus this number of minutes.
	 */
	unsigned int		time_offset;

	void *			reserved1[10];
	unsigned int		reserved2[10];
};

extern void
_vbi3_network_dump		(const vbi3_network *	nk,
				 FILE *			fp)
  __attribute__ ((_vbi3_nonnull (1, 2)));
extern vbi3_bool
_vbi3_network_set_name_from_ttx_header
				(vbi3_network *		nk,
				 const uint8_t		buffer[40])
  __attribute__ ((_vbi3_nonnull (1, 2)));
extern const struct _vbi3_network_pdc *
_vbi3_network_get_pdc		(const vbi3_network *	nk)
  __attribute__ ((_vbi3_nonnull (1)));

/** @} */

VBI3_END_DECLS

#endif /* __ZVBI3_NETWORK_H__ */
