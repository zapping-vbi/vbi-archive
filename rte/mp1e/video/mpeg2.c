/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 2002 Michael H. Schimek
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

/* $Id: mpeg2.c,v 1.12 2005-02-25 18:31:00 mschimek Exp $ */

#include "site_def.h"

#include <assert.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>

#include "../common/profile.h"
#include "../common/math.h"
#include "../common/types.h"
#include "../common/log.h"
#include "../common/mmx.h"
#include "../common/bstream.h"
#include "../common/fifo.h"
#include "../common/alloc.h"
#include "../systems/mpeg.h"
#include "../systems/systems.h"
#include "vlc.h"
#include "dct.h"
#include "motion.h"
#include "video.h"

#ifdef OPTIONS_M2I

#define VARQ 65536.0

static int quant_max = 31;

static const unsigned char
quant_res_intra[32] __attribute__ ((SECTION("video_tables") aligned (CACHE_LINE))) =
{
	1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
	16, 16, 18, 18, 20, 20, 22, 22,	24, 24, 26, 26, 28, 28, 30, 30
};

#define fdct_intra mp1e_mmx_fdct_intra
#define mpeg2_encode_intra mp1e_p6_mpeg2_encode_intra_14

#define MB_HIST(n) (mpeg1->mb_hist[(n) + 1])

static mpeg1_context *static_context = NULL;

#if 0

Just for reference.

6.2.3.1 Picture coding extension
picture_coding_extension() {
 No. of bits Mnemonic 
 extension_start_code 32 bslbf
 extension_start_code_identifier 4 uimsbf
 f_code[0][0] /* forward horizontal */ 4 uimsbf
 f_code[0][1] /* forward vertical */ 4 uimsbf
 f_code[1][0] /* backward horizontal */ 4 uimsbf
 f_code[1][1] /* backward vertical */ 4 uimsbf
 intra_dc_precision 2 uimsbf
 picture_structure 2 uimsbf
 top_field_first 1 uimsbf
 frame_pred_frame_dct 1 uimsbf
 concealment_motion_vectors 1 uimsbf
 q_scale_type 1 uimsbf
 intra_vlc_format 1 uimsbf
 alternate_scan 1 uimsbf
 repeat_first_field 1 uimsbf
 chroma_420_type 1 uimsbf
 progressive_frame 1 uimsbf
 composite_display_flag 1 uimsbf
 if ( composite_display_flag ) {
  v_axis 1 uimsbf
  field_sequence 3 uimsbf
  sub_carrier 1 uimsbf
  burst_amplitude 7 uimsbf
  sub_carrier_phase 8 uimsbf
 }
 next_start_code()
}

6.2.4 Slice
slice() {
 No. of bits Mnemonic 
 slice_start_code 32 bslbf
 if (vertical_size > 2800)
  slice_vertical_position_extension 3 uimsbf
 if (<sequence_scalable_extension() is present in the bitstream>) {
  if (scalable_mode == data partitioning)
   priority_breakpoint 7 uimsbf
  }
 quantiser_scale_code 5 uimsbf
 if (nextbits() == 1) {
  intra_slice_flag 1 bslbf
  intra_slice 1 uimsbf
  reserved_bits 7 uimsbf
  while (nextbits() == 1) {
   extra_bit_slice /* with the value  1  */ 1 uimsbf
   extra_information_slice 8 uimsbf
  }
 }
 extra_bit_slice /* with the value  0  */ 1 uimsbf
 do { macroblock() }
 while (nextbits() != 000 0000 0000 0000 0000 0000)
 next_start_code()
}

6.2.5 Macroblock
macroblock() {
 No. of bits Mnemonic
 while (nextbits() == 0000 0001 000)
  macroblock_escape 11 bslbf
 macroblock_address_increment 1-11 vlclbf
 macroblock_modes()
 if (macroblock_quant)
  quantiser_scale_code 5 uimsbf
 if (macroblock_motion_forward || (macroblock_intra && concealment_motion_vectors))
  motion_vectors(0)
 if (macroblock_motion_backward)
  motion_vectors(1)
 if (macroblock_intra && concealment_motion_vectors)
  marker_bit 1 bslbf
 if (macroblock_pattern)
  coded_block_pattern()
 for (i=0; i<block_count; i++) {
  block(i)
 }
}

6.2.5.1 Macroblock modes
macroblock_modes() {
 No. of bits Mnemonic
 macroblock_type 1-9 vlclbf
 if ((spatial_temporal_weight_code_flag == 1) && (spatial_temporal_weight_code_table_index != 00)) {
  spatial_temporal_weight_code 2 uimsbf
 }
 if (macroblock_motion_forward || macroblock_motion_backward) {
  if (picture_structure ==  frame) {
   if (frame_pred_frame_dct == 0)
    frame_motion_type 2 uimsbf
  } else {
   field_motion_type 2 uimsbf
  }
 }
 if ((picture_structure ==  Frame picture) && (frame_pred_frame_dct == 0) && (macroblock_intra || macoblock_pattern)){
  dct_type 1 uimsbf
 }
}

6.2.5.2 Motion vectors
motion_vectors (s) {
 No. of bits Mnemonic
 if (motion_vector_count == 1) {
  if ((mv_format == field) && (dmv != 1))
   motion_vertical_field_select[0][s] 1 uimsbf
   motion_vector(0, s)
 } else {
  motion_vertical_field_select[0][s] 1 uimsbf
  motion_vector(0, s)
  motion_vertical_field_select[1][s] 1 uimsbf
  motion_vector(1, s)
 }
}

6.2.5.2.1 Motion vector
motion_vector (r, s) {
 No. of bits Mnemonic
 motion_code[r][s][0] 1-11 vlclbf
 if ((f_code[s][0] != 1) && (motion_code[r][s][0] != 0))
  motion_residual[r][s][0] 1-8 uimsbf
 if (dmv == 1)
  dmvector[0] 1-2 vlclbf
 motion_code[r][s][1] 1-11 vlclbf
 if ((f_code[s][1] != 1) && (motion_code[r][s][1] != 0))
  motion_residual[r][s][1] 1-8 uimsbf
 if (dmv == 1)
  dmvector[1] 1-2 vlclbf
}

6.2.5.3 Coded block pattern
coded_block_pattern () {
 No. of bits Mnemonic
 coded_block_pattern_420 3-9 vlclbf
 if (chroma_format == 4:2:2)
  coded_block_pattern_1 2 uimsbf
 if (chroma_format == 4:4:4)
  coded_block_pattern_2 6 uimsbf
 }

6.2.6 Block
block(i) {
 No. of bits Mnemonic
 if (pattern_code[i]) {
  if (macroblock_intra) {
   if (i<4) {
    dct_dc_size_luminance 2-9 vlclbf
    if (dct_dc_size_luminance != 0)
     dct_dc_differential 1-11 uimsbf
   } else {
    dct_dc_size_chrominance 2-10 vlclbf
    if (dct_dc_size_chrominance !=0)
     dct_dc_differential 1-11 uimsbf
   }
  } else {
   First DCT coefficient 2-24
  }
  while (nextbits() != End of block)
   Subsequent DCT coefficients 3-24
  End of block 2 or 4 vlclbf
 }
}

#endif

static inline void
picture_header			(struct bs_rec *	bs,
				 picture_type		picture_coding_type,
				 picture_struct		picture_structure,
				 unsigned int		temporal_reference,
				 unsigned int		f_code00,
				 unsigned int		f_code01,
				 unsigned int		f_code10,
				 unsigned int		f_code11)
{
	const unsigned int vbv_delay = 0xFFFF; /* TODO */ 

	bepilog (bs);

	bputl (bs, PICTURE_START_CODE, 32);

	if (picture_coding_type == I_TYPE) {
		bputl (bs, ((temporal_reference & 0x3FF) << 22)	/* temporal_reference [10] */
		       + (picture_coding_type << 19)		/* picture_coding_type [3] */
		       + ((vbv_delay & 0xFFFF) << 3)		/* vbv_delay [16] */
		       + (0 << 2)				/* extra_bit_picture */
		       + 0, 32);				/* byte align '00' */
	} else {
		bputl (bs, ((temporal_reference & 0x3FF) << 19)	/* temporal_reference [10] */
		       + (picture_coding_type << 16)		/* picture_coding_type [3] */
		       + ((vbv_delay & 0xFFFF) << 0), 29);	/* vbv_delay [16] */

		if (picture_coding_type == P_TYPE) {
			bputl (bs, (0 << 10)			/* full_pel_forward_vector */
			       + (7 << 7)			/* forward_f_code '111' */
			       + (0 << 6)			/* extra_bit_picture */
			       + 0, 11);			/* byte align '000000' */
		} else {
			bputl (bs, (0 << 10)			/* full_pel_forward_vector */
			       + (7 << 7)			/* forward_f_code '111' */
			       + (0 << 6)   			/* full_pel_backward_vector */
			       + (7 << 3)   			/* backward_f_code '111' */
			       + (0 << 2)   			/* extra_bit_picture */
			       + 0, 11);			/* byte align '00' */
		}
	}

	bputl (bs, EXTENSION_START_CODE, 32);

	bputl (bs, (PICTURE_CODING_EXTENSION_ID << 28)		/* extension_start_code_identifier [4] */
	       + (f_code00 << 24)				/* f_code[s][t] [4] */
	       + (f_code01 << 20)				/* f_code[s][t] [4] */
	       + (f_code10 << 16)				/* f_code[s][t] [4] */
	       + (f_code11 << 12)				/* f_code[s][t] [4] */
	       + (0 << 10)					/* intra_dc_precision [2] */
	       + (picture_structure << 8)			/* picture_structure [2] */
	       + (0 << 7)					/* top_field_first */
	       + (0 << 6)					/* frame_pred_frame_dct */
	       + (0 << 5)					/* concealment_motion_vectors */
	       + (0 << 4)					/* q_scale_type */
	       + (0 << 3)					/* intra_vlc_format */
	       + (1 << 2) /* XXX */				/* alternate_scan */
	       + (0 << 1)					/* repeat_first_field */
	       + (0 << 0), 32);					/* chroma_420_type */

	bputl (bs, (0 << 7)					/* progressive_frame */
	       + (0 << 6)					/* composite_display_flag */
	       + 0, 8);						/* byte align '000000' */

	bprolog (bs);
}

typedef struct {
	struct vlc_rec		vlc;
	filter_param		fp;
	rc_field		rc;
	struct mblock_hist	mb_hist[(MAX_WIDTH >> 4) + 1];
} field;

#define MB_HIST2(n) (f->mb_hist[(n) + 1])

static inline void
reset_mb_hist			(field *		f)
{
	static const struct mblock_hist m = { -100, 0, { 0, 0 }};
	int i;

	for (i = -1; i < mb_width; i++)
		f->mb_hist[i + 1] = m;
}

static inline void
head_i				(mpeg1_context *	mpeg1,
				 field *		f,
				 rte_bool		bottom,
				 rte_bool		motion)
{
	printv (3, "Encoding I field%d #%lld GOP #%d, ref=%c\n",
		bottom, video_frame_count, mpeg1->gop_frame_count, "FT"[mpeg1->referenced]);

//	swap (mpeg1->oldref, newref);

//	reset_mba ();

	reset_mb_hist (f);

	/* Picture header */

	picture_header (&f->vlc.bstream, I_TYPE, TOP_FIELD + bottom,
			mpeg1->gop_frame_count, 15, 15, 15, 15);
}

static inline void
template_slice_i		(mpeg1_context *	mpeg1,
				 field *		f,
				 rte_bool		motion)
{
	reset_dct_pred (&f->vlc);

	for (mb_col = 0; mb_col < mb_width; mb_col++) {
		struct bs_rec *bs = &f->vlc.bstream;
		struct bs_rec mark;
		unsigned int var;
		int quant;

		pr_start (41, "Filter");
		var = f->fp.func (&f->fp, mb_col, mb_row);
		pr_end (41);

		emms ();

		{
			quant = rc_quant (&mpeg1->rc, &f->rc, MB_INTRA,
					  var / VARQ + 1, 0.0,
					  bwritten (bs), 0, quant_max);

			quant = quant_res_intra[quant];
		}

		/* Encode macroblock */

		brewind (&mark, bs);

		for (;;) {
			pr_start (22, "FDCT intra");
			fdct_intra (quant); // mblock[0] -> mblock[1]
			pr_end (22);

			bepilog (bs);

			if (__builtin_expect (mb_col == 0, 0)) {
				balign (bs);
				bstartq (SLICE_START_CODE + mb_row);
				bcatq ((quant << 3)		/* quantiser_scale_code [5] */
				       + (0 << 2)		/* extra_bit_slice */
				       + (1 << 1)		/* macroblock_address_increment +1 */
				       + (1 << 0), 8);		/* macroblock_type '1' (I Intra) */
				bputq (bs, 40);
			} else if (MB_HIST2 (mb_col - 1 /* last */).quant != quant) {
				bputl (bs, (1 << 7)		/* macroblock_address_increment +1 */
				       + (1 << 5)		/* macroblock_type '01' (I Intra, Quant) */
				       + quant, 8);		/* quantiser_scale_code [5] */
			} else {
				bputl (bs, (1 << 1)		/* macroblock_address_increment +1 */
				      + (1 << 0), 2);		/* macroblock_type '1' (I Intra) */
			}

			pr_start (44, "Encode intra");

			if (__builtin_expect (!mpeg2_encode_intra (&f->vlc, mblock[1], mp1e_iscan1, 1), 1)) {
				pr_end (44);
				break;
			}

			pr_end (44);

			quant++;
			brewind (bs, &mark);

			pr_event (42, "I/intra overflow");
		}

		bprolog (bs);

		f->rc.quant_sum += quant;
		MB_HIST2 (mb_col).quant = quant;
	}

	MB_HIST2 (-1) = MB_HIST2 (mb_col - 1);

	mba_row_incr ();
}

static inline int
template_tail_i			(mpeg1_context *	mpeg1,
				 field *		f,
				 rte_bool		bottom,
				 rte_bool		motion)
{
	int S = bflush (&f->vlc.bstream);

	rc_picture_end (&mpeg1->rc, &f->rc, I_TYPE, S, mb_num);

	return S >> 3;
}

#if 0 /* unfinished */

static void
slice_i_nomc			(mpeg1_context *	mpeg1,
				 field *		f)
{
	template_slice_i (mpeg1, f, FALSE);
}

#endif

static int
picture_zero (mpeg1_context *mpeg1)
{
	struct bs_rec *bs = &video_out.bstream;
	unsigned int field;
	unsigned int mb_row;

	printv (3, "Encoding 0 picture\n");

	bepilog (bs);

	for (field = 0; field < 2; field++) {
		picture_header (bs, P_TYPE, TOP_FIELD + field,
				mpeg1->gop_frame_count + field, 1, 1, 15, 15);

		for (mb_row = 0; mb_row < mb_width; mb_row++) {
			unsigned int code, length;
			int align, i;

			bputl (bs, SLICE_START_CODE + mb_row, 32);
			bputl (bs, (1 << 7)		/* quantiser_scale_code [5] */
			       + (0 << 6)		/* extra_bit_slice */
			       + (1 << 5)		/* macroblock_address_increment +1 */
			       + (1 << 2)		/* macroblock_type '001' (P MC, Not Coded) */
			       + 3, 12);		/* 11' MV(0, 0) */

			for (i = mb_width - 2; i >= 33; i -= 33)
				bputl (bs, 0x008, 11); /* mb addr escape */

			code = mp1e_macroblock_address_increment[i].code;
			length = mp1e_macroblock_address_increment[i].length; /* 1 ... 11 */

			align = (8 - bs->n - length - 5) & 7;

			bputl (bs, ((code << 5)		/* macroblock_address_increment [length] */
				    + (1 << 2)		/* macroblock_type '001' (P MC, Not Coded) */
				    + 3)		/* '11' MV(0, 0) */
			       << align, length + align + 5);
		}
	}

	bprolog (bs);

	emms ();

	return bflush (bs) >> 3;
}

/*
 *  Sequence layer
 */

enum {
	SKIP_METHOD_ZERO_P,
	SKIP_METHOD_MUX,
	SKIP_METHOD_FAKE
};

static int
sequence_header			(mpeg1_context *	mpeg1)
{
	struct bs_rec *bs = &video_out.bstream;
	unsigned int width, height;
	unsigned int bit_rate;
	unsigned int aspect_ratio_information;

	printv (3, "Encoding sequence header\n");

	width = mpeg1->coded_width;
	assert (width > 0 && (width & 0xFFF) > 0 && width <= 0x3FFF);

	height = mpeg1->coded_height;
	assert (height > 0 && (height & 0xFFF) > 0 && height <= 0x3FFF);

	if (fabs (mpeg1->codec.codec.params.video.sample_aspect - 1.0) < 0.1)
		aspect_ratio_information = 1; /* SAR = 1 */
	else {
		/*
		 *  MPEG-2: SAR = DAR * width / height.
		 *  Three display aspect ratios are supported,
		 *  we pick the closest one.
		 */
		double DAR = mpeg1->codec.codec.params.video.sample_aspect
			     * height / width;

		if (DAR < (9/16.0 + 3/4.0) / 2.0)
			aspect_ratio_information = 2; /* DAR = 3:4 */
		else if (DAR < (1/2.21 + 9/16.0) / 2.0)
			aspect_ratio_information = 3; /* DAR = 9:16 */
		else
			aspect_ratio_information = 4; /* DAR = 1:2.21 */
	}

	bit_rate = ceil (mpeg1->bit_rate / 400.0);
	assert (bit_rate > 0 && (bit_rate & 0x3FFFF) > 0 && bit_rate <= 0x3FFFFFFF);

	bepilog (bs);

	bputl (bs, SEQUENCE_HEADER_CODE, 32);
	bputl (bs, width & 0xFFF, 12);			/* horizontal_size_value */
	bputl (bs, height & 0xFFF, 12);			/* vertical_size_value */
	bputl (bs, aspect_ratio_information, 4);	/* aspect_ratio_information */
	bputl (bs, mpeg1->frame_rate_code, 4);		/* frame_rate_code */
	bputl (bs, bit_rate & 0x3FFFF, 18);		/* bit_rate_value */
	bputl (bs, 1, 1);				/* marker_bit */
	bputl (bs, 0 & 0x3FF, 10);			/* vbv_buffer_size_value */
	bputl (bs, 0, 1);				/* constrained_parameters_flag */
	bputl (bs, 0, 1);				/* load_intra_quantizer_matrix */
	bputl (bs, 0, 1);				/* load_non_intra_quantizer_matrix */

	bputl (bs, EXTENSION_START_CODE, 32);		/* extension_start_code */
	bputl (bs, SEQUENCE_EXTENSION_ID, 4);		/* extension_start_code_identifier */
	bputl (bs, 0x80 + (4 << 4) + (8), 8);		/* profile_and_level_indication MP@ML */
	bputl (bs, 0, 1);				/* progressive_sequence */
	bputl (bs, 1, 2);				/* chroma_format 4:2:0 */
	bputl (bs, width >> 12, 2);			/* horizontal_size_extension */
	bputl (bs, height >> 12, 2);			/* vertical_size_extension */
	bputl (bs, bit_rate >> 18, 12);			/* bit_rate_extension */
	bputl (bs, 1, 1);				/* marker_bit */
	bputl (bs, 0 >> 10, 8);				/* vbv_buffer_size_extension */
	bputl (bs, 0, 1);				/* low_delay */
// XXX
	bputl (bs, (1) - 1, 2);				/* frame_rate_extension_n */
	bputl (bs, (1) - 1, 5);				/* frame_rate_extension_d */

	bprolog (bs);

	emms ();

	return bflush (bs) >> 3;
}

static void
user_data			(struct bs_rec *	bs,
				 char *			s)
{
	int n = strlen(s);

	if (n > 0) {
		bputl (bs, USER_DATA_START_CODE, 32);
		while (n--) bputl (bs, *s++, 8);
	}
}

#define Rvbr (1.0 / 16)

extern int test_mode;

static inline void
_send_full_buffer(mpeg1_context *mpeg1, buffer *b)
{
	pthread_mutex_lock(&mpeg1->codec.codec.mutex);
	mpeg1->codec.status.frames_out++;
	mpeg1->codec.status.coded_time = b->time;
	mpeg1->codec.status.bytes_out += b->used;
	pthread_mutex_unlock(&mpeg1->codec.codec.mutex);

	send_full_buffer(&mpeg1->prod, b);
}

/* obsolete */
static inline rte_bool
encode_keyframe(mpeg1_context *mpeg1, stacked_frame *this,
		int stacked_bframes, int key_offset,
		buffer *obuf, rte_bool pframe)
{
	return 0;
}

static rte_bool
encode_stacked_frames(mpeg1_context *mpeg1, buffer *obuf, int stacked, rte_bool pframe)
{
	int stacked_bframes = stacked - 1;
	int key_offset = stacked_bframes;
	stacked_frame *this;

	if (mpeg1->skip_method == SKIP_METHOD_FAKE)
		key_offset += mpeg1->skipped_fake;

	/* Encode I or P frame */

	this = mpeg1->stack + stacked - 1;

	pframe = encode_keyframe(mpeg1, this, stacked_bframes,
				 key_offset, obuf, pframe);

	/* Encode B frames */

//	encode_bframes(mpeg1, stacked_bframes);

//	if (this->skipped > 0)
//		encode_skipped_frames(mpeg1, this);

	mpeg1->gop_frame_count++; /* the keyframe */

	mpeg1->skipped_fake = 0;

	return pframe;
}

/* FIXME 0P insertion and skip/fake can overflow GOP count */

/* obsolete */
static rte_bool
thread_body(mpeg1_context *mpeg1)
{
	rte_bool eof = FALSE;
	rte_bool done = FALSE;
	char *seq = "";
	buffer *obuf;

	while (!done) {
		stacked_frame *this = NULL;
		int sp = 0;

		for (;;) {
			this = mpeg1->stack + sp;

			if (mpeg1->last.org) {
				this->buffer = mpeg1->last.buffer;
				this->org = mpeg1->last.org;
				this->time = mpeg1->last.buffer->time;
				mpeg1->last.org = NULL;
			} else {
				buffer *b;

				this->buffer = b = wait_full_buffer(&mpeg1->cons);

				if (b->used > 0) {
					this->org = b->data;
					this->time = b->time;
				} else {
					this->buffer = NULL;
					this->org = NULL;

					send_empty_buffer(&mpeg1->cons, b);
				}
			}

			if (this->buffer && mpeg1->last.time
			    && this->time > (mpeg1->last.time + mpeg1->nominal_frame_period * 1.5)) {
				/* Count dropped frames we would skip anyway */
				/* video_frames_dropped++; */
				mpeg1->last.buffer = this->buffer;
				mpeg1->last.org = this->org;
				this->org = NULL;
				this->time = mpeg1->last.time += mpeg1->nominal_frame_period;
			} else {
				mpeg1->last.time = this->time;
			}

			sp++;

			mp1e_sync_drift(&mpeg1->codec.sstr, this->time, mpeg1->coded_elapsed);

			mpeg1->coded_elapsed += mpeg1->coded_frame_period;

			if (!this->buffer /* eof */
			    || mp1e_sync_break(&mpeg1->codec.sstr, this->time))
				eof = TRUE;

			if (eof || (mpeg1->coded_frames_countd - sp) < 0.0) {
				printv(2, "Video: End of file\n");

				if (this->buffer && this->org)
					send_empty_buffer(&mpeg1->cons, this->buffer);

				if (mpeg1->last.org) {
					send_empty_buffer(&mpeg1->cons, mpeg1->last.buffer);
					mpeg1->last.org = NULL;
				}

				while (*seq == 'B')
					seq++;

				done = TRUE;
				this--;
				sp--;

				if (sp <= 0)
					goto finish;

				goto next_frame; /* finish with B*[PI] */
			}

			mpeg1->skip_rate_acc += mpeg1->virtual_frame_rate;

			if (!this->org) { /* missed */
				if (mpeg1->skip_rate_acc >= mpeg1->nominal_frame_rate) {
					video_frames_dropped++;

					if (0 && sp >= 2 && *seq == 'B') {
						/*
						 *  We have BB, duplicate last B instead of
						 *  promoting to z (but only if the next I or P
						 *  isn't missing as well). NB this will advance
						 *  the GOP seq pointer, skipping doesn't.
					       	 *
						 *  Temporarily disabled: BBXBbX !-> Bp0Bp0
						 */
						goto next_frame;
					} else /* skip this */
						mpeg1->skip_rate_acc = 0;
				} /* else we skip it anyway */
			}

			/* XXX */
			this->time = mpeg1->coded_time_elapsed;
			mpeg1->coded_time_elapsed += mpeg1->coded_frame_period;

			if (mpeg1->skip_rate_acc < mpeg1->nominal_frame_rate) {
				switch (mpeg1->skip_method) {
				case SKIP_METHOD_MUX:
				case SKIP_METHOD_FAKE:
					if (this->buffer && this->org)
						send_empty_buffer(&mpeg1->cons, this->buffer);

					mpeg1->skipped_fake++;
					this->skipped++;
					sp--;

					continue;

				case SKIP_METHOD_ZERO_P:
		/*			skip_zerop(mpeg1, this, sp); */

					sp = 0;

					continue;
				}
			}
next_frame:
			mpeg1->skip_rate_acc -= mpeg1->nominal_frame_rate;

			if (*seq != 'B')
				break;

			/* This is a B picture, stack it up */

			seq++;
		}

		/* Encode P or I picture plus sequence or GOP headers */

		obuf = wait_empty_buffer(&mpeg1->prod);
		bstart(&video_out.bstream, obuf->data);

		if (!*seq) {
			/* End of GOP sequence */

			bepilog(&video_out.bstream);

			/* Sequence header */

			if (mpeg1->seq_frame_count >= mpeg1->frames_per_seqhdr) {
				printv(3, "[Sequence header]\n");

				memcpy(video_out.bstream.p, mpeg1->seq_header_template, 24);
				memset(video_out.bstream.p + 24, 0, 8);
				// XXX
				video_out.bstream.p += 32 * 8 / 64;

				if (video_frame_count == 0 && mpeg1->banner)
					user_data(&video_out.bstream, mpeg1->banner);

				mpeg1->seq_frame_count = 0;
				mpeg1->closed_gop = TRUE;
			}

			/* GOP header */

			if (mpeg1->insert_gop_header) {
				if (sp == 1)
					mpeg1->closed_gop = TRUE;

				printv(3, "[GOP header, closed=%c]\n", "FT"[mpeg1->closed_gop]);

				bputl(&video_out.bstream, GROUP_START_CODE, 32);
				bputl(&video_out.bstream, (mpeg1->closed_gop << 6) + (1 << 19), 32);
				/* FIXME
				 *  time_code [25 w/marker_bit] omitted, closed_gop,
				 *  broken_link '0', byte align [5]
				 */

				mpeg1->gop_frame_count = 0;
			}

			bprolog(&video_out.bstream);
			emms();

			seq = mpeg1->gop_sequence;

// XXX
if (mpeg1->rc.gop_count > 0) {
mpeg1->rc.ei = mpeg1->rc.Ei / mpeg1->rc.gop_count;
mpeg1->rc.ep = mpeg1->rc.Ep / mpeg1->rc.gop_count;
mpeg1->rc.eb = mpeg1->rc.Eb / mpeg1->rc.gop_count;
}
mpeg1->rc.gop_count++;
//printv(0, "Eit=%f Ept=%f Ebt=%f \n", ei, ep, eb);

			printv(4, "Rewind sequence R=%d\n", mpeg1->rc.R);

			mpeg1->rc.G0 = mpeg1->rc.Gn;
			mpeg1->rc.ob = 0;
		}

		mpeg1->referenced = (seq[1] == 'P') || (seq[1] == 'B')
			|| (sp > 1) /* || preview */;

		encode_stacked_frames(mpeg1, obuf, sp, *seq == 'P');

		mpeg1->closed_gop = FALSE;

		seq++;

		sp = 0;
	}

finish:
	/* Stream end code */

	if (video_frame_count > 0) {
		obuf = wait_empty_buffer(&mpeg1->prod);
		((unsigned int *) obuf->data)[0] = swab32(SEQUENCE_END_CODE);
		obuf->type = 0; /* no picture, no timestamp */
		obuf->offset = 0;
		obuf->used = 4;
		obuf->time = mpeg1->last.time +=
			mpeg1->nominal_frame_period; /* not used */
		_send_full_buffer(mpeg1, obuf);
	}

	return eof;
}

static void
rc_reset(mpeg1_context *mpeg1)
{
	double x;

	mpeg1->rc.R	= 0;
	mpeg1->rc.r31	= (double) quant_max
		/ lroundn(mpeg1->bit_rate / mpeg1->virtual_frame_rate * 1.0);
	mpeg1->rc.Tmin	= lroundn(mpeg1->bit_rate / mpeg1->virtual_frame_rate / 8.0);
	mpeg1->rc.act_avg_i = 400.0;
	mpeg1->rc.act_avg_p = 400.0;

	mpeg1->rc.Xi	= lroundn(160.0 * mpeg1->bit_rate / 115.0);
	mpeg1->rc.Xp	= lroundn( 60.0 * mpeg1->bit_rate / 115.0); 
	mpeg1->rc.Xb	= lroundn( 42.0 * mpeg1->bit_rate / 115.0); 

	mpeg1->rc.d0i	= 10.0 / mpeg1->rc.r31;
	mpeg1->rc.d0p	= 10.0 / mpeg1->rc.r31;
	mpeg1->rc.d0b	= 14.0 / mpeg1->rc.r31;

	x	= mpeg1->nominal_frame_rate / mpeg1->virtual_frame_rate - 1.0;

	mpeg1->rc.G0	= lroundn((mpeg1->rc.ni + mpeg1->rc.np + mpeg1->rc.nb - mpeg1->rc.ob)
				  * mpeg1->bit_rate / mpeg1->virtual_frame_rate);
	mpeg1->rc.Gn	= lroundn((mpeg1->rc.ni + mpeg1->rc.np + mpeg1->rc.nb)
				  * mpeg1->bit_rate / mpeg1->virtual_frame_rate);
	
	mpeg1->rc.Gn	-= (mpeg1->rc.ni + mpeg1->rc.np + mpeg1->rc.nb) * x * mpeg1->Sz;

	mpeg1->rc.Tavg	= lroundn(mpeg1->bit_rate / mpeg1->virtual_frame_rate);
	mpeg1->rc.G4	= mpeg1->rc.Gn * 4;

	mpeg1->rc.R = mpeg1->rc.G0 = mpeg1->rc.Gn;


	mpeg1->rc.Ei = 0, mpeg1->rc.Ep = 0, mpeg1->rc.Eb = 0;
	mpeg1->rc.ei = 0, mpeg1->rc.ep = 0, mpeg1->rc.eb = 0;
	mpeg1->rc.gop_count = 0;
}

void *
mp1e_mpeg2			(void *			codec)
{
	mpeg1_context *mpeg1 = PARENT (codec, mpeg1_context, codec);
	buffer *obuf;

	printv (3, "Video compression thread MPEG-2\n");

	assert (mpeg1->codec.codec.state == RTE_STATE_RUNNING);

	/* XXX this function isn't reentrant. yet. */
	assert (static_context == mpeg1);

	if (!add_consumer (mpeg1->codec.input, &mpeg1->cons))
		return (void *) -1;
	// XXX need better exit method

	if (!add_producer (mpeg1->codec.output, &mpeg1->prod)) {
		rem_consumer (&mpeg1->cons);
		return (void *) -1;
	}

	if (!mp1e_sync_run_in (&PARENT (mpeg1->codec.codec.context,
				        mp1e_context, context)->sync,
			       &mpeg1->codec.sstr, &mpeg1->cons, NULL)) {
		rem_consumer (&mpeg1->cons);
		rem_producer (&mpeg1->prod);
		return (void *) -1;
	}

	/* [0] top field, [1] bottom field */

	mpeg1->filter_param[0].dest = (void *) &mblock[0];

	mpeg1->filter_param[1] = mpeg1->filter_param[0];
	mpeg1->filter_param[1].offset += mpeg1->filter_param[1].stride;
	mpeg1->filter_param[1].u_offset += mpeg1->filter_param[1].uv_stride;
	mpeg1->filter_param[1].v_offset += mpeg1->filter_param[1].uv_stride;

	mpeg1->filter_param[0].stride *= 2;
	mpeg1->filter_param[0].uv_stride *= 2;
	mpeg1->filter_param[1].stride *= 2;
	mpeg1->filter_param[1].uv_stride *= 2;

	for (;;) {
		extern int split_sequence;

		if (thread_body (mpeg1) || !split_sequence)
			break;

		obuf = wait_empty_buffer (&mpeg1->prod);
		obuf->used = 0;
		obuf->error = -1;
		send_full_buffer (&mpeg1->prod, obuf);

		mpeg1->prod.eof_sent = FALSE;
		mpeg1->prod.fifo->eof_count = 0;

		mpeg1->coded_frames_countd += mpeg1->num_frames;

		mpeg1->seq_frame_count = mpeg1->frames_per_seqhdr;
		mpeg1->gop_frame_count = 0;

		mpeg1->skip_rate_acc = mpeg1->nominal_frame_rate
			- mpeg1->virtual_frame_rate
			+ mpeg1->nominal_frame_rate / 2.0;

		/* XXX */
		mpeg1->coded_time_elapsed = 0.0;

		rc_reset (mpeg1);

		mpeg1->last.org = NULL;
		mpeg1->last.time = 0;
		mpeg1->p_succ = 0;

		mpeg1->mb_cx_row = mb_height;
		mpeg1->mb_cx_thresh = 100000;

		if (mb_height >= 10) {
			mpeg1->mb_cx_row /= 3;
			mpeg1->mb_cx_thresh =
				lroundn (mpeg1->mb_cx_row * mb_width * 0.95);
		}
	}

	/* End of file */

	obuf = wait_empty_buffer (&mpeg1->prod);
	obuf->used = 0;
	obuf->error = 0xE0F;
	send_full_buffer (&mpeg1->prod, obuf);

	rem_consumer (&mpeg1->cons);
	rem_producer (&mpeg1->prod);

	static_context = NULL;

	return NULL;
}

/*
 *  API
 */

#include <stdarg.h>

#undef elements
#define elements(array) (sizeof(array) / sizeof(array[0]))

static void
video_reset(mpeg1_context *mpeg1)
{
	int header_size;

	mpeg1->coded_frame_rate = frame_rate_value[mpeg1->frame_rate_code];
	mpeg1->coded_frame_period = 1.0 / mpeg1->coded_frame_rate;

	if (mpeg1->virtual_frame_rate > mpeg1->coded_frame_rate)
		mpeg1->virtual_frame_rate = mpeg1->coded_frame_rate;

	mpeg1->seq_frame_count = mpeg1->frames_per_seqhdr;
	mpeg1->gop_frame_count = 0;

	if (!mpeg1->nominal_frame_rate) // XXX mp1e frontend
		mpeg1->nominal_frame_rate = frame_rate_value[mpeg1->frame_rate_code];

	mpeg1->nominal_frame_period = 1.0 / mpeg1->nominal_frame_rate;

	mpeg1->skip_rate_acc = mpeg1->nominal_frame_rate
		- mpeg1->virtual_frame_rate + mpeg1->nominal_frame_rate / 2.0;

	mpeg1->drop_timeout = mpeg1->nominal_frame_period * 1.5;
	mpeg1->coded_time_elapsed = 0.0;

	rc_reset(mpeg1);

	bstart(&video_out.bstream, mpeg1->seq_header_template);
	header_size = sequence_header(mpeg1);
	assert(header_size == 24);

	if (mpeg1->banner)
		free(mpeg1->banner);

	asprintf((char **) &mpeg1->banner,
		 mpeg1->anno && mpeg1->anno[0] ?
		 "MP1E " VERSION "\nANNO: %s\n" : "MP1E " VERSION "\n",
		 mpeg1->anno);
}

#define GOP_RULE							\
"A valid GOP sequence can consist of the picture types 'I' "		\
"intra coded, 'P' forward predicted and 'B' bidirectionally "		\
"predicted, in any order headed by an 'I' picture."

static rte_bool
gop_validation(mpeg1_context *mpeg1, char *gop_sequence)
{
	int bmax;
	int i;

	if (!gop_sequence
	    || gop_sequence[0] != 'I'
	    || strspn(gop_sequence, "IPB") != strlen(gop_sequence)) {
		rte_error_printf(mpeg1->codec.codec.context,
				 _("Invalid group of pictures sequence: \"%s\".\n"
				   GOP_RULE), gop_sequence);
		return FALSE;
	}

	if (strlen(gop_sequence) > 1024) {
		rte_error_printf(mpeg1->codec.codec.context,
				 _("Invalid group of pictures sequence: \"%s\", length %d.\n"
				   "The number of pictures in a GOP is limited to 1024."),
				 gop_sequence, strlen(gop_sequence));
		return FALSE;
	}

	if (strchr(gop_sequence, 'P') ||
	    strchr(gop_sequence, 'B'))
		mpeg1->insert_gop_header = TRUE;

	/*
	 *  I, P and B in GOP for rate control
	 */
	mpeg1->rc.ni =
	mpeg1->rc.np =
	mpeg1->rc.nb = 0;

	bmax = 0;

	for (i = 0; i < 1024; i++)
		switch (gop_sequence[i]) {
		case 'I':
			mpeg1->rc.ni++;
			mpeg1->rc.ob = 0;
			break;

		case 'P':
			mpeg1->rc.np++;
			mpeg1->rc.ob = 0;
			break;

		case 'B':
			/*
			 *  ob: GOP overlapping B pictures (BB|I)
			 *  bmax: max. successive B pictures
			 */
			mpeg1->rc.nb++;
			mpeg1->rc.ob++;

			if (mpeg1->rc.ob > bmax)
				bmax = mpeg1->rc.ob;

			break;

		default:
			i = 1024; /* abort */
		}

	/*
	 *  One position used by I or P.
	 */
	if (bmax >= elements(mpeg1->stack)) {
		rte_error_printf(mpeg1->codec.codec.context,
				 _("Invalid group of pictures sequence: \"%s\".\n"
				   "The number of successive 'B' bidirectionally predicted "
				   "pictures is limited to %u."),
				 gop_sequence, elements(mpeg1->stack) - 1);
		return FALSE;
	}

	return TRUE;
}

static inline void
do_free(void **pp)
{
	if (*pp) {
		free(*pp);
		*pp = NULL;
	}
}

#define FREE_ALIGNED(p)							\
do {									\
	if (NULL != (p)) {						\
		free_aligned (p);					\
		(p) = NULL;						\
	}								\
} while (0)

static void
uninit(rte_codec *codec)
{
	mpeg1_context *mpeg1 = PARENT(codec, mpeg1_context, codec);

	/* 0P */
	FREE_ALIGNED (mpeg1->zerop_template);

	/* main buffers */
//	FREE_ALIGNED (mm_mbrow);
	FREE_ALIGNED (newref);
	FREE_ALIGNED (mpeg1->oldref);

	FREE_ALIGNED (mpeg1->mb_hist);

	do_free((void **) &mpeg1->banner);

	if (static_context == mpeg1)
		static_context = NULL;
}

static rte_bool
parameters_set(rte_codec *codec, rte_stream_parameters *rsp)
{
	extern filter_fn pmmx_YUV420_0, sse_YUV420_0;
	extern filter_fn pmmx_YUYV_6, sse_YUYV_6;
	extern int filter_mode;
	mpeg1_context *mpeg1 = PARENT(codec, mpeg1_context, codec);
	rte_bool packed = TRUE;
	int size;

	switch (codec->state) {
	case RTE_STATE_NEW:
		break;

	case RTE_STATE_PARAM:
		uninit(codec);
		break;

	default:
		assert(!"reached");
	}

	if (rsp->video.width <= 0 || rsp->video.height <= 0)
		goto reject;

	// XXX relax
	rsp->video.width = (saturate(rsp->video.width, 16, MAX_WIDTH) + 8) & -16;
	rsp->video.height = (saturate(rsp->video.height, 16, MAX_HEIGHT) + 8) & -16;

	mpeg1->coded_width = rsp->video.width;
	mpeg1->coded_height = rsp->video.height;

	video_coding_size(mpeg1->coded_width, mpeg1->coded_height, TRUE);

	if (filter_mode != -1) { // XXX mp1e frontend
		extern int motion_min, motion_max;
		extern int width, height;

		mpeg1->coded_width = width;
		mpeg1->coded_height = height;

		video_coding_size(mpeg1->coded_width, mpeg1->coded_height, TRUE);

		mpeg1->motion_min = motion_min; // option
		mpeg1->motion_max = motion_max;
	} else {
		extern int cpu_type;
	  	int sse;

	// XXX relax
	rsp->video.width = (saturate(rsp->video.width, 16, MAX_WIDTH) + 8) & -16;
	rsp->video.height = (saturate(rsp->video.height, 16, MAX_HEIGHT) + 8) & -16;

	mpeg1->coded_width = rsp->video.width;
	mpeg1->coded_height = rsp->video.height;

	video_coding_size(mpeg1->coded_width, mpeg1->coded_height, TRUE);

	switch (cpu_type) {
	case CPU_PENTIUM_III:
	case CPU_CYRIX_NEHEMIAH:
	case CPU_PENTIUM_4:
	case CPU_ATHLON:
		sse = 1;
		break;

	default:
		sse = 0;
		break;
	}

	switch (rsp->video.pixfmt) {
	case RTE_PIXFMT_YUV420:
		if (rsp->video.stride == 0)
			rsp->video.stride = rsp->video.width;
		if (rsp->video.uv_stride == 0)
			rsp->video.uv_stride = rsp->video.width >> 1;
		if (rsp->video.u_offset == 0 || rsp->video.v_offset == 0) {
			rsp->video.u_offset = rsp->video.height * rsp->video.stride;
			rsp->video.v_offset += (rsp->video.height >> 1) * rsp->video.uv_stride;
		}

		mpeg1->codec.input_buffer_size =
			MAX(rsp->video.offset + rsp->video.stride * rsp->video.height,
			    MAX(rsp->video.u_offset, rsp->video.v_offset)
			    + rsp->video.uv_stride * (rsp->video.height >> 1));

		mpeg1->filter_param[0].dest = (void *) &mblock[0];
		mpeg1->filter_param[0].offset = rsp->video.offset;
		mpeg1->filter_param[0].u_offset = rsp->video.u_offset;
		mpeg1->filter_param[0].v_offset = rsp->video.v_offset;
		mpeg1->filter_param[0].stride = rsp->video.stride;
		mpeg1->filter_param[0].uv_stride = rsp->video.uv_stride;
		mpeg1->filter_param[0].func = sse ? sse_YUV420_0 : pmmx_YUV420_0;

		break;

	case RTE_PIXFMT_YUYV:
		if (rsp->video.stride == 0)
			rsp->video.stride = rsp->video.width * 2;

		mpeg1->codec.input_buffer_size =
			rsp->video.offset + rsp->video.stride * rsp->video.height;

		mpeg1->filter_param[0].dest = (void *) &mblock[0];
		mpeg1->filter_param[0].offset = rsp->video.offset;
		mpeg1->filter_param[0].stride = rsp->video.stride;
		mpeg1->filter_param[0].func = sse ? sse_YUYV_6 : pmmx_YUYV_6;

		break;

	default:
		goto reject;
	}
	}

	if (rsp->video.frame_rate <= 24.0)
		rsp->video.frame_rate = 24.0;

	mpeg1->nominal_frame_rate = rsp->video.frame_rate;

	if (!rsp->video.sample_aspect)
		rsp->video.sample_aspect = 1.0;

	rsp->video.sample_aspect = aspect_ratio_value[mpeg1->aspect_ratio_code];

	memcpy(&codec->params, rsp, sizeof(codec->params));

	/* XXX this codec isn't reentrant */
	if (!static_context)
		static_context = mpeg1;
	assert(static_context == mpeg1);


	memset(&mpeg1->codec.sstr, 0, sizeof(mpeg1->codec.sstr));

	mpeg1->codec.sstr.frame_period = mpeg1->nominal_frame_period;


	assert(gop_validation(mpeg1, mpeg1->gop_sequence));

	mpeg1->frames_per_seqhdr = 12;


	video_frames_dropped = 0;
	video_frame_count = 0;
	mpeg1->coded_frames_countd = mpeg1->num_frames;


	/* Init motion compensation */

	/* XXX */
	if (mpeg1->motion_compensation) {
		mpeg1->motion_min = 4;
		mpeg1->motion_max = 24;
	}

	if (mpeg1->motion_min && mpeg1->motion_max
	    && mpeg1->rc.np > 0 && mpeg1->rc.nb > 0) {
		switch (cpu_detection()) {
		case CPU_K6_2:
		case CPU_CYRIX_III:
			search = _3dn_search;
			break;

		case CPU_PENTIUM_4:
#if USE_SSE2
			search = sse2_search;
			break;
#endif
		case CPU_PENTIUM_III:
		case CPU_CYRIX_NEHEMIAH:
		case CPU_ATHLON:
			search = sse_search;
			break;

		default:
			search = mmx_search;
			break;
		}
#if REG_TEST
		motion = (mpeg1->motion_min + mpeg1->motion_max) << 7;
#else
		motion = (mpeg1->motion_min * 3 + mpeg1->motion_max * 1) * 256 / 4;
#endif
//		p_inter_bias *= 2;
//		b_inter_bias *= PBF;

		packed = FALSE;

//		mpeg1->picture_i = picture_i_mc;
//		mpeg1->picture_p = picture_p_mc;
//		mpeg1->picture_b = picture_b_mc;

		mm_buf_offs = 6 * 64 * mb_num;
//		mm_mbrow = calloc_aligned(mb_width * (16 * 8) * sizeof(short), 4096);

//		if (!mm_mbrow) {
//			rte_error_printf(codec->context, _("Out of memory for mc buffer."));
//			goto reject;
//		}
	} else {
		motion = 0;

//		mpeg1->picture_i = picture_i_nomc;
//		mpeg1->picture_p = picture_p_nomc;
//		mpeg1->picture_b = picture_b_nomc;
	}

	/* Init buffers */

	mpeg1->mb_hist = calloc_aligned ((mb_width + 1) * sizeof (*mpeg1->mb_hist), 32);

	// XXX too big. Interleave.

	size = (motion ? 10 * 64 : 6 * 64) * mb_num;

	if (!(mpeg1->oldref = calloc_aligned(size, 4096))) {
		rte_error_printf(codec->context, _("Out of memory for video buffer."));
		goto reject;
	}

	if (!(newref = calloc_aligned(size, 4096))) {
		rte_error_printf(codec->context, _("Out of memory for video buffer."));
		goto reject;
	}

	binit_write(&video_out.bstream);
	bstart(&video_out.bstream, mpeg1->oldref);
	mpeg1->Sz = picture_zero(mpeg1);

	if (!(mpeg1->zerop_template =
	      calloc_aligned(mpeg1->Sz * sizeof(unsigned char), CACHE_LINE))) {
		rte_error_printf(codec->context, _("Out of memory."));
		goto reject;
	}

	memcpy(mpeg1->zerop_template, mpeg1->oldref, mpeg1->Sz);

	if (packed) {
		int i;

		for (i = 0; i < 6; i++) {
			mb_address.block[i].offset	= 64;
			mb_address.block[i].pitch	= 8;
		}

		mb_address.col.lum	= 6 * 64;
		mb_address.col.chrom	= 0;
		mb_address.row.lum	= 0;
		mb_address.row.chrom	= 0;
		mb_address.chrom_0	= mb_address.block[4].offset;

		mpeg1->predict_forward = mmx_predict_forward_packed;
		mpeg1->predict_bidirectional = mmx_predict_bidirectional_packed;
	} else {
		int i;

		for (i = 0; i < 6; i++)
			mb_address.block[i].pitch = mb_width * ((i >= 4) ? 8 : 16);

		mb_address.block[1].offset = mb_width * 16 * 8;
		mb_address.block[2].offset = 8 - mb_width * 16 * 8;
		mb_address.block[3].offset = mb_width * 16 * 8;
		mb_address.block[4].offset = mb_num * 64 * 4 - mb_width * 16 * 8 - 8;
		mb_address.block[5].offset = mb_num * 64;

		mb_address.col.lum	= 16;
		mb_address.col.chrom	= 8 - 16;
		mb_address.row.lum	= mb_width * 16 * 15;
		mb_address.row.chrom	= mb_width * (8 * 7 - 16 * 15);
		mb_address.chrom_0	= mb_address.block[4].offset;

		mpeg1->predict_forward = mmx_predict_forward_planar;
		mpeg1->predict_bidirectional = predict_bidirectional_planar; // no MMX equv
	}

	/* Misc */

	mpeg1->last.org = NULL;
	mpeg1->last.time = 0;
	mpeg1->p_succ = 0;

	mpeg1->mb_cx_row = mb_height;
	mpeg1->mb_cx_thresh = 100000;

	if (mb_height >= 10) {
		mpeg1->mb_cx_row /= 3;
		mpeg1->mb_cx_thresh =
			lroundn(mpeg1->mb_cx_row * mb_width * 0.95);
	}

	video_reset(mpeg1);

	/* I/O */

	mpeg1->codec.sstr.frame_period = 1.0 / mpeg1->nominal_frame_rate;

	mpeg1->codec.io_stack_size = video_look_ahead(mpeg1->gop_sequence);

	mpeg1->codec.output_buffer_size = mb_num * 384 * 4;
	mpeg1->codec.output_bit_rate = mpeg1->bit_rate;
	mpeg1->codec.output_frame_rate = mpeg1->coded_frame_rate;

	memset(&mpeg1->codec.status, 0, sizeof(mpeg1->codec.status));
	mpeg1->codec.status.valid = 0
		+ RTE_STATUS_FRAMES_OUT
		+ RTE_STATUS_BYTES_OUT
		+ RTE_STATUS_CODED_TIME;

	codec->state = RTE_STATE_PARAM;

	return TRUE;

reject:
	uninit(codec);

	codec->state = RTE_STATE_NEW;

	return FALSE;
}

/*
 *  NB there are four frame rates:
 *  1) The nominal input frame rate (eg. 25 Hz)
 *  2) The actual input frame rate (eg. 25.001 Hz)
 *  3) The coded output frame rate (eg. 24 Hz)
 *  4) The virtual output frame rate,
 *     after forced frame dropping (eg. 8.5 Hz)
 */

static char *
menu_skip_method[] = {
	N_("Standard compliant"),
	N_("Multiplexer"),
	N_("Fake picture"),
};

static rte_option_info
mpeg1_options[] = {
	/* FILTER omitted, will change, default for now */
	/* FRAMES_PER_SEQ_HEADER omitted, ancient legacy */
	/* FIX vcd_parameters_set when adding w/h options */
	RTE_OPTION_INT_RANGE_INITIALIZER
	  ("bit_rate", N_("Bit rate"),
// XXX
	   2300000, 30000, 16000000, 1000,
	   N_("Output bit rate")),
	RTE_OPTION_REAL_MENU_INITIALIZER
	  ("coded_frame_rate", NULL,
	   2 /* 25.0 */, (double *) &frame_rate_value[1], 8, NULL),
	RTE_OPTION_REAL_RANGE_INITIALIZER
	  ("virtual_frame_rate", N_("Virtual frame rate"),
	   60.0, 0.0002, 60.0, 1e-3,
// XXX we'll encode the nominal frame rate, use frame_rate_ext_n/d for virtual
// (1..4)/(1..32), and dropping for actual and virtual residual
	   N_("MPEG-1 allows only a few discrete values for frames/s, "
	      "but this codec can skip frames if you wish. Choose the "
	      "output bit rate accordingly.")),
	RTE_OPTION_MENU_INITIALIZER
	  ("skip_method", N_("Virtual frame rate method"),
// obsolete
	   0, menu_skip_method, elements(menu_skip_method),
	   N_("The standard compliant method has one major drawback: "
	      "it may have to promote some or even all B to P pictures, "
	      "reducing the image quality.")),
	RTE_OPTION_STRING_INITIALIZER
	  ("gop_sequence", N_("GOP sequence"),
"IIIIIIIIIIII",
// test	   "IBBPBBPBBPBB", 
	   N_(GOP_RULE)),
	RTE_OPTION_BOOL_INITIALIZER
	  ("motion_compensation", N_("Motion compensation"),
	   FALSE, N_("Enable motion compensation to improve the image "
		     "quality. The motion search range is automatically "
		     "adjusted.")),
/*	RTE_OPTION_BOOL_INITIALIZER
	  ("desaturate", N_("Desaturate"), FALSE, (NULL)),
XXX
*/	RTE_OPTION_STRING_INITIALIZER
	  ("anno", N_("Annotation"), "",
	   N_("Add an annotation in the user data field shortly after "
	      "the video stream start. This is an mp1e extension, "
	      "players will ignore it.")),
	RTE_OPTION_REAL_RANGE_INITIALIZER
	  ("num_frames", NULL, DBL_MAX, 1, DBL_MAX, 1, NULL),
};

#define KEYWORD(name) strcmp(keyword, name) == 0

static char *
option_print(rte_codec *codec, const char *keyword, va_list args)
{
	char buf[80];
	rte_context *context = codec->context;

	if (KEYWORD("bit_rate")) {
	        snprintf(buf, sizeof(buf), _("%5.3f Mbit/s"), va_arg(args, int) / 1e6);
	} else if (KEYWORD("coded_frame_rate")) {
		snprintf(buf, sizeof(buf), _("%4.2f frames/s"),
			 frame_rate_value[rte_closest_double(
				  &frame_rate_value[1], 8,
				  va_arg(args, double)) + 1]);
	} else if (KEYWORD("virtual_frame_rate")) {
		snprintf(buf, sizeof(buf), _("%5.3f frames/s"), va_arg(args, double));
	} else if (KEYWORD("skip_method")) {
		return rte_strdup(context, NULL, _(menu_skip_method[
			RTE_OPTION_ARG_MENU(menu_skip_method)]));
	} else if (KEYWORD("gop_sequence") || KEYWORD("anno")) {
		return rte_strdup(context, NULL, va_arg(args, char *));
	} else if (KEYWORD("motion_compensation") || KEYWORD("monochrome")) {
		return rte_strdup(context, NULL, va_arg(args, int) ?
				  _("on") : _("off"));
	} else if (KEYWORD("num_frames")) {
		snprintf(buf, sizeof(buf), _("%f frames"), va_arg(args, double));
	} else {
		rte_unknown_option(context, codec, keyword);
	failed:
		return NULL;
	}

	return rte_strdup(context, NULL, buf);
}

static rte_bool
option_get(rte_codec *codec, const char *keyword, rte_option_value *v)
{
	mpeg1_context *mpeg1 = PARENT(codec, mpeg1_context, codec);
	rte_context *context = codec->context;

	if (KEYWORD("bit_rate")) {
		v->num = mpeg1->bit_rate;
	} else if (KEYWORD("coded_frame_rate")) {
		v->dbl = frame_rate_value[mpeg1->frame_rate_code];
	} else if (KEYWORD("virtual_frame_rate")) {
		v->dbl = mpeg1->virtual_frame_rate;
	} else if (KEYWORD("skip_method")) {
		v->num = mpeg1->skip_method;
	} else if (KEYWORD("gop_sequence")) {
		if (!(v->str = rte_strdup(context, NULL, mpeg1->gop_sequence)))
			return FALSE;
	} else if (KEYWORD("motion_compensation")) {
		v->num = !!mpeg1->motion_compensation;
	} else if (KEYWORD("monochrome")) {
		v->num = !!mpeg1->monochrome;
	} else if (KEYWORD("anno")) {
		if (!(v->str = rte_strdup(context, NULL, mpeg1->anno)))
			return FALSE;
	} else if (KEYWORD("num_frames")) {
		v->dbl = mpeg1->num_frames;
	} else {
		rte_unknown_option(context, codec, keyword);
		return FALSE;
	}

	return TRUE;
}

static rte_bool
option_set(rte_codec *codec, const char *keyword, va_list args)
{
	mpeg1_context *mpeg1 = PARENT(codec, mpeg1_context, codec);
	rte_context *context = codec->context;

	if (0) {
		static char *option_print(rte_codec *, const char *, va_list);
		char *str = option_print(codec, keyword, args);

		printv(0, "mpeg1/option_set(%p, %s, %s)\n", mpeg1, keyword, str);
		free(str);
	}

	/* Preview runtime changes here */

	switch (codec->state) {
	case RTE_STATE_NEW:
		break;

	case RTE_STATE_PARAM:
		uninit(codec);
		break;

	case RTE_STATE_READY:
		assert(!"reached");

	default:
		rte_error_printf(codec->context, "Cannot set %s options, codec is busy.",
				 codec->_class->_public->keyword);
		return FALSE;
	}

	if (KEYWORD("bit_rate")) {
		mpeg1->bit_rate = RTE_OPTION_ARG(int, 30000, 16000000);
	} else if (KEYWORD("coded_frame_rate")) {
		mpeg1->frame_rate_code =
			rte_closest_double(&frame_rate_value[1], 8,
					   va_arg(args, double)) + 1;
	} else if (KEYWORD("virtual_frame_rate")) {
		mpeg1->virtual_frame_rate =
			RTE_OPTION_ARG(double, 1 / 3600.0, 60.0);
	} else if (KEYWORD("skip_method")) {
		mpeg1->skip_method = RTE_OPTION_ARG_MENU(menu_skip_method);
	} else if (KEYWORD("gop_sequence")) {
		char *str;
		int i;
str = rte_strdup(context, NULL, "IIIIIIIIIIII");
//		if (!(str = rte_strdup(context, NULL, va_arg(args, char *))))
//			return FALSE;

		for (i = 0; str[i]; i++)
			str[i] = toupper(str[i]);

		if (!gop_validation(mpeg1, str)) {
			free(str);
			return FALSE;
		}

		if (mpeg1->gop_sequence)
			free(mpeg1->gop_sequence);

		mpeg1->gop_sequence = str;
	} else if (KEYWORD("motion_compensation")) {
		mpeg1->motion_compensation = !!va_arg(args, int);
	} else if (KEYWORD("monochrome")) {
		mpeg1->monochrome = !!va_arg(args, int);
	} else if (KEYWORD("anno")) {
		if (!rte_strdup(context, &mpeg1->anno, va_arg(args, char *)))
			return FALSE;
	} else if (KEYWORD("num_frames")) {
		mpeg1->num_frames = va_arg(args, double);
	} else {
		rte_unknown_option(context, codec, keyword);
	failed:
		return FALSE;
	}

	codec->state = RTE_STATE_NEW;

	return TRUE;
}

static rte_option_info *
option_enum(rte_codec *codec, unsigned int index)
{
	/* mpeg1_context *mpeg1 = PARENT(codec, mpeg1_context, codec); */

	if (index < 0 || index >= elements(mpeg1_options))
		return NULL;

	return mpeg1_options + index;
}

static void
codec_delete(rte_codec *codec)
{
	mpeg1_context *mpeg1 = PARENT(codec, mpeg1_context, codec);

	switch (codec->state) {
	case RTE_STATE_PARAM:
		uninit(codec);
		break;

	case RTE_STATE_READY:
		assert(!"reached");
		break;

	case RTE_STATE_RUNNING:
	case RTE_STATE_PAUSED:
		fprintf(stderr, "mp1e bug warning: attempt to delete "
			"running mpeg1 codec ignored\n");
		return;

	default:
		break;
	}

	/* Options */

	if (mpeg1->gop_sequence)
		free(mpeg1->gop_sequence);

	if (mpeg1->anno)
		free(mpeg1->anno);

	pthread_mutex_destroy(&codec->mutex);

	free_aligned(mpeg1);
}

static rte_codec *
codec_new(rte_codec_class *cc, char **errstr)
{
	mpeg1_context *mpeg1;
	rte_codec *codec;

	if (!(mpeg1 = calloc_aligned(sizeof(*mpeg1), 8192))) {
		rte_asprintf(errstr, _("Out of memory."));
		return NULL;
	}

	codec = &mpeg1->codec.codec;
	codec->_class = cc;

	pthread_mutex_init(&codec->mutex, NULL);

	codec->state = RTE_STATE_NEW;

	return codec;
}

static rte_codec_info
codec_info = {
	.stream_type = RTE_STREAM_VIDEO,
	.keyword = "mpeg2_video",
// experimental
//	.label = "MPEG-2 Video",
};

rte_codec_class
mp1e_mpeg2_video_codec = {
	._public	= &codec_info,
	._new		= codec_new,
	._delete	= codec_delete,
	.option_enum	= option_enum,
	.option_get	= option_get,
	.option_set	= option_set,
	.option_print	= option_print,
	.parameters_set = parameters_set,
};

void
mp1e_mpeg2_module_init(int test)
{
}

#else

void *
mp1e_mpeg2(void *codec)
{
	return NULL;
}

void
mp1e_mpeg2_module_init(int test)
{
}

rte_codec_class mp1e_mpeg2_video_codec;

#endif
