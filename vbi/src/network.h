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

/* $Id: network.h,v 1.1.2.4 2004-07-09 16:10:52 mschimek Exp $ */

#ifndef __ZVBI_NETWORK_H__
#define __ZVBI_NETWORK_H__

#include <stdio.h>		/* FILE */
#include <inttypes.h>		/* int64_t */
#include "macros.h"

VBI_BEGIN_DECLS

/**
 * @addtogroup Network
 * @{
 */

#if 0 // OBSOLETE
/**
 * For proper data caching libzvbi must assign networks a unique ID.
 * Since there is no single standard to follow we use arbitrary
 * 64 bit integers. IDs calculated by the library are random
 * negative numbers. You can store these IDs with your channel
 * data or assign custom IDs, for example a DVB PID or an index
 * into a channel table. Custom IDs must be in range 1 ... (1 << 63)
 * - 1. The value zero must be interpreted as unknown or invalid ID.
 */
typedef int64_t vbi_nuid;

#define VBI_NUID_CUSTOM(n) ((vbi_nuid)(n) & (vbi_nuid) INT64_MAX)
#define VBI_NUID_IS_CUSTOM(nuid) ((vbi_nuid)(nuid) > 0)

#define VBI_NUID_NONE ((vbi_nuid) 0)
#define VBI_NUID_UNKNOWN ((vbi_nuid) 0)

/**
 * @brief Network description.
 *
 * All strings are in locale format, local language, and @c NUL terminated.
 * Prepare for empty strings. Read only.
 *
 * Note this structure may grow in the future.
 */
typedef struct {
	vbi_nuid		nuid;

	char *			name;
	char *			call_sign;	/* network call letters (XDS) */
	const char *		country_code;

  int			tape_delay;	/* tape delay, minutes (XDS) */

	/* Private */

	unsigned int		cni_vps;
	unsigned int		cni_8301;
	unsigned int		cni_8302;
	unsigned int		cni_pdc_a;
	unsigned int		cni_pdc_b;

  int			cycle;
} vbi_network;

#endif

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

} vbi_network;

/**
 * The European Broadcasting Union (EBU) maintains several tables
 * of Country and Network Identification codes for different
 * purposes, presumably the result of independent development.
 */
typedef enum {
	VBI_CNI_TYPE_NONE,
	VBI_CNI_TYPE_UNKNOWN = VBI_CNI_TYPE_NONE,
	/** VPS format, for example from vbi_decode_vps_cni(). */
	VBI_CNI_TYPE_VPS,
	/**
	 * Teletext packet 8/30 format 1, for example
	 * from vbi_decode_teletext_8301_cni().
	 */
	VBI_CNI_TYPE_8301,
	/**
	 * Teletext packet 8/30 format 2, for example
	 * from vbi_decode_teletext_8302_cni().
	 */
	VBI_CNI_TYPE_8302,
	/**
	 * 5 digit PDC Preselection method "A" format
	 * encoded on Teletext pages.
	 */
	VBI_CNI_TYPE_PDC_A,
	/**
	 * 4 digit (0x3nnn) PDC Preselection method "B" format
	 * encoded in Teletext packet X/26 local enhancement data.
	 */
	VBI_CNI_TYPE_PDC_B,
} vbi_cni_type;

extern const char *
vbi_cni_type_name		(vbi_cni_type		type);
extern unsigned int
vbi_convert_cni			(vbi_cni_type		to_type,
				 vbi_cni_type		from_type,
				 unsigned int		cni);
extern vbi_bool
vbi_network_set_name		(vbi_network *		nk,
				 const char *		name);
extern vbi_bool
vbi_network_set_call_sign	(vbi_network *		nk,
				 const char *		call_sign);
extern vbi_bool
vbi_network_set_cni		(vbi_network *		nk,
				 vbi_cni_type		type,
				 unsigned int		cni);
extern void
vbi_network_reset		(vbi_network *		nk);
extern void
vbi_network_destroy		(vbi_network *		nk);
extern vbi_bool
vbi_network_copy		(vbi_network *		dst,
				 const vbi_network *	src);
extern vbi_bool
vbi_network_init		(vbi_network *		nk);
extern void
vbi_network_array_delete	(vbi_network *		nk,
				 unsigned int		nk_size);

/* Private */

extern void
_vbi_network_dump		(const vbi_network *	nk,
				 FILE *			fp);
extern vbi_bool
_vbi_network_set_name_from_ttx_header
				(vbi_network *		nk,
				 const uint8_t		buffer[40]);

/** @} */

VBI_END_DECLS

#endif /* __ZVBI_NETWORK_H__ */
