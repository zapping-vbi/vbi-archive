#include        <stdio.h>
#include        <string.h>
#include        <math.h>
#include        "../common.h"

#define LAYER1
#define LAYER2
#define LAYER3
#define REAL_IS_FLOAT
//# define NEW_DCT9

#ifdef REAL_IS_FLOAT
#  define real float
#elif defined(REAL_IS_LONG_DOUBLE)
#  define real long double
#else
#  define real double
#endif

#ifdef __GNUC__
#define INLINE inline
#else
#define INLINE
#endif

/* AUDIOBUFSIZE = n*64 with n=1,2,3 ...  */

#ifndef FALSE
#define         FALSE                   0
#endif
#ifndef FALSE
#define         TRUE                    1
#endif

#define         SBLIMIT                 32
#define         SSLIMIT                 18

#define         SCALE_BLOCK             12


#define         MPG_MD_STEREO           0
#define         MPG_MD_JOINT_STEREO     1
#define         MPG_MD_DUAL_CHANNEL     2
#define         MPG_MD_MONO             3

#define MAXFRAMESIZE 1792


/* Pre Shift fo 16 to 8 bit converter table */
#define AUSHIFT (3)

struct frame {
    int stereo;
    int jsbound;
    int single;
    int lsf;
    int mpeg25;
    int header_change;
    int bitrate_index;
    int sampling_frequency;
    int padding;
    int extension;
    int mode;
    int mode_ext;
    int copyright;
    int original;
    int emphasis;

    /* layer2 stuff */
    int II_sblimit;
    void *alloc;
};

struct parameter {
	int quiet;	/* shut up! */
	int tryresync;  /* resync stream after error */
	int verbose;    /* verbose level */
	int checkrange;
};

extern int set_pointer(long);

extern void make_decode_tables(long scaleval);
extern int do_layer3(struct frame *fr,unsigned char *,int *);
extern int do_layer2(struct frame *fr,unsigned char *,int *);
extern int do_layer1(struct frame *fr,unsigned char *,int *);



struct gr_info_s {
      int scfsi;
      unsigned part2_3_length;
      unsigned big_values;
      unsigned scalefac_compress;
      unsigned block_type;
      unsigned mixed_block_flag;
      unsigned table_select[3];
      unsigned subblock_gain[3];
      unsigned maxband[3];
      unsigned maxbandl;
      unsigned maxb;
      unsigned region1start;
      unsigned region2start;
      unsigned preflag;
      unsigned scalefac_scale;
      unsigned count1table_select;
      real *full_gain[3];
      real *pow2gain;
};

struct III_sideinfo
{
  unsigned main_data_begin;
  unsigned private_bits;
  struct {
    struct gr_info_s gr[2];
  } ch[2];
};

extern int synth_1to1 (real *,int,unsigned char *,int *);
extern int synth_1to1_mono (real *,unsigned char *,int *);

extern void init_layer3(int);
extern void init_layer2(void);
extern void make_decode_tables(long scale);
extern void dct64(real *,real *,real *);

extern void synth_ntom_set_step(long,long);

extern real muls[27][64];
extern real decwin[512+32];
extern real *pnts[5];

struct mpstr {
	struct frame fr;
	real hybrid_block[2][2][SBLIMIT*SSLIMIT];
	int hybrid_blc[2];
	real synth_buffs[2][2][0x110];
        int  synth_bo;
};

extern struct mpstr *gmp;
extern GetBitContext *gmp_gb;

extern inline int getbits_fast(int n) 
{
    if (n == 0)
        return 0;
    else
        return get_bits(gmp_gb, n);
}

#define getbits(n) getbits_fast(n)
#define get1bit() get_bits(gmp_gb, 1)
