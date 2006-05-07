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

/* $Id: pdc.h,v 1.1.2.11 2006-05-07 06:04:58 mschimek Exp $ */

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

/** @brief PDC Programme Identification Label. */
typedef unsigned int vbi3_pil;

/**
 * @brief Macro to create a PDC PIL.
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
 */
enum {
	/** No program ID available, use timer to record. */
	VBI3_PIL_TIMER_CONTROL		= VBI3_PIL (15, 0, 31, 63),

	VBI3_PIL_INHIBIT_TERMINATE	= VBI3_PIL (15, 0, 30, 63),
	VBI3_PIL_INTERRUPT		= VBI3_PIL (15, 0, 29, 63),
	VBI3_PIL_CONTINUE		= VBI3_PIL (15, 0, 28, 63),
};

/* Private */

extern void
_vbi3_pil_dump			(vbi3_pil		pil,
				 FILE *			fp);
extern vbi3_bool
_vbi3_str_to_pil		(vbi3_pil *		pil,
				 const char **		inout_s);

/**
 * A program identification can be transmitted on different logical
 * channels. The first four channels correspond to the Teletext Packet
 * 8/30 format 2 Label Channel Identifier. The remaining two identify
 * VPS and XDS as source.
 */
typedef enum {
	VBI3_PID_CHANNEL_LCI_0 = 0,
	VBI3_PID_CHANNEL_LCI_1,
	VBI3_PID_CHANNEL_LCI_2,
	VBI3_PID_CHANNEL_LCI_3,
	VBI3_PID_CHANNEL_VPS,
	VBI3_PID_CHANNEL_XDS
} vbi3_pid_channel;

/**
 * @brief PDC Programme Control Status, Audio.
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
	/**
	 * Network identifier type, one of VBI3_CNI_TYPE_UNKNOWN,
	 * VBI3_CNI_TYPE_NONE, VBI3_CNI_TYPE_8302 or VBI3_CNI_TYPE_VPS.
	 */
	vbi3_cni_type		cni_type;

	/**
	 * Network identifier, valid if the source is Teletext packet
	 * 8/30 format 2 or VPS. Note 8/30 CNIs may refer to other
	 * networks.
	 */
	unsigned int		cni;

	/** Source of this information. */
	vbi3_pid_channel		channel;

	/**
	 * Month, day, hour and minute are the first announced starting
	 * time of the program.
	 *
	 * month range 1 ... 12.
	 */
	unsigned int		month;
	unsigned int		day;		/**< 1 ... 31 */
	unsigned int		hour;		/**< 0 ... 23 */
	unsigned int		minute;		/**< 0 ... 59 */

	/**
	 * PDC programme identification label, that is a packed
	 * representation of the date above, or one of the
	 * service codes. Will be VBI3_PIL_TIMER_CONTROL if no
	 * program ID is available.
	 */
	vbi3_pil			pil;

	/**
	 * Duration of the program in minutes, zero if unknown.
	 * XDS transmits this.
	 */
	unsigned int		length;

	/**
	 * PDC Label Update flag. When set this label is intented to
	 * update VCR memory, it does not refer to the current program.
	 */
	vbi3_bool		luf;

	/**
	 * PDC Mode Identifier. When set labels are 30 seconds early,
	 * such that a PID can be transmitted announcing the following
	 * program before the current program ends.
	 */
	vbi3_bool		mi;

	/**
	 * PDC Prepare-to-Record flag. When set the program this label
	 * refers to is about to start. A transition to cleared state
	 * indicates the actual start of the program.
	 */
	vbi3_bool		prf;

	/** PDC Program Control Status audio. */
	vbi3_pcs_audio		pcs_audio;

	/** PDC program type code, 0 or 0xFF if none or unknown. */
	unsigned int		pty;

	/**
	 * XDS "program is tape delayed" flag, FALSE if unknown.
	 */
	vbi3_bool		tape_delayed;
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

/* Flags?
#define PDC_BILINGUAL	(1 << 0)
#define PDC_BLACKWHITE	(1 << 1)
#define PDC_DOLBY51	(1 << 2)
#define PDC_AUDIODESCR	(1 << 3)
#define PDC_ORIGINAL	(1 << 4)
#define PDC_REPEAT	(1 << 5)
#define PDC_SIGN	(1 << 6)
#define PDC_STEREO	(1 << 7)
#define PDC_SUBTITLES	(1 << 8)
#define PDC_SURROUND	(1 << 9)
#define PDC_WIDESCREEN	(1 << 10)
*/

/**
 * @brief VCR programming from Teletext.
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
	 * may list programs on other networks.
	 */
	unsigned int		cni;

	/**
	 * year, month, day, at1_hour and at1_minute is the
	 * most recently announced starting time of a
	 * program.
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
	 * Duration of the program in minutes.
	 */
	unsigned int		length;

	/**
	 * Offset of local time from UTC in minutes.
	 */
	int			lto;

	/** PDC program type code, 0 or 0xFF if none or unknown. */
	unsigned int		pty;

	/**
	 * Conditional Access Flag.
	 * TRUE if the program is encrypted, FALSE if clear or unknown.
	 */
	vbi3_bool		caf;

	/**
	 * Program title. Networks rarely identify the title
	 * unambiguously. Expect incomplete titles and unrelated text.
	 *
	 * This is a NUL-terminated UTF-8 string.
	 */
	char *			title;

	void *			reserved1;

	/* XXX cni + pgno + subno? */
	vbi3_pgno		pgno;

	/* unsigned long	flags; */

	/**
	 * @internal
	 * Method A: position of AT-1 and up to three PTLs.
	 * Method B: position of PTL; elements 0, 2, 3 unused.
	 */
	struct _vbi3_at1_ptl	_at1_ptl[4];
} vbi3_preselection;

extern time_t
vbi3_preselection_time		(const vbi3_preselection *p)
  __attribute__ ((_vbi3_nonnull (1)));
extern void
vbi3_preselection_destroy	(vbi3_preselection *	p)
  __attribute__ ((_vbi3_nonnull (1)));
extern vbi3_bool
vbi3_preselection_copy		(vbi3_preselection *	dst,
				 const vbi3_preselection *src)
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
