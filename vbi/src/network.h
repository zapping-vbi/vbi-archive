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

/* $Id: network.h,v 1.1.2.3 2004-04-08 23:36:25 mschimek Exp $ */

#ifndef __ZVBI_NETWORK_H__
#define __ZVBI_NETWORK_H__

#include <inttypes.h>		/* int64_t */
#include "macros.h"

VBI_BEGIN_DECLS

/**
 * @addtogroup Network
 * @{
 */

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

extern void
vbi_network_delete		(vbi_network *		n);
extern vbi_network *
vbi_network_new			(vbi_nuid		nuid);

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

extern vbi_nuid
vbi_nuid_from_cni		(vbi_cni_type		type,
				 unsigned int		cni);
extern unsigned int
vbi_convert_cni			(vbi_cni_type		to_type,
				 vbi_cni_type		from_type,
				 unsigned int		cni);
/** @} */

/* Private */

/* preliminary */
#define NUID0 0

extern vbi_nuid
vbi_nuid_from_call_sign		(const char *		call_sign,
				 unsigned int		length);
extern vbi_nuid
vbi_temporary_nuid		(void);

VBI_END_DECLS

#endif /* __ZVBI_NETWORK_H__ */
