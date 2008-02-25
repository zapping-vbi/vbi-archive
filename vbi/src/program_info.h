/*
 *  libzvbi
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

/* $Id: program_info.h,v 1.1.2.4 2008-02-25 20:57:35 mschimek Exp $ */

#ifndef __ZVBI3_PROGRAM_INFO_H__
#define __ZVBI3_PROGRAM_INFO_H__

#include <stdlib.h>		/* FILE */

#include <time.h>		/* time_t */
#include "bcd.h"
#include "network.h"
#include "link.h"		/* vbi3_link */
#include "aspect_ratio.h"	/* vbi3_aspect_ratio */
#include "pdc.h"		/* vbi3_program_id */

VBI3_BEGIN_DECLS


/*
 *  Program Info
 *
 *  ATTN this is new stuff and subject to change
 */

/**
 * @ingroup Event
 * @brief Program rating source.
 *
 * If program rating information is available (also known in the
 * U. S. as V-Chip data), this describes which rating scheme is
 * being used: U. S. film, U. S. TV, Canadian English or French TV. 
 * You can convert the rating code to a string with
 * vbi3_rating_string().
 *
 * When the scheme is @c VBI3_RATING_TV_US, additionally the
 * DLSV rating flags will be set.
 */
typedef enum {
	VBI3_RATING_AUTH_NONE = 0,
	VBI3_RATING_AUTH_MPAA,
	VBI3_RATING_AUTH_TV_US,
	VBI3_RATING_AUTH_TV_CA_EN,
	VBI3_RATING_AUTH_TV_CA_FR
} vbi3_rating_auth;

/**
 * @ingroup Event
 * @name US TV rating flags
 * @{
 */
#define VBI3_RATING_D 0x08 /**< "sexually suggestive dialog" */
#define VBI3_RATING_L 0x04 /**< "indecent language" */
#define VBI3_RATING_S 0x02 /**< "sexual situations" */
#define VBI3_RATING_V 0x01 /**< "violence" */
/** @} */

extern const char *	vbi3_rating_string(vbi3_rating_auth auth, int id);

/**
 * @ingroup Event
 * @brief Program classification schemes.
 *
 * libzvbi understands two different program classification schemes,
 * the EIA-608 based in the United States and the ETS 300 231 based
 * one in Europe. You can convert the program type code into a
 * string with vbi3_prog_type_string().
 **/
typedef enum {
	VBI3_PROG_CLASSF_NONE = 0,
	VBI3_PROG_CLASSF_EIA_608,
	VBI3_PROG_CLASSF_ETS_300231
} vbi3_prog_classf;

/**
 * @addtogroup Event
 * @{
 */
extern const char *	vbi3_prog_type_string(vbi3_prog_classf classf, int id);
/** @} */

/**
 * @ingroup Event
 * @brief Type of audio transmitted on one (mono or stereo)
 * audio track.
 */

/**
 * @ingroup Event
 *
 * Information about the current program, preliminary.
 */
typedef struct {
	/*
	 *  Refers to the current or next program.
	 *  (No [2] to allow clients filtering current data more easily.)
	 */
	unsigned int		future : 1;

	/* 01 Program Identification Number */

	/* If unknown all these fields are -1 */
	signed char		month;		/* 0 ... 11 */
	signed char		day;		/* 0 ... 30 */
	signed char		hour;		/* 0 ... 23 */
	signed char		min;		/* 0 ... 59 */

	/*
	 *  VD: "T indicates if a program is routinely tape delayed for the
	 *  Mountain and Pacific time zones."
	 */
	signed char		tape_delayed;

	/* 02 Program Length */

	/* If unknown all these fields are -1 */
	signed char		length_hour;	/* 0 ... 63 */
	signed char		length_min;	/* 0 ... 59 */

	signed char		elapsed_hour;	/* 0 ... 63 */
	signed char		elapsed_min;	/* 0 ... 59 */
	signed char		elapsed_sec;	/* 0 ... 59 */

	/* 03 Program name */

	/* If unknown title[0] == 0 */
	char			title[64];	/* ASCII + '\0' */

	/* 04 Program type */

	/*
	 *  If unknown type_classf == VBI3_PROG_CLASSF_NONE.
	 *  VBI3_PROG_CLASSF_EIA_608 can have up to 32 tags
	 *  identifying 96 keywords. Their numerical value
	 *  is given here instead of composing a string for
	 *  easier filtering. Use vbi3_prog_type_str_by_id to
	 *  get the keywords. A zero marks the end.
	 */
	vbi3_prog_classf		type_classf;
	int			type_id[33];

	/* 05 Program rating */

	/*
	 *  For details STFW for "v-chip"
	 *  If unknown rating_auth == VBI3_RATING_NONE
	 */
	vbi3_rating_auth		rating_auth;
	int			rating_id;

	/* Only valid when auth == VBI3_RATING_TV_US */
	int			rating_dlsv;

	/* 06 Program Audio Services */

	/*
	 *  BTSC audio (two independent tracks) is flagged according to XDS,
	 *  Zweiton/NICAM/EIA-J audio is flagged mono/none, stereo/none or
	 *  mono/mono for bilingual transmissions.
	 */
	struct {
		/* If unknown mode == VBI3_AUDIO_MODE_UNKNOWN */
/*		vbi3_audio_mode		mode; */
		/* If unknown lang_code == NULL */
		const char *		lang_code; /* ISO 639 */
	}			audio[2];	/* primary and secondary */

	/* 07 Program Caption Services */

	/*
	 *  Bits 0...7 corresponding to Caption page 1...8.
	 *  Note for the current program this information is also
	 *  available via vbi3_classify_page().
	 *
	 *  If unknown caption_services == -1, _lang_code[] = NULL
	 */
	int			caption_services;
	const char *		caption_lang_code[8]; /* ISO 639 */

	/* 08 Copy Generation Management System */

	/* If unknown cgms_a == -1 */
	int			cgms_a; /* XXX */

	/* 09 Aspect Ratio */

	/*
	 *  Note for the current program this information is also
	 *  available via VBI3_EVENT_ASPECT.
	 *
	 *  If unknown first_line == last_line == -1, ratio == 0.0
	 */
	vbi3_aspect_ratio	aspect;

	/* 10 - 17 Program Description */

	/*
	 *  8 rows of 0...32 ASCII chars + '\0',
	 *  if unknown description[0...7][0] == 0
	 */
	char			description[8][33];
} __vbi3_program_info;

/**
 * @addtogroup Event
 * @{
 */
extern void		vbi3_reset_prog_info(__vbi3_program_info *pi);
/** @} */

/* code depends on order, don't change */
typedef enum {
	VBI3_XDS_AUDIO_NONE = 0,			/**< No sound. */
	VBI3_XDS_AUDIO_MONO,			/**< Mono audio. */
	VBI3_XDS_AUDIO_STEREO,			/**< Stereo audio. */
	VBI3_XDS_AUDIO_STEREO_SURROUND,		/**< Surround. */
	VBI3_XDS_AUDIO_SIMULATED_STEREO,		/**< ? */
	/**
	 * Spoken descriptions of the program for the blind,
	 * on a secondary audio track.
	 */
	VBI3_XDS_AUDIO_VIDEO_DESCRIPTIONS,
	/**
	 * Unrelated to the current program.
	 */
	VBI3_XDS_AUDIO_NON_PROGRAM_AUDIO,
	VBI3_XDS_AUDIO_SPECIAL_EFFECTS,		/**< ? */
	VBI3_XDS_AUDIO_DATA_SERVICE,		/**< ? */
	/**
	 * We have no information what is transmitted.
	 */
	VBI3_XDS_AUDIO_UNKNOWN
} vbi3_xds_audio;

typedef struct {
	char *			title;

	struct {
		vbi3_xds_audio		mode;
		char *			lang_code;
	}			audio[2];

	char *			description;
} vbi3_program_info;

extern const char *
vbi3_xds_audio_name		(vbi3_xds_audio		mode);

extern void
vbi3_program_info_destroy	(vbi3_program_info *	pi);
extern void
vbi3_program_info_init		(vbi3_program_info *	pi);

/* Private */

extern void
_vbi3_program_info_dump		(const vbi3_program_info *pi,
				 FILE *			fp);

VBI3_END_DECLS

#endif /* __ZVBI3_PROGRAM_INFO_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
