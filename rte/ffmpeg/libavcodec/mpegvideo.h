/*
 * Generic DCT based hybrid video encoder
 * Copyright (c) 2000,2001 Gerard Lantau.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define FRAME_SKIPED 100 // return value for header parsers if frame is not coded

enum OutputFormat {
    FMT_MPEG1,
    FMT_H263,
    FMT_MJPEG, 
};

#define MPEG_BUF_SIZE (16 * 1024)

#define QMAT_SHIFT_MMX 19
#define QMAT_SHIFT 25

#define MAX_FCODE 7
#define MAX_MV 2048
#define REORDER_BUFFER_SIZE (FF_MAX_B_FRAMES+2)

typedef struct Predictor{
    double coeff;
    double count;
    double decay;
} Predictor;

typedef struct RateControlEntry{
    int pict_type;
    int qscale;
    int mv_bits;
    int i_tex_bits;
    int p_tex_bits;
    int misc_bits;
    uint64_t expected_bits;
    int new_pict_type;
    float new_qscale;
}RateControlEntry;

typedef struct RateControlContext{
    FILE *stats_file;
    int num_entries;
    RateControlEntry *entry;
}RateControlContext;

typedef struct ReorderBuffer{
    UINT8 *picture[3];
    int pict_type;
    int qscale;
    int force_type;
    int picture_number;
    int picture_in_gop_number;
} ReorderBuffer;

typedef struct MpegEncContext {
    struct AVCodecContext *avctx;
    /* the following parameters must be initialized before encoding */
    int width, height; /* picture size. must be a multiple of 16 */
    int gop_size;
    int frame_rate; /* number of frames per second */
    int intra_only; /* if true, only intra pictures are generated */
    int bit_rate;        /* wanted bit rate */
    int bit_rate_tolerance; /* amount of +- bits (>0)*/
    enum OutputFormat out_format; /* output format */
    int h263_pred;    /* use mpeg4/h263 ac/dc predictions */

/* the following codec id fields are deprecated in favor of codec_id */
    int h263_plus; /* h263 plus headers */
    int h263_rv10; /* use RV10 variation for H263 */
    int h263_msmpeg4; /* generate MSMPEG4 compatible stream (deprecated, use msmpeg4_version instead)*/
    int h263_intel; /* use I263 intel h263 header */
    
    int codec_id;     /* see CODEC_ID_xxx */
    int fixed_qscale; /* fixed qscale if non zero */
    float qcompress;  /* amount of qscale change between easy & hard scenes (0.0-1.0) */
    float qblur;      /* amount of qscale smoothing over time (0.0-1.0) */
    int qmin;         /* min qscale */
    int qmax;         /* max qscale */
    int max_qdiff;    /* max qscale difference between frames */
    int encoding;     /* true if we are encoding (vs decoding) */
    int flags;        /* AVCodecContext.flags (HQ, MV4, ...) */
    int force_input_type;/* 0= no force, otherwise I_TYPE, P_TYPE, ... */
    int max_b_frames; /* max number of b-frames for encoding */
    float b_quant_factor;/* qscale factor between ips and b frames */
    int rc_strategy;
    int b_frame_strategy;
    /* the following fields are managed internally by the encoder */

    /* bit output */
    PutBitContext pb;

    /* sequence parameters */
    int context_initialized;
    int input_picture_number;
    int input_picture_in_gop_number; /* 0-> first pic in gop, ... */
    int picture_number;
    int fake_picture_number; /* picture number at the bitstream frame rate */
    int gop_picture_number;  /* index of the first picture of a GOP based on fake_pic_num & mpeg1 specific */
    int picture_in_gop_number; /* 0-> first pic in gop, ... */
    int b_frames_since_non_b;  /* used for encoding, relative to not yet reordered input */
    int mb_width, mb_height;   /* number of MBs horizontally & vertically */
    int mb_num;                /* number of MBs of a picture */
    int linesize;              /* line size, in bytes, may be different from width */
    UINT8 *new_picture[3];     /* picture to be compressed */
    UINT8 *picture_buffer[REORDER_BUFFER_SIZE][3]; /* internal buffers used for reordering of input pictures */
    int picture_buffer_index;
    ReorderBuffer coded_order[REORDER_BUFFER_SIZE];
    UINT8 *last_picture[3];      /* previous picture */
    UINT8 *last_picture_base[3]; /* real start of the picture */
    UINT8 *next_picture[3];      /* previous picture (for bidir pred) */
    UINT8 *next_picture_base[3]; /* real start of the picture */
    UINT8 *aux_picture[3];       /* aux picture (for B frames only) */
    UINT8 *aux_picture_base[3];  /* real start of the picture */
    UINT8 *current_picture[3];   /* buffer to store the decompressed current picture */
    int num_available_buffers;   /* is 0 at the start & after seeking, after the first I frame its 1 after next I/P 2 */
    int last_dc[3];              /* last DC values for MPEG1 */
    INT16 *dc_val[3];            /* used for mpeg4 DC prediction, all 3 arrays must be continuous */
    int y_dc_scale, c_dc_scale;
    UINT8 *coded_block;          /* used for coded block pattern prediction (msmpeg4v3, wmv1)*/
    INT16 (*ac_val[3])[16];      /* used for for mpeg4 AC prediction, all 3 arrays must be continuous */
    int ac_pred;
    int mb_skiped;              /* MUST BE SET only during DECODING */
    UINT8 *mbskip_table;        /* used to avoid copy if macroblock skipped (for black regions for example) 
                                   and used for b-frame encoding & decoding (contains skip table of next P Frame) */
    UINT8 *mbintra_table;       /* used to avoid setting {ac, dc, cbp}-pred stuff to zero on inter MB decoding */

    int input_qscale;           /* qscale prior to reordering of frames */
    int input_pict_type;        /* pict_type prior to reordering of frames */
    int force_type;             /* 0= no force, otherwise I_TYPE, P_TYPE, ... */
    int qscale;                 /* QP */
    int last_non_b_qscale;	/* QP of last non b frame used for b frame qscale*/
    int pict_type;              /* I_TYPE, P_TYPE, B_TYPE, ... */
    int last_non_b_pict_type;   /* used for mpeg4 gmc b-frames & ratecontrol */
    int frame_rate_index;
    /* motion compensation */
    int unrestricted_mv;
    int h263_long_vectors; /* use horrible h263v1 long vector mode */

    int f_code; /* forward MV resolution */
    int b_code; /* backward MV resolution for B Frames (mpeg4) */
    INT16 (*motion_val)[2];            /* used for MV prediction (4MV per MB) */
    INT16 (*p_mv_table)[2];            /* MV table (1MV per MB) p-frame encoding */
    INT16 (*last_p_mv_table)[2];       /* MV table (1MV per MB) p-frame encoding */
    INT16 (*b_forw_mv_table)[2];       /* MV table (1MV per MB) forward mode b-frame encoding */
    INT16 (*b_back_mv_table)[2];       /* MV table (1MV per MB) backward mode b-frame encoding */
    INT16 (*b_bidir_forw_mv_table)[2]; /* MV table (1MV per MB) bidir mode b-frame encoding */
    INT16 (*b_bidir_back_mv_table)[2]; /* MV table (1MV per MB) bidir mode b-frame encoding */
    INT16 (*b_direct_forw_mv_table)[2];/* MV table (1MV per MB) direct mode b-frame encoding */
    INT16 (*b_direct_back_mv_table)[2];/* MV table (1MV per MB) direct mode b-frame encoding */
    INT16 (*b_direct_mv_table)[2];     /* MV table (1MV per MB) direct mode b-frame encoding */
    int me_method;                     /* ME algorithm */
    uint8_t *me_scratchpad;            /* data area for the me algo, so that the ME doesnt need to malloc/free */
    int mv_dir;
#define MV_DIR_BACKWARD  1
#define MV_DIR_FORWARD   2
#define MV_DIRECT        4 // bidirectional mode where the difference equals the MV of the last P/S/I-Frame (mpeg4)
    int mv_type;
#define MV_TYPE_16X16       0   /* 1 vector for the whole mb */
#define MV_TYPE_8X8         1   /* 4 vectors (h263, mpeg4 4MV) */
#define MV_TYPE_16X8        2   /* 2 vectors, one per 16x8 block */ 
#define MV_TYPE_FIELD       3   /* 2 vectors, one per field */ 
#define MV_TYPE_DMV         4   /* 2 vectors, special mpeg2 Dual Prime Vectors */
    /* motion vectors for a macroblock 
       first coordinate : 0 = forward 1 = backward
       second "         : depend on type
       third  "         : 0 = x, 1 = y
    */
    int mv[2][4][2];
    int field_select[2][2];
    int last_mv[2][2][2];             /* last MV, used for MV prediction in MPEG1 & B-frame MPEG4 */
    UINT16 (*mv_penalty)[MAX_MV*2+1]; /* amount of bits needed to encode a MV, used for ME */
    UINT8 *fcode_tab; /* smallest fcode needed for each MV */

    int has_b_frames;
    int no_rounding; /* apply no rounding to motion compensation (MPEG4, msmpeg4, ...) 
                        for b-frames rounding mode is allways 0 */

    int hurry_up;     /* when set to 1 during decoding, b frames will be skiped
                         when set to 2 idct/dequant will be skipped too */

    /* macroblock layer */
    int mb_x, mb_y;
    int mb_incr;
    int mb_intra;
    UINT16 *mb_var;    /* Table for MB variances */
    UINT8 *mb_type;    /* Table for MB type */
#define MB_TYPE_INTRA    0x01
#define MB_TYPE_INTER    0x02
#define MB_TYPE_INTER4V  0x04
#define MB_TYPE_SKIPED   0x08
#define MB_TYPE_DIRECT   0x10
#define MB_TYPE_FORWARD  0x20
#define MB_TYPE_BACKWARD 0x40
#define MB_TYPE_BIDIR    0x80

    int block_index[6]; /* index to current MB in block based arrays with edges*/
    int block_wrap[6];

    /* matrix transmitted in the bitstream */
    UINT16 intra_matrix[64];
    UINT16 chroma_intra_matrix[64];
    UINT16 inter_matrix[64];
    UINT16 chroma_inter_matrix[64];
#define QUANT_BIAS_SHIFT 4
    int intra_quant_bias;    /* bias for the quantizer */
    int inter_quant_bias;    /* bias for the quantizer */
    int min_qcoeff;          /* minimum encodable coefficient */
    int max_qcoeff;          /* maximum encodable coefficient */
    /* precomputed matrix (combine qscale and DCT renorm) */
    int q_intra_matrix[32][64];
    int q_inter_matrix[32][64];
    /* identical to the above but for MMX & these are not permutated */
    UINT16 __align8 q_intra_matrix16[32][64];
    UINT16 __align8 q_inter_matrix16[32][64];
    UINT16 __align8 q_intra_matrix16_bias[32][64];
    UINT16 __align8 q_inter_matrix16_bias[32][64];
    int block_last_index[6];  /* last non zero coefficient in block */

    void *opaque; /* private data for the user */

    /* bit rate control */
    int I_frame_bits; //FIXME used in mpeg12 ...
    int avg_mb_var;        /* average MB variance for current frame */
    int mc_mb_var;         /* motion compensated MB variance for current frame */
    int last_non_b_mc_mb_var;/* motion compensated MB variance for last non b frame */
    INT64 wanted_bits;
    INT64 total_bits;
    int frame_bits;        /* bits used for the current frame */
    int pb_frame_bits;     /* bits of the last b...bp group */
    Predictor i_pred;
    Predictor p_pred;
    double qsum;         /* sum of qscales */
    double qcount;       /* count of qscales */
    double short_term_qsum;   /* sum of recent qscales */
    double short_term_qcount; /* count of recent qscales */
    RateControlContext rc_context;

    /* statistics, used for 2-pass encoding */
    int mv_bits;
    int header_bits;
    int i_tex_bits;
    int p_tex_bits;
    int i_count;
    int p_count;
    int skip_count;
    int misc_bits; // cbp, mb_type
    int last_bits; //temp var used for calculating the above vars

    /* H.263 specific */
    int gob_number;
    int gob_index;
    int first_gob_line;
        
    /* H.263+ specific */
    int umvplus;
    int umvplus_dec;
    int h263_aic; /* Advanded INTRA Coding (AIC) */
    int h263_aic_dir; /* AIC direction: 0 = left, 1 = top */
    
    /* mpeg4 specific */
    int time_increment_resolution;
    int time_increment_bits;        /* number of bits to represent the fractional part of time */
    int last_time_base;
    int time_base;                  /* time in seconds of last I,P,S Frame */
    int64_t time;                   /* time of current frame */ 
    int64_t last_non_b_time;
    uint16_t pp_time;               /* time distance between the last 2 p,s,i frames */
    uint16_t bp_time;               /* time distance between the last b and p,s,i frame */
    int shape;
    int vol_sprite_usage;
    int sprite_width;
    int sprite_height;
    int sprite_left;
    int sprite_top;
    int sprite_brightness_change;
    int num_sprite_warping_points;
    int real_sprite_warping_points;
    int sprite_offset[2][2];
    int sprite_delta[2][2][2];
    int sprite_shift[2][2];
    int mcsel;
    int quant_precision;
    int quarter_sample;              /* 1->qpel, 0->half pel ME/MC */ 
    int scalability;
    int new_pred;
    int reduced_res_vop;
    int aspect_ratio_info;
    int sprite_warping_accuracy;
    int low_latency_sprite;
    int data_partioning;
    int resync_marker;
    int resync_x_pos;
    int low_delay;                   /* no reordering needed / has no b-frames */
    int vo_type;
    int vol_control_parameters;      /* does the stream contain the low_delay flag, used to workaround buggy encoders */

    /* divx specific, used to workaround (many) bugs in divx5 */
    int divx_version;
    int divx_build;
#define BITSTREAM_BUFFER_SIZE 1024*256
    uint8_t *bitstream_buffer; //Divx 5.01 puts several frames in a single one, this is used to reorder them
    int bitstream_buffer_size;
    
    /* RV10 specific */
    int rv10_version; /* RV10 version: 0 or 3 */
    int rv10_first_dc_coded[3];
    
    /* MJPEG specific */
    struct MJpegContext *mjpeg_ctx;
    int mjpeg_vsample[3]; /* vertical sampling factors, default = {2, 1, 1} */
    int mjpeg_hsample[3]; /* horizontal sampling factors, default = {2, 1, 1} */
    int mjpeg_write_tables; /* do we want to have quantisation- and
			       huffmantables in the jpeg file ? */
    int mjpeg_data_only_frames; /* frames only with SOI, SOS and EOI markers */

    /* MSMPEG4 specific */
    int mv_table_index;
    int rl_table_index;
    int rl_chroma_table_index;
    int dc_table_index;
    int use_skip_mb_code;
    int slice_height;      /* in macroblocks */
    int first_slice_line;  /* used in mpeg4 too to handle resync markers */
    int flipflop_rounding;
    int bitrate;
    int msmpeg4_version;   /* 0=not msmpeg4, 1=mp41, 2=mp42, 3=mp43/divx3 */
    /* decompression specific */
    GetBitContext gb;

    /* MPEG2 specific - I wish I had not to support this mess. */
    int progressive_sequence;
    int mpeg_f_code[2][2];
    int picture_structure;
/* picture type */
#define PICT_TOP_FIELD     1
#define PICT_BOTTOM_FIELD  2
#define PICT_FRAME         3

    int intra_dc_precision;
    int frame_pred_frame_dct;
    int top_field_first;
    int concealment_motion_vectors;
    int q_scale_type;
    int intra_vlc_format;
    int alternate_scan;
    int repeat_first_field;
    int chroma_420_type;
    int progressive_frame;
    int mpeg2;
    int full_pel[2];
    int interlaced_dct;
    int last_qscale;
    int first_slice;
    
    /* RTP specific */
    /* These are explained on avcodec.h */
    int rtp_mode;
    int rtp_payload_size;
    void (*rtp_callback)(void *data, int size, int packet_number);
    UINT8 *ptr_lastgob;
    UINT8 *ptr_last_mb_line;
    UINT32 mb_line_avgsize;
    
    DCTELEM (*block)[64]; /* points to one of the following blocks */
    DCTELEM blocks[2][6][64] __align8; // for HQ mode we need to keep the best block
    void (*dct_unquantize_mpeg1)(struct MpegEncContext *s, 
                           DCTELEM *block, int n, int qscale);
    void (*dct_unquantize_mpeg2)(struct MpegEncContext *s, 
                           DCTELEM *block, int n, int qscale);
    void (*dct_unquantize_h263)(struct MpegEncContext *s, 
                           DCTELEM *block, int n, int qscale);
    void (*dct_unquantize)(struct MpegEncContext *s, // unquantizer to use (mpeg4 can use both)
                           DCTELEM *block, int n, int qscale);
} MpegEncContext;

int MPV_common_init(MpegEncContext *s);
void MPV_common_end(MpegEncContext *s);
void MPV_decode_mb(MpegEncContext *s, DCTELEM block[6][64]);
void MPV_frame_start(MpegEncContext *s);
void MPV_frame_end(MpegEncContext *s);
#ifdef HAVE_MMX
void MPV_common_init_mmx(MpegEncContext *s);
#endif
int (*dct_quantize)(MpegEncContext *s, DCTELEM *block, int n, int qscale, int *overflow);
void (*draw_edges)(UINT8 *buf, int wrap, int width, int height, int w);

/* motion_est.c */
void ff_estimate_p_frame_motion(MpegEncContext * s,
                             int mb_x, int mb_y);
void ff_estimate_b_frame_motion(MpegEncContext * s,
                             int mb_x, int mb_y);
int ff_get_best_fcode(MpegEncContext * s, int16_t (*mv_table)[2], int type);
void ff_fix_long_p_mvs(MpegEncContext * s);
void ff_fix_long_b_mvs(MpegEncContext * s, int16_t (*mv_table)[2], int f_code, int type);

/* mpeg12.c */
extern INT16 default_intra_matrix[64];
extern INT16 default_non_intra_matrix[64];

void mpeg1_encode_picture_header(MpegEncContext *s, int picture_number);
void mpeg1_encode_mb(MpegEncContext *s,
                     DCTELEM block[6][64],
                     int motion_x, int motion_y);
void mpeg1_encode_init(MpegEncContext *s);

/* h263enc.c */

/* run length table */
#define MAX_RUN    64
#define MAX_LEVEL  64

typedef struct RLTable {
    int n; /* number of entries of table_vlc minus 1 */
    int last; /* number of values for last = 0 */
    const UINT16 (*table_vlc)[2];
    const INT8 *table_run;
    const INT8 *table_level;
    UINT8 *index_run[2]; /* encoding only */
    INT8 *max_level[2]; /* encoding & decoding */
    INT8 *max_run[2];   /* encoding & decoding */
    VLC vlc;            /* decoding only */
} RLTable;

void init_rl(RLTable *rl);
void init_vlc_rl(RLTable *rl);

static inline int get_rl_index(const RLTable *rl, int last, int run, int level)
{
    int index;
    index = rl->index_run[last][run];
    if (index >= rl->n)
        return rl->n;
    if (level > rl->max_level[last][run])
        return rl->n;
    return index + level - 1;
}

void h263_encode_mb(MpegEncContext *s, 
                    DCTELEM block[6][64],
                    int motion_x, int motion_y);
void mpeg4_encode_mb(MpegEncContext *s, 
                    DCTELEM block[6][64],
                    int motion_x, int motion_y);
void h263_encode_picture_header(MpegEncContext *s, int picture_number);
int h263_encode_gob_header(MpegEncContext * s, int mb_line);
void h263_dc_scale(MpegEncContext *s);
INT16 *h263_pred_motion(MpegEncContext * s, int block, 
                        int *px, int *py);
void mpeg4_pred_ac(MpegEncContext * s, INT16 *block, int n, 
                   int dir);
void ff_set_mpeg4_time(MpegEncContext * s, int picture_number);
void mpeg4_encode_picture_header(MpegEncContext *s, int picture_number);
void h263_encode_init(MpegEncContext *s);

void h263_decode_init_vlc(MpegEncContext *s);
int h263_decode_picture_header(MpegEncContext *s);
int h263_decode_gob_header(MpegEncContext *s);
int mpeg4_decode_picture_header(MpegEncContext * s);
int intel_h263_decode_picture_header(MpegEncContext *s);
int h263_decode_mb(MpegEncContext *s,
                   DCTELEM block[6][64]);
int h263_get_picture_format(int width, int height);

/* rv10.c */
void rv10_encode_picture_header(MpegEncContext *s, int picture_number);
int rv_decode_dc(MpegEncContext *s, int n);

/* msmpeg4.c */
void msmpeg4_encode_picture_header(MpegEncContext * s, int picture_number);
void msmpeg4_encode_ext_header(MpegEncContext * s);
void msmpeg4_encode_mb(MpegEncContext * s, 
                       DCTELEM block[6][64],
                       int motion_x, int motion_y);
int msmpeg4_decode_picture_header(MpegEncContext * s);
int msmpeg4_decode_ext_header(MpegEncContext * s, int buf_size);
int msmpeg4_decode_mb(MpegEncContext *s, 
                      DCTELEM block[6][64]);
int msmpeg4_decode_init_vlc(MpegEncContext *s);

/* mjpegenc.c */

int mjpeg_init(MpegEncContext *s);
void mjpeg_close(MpegEncContext *s);
void mjpeg_encode_mb(MpegEncContext *s, 
                     DCTELEM block[6][64]);
void mjpeg_picture_header(MpegEncContext *s);
void mjpeg_picture_trailer(MpegEncContext *s);

/* rate control */
int ff_rate_control_init(MpegEncContext *s);
int ff_rate_estimate_qscale(MpegEncContext *s);
int ff_rate_estimate_qscale_pass2(MpegEncContext *s);
void ff_write_pass1_stats(MpegEncContext *s);
void ff_rate_control_uninit(MpegEncContext *s);




