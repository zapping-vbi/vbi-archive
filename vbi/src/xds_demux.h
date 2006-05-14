/*
 *  libzvbi - Extended Data Service demultiplexer
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

/* $Id: xds_demux.h,v 1.1.2.6 2006-05-14 14:14:12 mschimek Exp $ */

#ifndef __ZVBI3_XDS_DEMUX_H__
#define __ZVBI3_XDS_DEMUX_H__

#include <inttypes.h>		/* uint8_t */
#include <stdio.h>		/* FILE */
#include "macros.h"

VBI3_BEGIN_DECLS

/**
 * @addtogroup XDSDemux
 * @{
 */

/**
 * @brief XDS packet class.
 * XDS data is transmitted in packets. Each packet belongs
 * to one of seven classes.
 */
typedef enum {
	/** Describes the program currently transmitted. */
	VBI3_XDS_CLASS_CURRENT = 0,

	/** Describes a later program. */
	VBI3_XDS_CLASS_FUTURE,

	/**
	 * Non-program specific information about the
	 * transmitting channel.
	 */
	VBI3_XDS_CLASS_CHANNEL,

	/** Miscellaneous other information. */
	VBI3_XDS_CLASS_MISCELLANEOUS,

	/** Public service messages, including weather warnings. */
	VBI3_XDS_CLASS_PUBLIC_SERVICE,

	/** Reserved for future extensions. */
	VBI3_XDS_CLASS_RESERVED,

	/** For use in closed systems. */
	VBI3_XDS_CLASS_PRIVATE,

	VBI3_XDS_MAX_CLASSES
} vbi3_xds_class;

/* Compatibility. */
#define VBI3_XDS_CLASS_MISC VBI3_XDS_CLASS_MISCELLANEOUS
#define VBI3_XDS_CLASS_UNDEFINED VBI3_XDS_CLASS_PRIVATE

/**
 * @brief @c VBI3_XDS_CLASS_CURRENT and @c VBI3_XDS_CLASS_FUTURE subclass.
 */
typedef enum {
	VBI3_XDS_PROGRAM_ID = 1,
	VBI3_XDS_PROGRAM_LENGTH,
	VBI3_XDS_PROGRAM_NAME,
	VBI3_XDS_PROGRAM_TYPE,
	VBI3_XDS_PROGRAM_RATING,
	VBI3_XDS_PROGRAM_AUDIO_SERVICES,
	VBI3_XDS_PROGRAM_CAPTION_SERVICES,
	VBI3_XDS_PROGRAM_CGMS,
	VBI3_XDS_PROGRAM_ASPECT_RATIO,
	VBI3_XDS_PROGRAM_DESCRIPTION_BEGIN = 0x10,
	VBI3_XDS_PROGRAM_DESCRIPTION_END = 0x18,
} vbi3_xds_subclass_program;

/** @brief @c VBI3_XDS_CLASS_CHANNEL subclass. */
typedef enum {
	VBI3_XDS_CHANNEL_NAME = 1,
	VBI3_XDS_CHANNEL_CALL_LETTERS,
	VBI3_XDS_CHANNEL_TAPE_DELAY,
} vbi3_xds_subclass_channel;

/** @brief @c VBI3_XDS_CLASS_MISC subclass. */
typedef enum {
	VBI3_XDS_MISC_TIME_OF_DAY = 1,
	VBI3_XDS_MISC_IMPULSE_CAPTURE_ID,
	VBI3_XDS_MISC_SUPPLEMENTAL_DATA_LOCATION,
	VBI3_XDS_MISC_LOCAL_TIME_ZONE,
} vbi3_xds_subclass_misc;

#define VBI3_XDS_MAX_SUBCLASSES 0x80

/**
 * @brief Generic XDS subclass.
 * You must cast to the appropriate
 * subclass type depending on the XDS class.
 */
typedef unsigned int vbi3_xds_subclass;

/**
 * @brief XDS Packet passed to the XDS demux callback.
 */
typedef struct {
	vbi3_xds_class		xds_class;
	vbi3_xds_subclass	xds_subclass;

	/** XDS packets have variable length 1 ... 32 bytes. */
	unsigned int		buffer_size;

	/**
	 * Packet data. Bit 7 (odd parity) is cleared,
	 * buffer[buffer_size] is 0.
	 */
	uint8_t			buffer[36];
} vbi3_xds_packet;

/* Private */

extern void
_vbi3_xds_packet_dump		(const vbi3_xds_packet *	xp,
				 FILE *			fp)
  __attribute__ ((_vbi3_nonnull (1, 2)));

/**
 * @brief XDS demultiplexer.
 *
 * The contents of this structure are private.
 * Call vbi3_xds_demux_new() to allocate a XDS demultiplexer.
 */
typedef struct _vbi3_xds_demux vbi3_xds_demux;

/**
 * @param xd XDS demultiplexer context allocated with vbi3_xds_demux_new().
 * @param user_data User data pointer given to vbi3_xds_demux_new().
 * @param xp Pointer to the received XDS data packet.
 * 
 * The XDS demux calls a function of this type when an XDS packet
 * has been completely received, all bytes have correct parity and the
 * packet checksum is correct. Other packets are discarded.
 */
typedef vbi3_bool
vbi3_xds_demux_cb		(vbi3_xds_demux *	xd,
				 const vbi3_xds_packet *	xp,
				 void *			user_data);

extern void
vbi3_xds_demux_reset		(vbi3_xds_demux *	xd)
  __attribute__ ((_vbi3_nonnull (1)));
extern vbi3_bool
vbi3_xds_demux_feed		(vbi3_xds_demux *	xd,
				 const uint8_t		buffer[2])
  __attribute__ ((_vbi3_nonnull (1, 2)));
extern const char *
vbi3_xds_demux_errstr		(vbi3_xds_demux *	xd)
  __attribute__ ((_vbi3_nonnull (1)));
extern void
vbi3_xds_demux_delete		(vbi3_xds_demux *	xd);
extern vbi3_xds_demux *
vbi3_xds_demux_new		(vbi3_xds_demux_cb *	callback,
				 void *			user_data)
  __attribute__ ((_vbi3_alloc));

/* Private */

/** @internal */
typedef struct {
	/** Bytes received so far, without start codes. */
	uint8_t			buffer[32];

	/** Number of bytes received, including start codes. */
	unsigned int		count;

	/** Sum of bytes received, including start codes. */
	unsigned int		checksum;
} _vbi3_xds_subpacket;

/** @internal */
struct _vbi3_xds_demux {
	/**
	 * Presumably the XDS spec prohibits multiplexing
	 * of packets but I don't have a copy.
	 */
	_vbi3_xds_subpacket	subpacket[VBI3_XDS_MAX_CLASSES]
					 [VBI3_XDS_MAX_SUBCLASSES];

	/** Packet being received. */
	vbi3_xds_packet		curr;

	/** Pointer into subpacket array. */
	_vbi3_xds_subpacket *	curr_sp;

	char *			errstr;

	vbi3_xds_demux_cb *	callback;
	void *			user_data;
};

extern void
_vbi3_xds_demux_destroy		(vbi3_xds_demux *	xd);
extern vbi3_bool
_vbi3_xds_demux_init		(vbi3_xds_demux *	xd,
				 vbi3_xds_demux_cb *	callback,
				 void *			user_data);

/** @} */

VBI3_END_DECLS

#endif /* __ZVBI3_XDS_DEMUX_H__ */
