/*
 *  libzvbi -- Teletext Independent Data Line packet demultiplexer
 *
 *  Copyright (C) 2005 Michael H. Schimek
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

/* $Id: idl_demux.h,v 1.3.2.8 2008-02-27 22:51:26 mschimek Exp $ */

#ifndef __ZVBI3_IDL_DEMUX_H__
#define __ZVBI3_IDL_DEMUX_H__

#include <stdio.h>		/* FILE */
#include <inttypes.h>		/* uint8_t */
#include "macros.h"
#include "sliced.h"

VBI3_BEGIN_DECLS

/* Public */

/**
 * @addtogroup IDLDemux Teletext IDL packet demultiplexer
 * @ingroup LowDec
 * @brief Functions to decode data transmissions in Teletext
 *   Independent Data Line packets (EN 300 708 section 6).
 * @{
 */

/**
 * @brief IDL demultiplexer context.
 *
 * The contents of this structure are private.
 *
 * Call vbi3_idl_demux_new() to allocate an IDL
 * demultiplexer context.
 */
typedef struct _vbi3_idl_demux vbi3_idl_demux;
/** @} */

/**
 * @addtogroup IDLDemux Teletext IDL packet demultiplexer
 * @name vbi3_idl_demux_cb flags
 * @{
 */

/**
 * Data was lost (not received or uncorrectable) between the current and
 * previous vbi3_idl_demux_feed() call.
 */
#define VBI3_IDL_DATA_LOST	(1 << 0)

/**
 * IDL Format A flag (EN 300 708 section 6.5.2): The data may require
 * the use of data in other channels or with other service packet
 * addresses as defined by the application.
 */
#define VBI3_IDL_DEPENDENT	(1 << 3)

/** @} */

/**
 * @addtogroup IDLDemux Teletext IDL packet demultiplexer
 * @{
 */

/**
 * @param dx IDL demultiplexer returned by
 *   vbi3_idl_a_demux_new() and given to vbi3_idl_demux_feed().
 * @param buffer Pointer to received user data.
 * @param n_bytes Number of bytes in the buffer. Can be @c 0 if
 *   the decoded packet did not contain user data.
 * @param flags @c VBI3_IDL_DATA_LOST, @c VBI3_IDL_DEPENDENT.
 * @param user_data User pointer passed to vbi3_idl_demux_new().
 * 
 * The vbi3_idl_demux_feed() function calls a function of this type
 * after successfully decoding an IDL packet.
 *
 * @returns
 * FALSE to abort vbi3_idl_demux_feed() and return FALSE.
 */
typedef vbi3_bool
vbi3_idl_demux_cb		(vbi3_idl_demux *	dx,
				 const uint8_t *	buffer,
				 unsigned int		n_bytes,
				 unsigned int		flags,
				 void *			user_data);

extern void
vbi3_idl_demux_reset		(vbi3_idl_demux *	dx)
  _vbi3_nonnull ((1));
extern vbi3_bool
vbi3_idl_demux_feed		(vbi3_idl_demux *	dx,
				 const uint8_t		buffer[42])
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  _vbi3_nonnull ((1, 2))
#endif
  ;
extern vbi3_bool
vbi3_idl_demux_feed_frame	(vbi3_idl_demux *	dx,
				 const vbi3_sliced *	sliced,
				 unsigned int		n_lines)
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  _vbi3_nonnull ((1, 2))
#endif
  ;
extern void
vbi3_idl_demux_delete		(vbi3_idl_demux *	dx);
extern vbi3_idl_demux *
vbi3_idl_a_demux_new		(unsigned int		channel,
				 unsigned int		address,
				 vbi3_idl_demux_cb *	callback,
				 void *			user_data)
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  _vbi3_alloc _vbi3_nonnull ((3))
#endif
  ;

/** @} */

/* Private */

/** @internal */
#define _VBI3_IDL_FORMAT_A		(1 << 0)
#define _VBI3_IDL_FORMAT_B		(1 << 1)
#define _VBI3_IDL_FORMAT_DATAVIDEO	(1 << 2)
#define	_VBI3_IDL_FORMAT_AUDETEL		(1 << 3)
#define	_VBI3_IDL_FORMAT_LBRA		(1 << 4)

/** @internal */
typedef unsigned int _vbi3_idl_format;

/**
 * @internal
 */
struct _vbi3_idl_demux {
	_vbi3_idl_format		format;

	/** Filter out packets of this channel, with this address. */
	int			channel;
	int			address;

	/** Expected next continuity indicator. */
	int			ci;

	/** Expected next repeat indicator. */
	int			ri;

	unsigned int		flags;

	vbi3_idl_demux_cb *	callback;
	void *			user_data;
};

extern void
_vbi3_idl_demux_destroy		(vbi3_idl_demux *	dx)
  _vbi3_nonnull ((1));
extern vbi3_bool
_vbi3_idl_demux_init		(vbi3_idl_demux *	dx,
				 _vbi3_idl_format	format,
				 unsigned int		channel,
				 unsigned int		address,
				 vbi3_idl_demux_cb *	callback,
				 void *			user_data)
  _vbi3_nonnull ((1, 5));

VBI3_END_DECLS

#endif /* __ZVBI3_IDL_DEMUX_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
