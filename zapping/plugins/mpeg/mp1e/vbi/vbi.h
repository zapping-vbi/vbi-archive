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

/* $Id: vbi.h,v 1.1 2000-09-29 17:54:33 mschimek Exp $ */

#include "../common/types.h"
#include "../common/fifo.h"

extern struct vbi_parameters {
	int		start[2];
	int		count[2];
	int		samples_per_line;
	int		sampling_rate;
	bool		interlaced;
} vbi_para;

extern fifo *		vbi_cap_fifo;

/* tables.c */

extern const char * country_names_en[];

extern const struct {
	int		country;
	char *		short_name;	/* 8 chars */
	char *		long_name;
	unsigned short	cni1;		/* Packet 8/30 format 1 */
	unsigned short	cni2;		/* Packet 8/30 format 2 */
	unsigned char	a, b;		/* Packet X/26 */
} PDC_CNI[];

extern const struct {
	int		country;
	char *		short_name;
	char *		long_name;
	unsigned int	cni;
} VPS_CNI[];

extern const char *program_class[16];
extern const char *program_type[8][16];

/* decode.c */

struct decode_rec {
	unsigned int	framing_code;
	unsigned int	frc_mask;
	int		frc_bytes;
	int		step8;
	int		bit_rate;
	int		oversampling_rate;
	int		payload;
	int		thresh;
};

extern unsigned char bit_reverse[256];
extern struct { unsigned odd : 1 __attribute__ ((packed)); } parity[256];
extern char hamming84[256];
extern char biphase[256];

static inline int
unham84(unsigned char *d)
{
	int c1, c2;

	c1 = hamming84[d[0]];
	c2 = hamming84[d[1]];

	if ((int)(c1 | c2) < 0)
		return -1;

	return c2 * 16 + c1;
}

static inline int
unbip(unsigned char *d)
{
	int c1, c2;

	c1 = biphase[d[0]];
	c2 = biphase[d[1]];

	if ((int)(c1 | c2) < 0)
		return -1;

	return c1 * 16 + c2;
}

extern void		init_decoder(struct decode_rec *d,
				int raw_bytes, int sampling_rate, int bit_rate,
				unsigned int frc, unsigned int frc_mask, int payload);
extern bool		decode_nrz(struct decode_rec *d, unsigned char *raw, unsigned char *buf);

/* pdc.c */

struct pdc_rec {
	int		cni;	/* country and network identifier */
	int		pil;	/* programme identification label */
	int		lci;	/* label channel identifier */
	int		luf;	/* label update flag */
	int		prf;	/* prepare-to-record flag */
	int		mi;	/* mode identifier */
};

extern void		pdc_update(struct pdc_rec *r);
extern void		decode_vps(unsigned char *buf);
extern void		decode_pdc(unsigned char *buf);

/* subtitles.c */

extern unsigned char	stuffing_packet[2][46];

extern int		init_dvb_packet_filter(char *s);
extern int		dvb_packet_filter(unsigned char *p, unsigned char *buf,
				int line, int magazine, int packet);
