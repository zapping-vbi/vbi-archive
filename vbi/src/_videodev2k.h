/* Generated file, do not edit! */

#include <stdio.h>
#include "io.h"

#ifndef __GNUC__
#undef __attribute__
#define __attribute__(x)
#endif

static size_t
snprint_enum_v4l2_buf_type (char *str, size_t size, int rw __attribute__ ((unused)), int value)
{
return snprint_symbolic (str, size, 1, value,
"VIDEO_CAPTURE", (unsigned long) V4L2_BUF_TYPE_VIDEO_CAPTURE,
"VIDEO_OUTPUT", (unsigned long) V4L2_BUF_TYPE_VIDEO_OUTPUT,
"VIDEO_OVERLAY", (unsigned long) V4L2_BUF_TYPE_VIDEO_OVERLAY,
"VBI_CAPTURE", (unsigned long) V4L2_BUF_TYPE_VBI_CAPTURE,
"VBI_OUTPUT", (unsigned long) V4L2_BUF_TYPE_VBI_OUTPUT,
"SLICED_VBI_CAPTURE", (unsigned long) V4L2_BUF_TYPE_SLICED_VBI_CAPTURE,
"SLICED_VBI_OUTPUT", (unsigned long) V4L2_BUF_TYPE_SLICED_VBI_OUTPUT,
"VIDEO_OUTPUT_OVERLAY", (unsigned long) V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY,
"PRIVATE", (unsigned long) V4L2_BUF_TYPE_PRIVATE,
(void *) 0);
}

static size_t
snprint_struct_v4l2_rect (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_rect *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "left=%ld "
"top=%ld "
"width=%ld "
"height=%ld ",
(long) t->left, 
(long) t->top, 
(long) t->width, 
(long) t->height);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_v4l2_crop (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_crop *t)
{
size_t n = 0;
n += strlcpy (str + n, "type=", size - n);
n = MIN (n, size);
n += snprint_enum_v4l2_buf_type (str + n, size - n, rw, t->type);
n += strlcpy (str + n, " c={", size - n);
n = MIN (n, size);
n += snprint_struct_v4l2_rect (str + n, size - n, rw, &t->c);
n += strlcpy (str + n, "} ", size - n);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_v4l2_fmtdesc (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_fmtdesc *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "index=%lu ",
(unsigned long) t->index);
n = MIN (n, size);
if (1 & rw) {
n += strlcpy (str + n, "type=", size - n);
n = MIN (n, size);
n += snprint_enum_v4l2_buf_type (str + n, size - n, rw, t->type);
n += strlcpy (str + n, " flags=", size - n);
n = MIN (n, size);
n += snprint_symbolic (str + n, size - n, 2, t->flags,
"COMPRESSED", (unsigned long) V4L2_FMT_FLAG_COMPRESSED,
(void *) 0);
n += snprintf (str + n, size - n, " description=\"%.*s\" "
"pixelformat=\"%.4s\"=0x%lx "
"reserved[] ",
32, (const char *) t->description, 
(const char *) & t->pixelformat, (unsigned long) t->pixelformat);
n = MIN (n, size);
}
return n;
}

static size_t
snprint_enum_v4l2_tuner_type (char *str, size_t size, int rw __attribute__ ((unused)), int value)
{
return snprint_symbolic (str, size, 1, value,
"RADIO", (unsigned long) V4L2_TUNER_RADIO,
"ANALOG_TV", (unsigned long) V4L2_TUNER_ANALOG_TV,
"DIGITAL_TV", (unsigned long) V4L2_TUNER_DIGITAL_TV,
(void *) 0);
}

static size_t
snprint_symbol_v4l2_tuner_cap_ (char *str, size_t size, int rw __attribute__ ((unused)), unsigned long value)
{
return snprint_symbolic (str, size, 0, value,
"LOW", (unsigned long) V4L2_TUNER_CAP_LOW,
"NORM", (unsigned long) V4L2_TUNER_CAP_NORM,
"STEREO", (unsigned long) V4L2_TUNER_CAP_STEREO,
"LANG2", (unsigned long) V4L2_TUNER_CAP_LANG2,
"SAP", (unsigned long) V4L2_TUNER_CAP_SAP,
"LANG1", (unsigned long) V4L2_TUNER_CAP_LANG1,
(void *) 0);
}

static size_t
snprint_symbol_v4l2_tuner_sub_ (char *str, size_t size, int rw __attribute__ ((unused)), unsigned long value)
{
return snprint_symbolic (str, size, 0, value,
"MONO", (unsigned long) V4L2_TUNER_SUB_MONO,
"STEREO", (unsigned long) V4L2_TUNER_SUB_STEREO,
"LANG2", (unsigned long) V4L2_TUNER_SUB_LANG2,
"SAP", (unsigned long) V4L2_TUNER_SUB_SAP,
"LANG1", (unsigned long) V4L2_TUNER_SUB_LANG1,
(void *) 0);
}

static size_t
snprint_symbol_v4l2_tuner_mode_ (char *str, size_t size, int rw __attribute__ ((unused)), unsigned long value)
{
return snprint_symbolic (str, size, 0, value,
"MONO", (unsigned long) V4L2_TUNER_MODE_MONO,
"STEREO", (unsigned long) V4L2_TUNER_MODE_STEREO,
"LANG2", (unsigned long) V4L2_TUNER_MODE_LANG2,
"SAP", (unsigned long) V4L2_TUNER_MODE_SAP,
"LANG1", (unsigned long) V4L2_TUNER_MODE_LANG1,
"LANG1_LANG2", (unsigned long) V4L2_TUNER_MODE_LANG1_LANG2,
(void *) 0);
}

static size_t
snprint_struct_v4l2_tuner (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_tuner *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "index=%lu ",
(unsigned long) t->index);
n = MIN (n, size);
if (1 & rw) {
n += snprintf (str + n, size - n, "name=\"%.*s\" "
"type=",
32, (const char *) t->name);
n = MIN (n, size);
n += snprint_enum_v4l2_tuner_type (str + n, size - n, rw, t->type);
n += strlcpy (str + n, " capability=", size - n);
n = MIN (n, size);
n += snprint_symbol_v4l2_tuner_cap_ (str + n, size - n, rw, t->capability);
n += snprintf (str + n, size - n, " rangelow=%lu "
"rangehigh=%lu "
"rxsubchans=",
(unsigned long) t->rangelow, 
(unsigned long) t->rangehigh);
n = MIN (n, size);
n += snprint_symbol_v4l2_tuner_sub_ (str + n, size - n, rw, t->rxsubchans);
n += strlcpy (str + n, " audmode=", size - n);
n = MIN (n, size);
n += snprint_symbol_v4l2_tuner_mode_ (str + n, size - n, rw, t->audmode);
n += snprintf (str + n, size - n, " signal=%ld "
"afc=%ld "
"reserved[] ",
(long) t->signal, 
(long) t->afc);
n = MIN (n, size);
}
return n;
}

static size_t
snprint_symbol_v4l2_cap_ (char *str, size_t size, int rw __attribute__ ((unused)), unsigned long value)
{
return snprint_symbolic (str, size, 0, value,
"VIDEO_CAPTURE", (unsigned long) V4L2_CAP_VIDEO_CAPTURE,
"VIDEO_OUTPUT", (unsigned long) V4L2_CAP_VIDEO_OUTPUT,
"VIDEO_OVERLAY", (unsigned long) V4L2_CAP_VIDEO_OVERLAY,
"VBI_CAPTURE", (unsigned long) V4L2_CAP_VBI_CAPTURE,
"VBI_OUTPUT", (unsigned long) V4L2_CAP_VBI_OUTPUT,
"SLICED_VBI_CAPTURE", (unsigned long) V4L2_CAP_SLICED_VBI_CAPTURE,
"SLICED_VBI_OUTPUT", (unsigned long) V4L2_CAP_SLICED_VBI_OUTPUT,
"RDS_CAPTURE", (unsigned long) V4L2_CAP_RDS_CAPTURE,
"VIDEO_OUTPUT_POS", (unsigned long) V4L2_CAP_VIDEO_OUTPUT_POS,
"VIDEO_OUTPUT_OVERLAY", (unsigned long) V4L2_CAP_VIDEO_OUTPUT_OVERLAY,
"TUNER", (unsigned long) V4L2_CAP_TUNER,
"AUDIO", (unsigned long) V4L2_CAP_AUDIO,
"RADIO", (unsigned long) V4L2_CAP_RADIO,
"READWRITE", (unsigned long) V4L2_CAP_READWRITE,
"ASYNCIO", (unsigned long) V4L2_CAP_ASYNCIO,
"STREAMING", (unsigned long) V4L2_CAP_STREAMING,
"TIMEPERFRAME", (unsigned long) V4L2_CAP_TIMEPERFRAME,
(void *) 0);
}

static size_t
snprint_struct_v4l2_capability (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_capability *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "driver=\"%.*s\" "
"card=\"%.*s\" "
"bus_info=\"%.*s\" "
"version=0x%lx "
"capabilities=",
16, (const char *) t->driver, 
32, (const char *) t->card, 
32, (const char *) t->bus_info, 
(unsigned long) t->version);
n = MIN (n, size);
n += snprint_symbol_v4l2_cap_ (str + n, size - n, rw, t->capabilities);
n += strlcpy (str + n, " reserved[] ", size - n);
n = MIN (n, size);
return n;
}

static size_t
snprint_symbol_v4l2_cid_ (char *str, size_t size, int rw __attribute__ ((unused)), unsigned long value)
{
return snprint_symbolic (str, size, 0, value,
"BASE", (unsigned long) V4L2_CID_BASE,
"USER_BASE", (unsigned long) V4L2_CID_USER_BASE,
"PRIVATE_BASE", (unsigned long) V4L2_CID_PRIVATE_BASE,
"USER_CLASS", (unsigned long) V4L2_CID_USER_CLASS,
"BRIGHTNESS", (unsigned long) V4L2_CID_BRIGHTNESS,
"CONTRAST", (unsigned long) V4L2_CID_CONTRAST,
"SATURATION", (unsigned long) V4L2_CID_SATURATION,
"HUE", (unsigned long) V4L2_CID_HUE,
"AUDIO_VOLUME", (unsigned long) V4L2_CID_AUDIO_VOLUME,
"AUDIO_BALANCE", (unsigned long) V4L2_CID_AUDIO_BALANCE,
"AUDIO_BASS", (unsigned long) V4L2_CID_AUDIO_BASS,
"AUDIO_TREBLE", (unsigned long) V4L2_CID_AUDIO_TREBLE,
"AUDIO_MUTE", (unsigned long) V4L2_CID_AUDIO_MUTE,
"AUDIO_LOUDNESS", (unsigned long) V4L2_CID_AUDIO_LOUDNESS,
"BLACK_LEVEL", (unsigned long) V4L2_CID_BLACK_LEVEL,
"AUTO_WHITE_BALANCE", (unsigned long) V4L2_CID_AUTO_WHITE_BALANCE,
"DO_WHITE_BALANCE", (unsigned long) V4L2_CID_DO_WHITE_BALANCE,
"RED_BALANCE", (unsigned long) V4L2_CID_RED_BALANCE,
"BLUE_BALANCE", (unsigned long) V4L2_CID_BLUE_BALANCE,
"GAMMA", (unsigned long) V4L2_CID_GAMMA,
"WHITENESS", (unsigned long) V4L2_CID_WHITENESS,
"EXPOSURE", (unsigned long) V4L2_CID_EXPOSURE,
"AUTOGAIN", (unsigned long) V4L2_CID_AUTOGAIN,
"GAIN", (unsigned long) V4L2_CID_GAIN,
"HFLIP", (unsigned long) V4L2_CID_HFLIP,
"VFLIP", (unsigned long) V4L2_CID_VFLIP,
"HCENTER", (unsigned long) V4L2_CID_HCENTER,
"VCENTER", (unsigned long) V4L2_CID_VCENTER,
"LASTP1", (unsigned long) V4L2_CID_LASTP1,
"MPEG_BASE", (unsigned long) V4L2_CID_MPEG_BASE,
"MPEG_CLASS", (unsigned long) V4L2_CID_MPEG_CLASS,
"MPEG_STREAM_TYPE", (unsigned long) V4L2_CID_MPEG_STREAM_TYPE,
"MPEG_STREAM_PID_PMT", (unsigned long) V4L2_CID_MPEG_STREAM_PID_PMT,
"MPEG_STREAM_PID_AUDIO", (unsigned long) V4L2_CID_MPEG_STREAM_PID_AUDIO,
"MPEG_STREAM_PID_VIDEO", (unsigned long) V4L2_CID_MPEG_STREAM_PID_VIDEO,
"MPEG_STREAM_PID_PCR", (unsigned long) V4L2_CID_MPEG_STREAM_PID_PCR,
"MPEG_STREAM_PES_ID_AUDIO", (unsigned long) V4L2_CID_MPEG_STREAM_PES_ID_AUDIO,
"MPEG_STREAM_PES_ID_VIDEO", (unsigned long) V4L2_CID_MPEG_STREAM_PES_ID_VIDEO,
"MPEG_STREAM_VBI_FMT", (unsigned long) V4L2_CID_MPEG_STREAM_VBI_FMT,
"MPEG_AUDIO_SAMPLING_FREQ", (unsigned long) V4L2_CID_MPEG_AUDIO_SAMPLING_FREQ,
"MPEG_AUDIO_ENCODING", (unsigned long) V4L2_CID_MPEG_AUDIO_ENCODING,
"MPEG_AUDIO_L1_BITRATE", (unsigned long) V4L2_CID_MPEG_AUDIO_L1_BITRATE,
"MPEG_AUDIO_L2_BITRATE", (unsigned long) V4L2_CID_MPEG_AUDIO_L2_BITRATE,
"MPEG_AUDIO_L3_BITRATE", (unsigned long) V4L2_CID_MPEG_AUDIO_L3_BITRATE,
"MPEG_AUDIO_MODE", (unsigned long) V4L2_CID_MPEG_AUDIO_MODE,
"MPEG_AUDIO_MODE_EXTENSION", (unsigned long) V4L2_CID_MPEG_AUDIO_MODE_EXTENSION,
"MPEG_AUDIO_EMPHASIS", (unsigned long) V4L2_CID_MPEG_AUDIO_EMPHASIS,
"MPEG_AUDIO_CRC", (unsigned long) V4L2_CID_MPEG_AUDIO_CRC,
"MPEG_AUDIO_MUTE", (unsigned long) V4L2_CID_MPEG_AUDIO_MUTE,
"MPEG_VIDEO_ENCODING", (unsigned long) V4L2_CID_MPEG_VIDEO_ENCODING,
"MPEG_VIDEO_ASPECT", (unsigned long) V4L2_CID_MPEG_VIDEO_ASPECT,
"MPEG_VIDEO_B_FRAMES", (unsigned long) V4L2_CID_MPEG_VIDEO_B_FRAMES,
"MPEG_VIDEO_GOP_SIZE", (unsigned long) V4L2_CID_MPEG_VIDEO_GOP_SIZE,
"MPEG_VIDEO_GOP_CLOSURE", (unsigned long) V4L2_CID_MPEG_VIDEO_GOP_CLOSURE,
"MPEG_VIDEO_PULLDOWN", (unsigned long) V4L2_CID_MPEG_VIDEO_PULLDOWN,
"MPEG_VIDEO_BITRATE_MODE", (unsigned long) V4L2_CID_MPEG_VIDEO_BITRATE_MODE,
"MPEG_VIDEO_BITRATE", (unsigned long) V4L2_CID_MPEG_VIDEO_BITRATE,
"MPEG_VIDEO_BITRATE_PEAK", (unsigned long) V4L2_CID_MPEG_VIDEO_BITRATE_PEAK,
"MPEG_VIDEO_TEMPORAL_DECIMATION", (unsigned long) V4L2_CID_MPEG_VIDEO_TEMPORAL_DECIMATION,
"MPEG_VIDEO_MUTE", (unsigned long) V4L2_CID_MPEG_VIDEO_MUTE,
"MPEG_VIDEO_MUTE_YUV", (unsigned long) V4L2_CID_MPEG_VIDEO_MUTE_YUV,
"MPEG_CX2341X_BASE", (unsigned long) V4L2_CID_MPEG_CX2341X_BASE,
"MPEG_CX2341X_VIDEO_SPATIAL_FILTER_MODE", (unsigned long) V4L2_CID_MPEG_CX2341X_VIDEO_SPATIAL_FILTER_MODE,
"MPEG_CX2341X_VIDEO_SPATIAL_FILTER", (unsigned long) V4L2_CID_MPEG_CX2341X_VIDEO_SPATIAL_FILTER,
"MPEG_CX2341X_VIDEO_LUMA_SPATIAL_FILTER_TYPE", (unsigned long) V4L2_CID_MPEG_CX2341X_VIDEO_LUMA_SPATIAL_FILTER_TYPE,
"MPEG_CX2341X_VIDEO_CHROMA_SPATIAL_FILTER_TYPE", (unsigned long) V4L2_CID_MPEG_CX2341X_VIDEO_CHROMA_SPATIAL_FILTER_TYPE,
"MPEG_CX2341X_VIDEO_TEMPORAL_FILTER_MODE", (unsigned long) V4L2_CID_MPEG_CX2341X_VIDEO_TEMPORAL_FILTER_MODE,
"MPEG_CX2341X_VIDEO_TEMPORAL_FILTER", (unsigned long) V4L2_CID_MPEG_CX2341X_VIDEO_TEMPORAL_FILTER,
"MPEG_CX2341X_VIDEO_MEDIAN_FILTER_TYPE", (unsigned long) V4L2_CID_MPEG_CX2341X_VIDEO_MEDIAN_FILTER_TYPE,
"MPEG_CX2341X_VIDEO_LUMA_MEDIAN_FILTER_BOTTOM", (unsigned long) V4L2_CID_MPEG_CX2341X_VIDEO_LUMA_MEDIAN_FILTER_BOTTOM,
"MPEG_CX2341X_VIDEO_LUMA_MEDIAN_FILTER_TOP", (unsigned long) V4L2_CID_MPEG_CX2341X_VIDEO_LUMA_MEDIAN_FILTER_TOP,
"MPEG_CX2341X_VIDEO_CHROMA_MEDIAN_FILTER_BOTTOM", (unsigned long) V4L2_CID_MPEG_CX2341X_VIDEO_CHROMA_MEDIAN_FILTER_BOTTOM,
"MPEG_CX2341X_VIDEO_CHROMA_MEDIAN_FILTER_TOP", (unsigned long) V4L2_CID_MPEG_CX2341X_VIDEO_CHROMA_MEDIAN_FILTER_TOP,
"MPEG_CX2341X_STREAM_INSERT_NAV_PACKETS", (unsigned long) V4L2_CID_MPEG_CX2341X_STREAM_INSERT_NAV_PACKETS,
(void *) 0);
}

static size_t
snprint_enum_v4l2_ctrl_type (char *str, size_t size, int rw __attribute__ ((unused)), int value)
{
return snprint_symbolic (str, size, 1, value,
"INTEGER", (unsigned long) V4L2_CTRL_TYPE_INTEGER,
"BOOLEAN", (unsigned long) V4L2_CTRL_TYPE_BOOLEAN,
"MENU", (unsigned long) V4L2_CTRL_TYPE_MENU,
"BUTTON", (unsigned long) V4L2_CTRL_TYPE_BUTTON,
"INTEGER64", (unsigned long) V4L2_CTRL_TYPE_INTEGER64,
"CTRL_CLASS", (unsigned long) V4L2_CTRL_TYPE_CTRL_CLASS,
(void *) 0);
}

static size_t
snprint_symbol_v4l2_ctrl_flag_ (char *str, size_t size, int rw __attribute__ ((unused)), unsigned long value)
{
return snprint_symbolic (str, size, 2, value,
"DISABLED", (unsigned long) V4L2_CTRL_FLAG_DISABLED,
"GRABBED", (unsigned long) V4L2_CTRL_FLAG_GRABBED,
"READ_ONLY", (unsigned long) V4L2_CTRL_FLAG_READ_ONLY,
"UPDATE", (unsigned long) V4L2_CTRL_FLAG_UPDATE,
"INACTIVE", (unsigned long) V4L2_CTRL_FLAG_INACTIVE,
"SLIDER", (unsigned long) V4L2_CTRL_FLAG_SLIDER,
"NEXT_CTRL", (unsigned long) V4L2_CTRL_FLAG_NEXT_CTRL,
(void *) 0);
}

static size_t
snprint_struct_v4l2_queryctrl (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_queryctrl *t)
{
size_t n = 0;
n += strlcpy (str + n, "id=", size - n);
n = MIN (n, size);
n += snprint_symbol_v4l2_cid_ (str + n, size - n, rw, t->id);
n += strlcpy (str + n, " ", size - n);
n = MIN (n, size);
if (1 & rw) {
n += strlcpy (str + n, "type=", size - n);
n = MIN (n, size);
n += snprint_enum_v4l2_ctrl_type (str + n, size - n, rw, t->type);
n += snprintf (str + n, size - n, " name=\"%.*s\" "
"minimum=%ld "
"maximum=%ld "
"step=%ld "
"default_value=%ld "
"flags=",
32, (const char *) t->name, 
(long) t->minimum, 
(long) t->maximum, 
(long) t->step, 
(long) t->default_value);
n = MIN (n, size);
n += snprint_symbol_v4l2_ctrl_flag_ (str + n, size - n, rw, t->flags);
n += strlcpy (str + n, " reserved[] ", size - n);
n = MIN (n, size);
}
return n;
}

static size_t
snprint_struct_v4l2_modulator (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_modulator *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "index=%lu ",
(unsigned long) t->index);
n = MIN (n, size);
if (1 & rw) {
n += snprintf (str + n, size - n, "name=\"%.*s\" "
"capability=",
32, (const char *) t->name);
n = MIN (n, size);
n += snprint_symbol_v4l2_tuner_cap_ (str + n, size - n, rw, t->capability);
n += snprintf (str + n, size - n, " rangelow=%lu "
"rangehigh=%lu "
"txsubchans=",
(unsigned long) t->rangelow, 
(unsigned long) t->rangehigh);
n = MIN (n, size);
n += snprint_symbol_v4l2_tuner_sub_ (str + n, size - n, rw, t->txsubchans);
n += strlcpy (str + n, " reserved[] ", size - n);
n = MIN (n, size);
}
return n;
}

static size_t
snprint_struct_v4l2_fract (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_fract *t)
{
size_t n = 0;
 n += snprintf (str + n, size - n, "%u/%u", t->numerator, t->denominator); n = MIN (size - 1, n); 
return n;
}

static size_t
snprint_struct_v4l2_frmival_stepwise (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_frmival_stepwise *t)
{
size_t n = 0;
n += strlcpy (str + n, "min={", size - n);
n = MIN (n, size);
n += snprint_struct_v4l2_fract (str + n, size - n, rw, &t->min);
n += strlcpy (str + n, "} max={", size - n);
n = MIN (n, size);
n += snprint_struct_v4l2_fract (str + n, size - n, rw, &t->max);
n += strlcpy (str + n, "} step={", size - n);
n = MIN (n, size);
n += snprint_struct_v4l2_fract (str + n, size - n, rw, &t->step);
n += strlcpy (str + n, "} ", size - n);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_v4l2_frmivalenum (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_frmivalenum *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "index=%lu "
"pixel_format=%lu "
"width=%lu "
"height=%lu "
"type=%lu ",
(unsigned long) t->index, 
(unsigned long) t->pixel_format, 
(unsigned long) t->width, 
(unsigned long) t->height, 
(unsigned long) t->type);
n = MIN (n, size);
n += strlcpy (str + n, "u={discrete={", size - n);
n = MIN (n, size);
n += snprint_struct_v4l2_fract (str + n, size - n, rw, &t->u.discrete);
n += strlcpy (str + n, "} stepwise={", size - n);
n = MIN (n, size);
n += snprint_struct_v4l2_frmival_stepwise (str + n, size - n, rw, &t->u.stepwise);
n += strlcpy (str + n, "} ", size - n);
n = MIN (n, size);
n += strlcpy (str + n, "} reserved[] ", size - n);
n = MIN (n, size);
return n;
}

static size_t
snprint_symbol_v4l2_std_ (char *str, size_t size, int rw __attribute__ ((unused)), unsigned long value)
{
return snprint_symbolic (str, size, 0, value,
"PAL_B", (unsigned long) V4L2_STD_PAL_B,
"PAL_B1", (unsigned long) V4L2_STD_PAL_B1,
"PAL_G", (unsigned long) V4L2_STD_PAL_G,
"PAL_H", (unsigned long) V4L2_STD_PAL_H,
"PAL_I", (unsigned long) V4L2_STD_PAL_I,
"PAL_D", (unsigned long) V4L2_STD_PAL_D,
"PAL_D1", (unsigned long) V4L2_STD_PAL_D1,
"PAL_K", (unsigned long) V4L2_STD_PAL_K,
"PAL_M", (unsigned long) V4L2_STD_PAL_M,
"PAL_N", (unsigned long) V4L2_STD_PAL_N,
"PAL_Nc", (unsigned long) V4L2_STD_PAL_Nc,
"PAL_60", (unsigned long) V4L2_STD_PAL_60,
"NTSC_M", (unsigned long) V4L2_STD_NTSC_M,
"NTSC_M_JP", (unsigned long) V4L2_STD_NTSC_M_JP,
"NTSC_443", (unsigned long) V4L2_STD_NTSC_443,
"NTSC_M_KR", (unsigned long) V4L2_STD_NTSC_M_KR,
"SECAM_B", (unsigned long) V4L2_STD_SECAM_B,
"SECAM_D", (unsigned long) V4L2_STD_SECAM_D,
"SECAM_G", (unsigned long) V4L2_STD_SECAM_G,
"SECAM_H", (unsigned long) V4L2_STD_SECAM_H,
"SECAM_K", (unsigned long) V4L2_STD_SECAM_K,
"SECAM_K1", (unsigned long) V4L2_STD_SECAM_K1,
"SECAM_L", (unsigned long) V4L2_STD_SECAM_L,
"SECAM_LC", (unsigned long) V4L2_STD_SECAM_LC,
"ATSC_8_VSB", (unsigned long) V4L2_STD_ATSC_8_VSB,
"ATSC_16_VSB", (unsigned long) V4L2_STD_ATSC_16_VSB,
"MN", (unsigned long) V4L2_STD_MN,
"B", (unsigned long) V4L2_STD_B,
"GH", (unsigned long) V4L2_STD_GH,
"DK", (unsigned long) V4L2_STD_DK,
"PAL_BG", (unsigned long) V4L2_STD_PAL_BG,
"PAL_DK", (unsigned long) V4L2_STD_PAL_DK,
"PAL", (unsigned long) V4L2_STD_PAL,
"NTSC", (unsigned long) V4L2_STD_NTSC,
"SECAM_DK", (unsigned long) V4L2_STD_SECAM_DK,
"SECAM", (unsigned long) V4L2_STD_SECAM,
"525_60", (unsigned long) V4L2_STD_525_60,
"625_50", (unsigned long) V4L2_STD_625_50,
"ATSC", (unsigned long) V4L2_STD_ATSC,
"UNKNOWN", (unsigned long) V4L2_STD_UNKNOWN,
"ALL", (unsigned long) V4L2_STD_ALL,
(void *) 0);
}

static size_t
snprint_symbol_v4l2_in_st_ (char *str, size_t size, int rw __attribute__ ((unused)), unsigned long value)
{
return snprint_symbolic (str, size, 0, value,
"NO_POWER", (unsigned long) V4L2_IN_ST_NO_POWER,
"NO_SIGNAL", (unsigned long) V4L2_IN_ST_NO_SIGNAL,
"NO_COLOR", (unsigned long) V4L2_IN_ST_NO_COLOR,
"NO_H_LOCK", (unsigned long) V4L2_IN_ST_NO_H_LOCK,
"COLOR_KILL", (unsigned long) V4L2_IN_ST_COLOR_KILL,
"NO_SYNC", (unsigned long) V4L2_IN_ST_NO_SYNC,
"NO_EQU", (unsigned long) V4L2_IN_ST_NO_EQU,
"NO_CARRIER", (unsigned long) V4L2_IN_ST_NO_CARRIER,
"MACROVISION", (unsigned long) V4L2_IN_ST_MACROVISION,
"NO_ACCESS", (unsigned long) V4L2_IN_ST_NO_ACCESS,
"VTR", (unsigned long) V4L2_IN_ST_VTR,
(void *) 0);
}

static size_t
snprint_struct_v4l2_input (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_input *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "index=%lu ",
(unsigned long) t->index);
n = MIN (n, size);
if (1 & rw) {
n += snprintf (str + n, size - n, "name=\"%.*s\" "
"type=",
32, (const char *) t->name);
n = MIN (n, size);
n += snprint_symbolic (str + n, size - n, 0, t->type,
"TUNER", (unsigned long) V4L2_INPUT_TYPE_TUNER,
"CAMERA", (unsigned long) V4L2_INPUT_TYPE_CAMERA,
(void *) 0);
n += snprintf (str + n, size - n, " audioset=%lu "
"tuner=%lu "
"std=",
(unsigned long) t->audioset, 
(unsigned long) t->tuner);
n = MIN (n, size);
n += snprint_symbol_v4l2_std_ (str + n, size - n, rw, t->std);
n += strlcpy (str + n, " status=", size - n);
n = MIN (n, size);
n += snprint_symbol_v4l2_in_st_ (str + n, size - n, rw, t->status);
n += strlcpy (str + n, " reserved[] ", size - n);
n = MIN (n, size);
}
return n;
}

static size_t
snprint_struct_v4l2_chip_ident (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_chip_ident *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "match_type=%lu "
"match_chip=%lu "
"ident=%lu "
"revision=%lu ",
(unsigned long) t->match_type, 
(unsigned long) t->match_chip, 
(unsigned long) t->ident, 
(unsigned long) t->revision);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_v4l2_sliced_vbi_cap (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_sliced_vbi_cap *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "service_set=%lu "
"type=",
(unsigned long) t->service_set);
n = MIN (n, size);
n += snprint_enum_v4l2_buf_type (str + n, size - n, rw, t->type);
n += strlcpy (str + n, " reserved[] ", size - n);
n = MIN (n, size);
return n;
}

static size_t
snprint_symbol_v4l2_pix_fmt_ (char *str, size_t size, int rw __attribute__ ((unused)), unsigned long value)
{
return snprint_symbolic (str, size, 0, value,
"RGB332", (unsigned long) V4L2_PIX_FMT_RGB332,
"RGB555", (unsigned long) V4L2_PIX_FMT_RGB555,
"RGB565", (unsigned long) V4L2_PIX_FMT_RGB565,
"RGB555X", (unsigned long) V4L2_PIX_FMT_RGB555X,
"RGB565X", (unsigned long) V4L2_PIX_FMT_RGB565X,
"BGR24", (unsigned long) V4L2_PIX_FMT_BGR24,
"RGB24", (unsigned long) V4L2_PIX_FMT_RGB24,
"BGR32", (unsigned long) V4L2_PIX_FMT_BGR32,
"RGB32", (unsigned long) V4L2_PIX_FMT_RGB32,
"GREY", (unsigned long) V4L2_PIX_FMT_GREY,
"YVU410", (unsigned long) V4L2_PIX_FMT_YVU410,
"YVU420", (unsigned long) V4L2_PIX_FMT_YVU420,
"YUYV", (unsigned long) V4L2_PIX_FMT_YUYV,
"UYVY", (unsigned long) V4L2_PIX_FMT_UYVY,
"YUV422P", (unsigned long) V4L2_PIX_FMT_YUV422P,
"YUV411P", (unsigned long) V4L2_PIX_FMT_YUV411P,
"Y41P", (unsigned long) V4L2_PIX_FMT_Y41P,
"NV12", (unsigned long) V4L2_PIX_FMT_NV12,
"NV21", (unsigned long) V4L2_PIX_FMT_NV21,
"YUV410", (unsigned long) V4L2_PIX_FMT_YUV410,
"YUV420", (unsigned long) V4L2_PIX_FMT_YUV420,
"YYUV", (unsigned long) V4L2_PIX_FMT_YYUV,
"HI240", (unsigned long) V4L2_PIX_FMT_HI240,
"HM12", (unsigned long) V4L2_PIX_FMT_HM12,
"RGB444", (unsigned long) V4L2_PIX_FMT_RGB444,
"SBGGR8", (unsigned long) V4L2_PIX_FMT_SBGGR8,
"MJPEG", (unsigned long) V4L2_PIX_FMT_MJPEG,
"JPEG", (unsigned long) V4L2_PIX_FMT_JPEG,
"DV", (unsigned long) V4L2_PIX_FMT_DV,
"MPEG", (unsigned long) V4L2_PIX_FMT_MPEG,
"WNVA", (unsigned long) V4L2_PIX_FMT_WNVA,
"SN9C10X", (unsigned long) V4L2_PIX_FMT_SN9C10X,
"PWC1", (unsigned long) V4L2_PIX_FMT_PWC1,
"PWC2", (unsigned long) V4L2_PIX_FMT_PWC2,
"ET61X251", (unsigned long) V4L2_PIX_FMT_ET61X251,
(void *) 0);
}

static size_t
snprint_enum_v4l2_field (char *str, size_t size, int rw __attribute__ ((unused)), int value)
{
return snprint_symbolic (str, size, 1, value,
"ANY", (unsigned long) V4L2_FIELD_ANY,
"NONE", (unsigned long) V4L2_FIELD_NONE,
"TOP", (unsigned long) V4L2_FIELD_TOP,
"BOTTOM", (unsigned long) V4L2_FIELD_BOTTOM,
"INTERLACED", (unsigned long) V4L2_FIELD_INTERLACED,
"SEQ_TB", (unsigned long) V4L2_FIELD_SEQ_TB,
"SEQ_BT", (unsigned long) V4L2_FIELD_SEQ_BT,
"ALTERNATE", (unsigned long) V4L2_FIELD_ALTERNATE,
"INTERLACED_TB", (unsigned long) V4L2_FIELD_INTERLACED_TB,
"INTERLACED_BT", (unsigned long) V4L2_FIELD_INTERLACED_BT,
(void *) 0);
}

static size_t
snprint_enum_v4l2_colorspace (char *str, size_t size, int rw __attribute__ ((unused)), int value)
{
return snprint_symbolic (str, size, 1, value,
"SMPTE170M", (unsigned long) V4L2_COLORSPACE_SMPTE170M,
"SMPTE240M", (unsigned long) V4L2_COLORSPACE_SMPTE240M,
"REC709", (unsigned long) V4L2_COLORSPACE_REC709,
"BT878", (unsigned long) V4L2_COLORSPACE_BT878,
"470_SYSTEM_M", (unsigned long) V4L2_COLORSPACE_470_SYSTEM_M,
"470_SYSTEM_BG", (unsigned long) V4L2_COLORSPACE_470_SYSTEM_BG,
"JPEG", (unsigned long) V4L2_COLORSPACE_JPEG,
"SRGB", (unsigned long) V4L2_COLORSPACE_SRGB,
(void *) 0);
}

static size_t
snprint_struct_v4l2_pix_format (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_pix_format *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "width=%lu "
"height=%lu "
"pixelformat=",
(unsigned long) t->width, 
(unsigned long) t->height);
n = MIN (n, size);
n += snprint_symbol_v4l2_pix_fmt_ (str + n, size - n, rw, t->pixelformat);
n += strlcpy (str + n, " field=", size - n);
n = MIN (n, size);
n += snprint_enum_v4l2_field (str + n, size - n, rw, t->field);
n += snprintf (str + n, size - n, " bytesperline=%lu "
"sizeimage=%lu "
"colorspace=",
(unsigned long) t->bytesperline, 
(unsigned long) t->sizeimage);
n = MIN (n, size);
n += snprint_enum_v4l2_colorspace (str + n, size - n, rw, t->colorspace);
n += snprintf (str + n, size - n, " priv=%lu "
"left=%lu "
"top=%lu ",
(unsigned long) t->priv, 
(unsigned long) t->left, 
(unsigned long) t->top);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_v4l2_window (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_window *t)
{
size_t n = 0;
n += strlcpy (str + n, "w={", size - n);
n = MIN (n, size);
n += snprint_struct_v4l2_rect (str + n, size - n, rw, &t->w);
n += strlcpy (str + n, "} field=", size - n);
n = MIN (n, size);
n += snprint_enum_v4l2_field (str + n, size - n, rw, t->field);
n += snprintf (str + n, size - n, " chromakey=%lu "
"clips=%p "
"clipcount=%lu "
"bitmap=%p "
"global_alpha=%lu ",
(unsigned long) t->chromakey, 
(const void *) t->clips, 
(unsigned long) t->clipcount, 
(const void *) t->bitmap, 
(unsigned long) t->global_alpha);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_v4l2_vbi_format (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_vbi_format *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "sampling_rate=%lu "
"offset=%lu "
"samples_per_line=%lu "
"sample_format=",
(unsigned long) t->sampling_rate, 
(unsigned long) t->offset, 
(unsigned long) t->samples_per_line);
n = MIN (n, size);
n += snprint_symbol_v4l2_pix_fmt_ (str + n, size - n, rw, t->sample_format);
n += strlcpy (str + n, " start[]=? "
"count[]=? "
"flags=", size - n);
n = MIN (n, size);
n += snprint_symbolic (str + n, size - n, 2, t->flags,
"UNSYNC", (unsigned long) V4L2_VBI_UNSYNC,
"INTERLACED", (unsigned long) V4L2_VBI_INTERLACED,
(void *) 0);
n += strlcpy (str + n, " reserved[] ", size - n);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_v4l2_sliced_vbi_format (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_sliced_vbi_format *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "service_set=%lu "
"io_size=%lu "
"reserved[] ",
(unsigned long) t->service_set, 
(unsigned long) t->io_size);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_v4l2_format (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_format *t)
{
size_t n = 0;
n += strlcpy (str + n, "type=", size - n);
n = MIN (n, size);
n += snprint_enum_v4l2_buf_type (str + n, size - n, rw, t->type);
n += strlcpy (str + n, " ", size - n);
n = MIN (n, size);
n += strlcpy (str + n, "fmt={", size - n);
n = MIN (n, size);
if (V4L2_BUF_TYPE_VIDEO_CAPTURE == t->type) {
n += strlcpy (str + n, "pix={", size - n);
n = MIN (n, size);
n += snprint_struct_v4l2_pix_format (str + n, size - n, rw, &t->fmt.pix);
n += strlcpy (str + n, "} ", size - n);
n = MIN (n, size);
}
if (V4L2_BUF_TYPE_VIDEO_OVERLAY == t->type) {
n += strlcpy (str + n, "win={", size - n);
n = MIN (n, size);
n += snprint_struct_v4l2_window (str + n, size - n, rw, &t->fmt.win);
n += strlcpy (str + n, "} ", size - n);
n = MIN (n, size);
}
if (V4L2_BUF_TYPE_VBI_CAPTURE == t->type) {
n += strlcpy (str + n, "vbi={", size - n);
n = MIN (n, size);
n += snprint_struct_v4l2_vbi_format (str + n, size - n, rw, &t->fmt.vbi);
n += strlcpy (str + n, "} ", size - n);
n = MIN (n, size);
}
n += strlcpy (str + n, "sliced={", size - n);
n = MIN (n, size);
n += snprint_struct_v4l2_sliced_vbi_format (str + n, size - n, rw, &t->fmt.sliced);
n += strlcpy (str + n, "} raw_data[]=? ", size - n);
n = MIN (n, size);
n += strlcpy (str + n, "} ", size - n);
n = MIN (n, size);
return n;
}

static size_t
snprint_symbol_v4l2_buf_flag_ (char *str, size_t size, int rw __attribute__ ((unused)), unsigned long value)
{
return snprint_symbolic (str, size, 2, value,
"MAPPED", (unsigned long) V4L2_BUF_FLAG_MAPPED,
"QUEUED", (unsigned long) V4L2_BUF_FLAG_QUEUED,
"DONE", (unsigned long) V4L2_BUF_FLAG_DONE,
"KEYFRAME", (unsigned long) V4L2_BUF_FLAG_KEYFRAME,
"PFRAME", (unsigned long) V4L2_BUF_FLAG_PFRAME,
"BFRAME", (unsigned long) V4L2_BUF_FLAG_BFRAME,
"TIMECODE", (unsigned long) V4L2_BUF_FLAG_TIMECODE,
"INPUT", (unsigned long) V4L2_BUF_FLAG_INPUT,
(void *) 0);
}

static size_t
snprint_symbol_v4l2_tc_type_ (char *str, size_t size, int rw __attribute__ ((unused)), unsigned long value)
{
return snprint_symbolic (str, size, 0, value,
"24FPS", (unsigned long) V4L2_TC_TYPE_24FPS,
"25FPS", (unsigned long) V4L2_TC_TYPE_25FPS,
"30FPS", (unsigned long) V4L2_TC_TYPE_30FPS,
"50FPS", (unsigned long) V4L2_TC_TYPE_50FPS,
"60FPS", (unsigned long) V4L2_TC_TYPE_60FPS,
(void *) 0);
}

static size_t
snprint_struct_v4l2_timecode (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_timecode *t)
{
size_t n = 0;
n += strlcpy (str + n, "type=", size - n);
n = MIN (n, size);
n += snprint_symbol_v4l2_tc_type_ (str + n, size - n, rw, t->type);
n += strlcpy (str + n, " flags=", size - n);
n = MIN (n, size);
n += snprint_symbolic (str + n, size - n, 2, t->flags,
"DROPFRAME", (unsigned long) V4L2_TC_FLAG_DROPFRAME,
"COLORFRAME", (unsigned long) V4L2_TC_FLAG_COLORFRAME,
(void *) 0);
n += snprintf (str + n, size - n, " frames=%lu "
"seconds=%lu "
"minutes=%lu "
"hours=%lu "
"userbits[]=? ",
(unsigned long) t->frames, 
(unsigned long) t->seconds, 
(unsigned long) t->minutes, 
(unsigned long) t->hours);
n = MIN (n, size);
return n;
}

static size_t
snprint_enum_v4l2_memory (char *str, size_t size, int rw __attribute__ ((unused)), int value)
{
return snprint_symbolic (str, size, 1, value,
"MMAP", (unsigned long) V4L2_MEMORY_MMAP,
"USERPTR", (unsigned long) V4L2_MEMORY_USERPTR,
"OVERLAY", (unsigned long) V4L2_MEMORY_OVERLAY,
(void *) 0);
}

static size_t
snprint_struct_v4l2_buffer (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_buffer *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "index=%lu "
"type=",
(unsigned long) t->index);
n = MIN (n, size);
n += snprint_enum_v4l2_buf_type (str + n, size - n, rw, t->type);
n += strlcpy (str + n, " ", size - n);
n = MIN (n, size);
if (1 & rw) {
n += snprintf (str + n, size - n, "bytesused=%lu ",
(unsigned long) t->bytesused);
n = MIN (n, size);
}
n += strlcpy (str + n, "flags=", size - n);
n = MIN (n, size);
n += snprint_symbol_v4l2_buf_flag_ (str + n, size - n, rw, t->flags);
n += strlcpy (str + n, " ", size - n);
n = MIN (n, size);
if (1 & rw) {
n += strlcpy (str + n, "field=", size - n);
n = MIN (n, size);
n += snprint_enum_v4l2_field (str + n, size - n, rw, t->field);
n += strlcpy (str + n, " timestamp=? timecode={", size - n);
n = MIN (n, size);
n += snprint_struct_v4l2_timecode (str + n, size - n, rw, &t->timecode);
n += snprintf (str + n, size - n, "} sequence=%lu "
"memory=",
(unsigned long) t->sequence);
n = MIN (n, size);
n += snprint_enum_v4l2_memory (str + n, size - n, rw, t->memory);
n += strlcpy (str + n, " ", size - n);
n = MIN (n, size);
n += strlcpy (str + n, "m={", size - n);
n = MIN (n, size);
}
if ((1 & rw) && V4L2_MEMORY_MMAP == t->memory) {
n += snprintf (str + n, size - n, "offset=%lu ",
(unsigned long) t->m.offset);
n = MIN (n, size);
}
if ((1 & rw) && V4L2_MEMORY_USERPTR == t->memory) {
n += snprintf (str + n, size - n, "userptr=%lu ",
(unsigned long) t->m.userptr);
n = MIN (n, size);
}
if (1 & rw) {
n += snprintf (str + n, size - n, "} length=%lu "
"input=%lu "
"reserved ",
(unsigned long) t->length, 
(unsigned long) t->input);
n = MIN (n, size);
}
return n;
}

static size_t
snprint_struct_v4l2_control (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_control *t)
{
size_t n = 0;
n += strlcpy (str + n, "id=", size - n);
n = MIN (n, size);
n += snprint_symbol_v4l2_cid_ (str + n, size - n, rw, t->id);
n += snprintf (str + n, size - n, " value=%ld ",
(long) t->value);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_v4l2_frmsize_discrete (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_frmsize_discrete *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "width=%lu "
"height=%lu ",
(unsigned long) t->width, 
(unsigned long) t->height);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_v4l2_frmsize_stepwise (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_frmsize_stepwise *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "min_width=%lu "
"max_width=%lu "
"step_width=%lu "
"min_height=%lu "
"max_height=%lu "
"step_height=%lu ",
(unsigned long) t->min_width, 
(unsigned long) t->max_width, 
(unsigned long) t->step_width, 
(unsigned long) t->min_height, 
(unsigned long) t->max_height, 
(unsigned long) t->step_height);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_v4l2_frmsizeenum (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_frmsizeenum *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "index=%lu "
"pixel_format=%lu "
"type=%lu ",
(unsigned long) t->index, 
(unsigned long) t->pixel_format, 
(unsigned long) t->type);
n = MIN (n, size);
n += strlcpy (str + n, "u={discrete={", size - n);
n = MIN (n, size);
n += snprint_struct_v4l2_frmsize_discrete (str + n, size - n, rw, &t->u.discrete);
n += strlcpy (str + n, "} stepwise={", size - n);
n = MIN (n, size);
n += snprint_struct_v4l2_frmsize_stepwise (str + n, size - n, rw, &t->u.stepwise);
n += strlcpy (str + n, "} ", size - n);
n = MIN (n, size);
n += strlcpy (str + n, "} reserved[] ", size - n);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_v4l2_captureparm (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_captureparm *t)
{
size_t n = 0;
n += strlcpy (str + n, "capability=", size - n);
n = MIN (n, size);
n += snprint_symbol_v4l2_cap_ (str + n, size - n, rw, t->capability);
n += strlcpy (str + n, " capturemode=", size - n);
n = MIN (n, size);
n += snprint_symbolic (str + n, size - n, 0, t->capturemode,
"HIGHQUALITY", (unsigned long) V4L2_MODE_HIGHQUALITY,
(void *) 0);
n += strlcpy (str + n, " timeperframe={", size - n);
n = MIN (n, size);
n += snprint_struct_v4l2_fract (str + n, size - n, rw, &t->timeperframe);
n += snprintf (str + n, size - n, "} extendedmode=%lu "
"readbuffers=%lu "
"reserved[] ",
(unsigned long) t->extendedmode, 
(unsigned long) t->readbuffers);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_v4l2_outputparm (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_outputparm *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "capability=%lu "
"outputmode=%lu "
"timeperframe={",
(unsigned long) t->capability, 
(unsigned long) t->outputmode);
n = MIN (n, size);
n += snprint_struct_v4l2_fract (str + n, size - n, rw, &t->timeperframe);
n += snprintf (str + n, size - n, "} extendedmode=%lu "
"writebuffers=%lu "
"reserved[] ",
(unsigned long) t->extendedmode, 
(unsigned long) t->writebuffers);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_v4l2_streamparm (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_streamparm *t)
{
size_t n = 0;
n += strlcpy (str + n, "type=", size - n);
n = MIN (n, size);
n += snprint_enum_v4l2_buf_type (str + n, size - n, rw, t->type);
n += strlcpy (str + n, " ", size - n);
n = MIN (n, size);
n += strlcpy (str + n, "parm={capture={", size - n);
n = MIN (n, size);
n += snprint_struct_v4l2_captureparm (str + n, size - n, rw, &t->parm.capture);
n += strlcpy (str + n, "} output={", size - n);
n = MIN (n, size);
n += snprint_struct_v4l2_outputparm (str + n, size - n, rw, &t->parm.output);
n += strlcpy (str + n, "} raw_data[]=? ", size - n);
n = MIN (n, size);
n += strlcpy (str + n, "} ", size - n);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_v4l2_frequency (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_frequency *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "tuner=%lu "
"type=",
(unsigned long) t->tuner);
n = MIN (n, size);
n += snprint_enum_v4l2_tuner_type (str + n, size - n, rw, t->type);
n += snprintf (str + n, size - n, " frequency=%lu "
"reserved[] ",
(unsigned long) t->frequency);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_v4l2_querymenu (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_querymenu *t)
{
size_t n = 0;
n += strlcpy (str + n, "id=", size - n);
n = MIN (n, size);
n += snprint_symbol_v4l2_cid_ (str + n, size - n, rw, t->id);
n += strlcpy (str + n, " index=", size - n);
n = MIN (n, size);
n += snprint_symbol_v4l2_cid_ (str + n, size - n, rw, t->index);
n += strlcpy (str + n, " ", size - n);
n = MIN (n, size);
if (1 & rw) {
n += snprintf (str + n, size - n, "name=\"%.*s\" "
"reserved ",
32, (const char *) t->name);
n = MIN (n, size);
}
return n;
}

static size_t
snprint_symbol_v4l2_jpeg_marker_ (char *str, size_t size, int rw __attribute__ ((unused)), unsigned long value)
{
return snprint_symbolic (str, size, 0, value,
"DHT", (unsigned long) V4L2_JPEG_MARKER_DHT,
"DQT", (unsigned long) V4L2_JPEG_MARKER_DQT,
"DRI", (unsigned long) V4L2_JPEG_MARKER_DRI,
"COM", (unsigned long) V4L2_JPEG_MARKER_COM,
"APP", (unsigned long) V4L2_JPEG_MARKER_APP,
(void *) 0);
}

static size_t
snprint_struct_v4l2_jpegcompression (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_jpegcompression *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "quality=%ld "
"APPn=%ld "
"APP_len=%ld "
"APP_data=\"%.*s\" "
"COM_len=%ld "
"COM_data=\"%.*s\" "
"jpeg_markers=",
(long) t->quality, 
(long) t->APPn, 
(long) t->APP_len, 
60, (const char *) t->APP_data, 
(long) t->COM_len, 
60, (const char *) t->COM_data);
n = MIN (n, size);
n += snprint_symbol_v4l2_jpeg_marker_ (str + n, size - n, rw, t->jpeg_markers);
n += strlcpy (str + n, " ", size - n);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_v4l2_audioout (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_audioout *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "index=%lu ",
(unsigned long) t->index);
n = MIN (n, size);
if (1 & rw) {
n += snprintf (str + n, size - n, "name=\"%.*s\" "
"capability=%lu "
"mode=%lu "
"reserved[] ",
32, (const char *) t->name, 
(unsigned long) t->capability, 
(unsigned long) t->mode);
n = MIN (n, size);
}
return n;
}

static size_t
snprint_struct_v4l2_requestbuffers (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_requestbuffers *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "count=%lu "
"type=",
(unsigned long) t->count);
n = MIN (n, size);
n += snprint_enum_v4l2_buf_type (str + n, size - n, rw, t->type);
n += strlcpy (str + n, " memory=", size - n);
n = MIN (n, size);
n += snprint_enum_v4l2_memory (str + n, size - n, rw, t->memory);
n += strlcpy (str + n, " reserved[] ", size - n);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_v4l2_enc_idx (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_enc_idx *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "entries=%lu "
"entries_cap=%lu "
"reserved[] "
"entry[]=? ",
(unsigned long) t->entries, 
(unsigned long) t->entries_cap);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_v4l2_register (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_register *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "match_type=%lu "
"match_chip=%lu "
"reg=%lu "
"val=%lu ",
(unsigned long) t->match_type, 
(unsigned long) t->match_chip, 
(unsigned long) t->reg, 
(unsigned long) t->val);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_v4l2_cropcap (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_cropcap *t)
{
size_t n = 0;
n += strlcpy (str + n, "type=", size - n);
n = MIN (n, size);
n += snprint_enum_v4l2_buf_type (str + n, size - n, rw, t->type);
n += strlcpy (str + n, " bounds={", size - n);
n = MIN (n, size);
n += snprint_struct_v4l2_rect (str + n, size - n, rw, &t->bounds);
n += strlcpy (str + n, "} defrect={", size - n);
n = MIN (n, size);
n += snprint_struct_v4l2_rect (str + n, size - n, rw, &t->defrect);
n += strlcpy (str + n, "} pixelaspect={", size - n);
n = MIN (n, size);
n += snprint_struct_v4l2_fract (str + n, size - n, rw, &t->pixelaspect);
n += strlcpy (str + n, "} ", size - n);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_v4l2_audio (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_audio *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "index=%lu ",
(unsigned long) t->index);
n = MIN (n, size);
if (1 & rw) {
n += snprintf (str + n, size - n, "name=\"%.*s\" "
"capability=",
32, (const char *) t->name);
n = MIN (n, size);
n += snprint_symbolic (str + n, size - n, 0, t->capability,
"STEREO", (unsigned long) V4L2_AUDCAP_STEREO,
"AVL", (unsigned long) V4L2_AUDCAP_AVL,
(void *) 0);
n += strlcpy (str + n, " mode=", size - n);
n = MIN (n, size);
n += snprint_symbolic (str + n, size - n, 0, t->mode,
"AVL", (unsigned long) V4L2_AUDMODE_AVL,
(void *) 0);
n += strlcpy (str + n, " reserved[] ", size - n);
n = MIN (n, size);
}
return n;
}

static size_t
snprint_struct_v4l2_encoder_cmd (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_encoder_cmd *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "cmd=%lu "
"flags=0x%lx ",
(unsigned long) t->cmd, 
(unsigned long) t->flags);
n = MIN (n, size);
n += strlcpy (str + n, "? ", size - n);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_v4l2_output (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_output *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "index=%lu ",
(unsigned long) t->index);
n = MIN (n, size);
if (1 & rw) {
n += snprintf (str + n, size - n, "name=\"%.*s\" "
"type=",
32, (const char *) t->name);
n = MIN (n, size);
n += snprint_symbolic (str + n, size - n, 0, t->type,
"MODULATOR", (unsigned long) V4L2_OUTPUT_TYPE_MODULATOR,
"ANALOG", (unsigned long) V4L2_OUTPUT_TYPE_ANALOG,
"ANALOGVGAOVERLAY", (unsigned long) V4L2_OUTPUT_TYPE_ANALOGVGAOVERLAY,
(void *) 0);
n += snprintf (str + n, size - n, " audioset=%lu "
"modulator=%lu "
"std=",
(unsigned long) t->audioset, 
(unsigned long) t->modulator);
n = MIN (n, size);
n += snprint_symbol_v4l2_std_ (str + n, size - n, rw, t->std);
n += strlcpy (str + n, " reserved[] ", size - n);
n = MIN (n, size);
}
return n;
}

static size_t
snprint_symbol_v4l2_fbuf_cap_ (char *str, size_t size, int rw __attribute__ ((unused)), unsigned long value)
{
return snprint_symbolic (str, size, 0, value,
"EXTERNOVERLAY", (unsigned long) V4L2_FBUF_CAP_EXTERNOVERLAY,
"CHROMAKEY", (unsigned long) V4L2_FBUF_CAP_CHROMAKEY,
"LIST_CLIPPING", (unsigned long) V4L2_FBUF_CAP_LIST_CLIPPING,
"BITMAP_CLIPPING", (unsigned long) V4L2_FBUF_CAP_BITMAP_CLIPPING,
"LOCAL_ALPHA", (unsigned long) V4L2_FBUF_CAP_LOCAL_ALPHA,
"GLOBAL_ALPHA", (unsigned long) V4L2_FBUF_CAP_GLOBAL_ALPHA,
(void *) 0);
}

static size_t
snprint_symbol_v4l2_fbuf_flag_ (char *str, size_t size, int rw __attribute__ ((unused)), unsigned long value)
{
return snprint_symbolic (str, size, 2, value,
"PRIMARY", (unsigned long) V4L2_FBUF_FLAG_PRIMARY,
"OVERLAY", (unsigned long) V4L2_FBUF_FLAG_OVERLAY,
"CHROMAKEY", (unsigned long) V4L2_FBUF_FLAG_CHROMAKEY,
"LOCAL_ALPHA", (unsigned long) V4L2_FBUF_FLAG_LOCAL_ALPHA,
"GLOBAL_ALPHA", (unsigned long) V4L2_FBUF_FLAG_GLOBAL_ALPHA,
(void *) 0);
}

static size_t
snprint_struct_v4l2_framebuffer (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_framebuffer *t)
{
size_t n = 0;
n += strlcpy (str + n, "capability=", size - n);
n = MIN (n, size);
n += snprint_symbol_v4l2_fbuf_cap_ (str + n, size - n, rw, t->capability);
n += strlcpy (str + n, " flags=", size - n);
n = MIN (n, size);
n += snprint_symbol_v4l2_fbuf_flag_ (str + n, size - n, rw, t->flags);
n += strlcpy (str + n, " base=? "
"fmt={", size - n);
n = MIN (n, size);
n += snprint_struct_v4l2_pix_format (str + n, size - n, rw, &t->fmt);
n += strlcpy (str + n, "} ", size - n);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_v4l2_standard (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_standard *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "index=%lu ",
(unsigned long) t->index);
n = MIN (n, size);
if (1 & rw) {
n += strlcpy (str + n, "id=", size - n);
n = MIN (n, size);
n += snprint_symbol_v4l2_std_ (str + n, size - n, rw, t->id);
n += snprintf (str + n, size - n, " name=\"%.*s\" "
"frameperiod={",
24, (const char *) t->name);
n = MIN (n, size);
n += snprint_struct_v4l2_fract (str + n, size - n, rw, &t->frameperiod);
n += snprintf (str + n, size - n, "} framelines=%lu "
"reserved[] ",
(unsigned long) t->framelines);
n = MIN (n, size);
}
return n;
}

static size_t
snprint_ioctl_arg (char *str, size_t size, unsigned int cmd, int rw, void *arg)
{
size_t n;
assert (size > 0);
switch (cmd) {
case VIDIOC_G_CROP:
if (!arg) { return strlcpy (str, "VIDIOC_G_CROP", size); }
case VIDIOC_S_CROP:
if (!arg) { return strlcpy (str, "VIDIOC_S_CROP", size); }
 return snprint_struct_v4l2_crop (str, size, rw, arg);
case VIDIOC_OVERLAY:
if (!arg) { return strlcpy (str, "VIDIOC_OVERLAY", size); }
case VIDIOC_STREAMON:
if (!arg) { return strlcpy (str, "VIDIOC_STREAMON", size); }
case VIDIOC_STREAMOFF:
if (!arg) { return strlcpy (str, "VIDIOC_STREAMOFF", size); }
case VIDIOC_G_INPUT:
if (!arg) { return strlcpy (str, "VIDIOC_G_INPUT", size); }
case VIDIOC_S_INPUT:
if (!arg) { return strlcpy (str, "VIDIOC_S_INPUT", size); }
case VIDIOC_G_OUTPUT:
if (!arg) { return strlcpy (str, "VIDIOC_G_OUTPUT", size); }
case VIDIOC_S_OUTPUT:
if (!arg) { return strlcpy (str, "VIDIOC_S_OUTPUT", size); }
 n = snprintf (str, size, "%ld", (long) * (int *) arg);
return MIN (size - 1, n);
case VIDIOC_ENUM_FMT:
if (!arg) { return strlcpy (str, "VIDIOC_ENUM_FMT", size); }
 return snprint_struct_v4l2_fmtdesc (str, size, rw, arg);
case VIDIOC_G_TUNER:
if (!arg) { return strlcpy (str, "VIDIOC_G_TUNER", size); }
case VIDIOC_S_TUNER:
if (!arg) { return strlcpy (str, "VIDIOC_S_TUNER", size); }
 return snprint_struct_v4l2_tuner (str, size, rw, arg);
case VIDIOC_QUERYCAP:
if (!arg) { return strlcpy (str, "VIDIOC_QUERYCAP", size); }
 return snprint_struct_v4l2_capability (str, size, rw, arg);
case VIDIOC_QUERYCTRL:
if (!arg) { return strlcpy (str, "VIDIOC_QUERYCTRL", size); }
 return snprint_struct_v4l2_queryctrl (str, size, rw, arg);
case VIDIOC_G_MODULATOR:
if (!arg) { return strlcpy (str, "VIDIOC_G_MODULATOR", size); }
case VIDIOC_S_MODULATOR:
if (!arg) { return strlcpy (str, "VIDIOC_S_MODULATOR", size); }
 return snprint_struct_v4l2_modulator (str, size, rw, arg);
case VIDIOC_ENUM_FRAMEINTERVALS:
if (!arg) { return strlcpy (str, "VIDIOC_ENUM_FRAMEINTERVALS", size); }
 return snprint_struct_v4l2_frmivalenum (str, size, rw, arg);
case VIDIOC_ENUMINPUT:
if (!arg) { return strlcpy (str, "VIDIOC_ENUMINPUT", size); }
 return snprint_struct_v4l2_input (str, size, rw, arg);
case VIDIOC_G_CHIP_IDENT:
if (!arg) { return strlcpy (str, "VIDIOC_G_CHIP_IDENT", size); }
 return snprint_struct_v4l2_chip_ident (str, size, rw, arg);
case VIDIOC_G_EXT_CTRLS:
if (!arg) { return strlcpy (str, "VIDIOC_G_EXT_CTRLS", size); }
case VIDIOC_S_EXT_CTRLS:
if (!arg) { return strlcpy (str, "VIDIOC_S_EXT_CTRLS", size); }
case VIDIOC_TRY_EXT_CTRLS:
if (!arg) { return strlcpy (str, "VIDIOC_TRY_EXT_CTRLS", size); }
 return 0; /* struct v4l2_ext_controls */
case VIDIOC_G_SLICED_VBI_CAP:
if (!arg) { return strlcpy (str, "VIDIOC_G_SLICED_VBI_CAP", size); }
 return snprint_struct_v4l2_sliced_vbi_cap (str, size, rw, arg);
case VIDIOC_G_FMT:
if (!arg) { return strlcpy (str, "VIDIOC_G_FMT", size); }
case VIDIOC_S_FMT:
if (!arg) { return strlcpy (str, "VIDIOC_S_FMT", size); }
case VIDIOC_TRY_FMT:
if (!arg) { return strlcpy (str, "VIDIOC_TRY_FMT", size); }
 return snprint_struct_v4l2_format (str, size, rw, arg);
case VIDIOC_QUERYBUF:
if (!arg) { return strlcpy (str, "VIDIOC_QUERYBUF", size); }
case VIDIOC_QBUF:
if (!arg) { return strlcpy (str, "VIDIOC_QBUF", size); }
case VIDIOC_DQBUF:
if (!arg) { return strlcpy (str, "VIDIOC_DQBUF", size); }
 return snprint_struct_v4l2_buffer (str, size, rw, arg);
case VIDIOC_G_CTRL:
if (!arg) { return strlcpy (str, "VIDIOC_G_CTRL", size); }
case VIDIOC_S_CTRL:
if (!arg) { return strlcpy (str, "VIDIOC_S_CTRL", size); }
 return snprint_struct_v4l2_control (str, size, rw, arg);
case VIDIOC_G_STD:
if (!arg) { return strlcpy (str, "VIDIOC_G_STD", size); }
case VIDIOC_S_STD:
if (!arg) { return strlcpy (str, "VIDIOC_S_STD", size); }
case VIDIOC_QUERYSTD:
if (!arg) { return strlcpy (str, "VIDIOC_QUERYSTD", size); }
 return snprint_symbol_v4l2_std_ (str, size, rw, * (__u64 *) arg);
case VIDIOC_ENUM_FRAMESIZES:
if (!arg) { return strlcpy (str, "VIDIOC_ENUM_FRAMESIZES", size); }
 return snprint_struct_v4l2_frmsizeenum (str, size, rw, arg);
case VIDIOC_G_PARM:
if (!arg) { return strlcpy (str, "VIDIOC_G_PARM", size); }
case VIDIOC_S_PARM:
if (!arg) { return strlcpy (str, "VIDIOC_S_PARM", size); }
 return snprint_struct_v4l2_streamparm (str, size, rw, arg);
case VIDIOC_G_FREQUENCY:
if (!arg) { return strlcpy (str, "VIDIOC_G_FREQUENCY", size); }
case VIDIOC_S_FREQUENCY:
if (!arg) { return strlcpy (str, "VIDIOC_S_FREQUENCY", size); }
 return snprint_struct_v4l2_frequency (str, size, rw, arg);
case VIDIOC_QUERYMENU:
if (!arg) { return strlcpy (str, "VIDIOC_QUERYMENU", size); }
 return snprint_struct_v4l2_querymenu (str, size, rw, arg);
case VIDIOC_G_JPEGCOMP:
if (!arg) { return strlcpy (str, "VIDIOC_G_JPEGCOMP", size); }
case VIDIOC_S_JPEGCOMP:
if (!arg) { return strlcpy (str, "VIDIOC_S_JPEGCOMP", size); }
 return snprint_struct_v4l2_jpegcompression (str, size, rw, arg);
case VIDIOC_G_AUDOUT:
if (!arg) { return strlcpy (str, "VIDIOC_G_AUDOUT", size); }
case VIDIOC_S_AUDOUT:
if (!arg) { return strlcpy (str, "VIDIOC_S_AUDOUT", size); }
case VIDIOC_ENUMAUDOUT:
if (!arg) { return strlcpy (str, "VIDIOC_ENUMAUDOUT", size); }
 return snprint_struct_v4l2_audioout (str, size, rw, arg);
case VIDIOC_REQBUFS:
if (!arg) { return strlcpy (str, "VIDIOC_REQBUFS", size); }
 return snprint_struct_v4l2_requestbuffers (str, size, rw, arg);
case VIDIOC_G_ENC_INDEX:
if (!arg) { return strlcpy (str, "VIDIOC_G_ENC_INDEX", size); }
 return snprint_struct_v4l2_enc_idx (str, size, rw, arg);
case VIDIOC_DBG_S_REGISTER:
if (!arg) { return strlcpy (str, "VIDIOC_DBG_S_REGISTER", size); }
case VIDIOC_DBG_G_REGISTER:
if (!arg) { return strlcpy (str, "VIDIOC_DBG_G_REGISTER", size); }
 return snprint_struct_v4l2_register (str, size, rw, arg);
case VIDIOC_CROPCAP:
if (!arg) { return strlcpy (str, "VIDIOC_CROPCAP", size); }
 return snprint_struct_v4l2_cropcap (str, size, rw, arg);
case VIDIOC_G_AUDIO:
if (!arg) { return strlcpy (str, "VIDIOC_G_AUDIO", size); }
case VIDIOC_S_AUDIO:
if (!arg) { return strlcpy (str, "VIDIOC_S_AUDIO", size); }
case VIDIOC_ENUMAUDIO:
if (!arg) { return strlcpy (str, "VIDIOC_ENUMAUDIO", size); }
 return snprint_struct_v4l2_audio (str, size, rw, arg);
case VIDIOC_ENCODER_CMD:
if (!arg) { return strlcpy (str, "VIDIOC_ENCODER_CMD", size); }
case VIDIOC_TRY_ENCODER_CMD:
if (!arg) { return strlcpy (str, "VIDIOC_TRY_ENCODER_CMD", size); }
 return snprint_struct_v4l2_encoder_cmd (str, size, rw, arg);
case VIDIOC_ENUMOUTPUT:
if (!arg) { return strlcpy (str, "VIDIOC_ENUMOUTPUT", size); }
 return snprint_struct_v4l2_output (str, size, rw, arg);
case VIDIOC_G_FBUF:
if (!arg) { return strlcpy (str, "VIDIOC_G_FBUF", size); }
case VIDIOC_S_FBUF:
if (!arg) { return strlcpy (str, "VIDIOC_S_FBUF", size); }
 return snprint_struct_v4l2_framebuffer (str, size, rw, arg);
case VIDIOC_ENUMSTD:
if (!arg) { return strlcpy (str, "VIDIOC_ENUMSTD", size); }
 return snprint_struct_v4l2_standard (str, size, rw, arg);
default:
if (!arg) { return snprint_unknown_ioctl (str, size, cmd, arg); }
str[0] = 0; return 0;
}
return 0; }

static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_QUERYCAP (struct v4l2_capability *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_ENUM_FMT (struct v4l2_fmtdesc *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_FMT (struct v4l2_format *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_FMT (struct v4l2_format *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_REQBUFS (struct v4l2_requestbuffers *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_QUERYBUF (struct v4l2_buffer *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_FBUF (struct v4l2_framebuffer *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_FBUF (const struct v4l2_framebuffer *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_OVERLAY (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_QBUF (struct v4l2_buffer *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_DQBUF (struct v4l2_buffer *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_STREAMON (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_STREAMOFF (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_PARM (struct v4l2_streamparm *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_PARM (struct v4l2_streamparm *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_STD (v4l2_std_id *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_STD (const v4l2_std_id *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_ENUMSTD (struct v4l2_standard *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_ENUMINPUT (struct v4l2_input *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_CTRL (struct v4l2_control *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_CTRL (struct v4l2_control *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_TUNER (struct v4l2_tuner *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_TUNER (const struct v4l2_tuner *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_AUDIO (struct v4l2_audio *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_AUDIO (const struct v4l2_audio *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_QUERYCTRL (struct v4l2_queryctrl *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_QUERYMENU (struct v4l2_querymenu *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_INPUT (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_INPUT (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_OUTPUT (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_OUTPUT (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_ENUMOUTPUT (struct v4l2_output *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_AUDOUT (struct v4l2_audioout *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_AUDOUT (const struct v4l2_audioout *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_MODULATOR (struct v4l2_modulator *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_MODULATOR (const struct v4l2_modulator *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_FREQUENCY (struct v4l2_frequency *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_FREQUENCY (const struct v4l2_frequency *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_CROPCAP (struct v4l2_cropcap *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_CROP (struct v4l2_crop *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_CROP (const struct v4l2_crop *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_JPEGCOMP (struct v4l2_jpegcompression *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_JPEGCOMP (const struct v4l2_jpegcompression *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_QUERYSTD (v4l2_std_id *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_TRY_FMT (struct v4l2_format *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_ENUMAUDIO (struct v4l2_audio *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_ENUMAUDOUT (struct v4l2_audioout *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_PRIORITY (enum v4l2_priority *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_PRIORITY (const enum v4l2_priority *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_SLICED_VBI_CAP (struct v4l2_sliced_vbi_cap *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_EXT_CTRLS (struct v4l2_ext_controls *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_EXT_CTRLS (struct v4l2_ext_controls *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_TRY_EXT_CTRLS (struct v4l2_ext_controls *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_ENUM_FRAMESIZES (struct v4l2_frmsizeenum *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_ENUM_FRAMEINTERVALS (struct v4l2_frmivalenum *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_ENC_INDEX (struct v4l2_enc_idx *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_ENCODER_CMD (struct v4l2_encoder_cmd *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_TRY_ENCODER_CMD (struct v4l2_encoder_cmd *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_DBG_S_REGISTER (const struct v4l2_register *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_DBG_G_REGISTER (struct v4l2_register *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_CHIP_IDENT (struct v4l2_chip_ident *arg __attribute__ ((unused))) {}

