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

/* $Id: aspect_ratio.h,v 1.1.2.5 2004-07-09 16:10:51 mschimek Exp $ */

#ifndef __ZVBI_ASPECT_RATIO_H__
#define __ZVBI_ASPECT_RATIO_H__

#include <stdio.h>		/* FILE */
#include "macros.h"
#include "sampling.h"		/* vbi_videostd_set */

VBI_BEGIN_DECLS

/**
 * @addtogroup AspectRatio
 * @{
 */

/**
 * @brief Caption / subtitle information.
 */
typedef enum {
	/**
	 * Nothing known about caption / subtitles.
	 */
	VBI_SUBTITLES_UNKNOWN,
	/**
	 * No caption / subtitles available.
	 */
	VBI_SUBTITLES_NONE,
	/**
	 * Open caption / subtitles inserted in active picture.
	 */
	VBI_SUBTITLES_ACTIVE,
	/**
	 * Open caption / subtitles inserted in upper or lower
	 * letterbox bar.
	 */
	VBI_SUBTITLES_MATTE,
	/**
	 * Closed caption / subtitles encoded in VBI.
	 */
	VBI_SUBTITLES_CLOSED,
} vbi_subtitles;

/* in wss.c */
extern const char *
vbi_subtitles_name		(vbi_subtitles		subtitles);

/**
 * @brief Information about the picture aspect ratio and open
 *   caption or subtitles.
 */
typedef struct {
	/**
	 * First scan line of active video, of the first and second field
	 * respectively, according to the ITU-R line numbering scheme
	 * (see vbi_sliced).
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
	/**
	 * @c TRUE when the source is known to be film transferred to
	 * video, as opposed to interlaced video from a video camera.
	 */
 	vbi_bool		film_mode;
	int			reserved1[6];
	/**
	 * Open subtitles are inserted into the picture:
	 * VBI_SUBTITLES_UNKNOWN, NONE, ACTIVE or MATTE.
	 */
	vbi_subtitles		open_subtitles;
	/**
	 * Closed subtitles are encoded in VBI:
	 * VBI_SUBTITLES_UNKNOWN, NONE, CLOSED.
	 *
	 * Both open and closed subtitles may be present.
	 */
	vbi_subtitles		closed_subtitles;
} vbi_aspect_ratio;

/* in wss.c */
extern void
vbi_aspect_ratio_destroy	(vbi_aspect_ratio *	ar);
extern void
vbi_aspect_ratio_init		(vbi_aspect_ratio *	ar,
				 vbi_videostd_set	videostd_set);

/* Private */

/* in wss.c */
extern void
_vbi_aspect_ratio_dump		(const vbi_aspect_ratio *ar,
				 FILE *			fp);

/** @} */

VBI_END_DECLS

#endif /* __ZVBI_ASPECT_RATIO_H__ */
