/*
 *  libzvbi - Program Delivery Control
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

/* $Id: pdc.h,v 1.1.2.9 2004-07-09 16:10:53 mschimek Exp $ */

#ifndef __ZVBI_PDC_H__
#define __ZVBI_PDC_H__

#include <inttypes.h>		/* uint8_t */
#include <stdio.h>		/* FILE */
#include <time.h>		/* time_t */
#include "macros.h"
#include "network.h"		/* vbi_cni_type */

VBI_BEGIN_DECLS

/**
 * @addtogroup ProgramID
 * @{
 */

/** @brief PDC Programme Identification Label. */
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
	/** No program ID available, use timer to record. */
	VBI_PIL_TIMER_CONTROL		= VBI_PIL (15, 0, 31, 63),

	VBI_PIL_INHIBIT_TERMINATE	= VBI_PIL (15, 0, 30, 63),
	VBI_PIL_INTERRUPT		= VBI_PIL (15, 0, 29, 63),
	VBI_PIL_CONTINUE		= VBI_PIL (15, 0, 28, 63),
};

/* Private */

extern void
_vbi_pil_dump			(vbi_pil		pil,
				 FILE *			fp);

/**
 * A program identification can be transmitted on different logical
 * channels. The first four channels correspond to the Teletext Packet
 * 8/30 format 2 Label Channel Identifier. The remaining two identify
 * VPS and XDS as source.
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

/**
 */
typedef struct {
	/**
	 * Network identifier type, one of VBI_CNI_TYPE_UNKNOWN,
	 * VBI_CNI_TYPE_NONE, VBI_CNI_TYPE_8302 or VBI_CNI_TYPE_VPS.
	 */
	vbi_cni_type		cni_type;

	/**
	 * Network identifier, valid if the source is Teletext packet
	 * 8/30 format 2 or VPS. Note 8/30 CNIs may refer to other
	 * networks.
	 */
	unsigned int		cni;

	/** Source of this information. */
	vbi_pid_channel		channel;

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
	 * service codes. Will be VBI_PIL_TIMER_CONTROL if no
	 * program ID is available.
	 */
	vbi_pil			pil;

	/**
	 * Duration of the program in minutes, zero if unknown.
	 * XDS transmits this.
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

	/**
	 * PDC Prepare-to-Record flag. When set the program this label
	 * refers to is about to start. A transition to cleared state
	 * indicates the actual start of the program.
	 */
	vbi_bool		prf;

	/** PDC Program Control Status audio. */
	vbi_pcs_audio		pcs_audio;

	/** PDC program type code, 0 or 0xFF if none or unknown. */
	unsigned int		pty;

	/**
	 * XDS "program is tape delayed" flag, FALSE if unknown.
	 */
	vbi_bool		tape_delayed;
} vbi_program_id;

extern void
vbi_program_id_destroy		(vbi_program_id *	pid);
extern void
vbi_program_id_init		(vbi_program_id *	pid,
				 vbi_pid_channel	channel);

/* Private */

extern void
_vbi_program_id_dump		(const vbi_program_id *	pid,
				 FILE *			fp);

/**
 * @brief VCR programming from Teletext.
 * This structure contains PDC data of a program selected
 * from a Teletext page.
 */
typedef struct {
	/**
	 * Network identifier type, either VBI_CNI_TYPE_PDC_A
	 * or VBI_CNI_TYPE_PDC_B.
	 */
	vbi_cni_type		cni_type;

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
	int			lto;

	/** PDC program type code, 0 or 0xFF if none or unknown. */
	unsigned int		pty;

	/**
	 * Conditional Access Flag.
	 * TRUE if the program is encrypted, FALSE if clear or unknown.
	 */
	vbi_bool		caf;

	/**
	 * Program title. Networks rarely identify the title
	 * unambiguously. Expect incomplete titles and unrelated text.
	 *
	 * This is a NUL-terminated string in locale encoding.
	 */
	char *			title;

	/**
	 * @internal
	 * Method A: position of AT-1 and up to three PTLs.
	 * Method B: position of PTL; elements 0, 2, 3 unused.
	 */
	struct {
		uint8_t			row;		/**< 1 ... 23 */
		uint8_t			pad;
		uint8_t			column_begin;	/**< 0 ... 39 */
		uint8_t			column_end;	/**< 0 ... 40 */
	}			_at1_ptl[4];
} vbi_preselection;

extern time_t
vbi_preselection_time		(const vbi_preselection *p);
extern void
vbi_preselection_destroy	(vbi_preselection *	p);
extern vbi_bool
vbi_preselection_copy		(vbi_preselection *	dst,
				 const vbi_preselection *src);
extern vbi_bool
vbi_preselection_init		(vbi_preselection *	p);

/* Private */

extern void
_vbi_preselection_dump		(const vbi_preselection *p,
				 FILE *			fp);
extern void
_vbi_preselection_array_dump	(const vbi_preselection *p,
				 unsigned int		p_size,
				 FILE *			fp);
extern void
_vbi_preselection_array_delete	(vbi_preselection *	p,
				 unsigned int		p_size);
extern vbi_preselection *
_vbi_preselection_array_dup	(const vbi_preselection *p,
				 unsigned int		p_size);
extern vbi_preselection *
_vbi_preselection_array_new	(unsigned int		array_size);
extern unsigned int
_vbi_pdc_method_a		(vbi_preselection *	table,
				 unsigned int		table_size,
				 const uint8_t		lop_raw[26][40]);

/** @} */

VBI_END_DECLS

#endif /* __ZVBI_PDC_H__ */
