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

/* $Id: network.h,v 1.1.2.1 2004-02-13 02:15:27 mschimek Exp $ */

#ifndef NUID_H
#define NUID_H

#include <inttypes.h>

#include "misc.h"

/* Public */

/**
 * @addtogroup Network
 * @{
 */

/**
 * For proper data caching libzvbi must assign networks a unique ID.
 * Since there is no single standard to follow we use arbitrary
 * 64 bit integers. IDs calculated by the library are random
 * negative numbers. You can store these IDs with your channel data or
 * assign custom IDs, for example an index into a channel table.
 * Custom IDs must be in range 1 ... (1 << 63) - 1.
 * The value zero must be interpreted as unknown or invalid ID.
 */
typedef int64_t vbi_nuid;

#define VBI_NUID_CUSTOM(n) ((vbi_nuid)(n) & (vbi_nuid) INT64_MAX)
#define VBI_NUID_IS_CUSTOM(nuid) ((vbi_nuid)(nuid) > 0)

#define VBI_NUID_NONE ((vbi_nuid) 0)
#define VBI_NUID_UNKNOWN ((vbi_nuid) 0)

/**
 * @brief Network description.
 *
 * All strings are ISO 8859-1, local language, and @c NUL terminated.
 * Prepare for empty strings. Read only.
 *
 * Note this structure may grow in the future.
 */
typedef struct {
	vbi_nuid		nuid;

	char			name[64];		/* descriptive name */
	char			call[40];		/* network call letters (XDS) */

	int			tape_delay;		/* tape delay, minutes (XDS) */

	/* Private */

	int			cni_vps;
	int			cni_8301;
	int			cni_8302;
	int			cni_x26;

	int			cycle;
} vbi_network;

/* Private */

extern vbi_bool
vbi_network_from_nuid		(vbi_network *		n,
				 vbi_nuid		nuid);
/* Public */

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
	/** VPS format. */
	VBI_CNI_TYPE_VPS,
	/** Teletext packet 8/30 format 1. */
	VBI_CNI_TYPE_8301,
	/** Teletext packet 8/30 format 2. */
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

#endif /* NUID_H */
