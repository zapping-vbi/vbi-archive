/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek
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

/* $Id: mpeg.h,v 1.1.1.1 2001-08-07 22:10:16 garetxe Exp $ */

#define PACKET_START_CODE		0x00000100L	// + stream_id
#define ISO_END_CODE			0x000001B9L
#define MPEG_PROGRAM_END_CODE		0x000001B9L
#define PACK_START_CODE			0x000001BAL
#define SYSTEM_HEADER_CODE		0x000001BBL

#define ALL_AUDIO_STREAMS		0xB8
#define ALL_VIDEO_STREAMS		0xB9

#define PROGRAM_STREAM_MAP		0xBC
#define PRIVATE_STREAM_1		0xBD
#define PADDING_STREAM			0xBE
#define PRIVATE_STREAM_2		0xBF
#define AUDIO_STREAM			0xC0		// 0xC0 ... 0xDF
#define VIDEO_STREAM			0xE0		// 0xE0 ... 0xEF
#define ECM_STREAM			0xF0
#define EMM_STREAM			0xF1
#define DSM_CC_STREAM			0xF2
#define ISO_13522_STREAM		0xF3
/* RESERVED_STREAM 0xF4 ... 0xFE */
#define PROGRAM_STREAM_DIRECTORY	0xFF

#define IS_AUDIO_STREAM(stream_id) (((unsigned int)(stream_id) & (~0x1F)) == AUDIO_STREAM)
#define IS_VIDEO_STREAM(stream_id) (((unsigned int)(stream_id) & (~0x0F)) == VIDEO_STREAM)

#define MARKER_SCR			2
#define MARKER_DTS			1
#define MARKER_PTS_ONLY			2
#define MARKER_PTS			3

#define SYSTEM_TICKS			90000
