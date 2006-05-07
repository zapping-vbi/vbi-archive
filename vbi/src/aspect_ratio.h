/*
 *  libzvbi - Video Aspect Ratio Information
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

/* $Id: aspect_ratio.h,v 1.1.2.6 2006-05-07 06:04:58 mschimek Exp $ */

#ifndef __ZVBI3_ASPECT_RATIO_H__
#define __ZVBI3_ASPECT_RATIO_H__

#include <stdio.h>		/* FILE */
#include "macros.h"
#include "sampling_par.h"	/* vbi3_videostd_set */

VBI3_BEGIN_DECLS

/**
 * @addtogroup AspectRatio
 * @{
 */

/**
 * @brief Caption / subtitle information.
 */
typedef enum {
	/** Nothing known about caption / subtitles. */
	VBI3_SUBTITLES_UNKNOWN,

	/** No caption / subtitles available. */
	VBI3_SUBTITLES_NONE,

	/** Open caption / subtitles inserted in active picture. */
	VBI3_SUBTITLES_ACTIVE,

	/**
	 * Open caption / subtitles inserted in upper or lower
	 * letterbox bar.
	 */
	VBI3_SUBTITLES_MATTE,

	/** Closed caption / subtitles encoded in VBI. */
	VBI3_SUBTITLES_CLOSED,
} vbi3_subtitles;

/* in wss.c */
extern const char *
vbi3_subtitles_name		(vbi3_subtitles		subtitles);

/**
 * @brief WSS 625 (EN 300 294) aspect ratio codes.
 */
typedef enum {
	/** Full format 4:3 video. */
	VBI3_WSS625_ASPECT_4_3 = 0,

	/**
	 * 14:9 aspect ratio, black bars at top and bottom of the picture.
	 */ 
	VBI3_WSS625_ASPECT_14_9,

	/** 14:9 aspect ratio, black bar at the bottom of the picture. */ 
	VBI3_WSS625_ASPECT_14_9_TOP,

	/** 16:9 aspect ratio, black bars. */
	VBI3_WSS625_ASPECT_16_9,

	/** 16:9 aspect ratio, black bar at the bottom of the picture. */
	VBI3_WSS625_ASPECT_16_9_TOP,

	/** Aspect ratio > 16:9. */
	VBI3_WSS625_ASPECT_16_9_PLUS,

	/** 14:9 aspect ratio without black bars (shot in 4:3). */
	VBI3_WSS625_ASPECT_14_9_SOFT_MATTE,

	/** 16:9 anamorphic video. */
	VBI3_WSS625_ASPECT_16_9_ANAMORPHIC
} vbi3_wss625_aspect;

/**
 * @brief Information about the picture aspect ratio and open
 *   caption or subtitles.
 */
typedef struct {
	/**
	 * First scan line of active video, of the first and second field
	 * respectively, according to the ITU-R line numbering scheme
	 * (see vbi3_sliced).
	 */
	unsigned int		start[2];

	/**
	 * Number of scan lines of active video, of the first and second
	 * field respectively. This excludes letterbox bars. Full format
	 * video has 240 or 288 scan lines per field, depending on the
	 * video standard.
	 */
	unsigned int		count[2];

	/**
	 * The picture aspect ratio in <em>anamorphic</em> mode, usually
	 * 3/4. Full format or letterboxed video has aspect ratio 1/1.
	 */
	double			ratio;

	/** WSS 625 (EN 300 294) aspect ratio code. */
	vbi3_wss625_aspect	wss625_aspect;

	/**
	 * @c TRUE when the source is film transferred to video, as
	 * opposed to interlaced video from a video camera
	 * (EN 300 294 for PAL Plus).
	 */
 	vbi3_bool		film_mode;

	/**
	 * @c TRUE if the picture is PAL Plus Motion Adaptive Color Plus
	 * modulated.
	 */
	vbi3_bool		palplus_macp;

	/** @c TRUE if a PAL Plus modulated helper is present. */
	vbi3_bool		palplus_helper;

	/** @c TRUE if surround sound is transmitted. */
	vbi3_bool		surround_sound;

	/**
	 * @c TRUE if a copyright is asserted, @c FALSE if not
	 * or status unknown (EN 300 294 WSS).
	 */
	vbi3_bool		copyright_asserted;
                                                                        
	/** @c TRUE if copying is restricted (EN 300 294 WSS). */
	vbi3_bool		copying_restricted;

	vbi3_bool		_reserved1[2];

	/**
	 * Open subtitles are inserted into the picture:
	 * VBI3_SUBTITLES_UNKNOWN, NONE, ACTIVE or MATTE.
	 */
	vbi3_subtitles		open_subtitles;

	/**
	 * Closed subtitles are encoded in VBI:
	 * VBI3_SUBTITLES_UNKNOWN, NONE, CLOSED.
	 *
	 * Both open and closed subtitles may be present.
	 */
	vbi3_subtitles		closed_subtitles;

	void *			_reserved2[2];
	int			_reserved3[2];
} vbi3_aspect_ratio;

/* in wss.c */
extern void
vbi3_aspect_ratio_destroy	(vbi3_aspect_ratio *	ar);
extern void
vbi3_aspect_ratio_init		(vbi3_aspect_ratio *	ar,
				 vbi3_videostd_set	videostd_set);

/* Private */

/* in wss.c */
extern void
_vbi3_aspect_ratio_dump		(const vbi3_aspect_ratio *ar,
				 FILE *			fp);

/** @} */

VBI3_END_DECLS

#endif /* __ZVBI3_ASPECT_RATIO_H__ */
