/*
 *  libzvbi - Extended Data Service decoder
 *
 *  Copyright (C) 2000-2004 Michael H. Schimek
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

/* $Id: xds.h,v 1.1.2.1 2004-02-13 02:15:27 mschimek Exp $ */

#ifndef XDS_H
#define XDS_H

#include <inttypes.h>

#include "misc.h"
#include "pdc.h"
#include "event.h" /* vbi_aspect_ratio */

/* Public */

typedef enum {
	VBI_XDS_CLASS_CURRENT = 0,
	VBI_XDS_CLASS_FUTURE,
	VBI_XDS_CLASS_CHANNEL,
	VBI_XDS_CLASS_MISC,
	VBI_XDS_CLASS_PUBLIC_SERVICE,
	VBI_XDS_CLASS_RESERVED,
	VBI_XDS_CLASS_UNDEFINED,
} vbi_xds_class;

#define VBI_XDS_MAX_CLASSES (VBI_XDS_CLASS_UNDEFINED + 1)

/* This is the CURRENT and FUTURE subclass. */
typedef enum {
	VBI_XDS_PROGRAM_ID = 1,
	VBI_XDS_PROGRAM_LENGTH,
	VBI_XDS_PROGRAM_NAME,
	VBI_XDS_PROGRAM_TYPE,
	VBI_XDS_PROGRAM_RATING,
	VBI_XDS_PROGRAM_AUDIO_SERVICES,
	VBI_XDS_PROGRAM_CAPTION_SERVICES,
	VBI_XDS_PROGRAM_CGMS,
	VBI_XDS_PROGRAM_ASPECT_RATIO,
	VBI_XDS_PROGRAM_DESCRIPTION_BEGIN = 0x10,
	VBI_XDS_PROGRAM_DESCRIPTION_END = 0x18,
} vbi_xds_subclass_program;

typedef enum {
	VBI_XDS_CHANNEL_NAME = 1,
	VBI_XDS_CHANNEL_CALL_LETTERS,
	VBI_XDS_CHANNEL_TAPE_DELAY,
} vbi_xds_subclass_channel;

typedef enum {
	VBI_XDS_MISC_TIME_OF_DAY = 1,
	VBI_XDS_MISC_IMPULSE_CAPTURE_ID,
	VBI_XDS_MISC_SUPPLEMENTAL_DATA_LOCATION,
	VBI_XDS_MISC_LOCAL_TIME_ZONE,
} vbi_xds_subclass_misc;

#define VBI_XDS_MAX_SUBCLASSES (0x18)

/**
 * Generic XDS subclass. You must cast to the appropriate
 * subclass type depending on the XDS class.
 */
typedef unsigned int vbi_xds_subclass;

/**
 * @brief XDS demultiplexer context.
 *
 * The contents of this structure are private.
 */
typedef struct vbi_xds_demux vbi_xds_demux;

typedef void
vbi_xds_demux_cb		(vbi_xds_demux *	xd,
				 void *			user_data,
				 vbi_xds_class		sp_class,
				 vbi_xds_subclass	sp_subclass,
				 const uint8_t *	buffer,
				 unsigned int		buffer_size);

/* Private */

typedef struct {
	uint8_t			buffer[32];
	unsigned int		count;		/* number of bytes received */
	unsigned int		checksum;	/* calculated on the fly */
} xds_subpacket;

struct vbi_xds_demux {
	xds_subpacket		subpacket[VBI_XDS_MAX_CLASSES]
					 [VBI_XDS_MAX_SUBCLASSES];

	xds_subpacket *		curr_sp;
	vbi_xds_class		curr_class;
	vbi_xds_subclass	curr_subclass;

	vbi_xds_demux_cb *	callback;
	void *			user_data;
};

extern vbi_bool
vbi_xds_demux_init		(vbi_xds_demux *	xd,
				 vbi_xds_demux_cb *	cb,
				 void *			user_data);
extern void
vbi_xds_demux_destroy		(vbi_xds_demux *	xd);

/* Public */

extern vbi_xds_demux *
vbi_xds_demux_new		(vbi_xds_demux_cb *	cb,
				 void *			user_data);
extern void
vbi_xds_demux_delete		(vbi_xds_demux *	xd);
extern void
vbi_xds_demux_demux		(vbi_xds_demux *	xd,
				 const uint8_t		buffer[2]);
extern void
vbi_xds_demux_reset		(vbi_xds_demux *	xd);

typedef unsigned int vbi_xds_date_flags; /* todo */

extern vbi_bool
vbi_decode_xds_aspect_ratio	(vbi_aspect_ratio *	ar,
				 const uint8_t *	buffer,
				 unsigned int		buffer_size);
extern vbi_bool
vbi_decode_xds_tape_delay	(unsigned int *		tape_delay,
				 const uint8_t *	buffer,
				 unsigned int		buffer_size);
extern vbi_bool
vbi_decode_xds_time_of_day	(struct tm *		tm,
				 vbi_xds_date_flags *	date_flags,
				 const uint8_t *	buffer,
				 unsigned int		buffer_size);
extern vbi_bool
vbi_decode_xds_time_zone	(unsigned int *		hours_west,
				 vbi_bool *		dso,
				 const uint8_t *	buffer,
				 unsigned int		buffer_size);
extern vbi_bool
vbi_decode_xds_impulse_capture_id
				(vbi_program_id *	pi,
				 const uint8_t *	buffer,
				 unsigned int		buffer_size);

/* Private */

void
vbi_decode_xds (vbi_xds_demux *xd,
	     void *user_data,
	     vbi_xds_class _class,
	     vbi_xds_subclass type,
	     const uint8_t *buffer,
	     unsigned int length);

#endif /* XDS_H */
