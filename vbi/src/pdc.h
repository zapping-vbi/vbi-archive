/*
 *  libzvbi - Program Delivery Control
 *
 *  Copyright (C) 2000, 2001, 2002, 2003, 2004 Michael H. Schimek
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

/* $Id: pdc.h,v 1.1.2.12 2006-05-14 14:14:12 mschimek Exp $ */

#ifndef __ZVBI3_PDC_H__
#define __ZVBI3_PDC_H__

#include <inttypes.h>		/* uint8_t */
#include <stdio.h>		/* FILE */
#include <time.h>		/* time_t */
#include "bcd.h"		/* vbi3_pgno */
#include "macros.h"
#include "network.h"		/* vbi3_cni_type */

VBI3_BEGIN_DECLS

/**
 * @addtogroup ProgramID
 * @{
 */

/**
 * @brief PDC Programme Identification Label.
 *
 * This is a packed representation of the announced start date and time
 * of a program.
 */
typedef unsigned int vbi3_pil;

/**
 * @brief Macro to create a PDC PIL.
 *
 * Note month and day start at 1.
 */
#define VBI3_PIL(month, day, hour, minute)				\
	(((day) << 15) | ((month) << 11) | ((hour) << 6) | (minute))

#define VBI3_PIL_MONTH(pil)	(((pil) >> 11) & 15)	/**< 1 ... 12 */
#define VBI3_PIL_DAY(pil)	((pil) >> 15)		/**< 1 ... 31 */
#define VBI3_PIL_HOUR(pil)	(((pil) >> 6) & 31)	/**< 0 ... 23 */
#define VBI3_PIL_MINUTE(pil)	((pil) & 63)		/**< 0 ... 59 */

/**
 * @brief PDC PIL Service Codes.
 *
 * Some PILs which do not represent a valid date have a special meaning.
 */
enum {
	/** XDS: End of program. */
	VBI3_PIL_END			= VBI3_PIL (15, 15, 31, 63),

	/** PDC: No program IDs available, use timer to record. */
	VBI3_PIL_TIMER_CONTROL		= VBI3_PIL (15, 0, 31, 63),

	/**
	 * PDC: End of program, no ID for current program (e.g.
	 * during program announcements).
	 */
	VBI3_PIL_INHIBIT_TERMINATE	= VBI3_PIL (15, 0, 30, 63),

	/** PDC: Interrupt recording (e.g. during a halftime pause). */
	VBI3_PIL_INTERRUPT		= VBI3_PIL (15, 0, 29, 63),

	/** PDC: Continue recording. */
	VBI3_PIL_CONTINUE		= VBI3_PIL (15, 0, 28, 63),
};

extern time_t
vbi3_pil_to_time		(vbi3_pil		pil,
				 int			seconds_east);
extern vbi3_bool
vbi3_pil_from_string		(vbi3_pil *		pil,
				 const char **		inout_s)
  __attribute__ ((_vbi3_nonnull (1, 2)));

/* Private */

extern void
_vbi3_pil_dump			(vbi3_pil		pil,
				 FILE *			fp)
  __attribute__ ((_vbi3_nonnull (2)));

/**
 * A program identification can be transmitted on different logical
 * channels. The first four channels correspond to the Teletext packet
 * 8/30 format 2 Label Channel Identifier.
 */
typedef enum {
	/** Data from Teletext packet 8/30 format 2, Label Channel 0. */
	VBI3_PID_CHANNEL_LCI_0 = 0,

	/** Data from Teletext packet 8/30 format 2, Label Channel 1. */
	VBI3_PID_CHANNEL_LCI_1,

	/** Data from Teletext packet 8/30 format 2, Label Channel 2. */
	VBI3_PID_CHANNEL_LCI_2,

	/** Data from Teletext packet 8/30 format 2, Label Channel 3. */
	VBI3_PID_CHANNEL_LCI_3,

	/** Data from a VPS packet. */
	VBI3_PID_CHANNEL_VPS,

	/**
	 * Data from an XDS current class packet (about the currently
	 * received program).
	 */
	VBI3_PID_CHANNEL_XDS_CURRENT,

	/** Data from another XDS packet (about a future program). */
	VBI3_PID_CHANNEL_XDS_FUTURE,

	VBI3_MAX_PID_CHANNELS
} vbi3_pid_channel;

/**
 * @brief PDC Programme Control Status - Audio.
 */
typedef enum {
	/** Nothing known about audio channels. */
	VBI3_PCS_AUDIO_UNKNOWN = 0,

	/** Mono audio. */
	VBI3_PCS_AUDIO_MONO,

	/** Stereo audio. */
	VBI3_PCS_AUDIO_STEREO,

	/** Primary language on left channel, secondary on right. */ 
	VBI3_PCS_AUDIO_BILINGUAL
} vbi3_pcs_audio;

/**
 * DOCUMENT ME
 */
typedef struct {
	/** Source of this information. */
	vbi3_pid_channel		channel;

	/**
	 * Network identifier type, one of @c VBI3_CNI_TYPE_UNKNOWN,
	 * @c VBI3_CNI_TYPE_NONE, @c VBI3_CNI_TYPE_8302 or @c VBI3_CNI_TYPE_VPS.
	 */
	vbi3_cni_type		cni_type;

	/**
	 * Network identifier, valid if the source is Teletext packet
	 * 8/30 format 2 or VPS. Note 8/30 CNIs may refer to other
	 * networks than the one transmitting the PID.
	 */
	unsigned int		cni;

	/**
	 * Month, day, hour and minute are the announced starting
	 * date and time of the program.
	 *
	 * month range 1 ... 12.
	 */
	unsigned int		month;
	unsigned int		day;		/**< 1 ... 31 */
	unsigned int		hour;		/**< 0 ... 23 */
	unsigned int		minute;		/**< 0 ... 59 */

	/**
	 * PDC Programme Identification Label, that is a packed
	 * representation of the date above, or one of the
	 * service codes.
	 *
	 * This field is also valid when the source is XDS.
	 */
	vbi3_pil			pil;

	/** XDS Program Length in minutes, zero if unknown. */
	unsigned int		length;

	/**
	 * PDC Label Update flag. When set this program ID is intented to
	 * update VCR memory, it does not refer to the current program.
	 */
	vbi3_bool		luf;

	/**
	 * PDC Mode Identifier. When set, program IDs are 30 seconds early,
	 * such that a PID can be transmitted announcing the following
	 * program before the current program ends.
	 */
	vbi3_bool		mi;

	/**
	 * PDC Prepare-to-Record flag. When set the program this PID
	 * refers to is about to start. A transition to cleared state
	 * indicates the actual start of the program.
	 */
	vbi3_bool		prf;

	/** PDC Program Control Status - Audio. */
	vbi3_pcs_audio		pcs_audio;

	/** PDC Program Type code, 0 or 0xFF if none or unknown. */
	unsigned int		pty;

	/**
	 * XDS "program is tape delayed" flag, FALSE if not or unknown.
	 *
	 * The actual delay is transmitted in a XDS Channel class
	 * Tape Delay packet.
	 */
	vbi3_bool		tape_delayed;

	void *			_reserved1[2];
	int			_reserved2[4];
} vbi3_program_id;

extern void
vbi3_program_id_destroy		(vbi3_program_id *	pid)
  __attribute__ ((_vbi3_nonnull (1)));
extern void
vbi3_program_id_init		(vbi3_program_id *	pid,
				 vbi3_pid_channel	channel)
  __attribute__ ((_vbi3_nonnull (1)));

/* Private */

extern void
_vbi3_program_id_dump		(const vbi3_program_id *	pid,
				 FILE *			fp)
  __attribute__ ((_vbi3_nonnull (1, 2)));

struct _vbi3_at1_ptl {
	uint8_t			row;		/**< 1 ... 23 */
	uint8_t			pad;
	uint8_t			column_begin;	/**< 0 ... 39 */
	uint8_t			column_end;	/**< 0 ... 40 */
};

typedef enum {
	/** The program is encrypted (PDC CAF Conditional Access flag). */
	VBI3_PDC_ENCRYPTED	= (1 << 0),

#if 0 /* future stuff */
	VBI3_PDC_BILINGUAL	= (1 << 1),
	VBI3_PDC_BW		= (1 << 2),
	VBI3_PDC_DOLBY51	= (1 << 3),
	VBI3_PDC_AUDIO_DESCR	= (1 << 4),
	VBI3_PDC_ORIGINAL	= (1 << 5),
	VBI3_PDC_REPEAT		= (1 << 6),
	VBI3_PDC_SIGN_LANG	= (1 << 7),
	VBI3_PDC_STEREO		= (1 << 8),
	VBI3_PDC_SUBTITLES	= (1 << 9),
	VBI3_PDC_SURROUND	= (1 << 10),
	VBI3_PDC_WIDESCREEN	= (1 << 11),
#endif
} vbi3_preselection_flags;

/**
 * @brief VCR programming from Teletext.
 *
 * This structure contains PDC data of a program selected
 * from a Teletext page.
 */
typedef struct {
	/**
	 * Network identifier type, either VBI3_CNI_TYPE_PDC_A
	 * or VBI3_CNI_TYPE_PDC_B.
	 */
	vbi3_cni_type		cni_type;

	/**
	 * Network identifier, always valid. Note Teletext pages
	 * may refer to other networks than the one transmitting this information.
	 */
	unsigned int		cni;

	/**
	 * year, month, day, at1_hour and at1_minute is the
	 * most recently announced starting date and time of the
	 * program. This time is given relative to the time zone
	 * of the intended audience.
	 *
	 * year range: 2000+.
	 */
	unsigned int		year;
	unsigned int		month;		/**< 1 ... 12 */
	unsigned int		day;		/**< 1 ... 31 */
	unsigned int		at1_hour;	/**< 0 ... 23 */
	unsigned int		at1_minute;	/**< 0 ... 59 */

	/**
	 * at2_hour and at2_minute is the first announced
	 * starting time of a program. This is what you want
	 * to compare against vbi3_program_id hour and minute.
	 *
	 * at2_hour range: 0 .. 23.
	 */
	unsigned int		at2_hour;
	unsigned int		at2_minute;	/**< 0 ... 59 */

	/**
	 * Program length in minutes.
	 */
	unsigned int		length;

	/**
	 * Offset of the time zone of the intended audience from UTC in
	 * seconds east. CET for example is 1 * 60 * 60 seconds east of
	 * UTC.
	 */
	int			seconds_east;

	/**
	 * The seconds_east field is valid (a LTO [Local Time Offset] was
	 * encoded on the page).
	 */
	vbi3_bool		seconds_east_valid;

	/** PDC Program Type code, 0 or 0xFF if none or unknown. */
	unsigned int		pty;

	vbi3_preselection_flags	flags;

	/**
	 * Program title, %c NULL if unknown. Networks rarely identify
	 * the title unambiguously. Expect incomplete titles and unrelated
	 * text.
	 *
	 * This is a NUL-terminated string in the encoding used by gettext
	 * or the current locale.
	 */
	char *			title;

	/**
	 * @internal
	 * Method A: position of AT-1 and up to three PTLs.
	 * Method B: position of PTL; elements 0, 2, 3 unused.
	 */
	struct _vbi3_at1_ptl	_at1_ptl[4];

	/* XXX cni + pgno + subno? */
	vbi3_pgno		_pgno;

	void *			_reserved1[2];
	int			_reserved2[4];
} vbi3_preselection;

extern time_t
vbi3_preselection_time		(const vbi3_preselection *p,
				 int			seconds_east)
  __attribute__ ((_vbi3_nonnull (1)));
extern vbi3_bool
vbi3_preselection_copy		(vbi3_preselection *	dst,
				 const vbi3_preselection *src)
  __attribute__ ((_vbi3_nonnull (1)));
extern void
vbi3_preselection_destroy	(vbi3_preselection *	p)
  __attribute__ ((_vbi3_nonnull (1)));
extern vbi3_bool
vbi3_preselection_init		(vbi3_preselection *	p)
  __attribute__ ((_vbi3_nonnull (1)));

/* Private */

extern void
_vbi3_preselection_dump		(const vbi3_preselection *p,
				 FILE *			fp)
  __attribute__ ((_vbi3_nonnull (1, 2)));
extern void
_vbi3_preselection_array_dump	(const vbi3_preselection *p,
				 unsigned int		n_elements,
				 FILE *			fp)
  __attribute__ ((_vbi3_nonnull (1, 3)));
extern void
_vbi3_preselection_array_delete	(vbi3_preselection *	p,
				 unsigned int		n_elements);
extern vbi3_preselection *
_vbi3_preselection_array_dup	(const vbi3_preselection *p,
				 unsigned int		n_elements)
  __attribute__ ((malloc));
extern vbi3_preselection *
_vbi3_preselection_array_new	(unsigned int		n_elements)
  __attribute__ ((malloc));
extern unsigned int
_vbi3_pdc_method_a		(vbi3_preselection *	table,
				 unsigned int		n_elements,
				 const vbi3_network *	nk,
				 vbi3_pgno		pgno,
				 const uint8_t		lop_raw[26][40])
  __attribute__ ((_vbi3_nonnull (1, 3, 5)));

/** @} */

VBI3_END_DECLS

#endif /* __ZVBI3_PDC_H__ */
