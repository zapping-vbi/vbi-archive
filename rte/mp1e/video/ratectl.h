/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2002 Michael H. Schimek
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

/* $Id: ratectl.h,v 1.1 2002-09-14 04:18:55 mschimek Exp $ */

#ifndef RATECTL_H
#define RATECTL_H

#include <assert.h>
#include "mpeg.h"
#include "../common/math.h"

#define RC_DUMP 0

typedef struct {
	double			act_sum_i;	/* spatial activity sum, intra */
	double			act_sum_p;	/* spatial activity sum, inter */
	double			Ti;		/* current virt buf fullness */
	double			Tp;		/* estimated target bits / picture */
	double			Tmb;		/* estimated target bits / mblock */
	unsigned int		quant_sum;
} rc_field;

typedef struct {
} rc_type;

struct rc {
	int		ni, np, nb, ob;		/* picture types per GOP */
	long long	Ei, Ep, Eb;
	long long	gop_count;
	double		ei, ep, eb;
	int		G0, Gn;			/* estimated target bits per GOP */
	double		G4;
	int		Tavg;			/* estimated avg. bits per frame */

	unsigned int	Tmin;			/* minimum target bits / picture */

	int		R;			/* remaining bits in GOP */

	double		Xi, Xp, Xb;		/* global complexity measure */
	double		d0i, d0p, d0b;		/* virtual buffer fullness */
	double		r31;			/* reaction parameter */

	double		act_avg_i;		/* average spatial activity, intra coded */
	double		act_avg_p;		/* average spatial activity, inter coded */

	rc_field	f;
};

/*
 *  Max. successive P pictures when overriding gop_sequence
 *  (error accumulation) and max. successive B pictures we can stack up
 */
#define MAX_P_SUCC 3
#define MAX_B_SUCC 31

#define B_SHARE 1.4

static inline void
rc_picture_start(struct rc *rc, rc_field *f, picture_type type, int mb_num)
{
	switch (type) {
	case I_TYPE:
		/*
		 *  Tp = lroundn(R / (+ (ni + ei) * Xi / (Xi * 1.0)
		 *		      + (np + ep) * Xp / (Xi * 1.0)
		 *		      + (nb + eb) * Xb / (Xi * 1.4)));
		 */
		f->Tp = lroundn(rc->R / ((rc->ni + rc->ei)
					 + ((rc->np + rc->ep) * rc->Xp
					    + (rc->nb + rc->eb) * rc->Xb * (1 / B_SHARE))
					 / rc->Xi));
		f->Ti = -rc->d0i;
		break;

	case P_TYPE:
		f->Tp = lroundn(rc->R / ((rc->np + rc->ep)
					 + ((rc->ni + rc->ei) * rc->Xi
					    + (rc->nb + rc->eb) * rc->Xb * (1 / B_SHARE))
					 / rc->Xp));
		f->Ti = -rc->d0p;
		break;

	case B_TYPE:
		/*
		 *  Tp = lroundn(R / (+ (ni + ei) * Xi * 1.4 / Xb
		 *		      + (np + ep) * Xp * 1.4 / Xb
		 *		      + (nb + eb) * Xb / Xb));
		 */
		f->Tp = lroundn(rc->R / (((rc->ni + rc->ei) * rc->Xi
					  + (rc->np + rc->ep) * rc->Xp) * B_SHARE
					 / rc->Xb + (rc->nb + rc->eb)));
		f->Ti = -rc->d0b;
		break;

	default:
		FAIL("!reached");
	}

	if (f->Tp < rc->Tmin)
		f->Tp = rc->Tmin;

	f->Tmb = f->Tp / mb_num;

	f->act_sum_i = 0.0;
	f->act_sum_p = 0.0;

	f->quant_sum = 0;

	if (RC_DUMP)
		fprintf(stderr, "P%d Tp=%f Ti=%f Tmin=%8u Tavg=%8d Tmb=%f X=%f,%f,%f\n",
			type, f->Tp, f->Ti, rc->Tmin, rc->Tavg, f->Tmb, rc->Xi, rc->Xp, rc->Xb);
}

static inline int
rc_quant(struct rc *rc, rc_field *f, mb_type type,
	 double acti, double actp,
	 int bits_out, int qs, int quant_max)
{
	double t;
	int quant;

	switch (type) {
	case MB_INTRA:
		f->act_sum_i += acti;
		t = acti + rc->act_avg_i;
		acti = (acti + t) / (rc->act_avg_i + t);
		quant = lroundn((bits_out - f->Ti) * rc->r31 * acti);
		quant = saturate(quant >> qs, 1, quant_max);

		if (RC_DUMP)
			fprintf(stderr, "<< %f %f %d\n", (double) bits_out, (double) f->Ti, quant);
		break;

	case MB_FORWARD:
		f->act_sum_i += acti;
	case MB_BACKWARD:
		f->act_sum_p += actp;
		t = actp + rc->act_avg_p;
		actp = (actp + t) / (rc->act_avg_p + t);
		quant = lroundn((bits_out - f->Ti) * rc->r31 * actp);
		quant = saturate(quant >> qs, 1, quant_max);
		break;

	case MB_INTERP:
	     /* f->act_sum_i += acti; */
		f->act_sum_p += actp;
		t = actp + rc->act_avg_p;
		actp = (actp + t) / (rc->act_avg_p + t);
		quant = lroundn((bits_out - f->Ti) * rc->r31 * actp);
		/* quant = saturate(quant, 1, quant_max); */
		break;

	default:
		assert (!"reached");
		exit (EXIT_FAILURE);
	}

	f->Ti += f->Tmb;

	return quant;
}

// XXX this is no CBR/VBR anymore, but for now better than nothing.
// zap: Wed, 30 Jan 2002 11:07:55 +0100
#define RQ(d0) do { if (d0 < -2 * f->Tp) d0 = -2 * f->Tp; } while (0)

static inline void
rc_picture_end(struct rc *rc, rc_field *f, picture_type type,
	       int S, int mb_num)
{
	double mb_num_inv = 1.0 / mb_num;

	switch (type) {
	case I_TYPE:
		rc->act_avg_i = f->act_sum_i * mb_num_inv;
		rc->Xi = lroundn(S * (double) f->quant_sum * mb_num_inv);
		rc->d0i += S - f->Tp;
		RQ(rc->d0i);
		break;

	case P_TYPE:
		rc->act_avg_i = f->act_sum_i * mb_num_inv;
		rc->act_avg_p = f->act_sum_p * mb_num_inv;
		rc->Xp = lroundn(S * (double) f->quant_sum * mb_num_inv);
		rc->d0p += S - f->Tp;
		RQ(rc->d0p);
		break;

	case B_TYPE:
		/*
		 *  XXX?
		 *  BBPBBI BPBI BPI BBIBBI
		 *   *---> *--> *->  *--->
		 */
	     /* rc->act_avg_i = f->act_sum_i * mb_num_inv; */
		rc->act_avg_p = f->act_sum_p * mb_num_inv;
		rc->Xb = lroundn(S * (double) f->quant_sum * mb_num_inv);
		rc->d0b += S - f->Tp;
		RQ(rc->d0b);
		break;

	default:
		FAIL("!reached");
	}
}

#endif /* RATECTL_H */
