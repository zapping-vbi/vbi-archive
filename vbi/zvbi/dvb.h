/*
 *  libzvbi -- DVB definitions
 *
 *  Copyright (C) 2004, 2008 Michael H. Schimek
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

/* $Id: dvb.h,v 1.1.2.1 2008-08-19 10:56:02 mschimek Exp $ */

#ifndef DVB_H
#define DVB_H

/**
 * @internal
 * ISO 13818-1 Section 2.4.3.7, Table 2-19.
 * ISO 13818-2 Section 6.2.1, Table 6-1.
 */
enum mpeg2_start_code {
	/* 0x00 ... 0xB8 video start codes */

	PICTURE_START_CODE = 0x00,
	/* 0x01 ... 0xAF slice_start_code */
	/* 0xB0 reserved */
	/* 0xB1 reserved */
	USER_DATA_START_CODE = 0xB2,
	SEQUENCE_HEADER_CODE = 0xB3,
	SEQUENCE_ERROR_CODE = 0xB4,
	EXTENSION_START_CODE = 0xB5,
	/* 0xB6 reserved */
	SEQUENCE_END_CODE = 0xB7,
	GROUP_START_CODE = 0xB8,

	/* 0xB9 ... 0xFF system start codes */

	PRIVATE_STREAM_1 = 0xBD,
	PADDING_STREAM = 0xBE,
	PRIVATE_STREAM_2 = 0xBF,
	AUDIO_STREAM_0 = 0xC0,
	AUDIO_STREAM_31 = 0xDF,
	VIDEO_STREAM_0 = 0xE0,
	VIDEO_STREAM_15 = 0xEF,
};

/**
 * @internal
 * ISO 13818-2 Section 6.3.1, Table 6-2.
 */
enum mpeg2_extension_start_code_identifier {
	/* 0x0 reserved */
	SEQUENCE_EXTENSION_ID = 0x1,
	SEQUENCE_DISPLAY_EXTENSION_ID,
	QUANT_MATRIX_EXTENSION_ID,
	COPYRIGHT_EXTENSION_ID,
	SEQUENCE_SCALABLE_EXTENSION_ID,
	/* 0x6 reserved */
	PICTURE_DISPLAY_EXTENSION_ID = 0x7,
	PICTURE_CODING_EXTENSION_ID,
	PICTURE_SPATIAL_SCALABLE_EXTENSION_ID,
	PICTURE_TEMPORAL_SCALABLE_EXTENSION_ID,
	/* 0xB ... 0xF reserved */
};

/**
 * @internal
 * ISO 13818-2 Section 6.3.9, Table 6-12.
 */
enum mpeg2_picture_coding_type {
	/* 0 forbidden */
	I_TYPE = 1,
	P_TYPE,
	B_TYPE,
	D_TYPE,
	/* 5 ... 7 reserved */
};

/**
 * @internal
 * ISO 13818-2 Section 6.3.10, Table 6-14.
 */
enum mpeg2_picture_structure {
	/* 0 reserved */
	TOP_FIELD = 1,
	BOTTOM_FIELD,
	FRAME_PICTURE
};

/* PTSs and DTSs are 33 bits wide. */
#define MPEG2_TIMESTAMP_MASK (((int64_t) 1 << 33) - 1)

/**
 * @internal
 * EN 301 775 Section 4.3.2, Table 2.
 */
enum dvb_vbi_data_identifier {
	/* 0x00 ... 0x0F reserved. */

	/* Teletext combined with VPS and/or WSS and/or CC
	   and/or VBI sample data. (EN 300 472, 301 775) */
	DATA_ID_EBU_TELETEXT_BEGIN		= 0x10,
	DATA_ID_EBU_TELETEXT_END		= 0x20,

	/* 0x20 ... 0x7F reserved. */

	DATA_ID_USER_DEFINED1_BEGIN		= 0x80,
	DATA_ID_USER_DEFINED2_END		= 0x99,

	/* Teletext and/or VPS and/or WSS and/or CC and/or
	   VBI sample data. (EN 301 775) */
	DATA_ID_EBU_DATA_BEGIN			= 0x99,
	DATA_ID_EBU_DATA_END			= 0x9C,

	/* 0x9C ... 0xFF reserved. */
};

/**
 * @internal
 * EN 301 775 Section 4.3.2, Table 3.
 */
enum dvb_vbi_data_unit_id {
	/* 0x00 ... 0x01 reserved. */

	DATA_UNIT_EBU_TELETEXT_NON_SUBTITLE	= 0x02,
	DATA_UNIT_EBU_TELETEXT_SUBTITLE,

	/* 0x04 ... 0x7F reserved. */

	DATA_UNIT_USER_DEFINED1_BEGIN		= 0x80,
	DATA_UNIT_USER_DEFINED1_END		= 0xC0,

	/* Libzvbi private, not defined in EN 301 775. */
	DATA_UNIT_ZVBI_WSS_CPR1204		= 0xB4,
	DATA_UNIT_ZVBI_CLOSED_CAPTION_525,
	DATA_UNIT_ZVBI_MONOCHROME_SAMPLES_525,

	DATA_UNIT_EBU_TELETEXT_INVERTED		= 0xC0,

	/* EN 301 775 Table 1 says this is Teletext data,
	   Table 3 says reserved. */
	DATA_UNIT_C1				= 0xC1,

	/* 0xC2 reserved */

	DATA_UNIT_VPS				= 0xC3,
	DATA_UNIT_WSS,
	DATA_UNIT_CLOSED_CAPTION,
	DATA_UNIT_MONOCHROME_SAMPLES,

	DATA_UNIT_USER_DEFINED2_BEGIN		= 0xC7,
	DATA_UNIT_USER_DEFINED2_END		= 0xFE,

	DATA_UNIT_STUFFING			= 0xFF
};

#endif /* DVB_H */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
