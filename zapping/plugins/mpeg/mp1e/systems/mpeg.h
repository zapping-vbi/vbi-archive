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

/* $Id: mpeg.h,v 1.1 2000-07-05 18:09:34 mschimek Exp $ */

#define PACKET_START_CODE	0x00000100L
#define ISO_END_CODE		0x000001B9L
#define PACK_START_CODE		0x000001BAL
#define SYSTEM_HEADER_CODE	0x000001BBL

#define AUDIO_STREAM_0		0xC0
#define VIDEO_STREAM_0		0xE0

#define MARKER_SCR		2
#define MARKER_DTS		1
#define MARKER_PTS_ONLY		2
#define MARKER_PTS		3

#define SYSTEM_TICKS		90000
