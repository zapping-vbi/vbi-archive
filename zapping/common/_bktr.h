/* Generated file, do not edit! */

#include <stdio.h>
#include "common/device.h"

#ifndef __GNUC__
#undef __attribute__
#define __attribute__(x)
#endif

static void
fprint_METEORSSIGNAL (FILE *fp, int rw __attribute__ ((unused)), unsigned int *arg)
{
fprint_symbolic (fp, 0, (unsigned long) *arg,
"MODE_MASK", (unsigned long) METEOR_SIG_MODE_MASK,
"FRAME", (unsigned long) METEOR_SIG_FRAME,
"FIELD", (unsigned long) METEOR_SIG_FIELD,
0);
}

static void
fprint_METEORSINPUT (FILE *fp, int rw __attribute__ ((unused)), unsigned long *arg)
{
fprint_symbolic (fp, 0, (unsigned long) *arg,
"DEV0", (unsigned long) METEOR_INPUT_DEV0,
"DEV_RCA", (unsigned long) METEOR_INPUT_DEV_RCA,
"DEV1", (unsigned long) METEOR_INPUT_DEV1,
"DEV2", (unsigned long) METEOR_INPUT_DEV2,
"DEV3", (unsigned long) METEOR_INPUT_DEV3,
"DEV_RGB", (unsigned long) METEOR_INPUT_DEV_RGB,
"DEV_SVIDEO", (unsigned long) METEOR_INPUT_DEV_SVIDEO,
0);
}

static void
fprint_struct_bktr_remote (FILE *fp, int rw __attribute__ ((unused)), const struct bktr_remote *t)
{
fputs ("data[]=? ", fp);
}

static void
fprint_BT848_SAUDIO (FILE *fp, int rw __attribute__ ((unused)), int *arg)
{
fprint_symbolic (fp, 0, (unsigned long) *arg,
"TUNER", (unsigned long) AUDIO_TUNER,
"EXTERN", (unsigned long) AUDIO_EXTERN,
"INTERN", (unsigned long) AUDIO_INTERN,
"MUTE", (unsigned long) AUDIO_MUTE,
"UNMUTE", (unsigned long) AUDIO_UNMUTE,
0);
}

static void
fprint_struct_meteor_video (FILE *fp, int rw __attribute__ ((unused)), const struct meteor_video *t)
{
fprintf (fp, "addr=%lu "
"width=%lu "
"banksize=%lu "
"ramsize=%lu ",
(unsigned long) t->addr, 
(unsigned long) t->width, 
(unsigned long) t->banksize, 
(unsigned long) t->ramsize);
}

static void
fprint_symbol_meteor_geo_ (FILE *fp, int rw __attribute__ ((unused)), unsigned long value)
{
fprint_symbolic (fp, 0, value,
"RGB16", (unsigned long) METEOR_GEO_RGB16,
"RGB24", (unsigned long) METEOR_GEO_RGB24,
"YUV_PACKED", (unsigned long) METEOR_GEO_YUV_PACKED,
"YUV_PLANAR", (unsigned long) METEOR_GEO_YUV_PLANAR,
"YUV_PLANER", (unsigned long) METEOR_GEO_YUV_PLANER,
"UNSIGNED", (unsigned long) METEOR_GEO_UNSIGNED,
"EVEN_ONLY", (unsigned long) METEOR_GEO_EVEN_ONLY,
"ODD_ONLY", (unsigned long) METEOR_GEO_ODD_ONLY,
"FIELD_MASK", (unsigned long) METEOR_GEO_FIELD_MASK,
"YUV_422", (unsigned long) METEOR_GEO_YUV_422,
"OUTPUT_MASK", (unsigned long) METEOR_GEO_OUTPUT_MASK,
"YUV_12", (unsigned long) METEOR_GEO_YUV_12,
"YUV_9", (unsigned long) METEOR_GEO_YUV_9,
0);
}

static void
fprint_struct_meteor_geomet (FILE *fp, int rw __attribute__ ((unused)), const struct meteor_geomet *t)
{
fprintf (fp, "rows=%lu "
"columns=%lu "
"frames=%lu "
"oformat=",
(unsigned long) t->rows, 
(unsigned long) t->columns, 
(unsigned long) t->frames);
fprint_symbol_meteor_geo_ (fp, rw, t->oformat);
fputs (" ", fp);
}

static void
fprint_METEORCAPTUR (FILE *fp, int rw __attribute__ ((unused)), int *arg)
{
fprint_symbolic (fp, 0, (unsigned long) *arg,
"SINGLE", (unsigned long) METEOR_CAP_SINGLE,
"CONTINOUS", (unsigned long) METEOR_CAP_CONTINOUS,
"STOP_CONT", (unsigned long) METEOR_CAP_STOP_CONT,
"N_FRAMES", (unsigned long) METEOR_CAP_N_FRAMES,
"STOP_FRAMES", (unsigned long) METEOR_CAP_STOP_FRAMES,
0);
}

static void
fprint_BT848SFMT (FILE *fp, int rw __attribute__ ((unused)), unsigned long *arg)
{
fprint_symbolic (fp, 0, (unsigned long) *arg,
"RSVD", (unsigned long) BT848_IFORM_F_RSVD,
"SECAM", (unsigned long) BT848_IFORM_F_SECAM,
"PALN", (unsigned long) BT848_IFORM_F_PALN,
"PALM", (unsigned long) BT848_IFORM_F_PALM,
"PALBDGHI", (unsigned long) BT848_IFORM_F_PALBDGHI,
"NTSCJ", (unsigned long) BT848_IFORM_F_NTSCJ,
"NTSCM", (unsigned long) BT848_IFORM_F_NTSCM,
"AUTO", (unsigned long) BT848_IFORM_F_AUTO,
0);
}

static void
fprint_struct__bktr_clip (FILE *fp, int rw __attribute__ ((unused)), const struct _bktr_clip *t)
{
fputs ("x[]=? ", fp);
}

static void
fprint_TVTUNER_GETCHNL (FILE *fp, int rw __attribute__ ((unused)), unsigned int *arg)
{
fprint_symbolic (fp, 0, (unsigned long) *arg,
"NABCST", (unsigned long) CHNLSET_NABCST,
"CABLEIRC", (unsigned long) CHNLSET_CABLEIRC,
"CABLEHRC", (unsigned long) CHNLSET_CABLEHRC,
"WEUROPE", (unsigned long) CHNLSET_WEUROPE,
"JPNBCST", (unsigned long) CHNLSET_JPNBCST,
"JPNCABLE", (unsigned long) CHNLSET_JPNCABLE,
"XUSSR", (unsigned long) CHNLSET_XUSSR,
"AUSTRALIA", (unsigned long) CHNLSET_AUSTRALIA,
"FRANCE", (unsigned long) CHNLSET_FRANCE,
"MIN", (unsigned long) CHNLSET_MIN,
"MAX", (unsigned long) CHNLSET_MAX,
0);
}

static void
fprint_METEORSFMT (FILE *fp, int rw __attribute__ ((unused)), unsigned long *arg)
{
fprint_symbolic (fp, 0, (unsigned long) *arg,
"NTSC", (unsigned long) METEOR_FMT_NTSC,
"PAL", (unsigned long) METEOR_FMT_PAL,
"SECAM", (unsigned long) METEOR_FMT_SECAM,
"AUTOMODE", (unsigned long) METEOR_FMT_AUTOMODE,
0);
}

static void
fprint_METEORGSIGNAL (FILE *fp, int rw __attribute__ ((unused)), unsigned int *arg)
{
fprint_symbolic (fp, 0, (unsigned long) *arg,
"MODE_MASK", (unsigned long) METEOR_SIG_MODE_MASK,
"FRAME", (unsigned long) METEOR_SIG_FRAME,
"FIELD", (unsigned long) METEOR_SIG_FIELD,
0);
}

static void
fprint_struct_bktr_msp_control (FILE *fp, int rw __attribute__ ((unused)), const struct bktr_msp_control *t)
{
fprintf (fp, "function=%lu "
"address=%lu "
"data=%lu ",
(unsigned long) t->function, 
(unsigned long) t->address, 
(unsigned long) t->data);
}

static void
fprint_METEORSTATUS (FILE *fp, int rw __attribute__ ((unused)), unsigned short *arg)
{
fprint_symbolic (fp, 0, (unsigned long) *arg,
"ID_MASK", (unsigned long) METEOR_STATUS_ID_MASK,
"DIR", (unsigned long) METEOR_STATUS_DIR,
"OEF", (unsigned long) METEOR_STATUS_OEF,
"SVP", (unsigned long) METEOR_STATUS_SVP,
"STTC", (unsigned long) METEOR_STATUS_STTC,
"HCLK", (unsigned long) METEOR_STATUS_HCLK,
"FIDT", (unsigned long) METEOR_STATUS_FIDT,
"ALTD", (unsigned long) METEOR_STATUS_ALTD,
"CODE", (unsigned long) METEOR_STATUS_CODE,
0);
}

static void
fprint_struct_meteor_capframe (FILE *fp, int rw __attribute__ ((unused)), const struct meteor_capframe *t)
{
fprintf (fp, "command=%ld "
"lowat=%ld "
"hiwat=%ld ",
(long) t->command, 
(long) t->lowat, 
(long) t->hiwat);
}

static void
fprint_RADIO_SETMODE (FILE *fp, int rw __attribute__ ((unused)), unsigned int *arg)
{
fprint_symbolic (fp, 0, (unsigned long) *arg,
"AFC", (unsigned long) RADIO_AFC,
"MONO", (unsigned long) RADIO_MONO,
"MUTE", (unsigned long) RADIO_MUTE,
0);
}

static void
fprint_struct_meteor_pixfmt (FILE *fp, int rw __attribute__ ((unused)), const struct meteor_pixfmt *t)
{
fprintf (fp, "index=%lu "
"type=? "
"Bpp=%lu "
"masks[]=? ",
(unsigned long) t->index, 

(unsigned long) t->Bpp);
}

static void
fprint_struct_bktr_capture_area (FILE *fp, int rw __attribute__ ((unused)), const struct bktr_capture_area *t)
{
fprintf (fp, "x_offset=%ld "
"y_offset=%ld "
"x_size=%ld "
"y_size=%ld ",
(long) t->x_offset, 
(long) t->y_offset, 
(long) t->x_size, 
(long) t->y_size);
}

static void
fprint_struct_meteor_counts (FILE *fp, int rw __attribute__ ((unused)), const struct meteor_counts *t)
{
fprintf (fp, "fifo_errors=%lu "
"dma_errors=%lu "
"frames_captured=%lu "
"even_fields_captured=%lu "
"odd_fields_captured=%lu ",
(unsigned long) t->fifo_errors, 
(unsigned long) t->dma_errors, 
(unsigned long) t->frames_captured, 
(unsigned long) t->even_fields_captured, 
(unsigned long) t->odd_fields_captured);
}

static void
fprint_METEORGINPUT (FILE *fp, int rw __attribute__ ((unused)), unsigned long *arg)
{
fprint_symbolic (fp, 0, (unsigned long) *arg,
"DEV0", (unsigned long) METEOR_INPUT_DEV0,
"DEV_RCA", (unsigned long) METEOR_INPUT_DEV_RCA,
"DEV1", (unsigned long) METEOR_INPUT_DEV1,
"DEV2", (unsigned long) METEOR_INPUT_DEV2,
"DEV3", (unsigned long) METEOR_INPUT_DEV3,
"DEV_RGB", (unsigned long) METEOR_INPUT_DEV_RGB,
"DEV_SVIDEO", (unsigned long) METEOR_INPUT_DEV_SVIDEO,
0);
}

static void
fprint_METEORGFMT (FILE *fp, int rw __attribute__ ((unused)), unsigned long *arg)
{
fprint_symbolic (fp, 0, (unsigned long) *arg,
"NTSC", (unsigned long) METEOR_FMT_NTSC,
"PAL", (unsigned long) METEOR_FMT_PAL,
"SECAM", (unsigned long) METEOR_FMT_SECAM,
"AUTOMODE", (unsigned long) METEOR_FMT_AUTOMODE,
0);
}

static void
fprint_RADIO_GETMODE (FILE *fp, int rw __attribute__ ((unused)), unsigned char *arg)
{
fprint_symbolic (fp, 0, (unsigned long) *arg,
"AFC", (unsigned long) RADIO_AFC,
"MONO", (unsigned long) RADIO_MONO,
"MUTE", (unsigned long) RADIO_MUTE,
0);
}

static void
fprint_struct_eeProm (FILE *fp, int rw __attribute__ ((unused)), const struct eeProm *t)
{
fprintf (fp, "offset=%ld "
"count=%ld ",
(long) t->offset, 
(long) t->count);
}

static void
fprint_BT848_GAUDIO (FILE *fp, int rw __attribute__ ((unused)), int *arg)
{
fprint_symbolic (fp, 0, (unsigned long) *arg,
"TUNER", (unsigned long) AUDIO_TUNER,
"EXTERN", (unsigned long) AUDIO_EXTERN,
"INTERN", (unsigned long) AUDIO_INTERN,
"MUTE", (unsigned long) AUDIO_MUTE,
"UNMUTE", (unsigned long) AUDIO_UNMUTE,
0);
}

static void
fprint_struct_bktr_chnlset (FILE *fp, int rw __attribute__ ((unused)), const struct bktr_chnlset *t)
{
fprintf (fp, "index=%ld "
"max_channel=%ld "
"name=\"%.*s\" ",
(long) t->index, 
(long) t->max_channel, 
BT848_MAX_CHNLSET_NAME_LEN, (const char *) t->name);
}

static void
fprint_TVTUNER_SETCHNL (FILE *fp, int rw __attribute__ ((unused)), unsigned int *arg)
{
fprint_symbolic (fp, 0, (unsigned long) *arg,
"NABCST", (unsigned long) CHNLSET_NABCST,
"CABLEIRC", (unsigned long) CHNLSET_CABLEIRC,
"CABLEHRC", (unsigned long) CHNLSET_CABLEHRC,
"WEUROPE", (unsigned long) CHNLSET_WEUROPE,
"JPNBCST", (unsigned long) CHNLSET_JPNBCST,
"JPNCABLE", (unsigned long) CHNLSET_JPNCABLE,
"XUSSR", (unsigned long) CHNLSET_XUSSR,
"AUSTRALIA", (unsigned long) CHNLSET_AUSTRALIA,
"FRANCE", (unsigned long) CHNLSET_FRANCE,
"MIN", (unsigned long) CHNLSET_MIN,
"MAX", (unsigned long) CHNLSET_MAX,
0);
}

static void
fprint_ioctl_arg (FILE *fp, unsigned int cmd, int rw, void *arg)
{
switch (cmd) {
case METEORSSIGNAL:
if (!arg) { fputs ("METEORSSIGNAL", fp); return; }
 fprint_METEORSSIGNAL (fp, rw, arg);
break;
case METEORSINPUT:
if (!arg) { fputs ("METEORSINPUT", fp); return; }
 fprint_METEORSINPUT (fp, rw, arg);
break;
case REMOTE_GETKEY:
if (!arg) { fputs ("REMOTE_GETKEY", fp); return; }
 fprint_struct_bktr_remote (fp, rw, arg);
break;
case TVTUNER_SETTYPE:
if (!arg) { fputs ("TVTUNER_SETTYPE", fp); return; }
case TVTUNER_GETTYPE:
if (!arg) { fputs ("TVTUNER_GETTYPE", fp); return; }
case TVTUNER_GETSTATUS:
if (!arg) { fputs ("TVTUNER_GETSTATUS", fp); return; }
case TVTUNER_SETFREQ:
if (!arg) { fputs ("TVTUNER_SETFREQ", fp); return; }
case TVTUNER_GETFREQ:
if (!arg) { fputs ("TVTUNER_GETFREQ", fp); return; }
case BT848_GSTATUS:
if (!arg) { fputs ("BT848_GSTATUS", fp); return; }
case RADIO_SETFREQ:
if (!arg) { fputs ("RADIO_SETFREQ", fp); return; }
case RADIO_GETFREQ:
if (!arg) { fputs ("RADIO_GETFREQ", fp); return; }
 fprintf (fp, "%lu", (unsigned long) * (unsigned int *) arg);
break;
case BT848_SAUDIO:
if (!arg) { fputs ("BT848_SAUDIO", fp); return; }
 fprint_BT848_SAUDIO (fp, rw, arg);
break;
case METEORSVIDEO:
if (!arg) { fputs ("METEORSVIDEO", fp); return; }
case METEORGVIDEO:
if (!arg) { fputs ("METEORGVIDEO", fp); return; }
 fprint_struct_meteor_video (fp, rw, arg);
break;
case METEORSETGEO:
if (!arg) { fputs ("METEORSETGEO", fp); return; }
case METEORGETGEO:
if (!arg) { fputs ("METEORGETGEO", fp); return; }
 fprint_struct_meteor_geomet (fp, rw, arg);
break;
case METEORCAPTUR:
if (!arg) { fputs ("METEORCAPTUR", fp); return; }
 fprint_METEORCAPTUR (fp, rw, arg);
break;
case BT848SFMT:
if (!arg) { fputs ("BT848SFMT", fp); return; }
 fprint_BT848SFMT (fp, rw, arg);
break;
case BT848SCLIP:
if (!arg) { fputs ("BT848SCLIP", fp); return; }
case BT848GCLIP:
if (!arg) { fputs ("BT848GCLIP", fp); return; }
 fprint_struct__bktr_clip (fp, rw, arg);
break;
case TVTUNER_GETCHNL:
if (!arg) { fputs ("TVTUNER_GETCHNL", fp); return; }
 fprint_TVTUNER_GETCHNL (fp, rw, arg);
break;
case METEORSFMT:
if (!arg) { fputs ("METEORSFMT", fp); return; }
 fprint_METEORSFMT (fp, rw, arg);
break;
case METEORGSIGNAL:
if (!arg) { fputs ("METEORGSIGNAL", fp); return; }
 fprint_METEORGSIGNAL (fp, rw, arg);
break;
case BT848_I2CWR:
if (!arg) { fputs ("BT848_I2CWR", fp); return; }
 fprintf (fp, "%lu", (unsigned long) * (u_long *) arg);
break;
case BT848_MSP_READ:
if (!arg) { fputs ("BT848_MSP_READ", fp); return; }
case BT848_MSP_WRITE:
if (!arg) { fputs ("BT848_MSP_WRITE", fp); return; }
 fprint_struct_bktr_msp_control (fp, rw, arg);
break;
case METEORSCHCV:
if (!arg) { fputs ("METEORSCHCV", fp); return; }
case METEORGCHCV:
if (!arg) { fputs ("METEORGCHCV", fp); return; }
case METEORSBRIG:
if (!arg) { fputs ("METEORSBRIG", fp); return; }
case METEORGBRIG:
if (!arg) { fputs ("METEORGBRIG", fp); return; }
case METEORSCSAT:
if (!arg) { fputs ("METEORSCSAT", fp); return; }
case METEORGCSAT:
if (!arg) { fputs ("METEORGCSAT", fp); return; }
case METEORSCONT:
if (!arg) { fputs ("METEORSCONT", fp); return; }
case METEORGCONT:
if (!arg) { fputs ("METEORGCONT", fp); return; }
case METEORSHWS:
if (!arg) { fputs ("METEORSHWS", fp); return; }
case METEORGHWS:
if (!arg) { fputs ("METEORGHWS", fp); return; }
case METEORSVWS:
if (!arg) { fputs ("METEORSVWS", fp); return; }
case METEORGVWS:
if (!arg) { fputs ("METEORGVWS", fp); return; }
case METEORSTS:
if (!arg) { fputs ("METEORSTS", fp); return; }
case METEORGTS:
if (!arg) { fputs ("METEORGTS", fp); return; }
 fprintf (fp, "%lu", (unsigned long) * (unsigned char *) arg);
break;
case METEORSTATUS:
if (!arg) { fputs ("METEORSTATUS", fp); return; }
 fprint_METEORSTATUS (fp, rw, arg);
break;
case METEORCAPFRM:
if (!arg) { fputs ("METEORCAPFRM", fp); return; }
 fprint_struct_meteor_capframe (fp, rw, arg);
break;
case RADIO_SETMODE:
if (!arg) { fputs ("RADIO_SETMODE", fp); return; }
 fprint_RADIO_SETMODE (fp, rw, arg);
break;
case METEORGSUPPIXFMT:
if (!arg) { fputs ("METEORGSUPPIXFMT", fp); return; }
 fprint_struct_meteor_pixfmt (fp, rw, arg);
break;
case BT848_SCAPAREA:
if (!arg) { fputs ("BT848_SCAPAREA", fp); return; }
case BT848_GCAPAREA:
if (!arg) { fputs ("BT848_GCAPAREA", fp); return; }
 fprint_struct_bktr_capture_area (fp, rw, arg);
break;
case BT848_SHUE:
if (!arg) { fputs ("BT848_SHUE", fp); return; }
case BT848_GHUE:
if (!arg) { fputs ("BT848_GHUE", fp); return; }
case BT848_SBRIG:
if (!arg) { fputs ("BT848_SBRIG", fp); return; }
case BT848_GBRIG:
if (!arg) { fputs ("BT848_GBRIG", fp); return; }
case BT848_SCSAT:
if (!arg) { fputs ("BT848_SCSAT", fp); return; }
case BT848_GCSAT:
if (!arg) { fputs ("BT848_GCSAT", fp); return; }
case BT848_SCONT:
if (!arg) { fputs ("BT848_SCONT", fp); return; }
case BT848_GCONT:
if (!arg) { fputs ("BT848_GCONT", fp); return; }
case BT848_SVSAT:
if (!arg) { fputs ("BT848_SVSAT", fp); return; }
case BT848_GVSAT:
if (!arg) { fputs ("BT848_GVSAT", fp); return; }
case BT848_SUSAT:
if (!arg) { fputs ("BT848_SUSAT", fp); return; }
case BT848_GUSAT:
if (!arg) { fputs ("BT848_GUSAT", fp); return; }
case BT848_SCBARS:
if (!arg) { fputs ("BT848_SCBARS", fp); return; }
case BT848_CCBARS:
if (!arg) { fputs ("BT848_CCBARS", fp); return; }
case BT848_SBTSC:
if (!arg) { fputs ("BT848_SBTSC", fp); return; }
case TVTUNER_SETAFC:
if (!arg) { fputs ("TVTUNER_SETAFC", fp); return; }
case TVTUNER_GETAFC:
if (!arg) { fputs ("TVTUNER_GETAFC", fp); return; }
case BT848_SLNOTCH:
if (!arg) { fputs ("BT848_SLNOTCH", fp); return; }
case BT848_GLNOTCH:
if (!arg) { fputs ("BT848_GLNOTCH", fp); return; }
case METEORSACTPIXFMT:
if (!arg) { fputs ("METEORSACTPIXFMT", fp); return; }
case METEORGACTPIXFMT:
if (!arg) { fputs ("METEORGACTPIXFMT", fp); return; }
case BT848SCBUF:
if (!arg) { fputs ("BT848SCBUF", fp); return; }
case BT848GCBUF:
if (!arg) { fputs ("BT848GCBUF", fp); return; }
case BT848_GPIO_SET_EN:
if (!arg) { fputs ("BT848_GPIO_SET_EN", fp); return; }
case BT848_GPIO_GET_EN:
if (!arg) { fputs ("BT848_GPIO_GET_EN", fp); return; }
case BT848_GPIO_SET_DATA:
if (!arg) { fputs ("BT848_GPIO_SET_DATA", fp); return; }
case BT848_GPIO_GET_DATA:
if (!arg) { fputs ("BT848_GPIO_GET_DATA", fp); return; }
 fprintf (fp, "%ld", (long) * (int *) arg);
break;
case BT848GFMT:
if (!arg) { fputs ("BT848GFMT", fp); return; }
 fprintf (fp, "%lu", (unsigned long) * (unsigned long *) arg);
break;
case METEORSCOUNT:
if (!arg) { fputs ("METEORSCOUNT", fp); return; }
case METEORGCOUNT:
if (!arg) { fputs ("METEORGCOUNT", fp); return; }
 fprint_struct_meteor_counts (fp, rw, arg);
break;
case METEORGINPUT:
if (!arg) { fputs ("METEORGINPUT", fp); return; }
 fprint_METEORGINPUT (fp, rw, arg);
break;
case METEORGFMT:
if (!arg) { fputs ("METEORGFMT", fp); return; }
 fprint_METEORGFMT (fp, rw, arg);
break;
case RADIO_GETMODE:
if (!arg) { fputs ("RADIO_GETMODE", fp); return; }
 fprint_RADIO_GETMODE (fp, rw, arg);
break;
case BT848_WEEPROM:
if (!arg) { fputs ("BT848_WEEPROM", fp); return; }
case BT848_REEPROM:
if (!arg) { fputs ("BT848_REEPROM", fp); return; }
case BT848_SIGNATURE:
if (!arg) { fputs ("BT848_SIGNATURE", fp); return; }
 fprint_struct_eeProm (fp, rw, arg);
break;
case BT848_GAUDIO:
if (!arg) { fputs ("BT848_GAUDIO", fp); return; }
 fprint_BT848_GAUDIO (fp, rw, arg);
break;
case TVTUNER_GETCHNLSET:
if (!arg) { fputs ("TVTUNER_GETCHNLSET", fp); return; }
 fprint_struct_bktr_chnlset (fp, rw, arg);
break;
case METEORSFPS:
if (!arg) { fputs ("METEORSFPS", fp); return; }
case METEORGFPS:
if (!arg) { fputs ("METEORGFPS", fp); return; }
case METEORSBT254:
if (!arg) { fputs ("METEORSBT254", fp); return; }
case METEORGBT254:
if (!arg) { fputs ("METEORGBT254", fp); return; }
 fprintf (fp, "%lu", (unsigned long) * (unsigned short *) arg);
break;
case TVTUNER_SETCHNL:
if (!arg) { fputs ("TVTUNER_SETCHNL", fp); return; }
 fprint_TVTUNER_SETCHNL (fp, rw, arg);
break;
	default:
		if (!arg) { fprint_unknown_ioctl (fp, cmd, arg); return; }
		break;
	}
}

static __inline__ void IOCTL_ARG_TYPE_CHECK_TVTUNER_SETCHNL (const unsigned int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_TVTUNER_GETCHNL (unsigned int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_TVTUNER_SETTYPE (const unsigned int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_TVTUNER_GETTYPE (unsigned int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_TVTUNER_GETSTATUS (unsigned int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_TVTUNER_SETFREQ (const unsigned int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_TVTUNER_GETFREQ (unsigned int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_SHUE (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_GHUE (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_SBRIG (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_GBRIG (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_SCSAT (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_GCSAT (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_SCONT (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_GCONT (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_SVSAT (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_GVSAT (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_SUSAT (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_GUSAT (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_SCBARS (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_CCBARS (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_SAUDIO (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_GAUDIO (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_SBTSC (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_GSTATUS (unsigned int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_WEEPROM (struct eeProm *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_REEPROM (struct eeProm *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_SIGNATURE (struct eeProm *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_TVTUNER_SETAFC (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_TVTUNER_GETAFC (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_SLNOTCH (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_GLNOTCH (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_I2CWR (u_long *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_MSP_READ (struct bktr_msp_control *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_MSP_WRITE (struct bktr_msp_control *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_RADIO_SETMODE (const unsigned int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_RADIO_GETMODE (unsigned char *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_RADIO_SETFREQ (const unsigned int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_RADIO_GETFREQ (unsigned int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORSACTPIXFMT (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORGACTPIXFMT (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORGSUPPIXFMT (struct meteor_pixfmt *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848SCLIP (const struct _bktr_clip *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848GCLIP (struct _bktr_clip *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848SFMT (const unsigned long *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848GFMT (unsigned long *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848SCBUF (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848GCBUF (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_SCAPAREA (const struct bktr_capture_area *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_GCAPAREA (struct bktr_capture_area *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_TVTUNER_GETCHNLSET (struct bktr_chnlset *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_REMOTE_GETKEY (struct bktr_remote *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_GPIO_SET_EN (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_GPIO_GET_EN (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_GPIO_SET_DATA (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_BT848_GPIO_GET_DATA (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORCAPTUR (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORCAPFRM (const struct meteor_capframe *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORSETGEO (const struct meteor_geomet *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORGETGEO (struct meteor_geomet *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORSTATUS (unsigned short *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORSHUE (const signed char *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORGHUE (signed char *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORSFMT (const unsigned long *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORGFMT (unsigned long *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORSINPUT (const unsigned long *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORGINPUT (unsigned long *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORSCHCV (const unsigned char *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORGCHCV (unsigned char *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORSCOUNT (const struct meteor_counts *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORGCOUNT (struct meteor_counts *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORSFPS (const unsigned short *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORGFPS (unsigned short *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORSSIGNAL (const unsigned int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORGSIGNAL (unsigned int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORSVIDEO (const struct meteor_video *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORGVIDEO (struct meteor_video *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORSBRIG (const unsigned char *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORGBRIG (unsigned char *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORSCSAT (const unsigned char *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORGCSAT (unsigned char *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORSCONT (const unsigned char *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORGCONT (unsigned char *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORSBT254 (const unsigned short *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORGBT254 (unsigned short *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORSHWS (const unsigned char *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORGHWS (unsigned char *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORSVWS (const unsigned char *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORGVWS (unsigned char *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORSTS (const unsigned char *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_METEORGTS (unsigned char *arg __attribute__ ((unused))) {}

