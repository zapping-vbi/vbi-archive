/*
 *  libzvbi
 *
 *  Copyright (C) 2000-2004 Michael H. Schimek
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

/* $Id: pdc.h,v 1.1.2.7 2004-04-17 05:52:25 mschimek Exp $ */

#ifndef __ZVBI_PDC_H__
#define __ZVBI_PDC_H__

#include <inttypes.h>		/* uint8_t */
#include <stdio.h>		/* FILE */
#include <time.h>		/* time_t */
#include "macros.h"
#include "network.h"		/* vbi_nuid */
#include "format.h"		/* vbi_page */

VBI_BEGIN_DECLS

/**
 * @brief PDC Programme Identification Label.
 */
typedef unsigned int vbi_pil;

/**
 * @brief Macro to create a PDC PIL.
 * Note month and day start at 1.
 */
#define VBI_PIL(month, day, hour, minute)				\
	(((day) << 15) | ((month) << 11) | ((hour) << 6) | (minute))

#define VBI_PIL_MONTH(pil)	(((pil) >> 11) & 15)	/**< 1 ... 12 */
#define VBI_PIL_DAY(pil)	((pil) >> 15)		/**< 1 ... 31 */
#define VBI_PIL_HOUR(pil)	(((pil) >> 6) & 31)	/**< 0 ... 23 */
#define VBI_PIL_MINUTE(pil)	((pil) & 63)		/**< 0 ... 59 */

/**
 * @brief PDC PIL Service Codes.
 */
enum {
	VBI_PIL_TIMER_CONTROL		= VBI_PIL (15, 0, 31, 63),
	VBI_PIL_INHIBIT_TERMINATE	= VBI_PIL (15, 0, 30, 63),
	VBI_PIL_INTERRUPT		= VBI_PIL (15, 0, 29, 63),
	VBI_PIL_CONTINUE		= VBI_PIL (15, 0, 28, 63),
};

/* Private */

extern void
vbi_pil_dump			(vbi_pil		pil,
				 FILE *			fp);
/* Public */

/**
 * A program identification can be transmitted on different logical
 * channels. The first four channels correspond to the Teletext Packet
 * 8/30 format 2 Label Channel Identifier.
 * The remaining two identify VPS and XDS as source.
 */
typedef enum {
	VBI_PID_CHANNEL_LCI_0 = 0,
	VBI_PID_CHANNEL_LCI_1,
	VBI_PID_CHANNEL_LCI_2,
	VBI_PID_CHANNEL_LCI_3,
	VBI_PID_CHANNEL_VPS,
	VBI_PID_CHANNEL_XDS
} vbi_pid_channel;

/**
 * @brief PDC Programme Control Status, Audio.
 */
typedef enum {
	VBI_PCS_AUDIO_UNKNOWN = 0,
	VBI_PCS_AUDIO_MONO,
	VBI_PCS_AUDIO_STEREO,
	VBI_PCS_AUDIO_BILINGUAL
} vbi_pcs_audio;

typedef struct {
	/**
	 * Network identifier, VBI_NUID_UNKNOWN if unknown.
	 */
	vbi_nuid		nuid;

	vbi_pid_channel		channel;

	/**
	 * Month, day, hour and minute are the first announced starting
	 * time of the program.
	 *
	 * month range 0 ... 11.
	 */
	unsigned int		month;
	unsigned int		day;		/**< 0 ... 30 */
	unsigned int		hour;		/**< 0 ... 23 */
	unsigned int		minute;		/**< 0 ... 59 */

	/**
	 * PDC programme identification label, that is a packed
	 * representation of the date above, or one of the
	 * service codes.
	 */
	vbi_pil			pil;

	/**
	 * Duration of the program in minutes, zero if unknown.
	 */
	unsigned int		length;

	/**
	 * PDC Label Update flag. When set this label is intented to
	 * update VCR memory, it does not refer to the current program.
	 */
	vbi_bool		luf;

	/**
	 * PDC Mode Identifier. When set labels are 30 seconds early,
	 * such that a PID can be transmitted announcing the following
	 * program before the current program ends.
	 */
	vbi_bool		mi;

	/** PDC Prepare-to-Record flag. When set the program this label
	 * refers to is about to start. A transition to cleared state
	 * indicates the actual start of the program.
	 */
	vbi_bool		prf;

	/** PDC Program Control Status audio. */
	vbi_pcs_audio		pcs_audio;

	/** PDC program type code, 0 or 0xFF if none or unknown. */
	unsigned int		pty;

	/**
	 * XDS program is tape delayed flag, FALSE if unknown.
	 */
	vbi_bool		tape_delayed;
} vbi_program_id;

extern void
_vbi_program_id_init		(vbi_program_id *	pid,
				 vbi_pid_channel	channel);
extern void
_vbi_program_id_dump		(const vbi_program_id *	pid,
				 FILE *			fp);

/**
 * This structure contains PDC data for a program selected
 * from a Teletext page.
 */
typedef struct {
	vbi_nuid		nuid;

	/**
	 * year, month, day, at1_hour and at1_minute is the
	 * most recently announced starting time of a
	 * program.
	 *
	 * year range: 2000 ... UINT_MAX.
	 */
	unsigned int		year;
	unsigned int		month;		/**< 0 ... 11 */
	unsigned int		day;		/**< 0 ... 30 */
	unsigned int		at1_hour;	/**< 0 ... 23 */
	unsigned int		at1_minute;	/**< 0 ... 59 */

	/**
	 * at2_hour and at2_minute is the first announced
	 * starting time of a program. This is what you want
	 * to compare against vbi_program_id hour and minute.
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
//	int			lto;

//	unsigned int		pty;

//	vbi_bool		caf;

//	char			title [200];
} vbi_preselection;

/* Private */

extern void
vbi_preselection_dump		(const vbi_preselection *pl,
				 FILE *			fp);
/* Public */

extern time_t
vbi_preselection_time		(const vbi_preselection *pl);

#ifndef EVENT_H
// FIXME doesn't belong here
extern vbi_bool
vbi_page_pdc_link		(const vbi_page *	pg,
				 vbi_preselection *	pl,
				 unsigned int		column,
				 unsigned int		row);
extern vbi_bool
vbi_page_pdc_enum		(const vbi_page *	pg,
				 vbi_preselection *	pl,
				 unsigned int		index);
#endif

/* Private */

/**
 * @internal
 */
struct pdc_date {
	signed			year		: 8;	/**< 0 ... 99 */
	signed			month		: 8;	/**< 0 ... 11 */
	signed			day		: 8;	/**< 0 ... 30 */
} __attribute__ ((packed));

typedef struct pdc_date pdc_date;

/**
 * @internal
 */
struct pdc_time {
	signed			hour		: 8;	/**< 0 ... 23 */
	signed			min		: 8;	/**< 0 ... 59 */
} __attribute__ ((packed));

typedef struct pdc_time pdc_time;

/**
 * @internal
 */
struct pdc_position {
	signed			row		: 8;	/* 1 ... 23 */
	signed			column_begin	: 8;	/* 0 ... 39 */
	signed			column_end	: 8;	/* 0 ... 40 */
} __attribute__ ((packed));

typedef struct pdc_position pdc_position;

/**
 * @internal
 *
 * Internal PDC preselection record. Signed values are -1 if the
 * respective value was not given.
 */
typedef struct {
	/** Method B: year always -1 */
	pdc_date		ad;

	unsigned		pty		: 8;

	/** Method B: not given, all values -1 */
	pdc_time		at1;

	pdc_time		at2;

	/** CNI of type VBI_CNI_TYPE_PDC_A or _B. */
	signed			cni		: 32;

	/** Program duration in minutes. */
	signed			length		: 16;

	/**
	 * Method A: position of AT-1 and up to three PTLs.
	 * Method B: position of PTL; elements 0, 2, 3 unused.
	 */
	pdc_position		at1_ptl_pos[4];

	/** Local time offset in +/- 15 minute units. */
	signed			lto		: 8;

	/** Method A: not given */
	unsigned		caf		: 1;
} pdc_program;

/**
 * @internal
 * @param begin Time value.
 * @param end Time value.
 *
 * @returns
 * Absolute difference of begin and end in minutes.
 */
vbi_inline unsigned int
pdc_time_diff			(pdc_time *		begin,
				 pdc_time *		end)
{
	int d;

	d = (end->hour - begin->hour) * 60
		+ end->min - begin->min;

	if (d < 0)
		d += 24 * 60;

	return (unsigned int) d;
}

extern void
pdc_program_dump		(const pdc_program *	p,
				 FILE *			fp);
extern void
pdc_program_array_dump		(const pdc_program *	p,
				 unsigned int		size,
				 FILE *			fp);

extern unsigned int
pdc_method_a			(pdc_program *		table,
				 unsigned int		table_size,
				 const uint8_t		lop_raw[26][40]);
#ifndef EVENT_H
// FIXME doesn't belong here
extern void
vbi_page_mark_pdc		(vbi_page *		pg,
				 const pdc_program *	pbegin,
				 const pdc_program *	pend);
#endif

VBI_END_DECLS

#endif /* __ZVBI_PDC_H__ */
