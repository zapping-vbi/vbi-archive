/*
 *  libzvbi - Sliced vbi data object
 *
 *  Copyright (C) 2000, 2001 Michael H. Schimek
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

/* $Id: sliced.h,v 1.10 2007-08-30 12:29:28 mschimek Exp $ */

#ifndef __ZVBI3_SLICED_H__
#define __ZVBI3_SLICED_H__

#include <inttypes.h>		/* uintN_t */
#include "macros.h"

VBI3_BEGIN_DECLS

/**
 * @addtogroup Sliced Sliced VBI data
 * @ingroup Raw
 * @brief Definition of sliced VBI data.
 *
 * The output of the libzvbi raw VBI decoder, and input to the data
 * service decoder, is VBI data in binary format as defined in this
 * section. It is similar to the output of hardware VBI decoders
 * and VBI data transmitted in digital TV streams.
 */

/**
 * @name Data service symbols
 * @ingroup Sliced
 * @{
 */

/**
 * @anchor VBI3_SLICED_
 * No data service, blank vbi3_sliced structure.
 */
#define VBI3_SLICED_NONE			0
/**
 * Unknown data service (vbi3_dvb_demux).
 */
#define VBI3_SLICED_UNKNOWN		0
/**
 * Antiope a.k.a. Teletext System A
 *
 * Reference: <a href="http://www.itu.ch">ITU-R BT.653
 * "Teletext Systems"</a>
 *
 * vbi3_sliced payload: Last 37 bytes, without clock run-in and
 * framing code, lsb first transmitted.
 */
#define VBI3_SLICED_ANTIOPE		0x00002000
#define VBI3_SLICED_TELETEXT_A		0x00002000
#define VBI3_SLICED_TELETEXT_B_L10_625	0x00000001
#define VBI3_SLICED_TELETEXT_B_L25_625	0x00000002
/**
 * Teletext System B for 625 line systems
 *
 * Note this is separated into Level 1.0 and Level 2.5+ since the latter
 * permits occupation of scan line 6 which is frequently out of
 * range of raw VBI capture drivers. Clients should request decoding of both,
 * may then verify Level 2.5 is covered. Also sliced data can be tagged
 * as both Level 1.0 and 2.5+, i. e. VBI3_SLICED_TELETEXT_B.
 *
 * Reference: <a href="http://www.etsi.org">EN 300 706
 * "Enhanced Teletext specification"</a>, <a href="http://www.itu.ch">
 * ITU-R BT.653 "Teletext Systems"</a>
 *
 * vbi3_sliced payload: Last 42 of the 45 byte Teletext packet, that is
 * without clock run-in and framing code, lsb first transmitted.
 */
#define VBI3_SLICED_TELETEXT_B_625	(VBI3_SLICED_TELETEXT_B_L10_625	\
					 | VBI3_SLICED_TELETEXT_B_L25_625)
/* Older definition */
#define VBI3_SLICED_TELETEXT_B		VBI3_SLICED_TELETEXT_B_625
/**
 * Teletext System C for 625 line systems
 *
 * Reference: <a href="http://www.itu.ch">ITU-R BT.653
 * "Teletext Systems"</a>
 *
 * vbi3_sliced payload: Last 33 bytes, without clock run-in and
 * framing code, lsb first transmitted.
 */
#define VBI3_SLICED_TELETEXT_C_625	0x00004000
/**
 * Teletext System D for 625 line systems
 *
 * Reference: <a href="http://www.itu.ch">ITU-R BT.653
 * "Teletext Systems"</a>
 *
 * vbi3_sliced payload: Last 34 bytes, without clock run-in and
 * framing code, lsb first transmitted.
 */
#define VBI3_SLICED_TELETEXT_D_625	0x00008000
/**
 * Video Program System
 *
 * Reference: <a href="http://www.etsi.org">ETS 300 231
 * "Specification of the domestic video Programme 
 * Delivery Control system (PDC)"</a>, <a href="http://www.irt.de">
 * IRT 8R2 "Video-Programm-System (VPS)"</a>.
 *
 * vbi3_sliced payload: Byte number 3 to 15 according to Figure 9,
 * lsb first transmitted.
 */
#define VBI3_SLICED_VPS			0x00000004
/**
 * Pseudo-VPS signal transmitted on field 2
 *
 * Reference: ?
 *
 * vbi3_sliced payload: 13 bytes.
 */
#define VBI3_SLICED_VPS_F2		0x00001000
#define VBI3_SLICED_CAPTION_625_F1	0x00000008
#define VBI3_SLICED_CAPTION_625_F2	0x00000010
/**
 * Closed Caption for 625 line systems
 *
 * Note this is split into field one and two services since for basic
 * caption decoding only field one is required. Clients should compare
 * the vbi3_sliced line number, not the type to determine the source field.
 *
 * Reference: <a href="http://global.ihs.com">EIA 608
 * "Recommended Practice for Line 21 Data Service"</a>.
 *
 * vbi3_sliced payload: First and second byte including parity,
 * lsb first transmitted.
 */
#define VBI3_SLICED_CAPTION_625		(VBI3_SLICED_CAPTION_625_F1	\
					 | VBI3_SLICED_CAPTION_625_F2)
/**
 * Wide Screen Signalling for 625 line systems
 *
 * Reference: <a href="http://www.etsi.org">EN 300 294
 * "625-line television Wide Screen Signalling (WSS)"</a>.
 *
 * vbi3_sliced payload:
 * <pre>
 * Byte         0                  1
 *       msb         lsb  msb             lsb
 * bit   7 6 5 4 3 2 1 0  x x 13 12 11 10 9 8<br></pre>
 * according to EN 300 294, Table 1, lsb first transmitted. 
 */
#define VBI3_SLICED_WSS_625		0x00000400

#define VBI3_SLICED_CAPTION_525_F1	0x00000020
#define VBI3_SLICED_CAPTION_525_F2	0x00000040
/**
 * Closed Caption for 525 line systems (NTSC)
 *
 * Note this is split into field one and two services since for basic
 * caption decoding only field one is required. Clients should compare
 * the vbi3_sliced line number, not the type to determine the source field.
 * VBI3_SLICED_CAPTION_525 also covers XDS (Extended Data Service), V-Chip data
 * and ITV data.
 *
 * Reference: <a href="http://global.ihs.com">EIA 608
 * "Recommended Practice for Line 21 Data Service"</a>.
 *
 * vbi3_sliced payload: First and second byte including parity,
 * lsb first transmitted.
 */
#define VBI3_SLICED_CAPTION_525		(VBI3_SLICED_CAPTION_525_F1	\
					 | VBI3_SLICED_CAPTION_525_F2)
/**
 * Closed Caption at double bit rate for 525 line systems
 *
 * Reference: ?
 *
 * vbi3_sliced payload: First to fourth byte including parity bit,
 * lsb first transmitted.
 */
#define VBI3_SLICED_2xCAPTION_525	0x00000080
/**
 * Teletext System B for 525 line systems
 *
 * Reference: <a href="http://www.itu.ch">ITU-R BT.653
 * "Teletext Systems"</a>
 *
 * vbi3_sliced payload: Last 34 bytes, without clock run-in and
 * framing code, lsb first transmitted.
 */
#define VBI3_SLICED_TELETEXT_B_525	0x00010000
/**
 * North American Basic Teletext Specification
 * a. k. a. Teletext System C for 525 line systems
 *
 * Reference: <a href="http://global.ihs.com">EIA-516
 * "North American Basic Teletext Specification (NABTS)"</a>,
 * <a href="http://www.itu.ch">ITU-R BT.653 "Teletext Systems"</a>
 *
 * vbi3_sliced payload: Last 33 bytes, without clock run-in and
 * framing code, lsb first transmitted.
 */
#define VBI3_SLICED_NABTS		0x00000100
#define VBI3_SLICED_TELETEXT_C_525	0x00000100
/**
 * Incorrect, don't use in new code.
 */
#define VBI3_SLICED_TELETEXT_BD_525	0x00000200
/**
 * Teletext System D for 525 line systems
 *
 * Reference: <a href="http://www.itu.ch">ITU-R BT.653
 * "Teletext Systems"</a>
 *
 * vbi3_sliced payload: Last 34 bytes, without clock run-in and
 * framing code, lsb first transmitted.
 */
#define VBI3_SLICED_TELETEXT_D_525	0x00020000
/**
 * Wide Screen Signalling for NTSC Japan
 *
 * Reference: <a href="http://www.jeita.or.jp">EIA-J CPR-1204</a>
 *
 * vbi3_sliced payload:
 * <pre>
 * Byte         0                    1                  2
 *       msb         lsb  msb               lsb  msb             lsb
 * bit   7 6 5 4 3 2 1 0  15 14 13 12 11 10 9 8  x x x x 19 18 17 16
 * </pre>
 */
#define VBI3_SLICED_WSS_CPR1204		0x00000800
/**
 * No actual data service. This symbol is used to request capturing
 * of all PAL/SECAM VBI data lines from the libzvbi driver interface,
 * as opposed to just those lines
 * used to transmit the requested data services.
 */
#define VBI3_SLICED_VBI_625		0x20000000
/**
 * No actual data service. This symbol is used to request capturing
 * of all NTSC VBI data lines from the libzvbi driver interface,
 * as opposed to just those lines
 * used to transmit the requested data services.
 */
#define VBI3_SLICED_VBI_525		0x40000000
/** @} */

/**
 * @ingroup Sliced
 * Data service identifier. A logical 'or' of VBI3_SLICED_ symbols.
 */
/* NB must be u32 for compatibility with older struct vbi3_sliced. */
typedef uint32_t vbi3_service_set;

/**
 * @ingroup Sliced
 * @brief This structure holds one scan line of sliced vbi data.
 *
 * For example the contents of NTSC line 21, two bytes of Closed Caption
 * data. Usually an array of vbi3_sliced is used, covering all
 * VBI lines of the two fields of a video frame.
 */
typedef struct {
	/**
	 * A @ref VBI3_SLICED_ symbol identifying the data service. Under cirumstances
	 * (see VBI3_SLICED_TELETEXT_B) this can be a set of VBI3_SLICED_ symbols.
	 */
	vbi3_service_set		id;
	/**
	 * Source line number according to the ITU-R line numbering scheme,
	 * a value of @c 0 if the exact line number is unknown. Note that some
	 * data services cannot be reliable decoded without line number.
	 *
	 * @image html zvbi3_625.gif "ITU-R PAL/SECAM line numbering scheme"
	 * @image html zvbi3_525.gif "ITU-R NTSC line numbering scheme"
	 */
	uint32_t		line;
	/**
	 * The actual payload. See the documentation of @ref VBI3_SLICED_ symbols
	 * for details.
	 */
	uint8_t			data[56];
} vbi3_sliced;

/**
 * @addtogroup Sliced
 * @{
 */
extern const char *
vbi3_sliced_name			(vbi3_service_set	service)
  __attribute__ ((const));
extern unsigned int
vbi3_sliced_payload_bits		(vbi3_service_set	service)
  __attribute__ ((const));
/** @} */

/* Private */

struct _vbi3_service_par;	/* defined in raw_decoder.h */
typedef struct _vbi3_service_par _vbi3_service_par;

VBI3_END_DECLS

#endif /* __ZVBI3_SLICED_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
