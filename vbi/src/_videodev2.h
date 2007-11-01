/* Generated file, do not edit! */

#include <stdio.h>
#include "io.h"

#ifndef __GNUC__
#undef __attribute__
#define __attribute__(x)
#endif

static size_t
snprint_struct_v4l2_performance (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_performance *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "frames=%ld "
"framesdropped=%ld "
"bytesin=%lu "
"bytesout=%lu "
"reserved[] ",
(long) t->frames, 
(long) t->framesdropped, 
(unsigned long) t->bytesin, 
(unsigned long) t->bytesout);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_v4l2_compression (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_compression *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "quality=%ld "
"keyframerate=%ld "
"pframerate=%ld "
"reserved[] ",
(long) t->quality, 
(long) t->keyframerate, 
(long) t->pframerate);
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
"YVU422P", (unsigned long) V4L2_PIX_FMT_YVU422P,
"YVU411P", (unsigned long) V4L2_PIX_FMT_YVU411P,
"Y41P", (unsigned long) V4L2_PIX_FMT_Y41P,
"YUV410", (unsigned long) V4L2_PIX_FMT_YUV410,
"YUV420", (unsigned long) V4L2_PIX_FMT_YUV420,
"YYUV", (unsigned long) V4L2_PIX_FMT_YYUV,
"HI240", (unsigned long) V4L2_PIX_FMT_HI240,
"WNVA", (unsigned long) V4L2_PIX_FMT_WNVA,
(void *) 0);
}

static size_t
snprint_struct_v4l2_fmtdesc (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_fmtdesc *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "index=%ld ",
(long) t->index);
n = MIN (n, size);
if (1 & rw) {
n += snprintf (str + n, size - n, "description=\"%.*s\" "
"pixelformat=",
32, (const char *) t->description);
n = MIN (n, size);
n += snprint_symbol_v4l2_pix_fmt_ (str + n, size - n, rw, t->pixelformat);
n += snprintf (str + n, size - n, " flags=0x%lx "
"depth=%lu "
"reserved[] ",
(unsigned long) t->flags, 
(unsigned long) t->depth);
n = MIN (n, size);
}
return n;
}

static size_t
snprint_symbol_v4l2_transm_std_ (char *str, size_t size, int rw __attribute__ ((unused)), unsigned long value)
{
return snprint_symbolic (str, size, 0, value,
"B", (unsigned long) V4L2_TRANSM_STD_B,
"D", (unsigned long) V4L2_TRANSM_STD_D,
"G", (unsigned long) V4L2_TRANSM_STD_G,
"H", (unsigned long) V4L2_TRANSM_STD_H,
"I", (unsigned long) V4L2_TRANSM_STD_I,
"K", (unsigned long) V4L2_TRANSM_STD_K,
"K1", (unsigned long) V4L2_TRANSM_STD_K1,
"L", (unsigned long) V4L2_TRANSM_STD_L,
"M", (unsigned long) V4L2_TRANSM_STD_M,
"N", (unsigned long) V4L2_TRANSM_STD_N,
(void *) 0);
}

static size_t
snprint_struct_v4l2_standard (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_standard *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "name=\"%.*s\" ",
24, (const char *) t->name);
n = MIN (n, size);
n += snprintf (str + n, size - n, "framerate={numerator=%lu "
"denominator=%lu ",
(unsigned long) t->framerate.numerator, 
(unsigned long) t->framerate.denominator);
n = MIN (n, size);
n += snprintf (str + n, size - n, "} framelines=%lu "
"reserved1 "
"colorstandard=",
(unsigned long) t->framelines);
n = MIN (n, size);
n += snprint_symbolic (str + n, size - n, 0, t->colorstandard,
"PAL", (unsigned long) V4L2_COLOR_STD_PAL,
"NTSC", (unsigned long) V4L2_COLOR_STD_NTSC,
"SECAM", (unsigned long) V4L2_COLOR_STD_SECAM,
(void *) 0);
n += strlcpy (str + n, " ", size - n);
n = MIN (n, size);
n += strlcpy (str + n, "colorstandard_data={", size - n);
n = MIN (n, size);
if (V4L2_COLOR_STD_PAL == t->colorstandard) {
n += strlcpy (str + n, "pal={", size - n);
n = MIN (n, size);
}
n += snprintf (str + n, size - n, "colorsubcarrier=%lu ",
(unsigned long) t->colorstandard_data.pal.colorsubcarrier);
n = MIN (n, size);
if (V4L2_COLOR_STD_PAL == t->colorstandard) {
n += strlcpy (str + n, "} ", size - n);
n = MIN (n, size);
}
if (V4L2_COLOR_STD_NTSC == t->colorstandard) {
n += strlcpy (str + n, "ntsc={", size - n);
n = MIN (n, size);
}
n += snprintf (str + n, size - n, "colorsubcarrier=%lu ",
(unsigned long) t->colorstandard_data.ntsc.colorsubcarrier);
n = MIN (n, size);
if (V4L2_COLOR_STD_NTSC == t->colorstandard) {
n += strlcpy (str + n, "} ", size - n);
n = MIN (n, size);
}
if (V4L2_COLOR_STD_SECAM == t->colorstandard) {
n += strlcpy (str + n, "secam={", size - n);
n = MIN (n, size);
}
n += snprintf (str + n, size - n, "f0b=%lu "
"f0r=%lu ",
(unsigned long) t->colorstandard_data.secam.f0b, 
(unsigned long) t->colorstandard_data.secam.f0r);
n = MIN (n, size);
if (V4L2_COLOR_STD_SECAM == t->colorstandard) {
n += strlcpy (str + n, "} ", size - n);
n = MIN (n, size);
}
n += strlcpy (str + n, "reserved[] ", size - n);
n = MIN (n, size);
n += strlcpy (str + n, "} transmission=", size - n);
n = MIN (n, size);
n += snprint_symbol_v4l2_transm_std_ (str + n, size - n, rw, t->transmission);
n += strlcpy (str + n, " reserved2 ", size - n);
n = MIN (n, size);
return n;
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
(void *) 0);
}

static size_t
snprint_struct_v4l2_tuner (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_tuner *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "input=%ld ",
(long) t->input);
n = MIN (n, size);
if (1 & rw) {
n += snprintf (str + n, size - n, "name=\"%.*s\" "
"std={",
32, (const char *) t->name);
n = MIN (n, size);
n += snprint_struct_v4l2_standard (str + n, size - n, rw, &t->std);
n += strlcpy (str + n, "} capability=", size - n);
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
snprint_symbol_v4l2_type_ (char *str, size_t size, int rw __attribute__ ((unused)), unsigned long value)
{
return snprint_symbolic (str, size, 0, value,
"CAPTURE", (unsigned long) V4L2_TYPE_CAPTURE,
"CODEC", (unsigned long) V4L2_TYPE_CODEC,
"OUTPUT", (unsigned long) V4L2_TYPE_OUTPUT,
"FX", (unsigned long) V4L2_TYPE_FX,
"VBI", (unsigned long) V4L2_TYPE_VBI,
"VTR", (unsigned long) V4L2_TYPE_VTR,
"VTX", (unsigned long) V4L2_TYPE_VTX,
"RADIO", (unsigned long) V4L2_TYPE_RADIO,
"VBI_INPUT", (unsigned long) V4L2_TYPE_VBI_INPUT,
"VBI_OUTPUT", (unsigned long) V4L2_TYPE_VBI_OUTPUT,
"PRIVATE", (unsigned long) V4L2_TYPE_PRIVATE,
(void *) 0);
}

static size_t
snprint_symbol_v4l2_flag_ (char *str, size_t size, int rw __attribute__ ((unused)), unsigned long value)
{
return snprint_symbolic (str, size, 2, value,
"READ", (unsigned long) V4L2_FLAG_READ,
"WRITE", (unsigned long) V4L2_FLAG_WRITE,
"STREAMING", (unsigned long) V4L2_FLAG_STREAMING,
"PREVIEW", (unsigned long) V4L2_FLAG_PREVIEW,
"SELECT", (unsigned long) V4L2_FLAG_SELECT,
"TUNER", (unsigned long) V4L2_FLAG_TUNER,
"MONOCHROME", (unsigned long) V4L2_FLAG_MONOCHROME,
"DATA_SERVICE", (unsigned long) V4L2_FLAG_DATA_SERVICE,
(void *) 0);
}

static size_t
snprint_struct_v4l2_capability (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_capability *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "name=\"%.*s\" "
"type=",
32, (const char *) t->name);
n = MIN (n, size);
n += snprint_symbol_v4l2_type_ (str + n, size - n, rw, t->type);
n += snprintf (str + n, size - n, " inputs=%ld "
"outputs=%ld "
"audios=%ld "
"maxwidth=%ld "
"maxheight=%ld "
"minwidth=%ld "
"minheight=%ld "
"maxframerate=%ld "
"flags=",
(long) t->inputs, 
(long) t->outputs, 
(long) t->audios, 
(long) t->maxwidth, 
(long) t->maxheight, 
(long) t->minwidth, 
(long) t->minheight, 
(long) t->maxframerate);
n = MIN (n, size);
n += snprint_symbol_v4l2_flag_ (str + n, size - n, rw, t->flags);
n += strlcpy (str + n, " reserved[] ", size - n);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_v4l2_enumstd (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_enumstd *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "index=%ld ",
(long) t->index);
n = MIN (n, size);
if (1 & rw) {
n += strlcpy (str + n, "std={", size - n);
n = MIN (n, size);
n += snprint_struct_v4l2_standard (str + n, size - n, rw, &t->std);
n += snprintf (str + n, size - n, "} inputs=%lu "
"outputs=%lu "
"reserved[] ",
(unsigned long) t->inputs, 
(unsigned long) t->outputs);
n = MIN (n, size);
}
return n;
}

static size_t
snprint_symbol_v4l2_cid_ (char *str, size_t size, int rw __attribute__ ((unused)), unsigned long value)
{
return snprint_symbolic (str, size, 0, value,
"BASE", (unsigned long) V4L2_CID_BASE,
"PRIVATE_BASE", (unsigned long) V4L2_CID_PRIVATE_BASE,
"EFFECT_BASE", (unsigned long) V4L2_CID_EFFECT_BASE,
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
(void *) 0);
}

static size_t
snprint_symbol_v4l2_ctrl_type_ (char *str, size_t size, int rw __attribute__ ((unused)), unsigned long value)
{
return snprint_symbolic (str, size, 0, value,
"INTEGER", (unsigned long) V4L2_CTRL_TYPE_INTEGER,
"BOOLEAN", (unsigned long) V4L2_CTRL_TYPE_BOOLEAN,
"MENU", (unsigned long) V4L2_CTRL_TYPE_MENU,
"BUTTON", (unsigned long) V4L2_CTRL_TYPE_BUTTON,
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
n += snprintf (str + n, size - n, "name=\"%.*s\" "
"minimum=%ld "
"maximum=%ld "
"step=%lu "
"default_value=%ld "
"type=",
32, (const char *) t->name, 
(long) t->minimum, 
(long) t->maximum, 
(unsigned long) t->step, 
(long) t->default_value);
n = MIN (n, size);
n += snprint_symbol_v4l2_ctrl_type_ (str + n, size - n, rw, t->type);
n += strlcpy (str + n, " flags=", size - n);
n = MIN (n, size);
n += snprint_symbolic (str + n, size - n, 2, t->flags,
"DISABLED", (unsigned long) V4L2_CTRL_FLAG_DISABLED,
"GRABBED", (unsigned long) V4L2_CTRL_FLAG_GRABBED,
(void *) 0);
n += strlcpy (str + n, " category=", size - n);
n = MIN (n, size);
n += snprint_symbolic (str + n, size - n, 0, t->category,
"VIDEO", (unsigned long) V4L2_CTRL_CAT_VIDEO,
"AUDIO", (unsigned long) V4L2_CTRL_CAT_AUDIO,
"EFFECT", (unsigned long) V4L2_CTRL_CAT_EFFECT,
(void *) 0);
n += snprintf (str + n, size - n, " group=\"%.*s\" "
"reserved[] ",
32, (const char *) t->group);
n = MIN (n, size);
}
return n;
}

static size_t
snprint_struct_v4l2_modulator (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_modulator *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "output=%ld ",
(long) t->output);
n = MIN (n, size);
if (1 & rw) {
n += snprintf (str + n, size - n, "name=\"%.*s\" "
"std={",
32, (const char *) t->name);
n = MIN (n, size);
n += snprint_struct_v4l2_standard (str + n, size - n, rw, &t->std);
n += strlcpy (str + n, "} capability=", size - n);
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
snprint_struct_v4l2_input (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_input *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "index=%ld ",
(long) t->index);
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
n += strlcpy (str + n, " capability=", size - n);
n = MIN (n, size);
n += snprint_symbolic (str + n, size - n, 0, t->capability,
"AUDIO", (unsigned long) V4L2_INPUT_CAP_AUDIO,
(void *) 0);
n += snprintf (str + n, size - n, " assoc_audio=%ld "
"reserved[] ",
(long) t->assoc_audio);
n = MIN (n, size);
}
return n;
}

static size_t
snprint_symbol_v4l2_buf_type_ (char *str, size_t size, int rw __attribute__ ((unused)), unsigned long value)
{
return snprint_symbolic (str, size, 0, value,
"field", (unsigned long) V4L2_BUF_TYPE_field,
"CAPTURE", (unsigned long) V4L2_BUF_TYPE_CAPTURE,
"CODECIN", (unsigned long) V4L2_BUF_TYPE_CODECIN,
"CODECOUT", (unsigned long) V4L2_BUF_TYPE_CODECOUT,
"EFFECTSIN", (unsigned long) V4L2_BUF_TYPE_EFFECTSIN,
"EFFECTSIN2", (unsigned long) V4L2_BUF_TYPE_EFFECTSIN2,
"EFFECTSOUT", (unsigned long) V4L2_BUF_TYPE_EFFECTSOUT,
"VIDEOOUT", (unsigned long) V4L2_BUF_TYPE_VIDEOOUT,
"FXCONTROL", (unsigned long) V4L2_BUF_TYPE_FXCONTROL,
"VBI", (unsigned long) V4L2_BUF_TYPE_VBI,
"PRIVATE", (unsigned long) V4L2_BUF_TYPE_PRIVATE,
(void *) 0);
}

static size_t
snprint_symbol_v4l2_fmt_flag_ (char *str, size_t size, int rw __attribute__ ((unused)), unsigned long value)
{
return snprint_symbolic (str, size, 2, value,
"COMPRESSED", (unsigned long) V4L2_FMT_FLAG_COMPRESSED,
"BYTESPERLINE", (unsigned long) V4L2_FMT_FLAG_BYTESPERLINE,
"NOT_INTERLACED", (unsigned long) V4L2_FMT_FLAG_NOT_INTERLACED,
"INTERLACED", (unsigned long) V4L2_FMT_FLAG_INTERLACED,
"TOPFIELD", (unsigned long) V4L2_FMT_FLAG_TOPFIELD,
"BOTFIELD", (unsigned long) V4L2_FMT_FLAG_BOTFIELD,
"ODDFIELD", (unsigned long) V4L2_FMT_FLAG_ODDFIELD,
"EVENFIELD", (unsigned long) V4L2_FMT_FLAG_EVENFIELD,
"COMBINED", (unsigned long) V4L2_FMT_FLAG_COMBINED,
"FIELD_field", (unsigned long) V4L2_FMT_FLAG_FIELD_field,
"SWCONVERSION", (unsigned long) V4L2_FMT_FLAG_SWCONVERSION,
(void *) 0);
}

static size_t
snprint_struct_v4l2_pix_format (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_pix_format *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "width=%lu "
"height=%lu "
"depth=%lu "
"pixelformat=",
(unsigned long) t->width, 
(unsigned long) t->height, 
(unsigned long) t->depth);
n = MIN (n, size);
n += snprint_symbol_v4l2_pix_fmt_ (str + n, size - n, rw, t->pixelformat);
n += strlcpy (str + n, " flags=", size - n);
n = MIN (n, size);
n += snprint_symbol_v4l2_fmt_flag_ (str + n, size - n, rw, t->flags);
n += snprintf (str + n, size - n, " bytesperline=%lu "
"sizeimage=%lu "
"priv=%lu ",
(unsigned long) t->bytesperline, 
(unsigned long) t->sizeimage, 
(unsigned long) t->priv);
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
n += snprint_symbolic (str + n, size - n, 0, t->sample_format,
"UBYTE", (unsigned long) V4L2_VBI_SF_UBYTE,
(void *) 0);
n += strlcpy (str + n, " start[]=? "
"count[]=? "
"flags=", size - n);
n = MIN (n, size);
n += snprint_symbolic (str + n, size - n, 2, t->flags,
"SF_UBYTE", (unsigned long) V4L2_VBI_SF_UBYTE,
"UNSYNC", (unsigned long) V4L2_VBI_UNSYNC,
"INTERLACED", (unsigned long) V4L2_VBI_INTERLACED,
(void *) 0);
n += strlcpy (str + n, " reserved2 ", size - n);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_v4l2_format (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_format *t)
{
size_t n = 0;
n += strlcpy (str + n, "type=", size - n);
n = MIN (n, size);
n += snprint_symbol_v4l2_buf_type_ (str + n, size - n, rw, t->type);
n += strlcpy (str + n, " ", size - n);
n = MIN (n, size);
if (1 & rw) {
n += strlcpy (str + n, "fmt={", size - n);
n = MIN (n, size);
}
if ((1 & rw) && V4L2_BUF_TYPE_CAPTURE == t->type) {
n += strlcpy (str + n, "pix={", size - n);
n = MIN (n, size);
n += snprint_struct_v4l2_pix_format (str + n, size - n, rw, &t->fmt.pix);
n += strlcpy (str + n, "} ", size - n);
n = MIN (n, size);
}
if ((1 & rw) && V4L2_BUF_TYPE_VBI == t->type) {
n += strlcpy (str + n, "vbi={", size - n);
n = MIN (n, size);
n += snprint_struct_v4l2_vbi_format (str + n, size - n, rw, &t->fmt.vbi);
n += strlcpy (str + n, "} ", size - n);
n = MIN (n, size);
}
if (1 & rw) {
n += strlcpy (str + n, "raw_data[]=? ", size - n);
n = MIN (n, size);
n += strlcpy (str + n, "} ", size - n);
n = MIN (n, size);
}
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
"TOPFIELD", (unsigned long) V4L2_BUF_FLAG_TOPFIELD,
"BOTFIELD", (unsigned long) V4L2_BUF_FLAG_BOTFIELD,
"ODDFIELD", (unsigned long) V4L2_BUF_FLAG_ODDFIELD,
"EVENFIELD", (unsigned long) V4L2_BUF_FLAG_EVENFIELD,
"TIMECODE", (unsigned long) V4L2_BUF_FLAG_TIMECODE,
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
n += snprintf (str + n, size - n, "frames=%lu "
"seconds=%lu "
"minutes=%lu "
"hours=%lu "
"userbits[]=? "
"flags=",
(unsigned long) t->frames, 
(unsigned long) t->seconds, 
(unsigned long) t->minutes, 
(unsigned long) t->hours);
n = MIN (n, size);
n += snprint_symbolic (str + n, size - n, 2, t->flags,
"DROPFRAME", (unsigned long) V4L2_TC_FLAG_DROPFRAME,
"COLORFRAME", (unsigned long) V4L2_TC_FLAG_COLORFRAME,
(void *) 0);
n += strlcpy (str + n, " type=", size - n);
n = MIN (n, size);
n += snprint_symbol_v4l2_tc_type_ (str + n, size - n, rw, t->type);
n += strlcpy (str + n, " ", size - n);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_v4l2_buffer (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_buffer *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "index=%ld "
"type=",
(long) t->index);
n = MIN (n, size);
n += snprint_symbol_v4l2_buf_type_ (str + n, size - n, rw, t->type);
n += strlcpy (str + n, " ", size - n);
n = MIN (n, size);
if (1 & rw) {
n += snprintf (str + n, size - n, "offset=%lu "
"length=%lu "
"bytesused=%lu "
"flags=",
(unsigned long) t->offset, 
(unsigned long) t->length, 
(unsigned long) t->bytesused);
n = MIN (n, size);
n += snprint_symbol_v4l2_buf_flag_ (str + n, size - n, rw, t->flags);
n += snprintf (str + n, size - n, " timestamp=%ld "
"timecode={",
(long) t->timestamp);
n = MIN (n, size);
n += snprint_struct_v4l2_timecode (str + n, size - n, rw, &t->timecode);
n += snprintf (str + n, size - n, "} sequence=%lu "
"reserved[] ",
(unsigned long) t->sequence);
n = MIN (n, size);
}
return n;
}

static size_t
snprint_struct_v4l2_cvtdesc (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_cvtdesc *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "index=%ld ",
(long) t->index);
n = MIN (n, size);
if (1 & rw) {
n += snprintf (str + n, size - n, "in={pixelformat=%lu "
"flags=0x%lx "
"depth=%lu "
"reserved[] ",
(unsigned long) t->in.pixelformat, 
(unsigned long) t->in.flags, 
(unsigned long) t->in.depth);
n = MIN (n, size);
n += strlcpy (str + n, "} ", size - n);
n = MIN (n, size);
n += snprintf (str + n, size - n, "out={pixelformat=%lu "
"flags=0x%lx "
"depth=%lu "
"reserved[] ",
(unsigned long) t->out.pixelformat, 
(unsigned long) t->out.flags, 
(unsigned long) t->out.depth);
n = MIN (n, size);
n += strlcpy (str + n, "} ", size - n);
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
snprint_struct_v4l2_captureparm (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_captureparm *t)
{
size_t n = 0;
n += strlcpy (str + n, "capability=", size - n);
n = MIN (n, size);
n += snprint_symbolic (str + n, size - n, 0, t->capability,
"TIMEPERFRAME", (unsigned long) V4L2_CAP_TIMEPERFRAME,
(void *) 0);
n += strlcpy (str + n, " capturemode=", size - n);
n = MIN (n, size);
n += snprint_symbolic (str + n, size - n, 0, t->capturemode,
"HIGHQUALITY", (unsigned long) V4L2_MODE_HIGHQUALITY,
(void *) 0);
n += snprintf (str + n, size - n, " timeperframe=%lu "
"extendedmode=%lu "
"reserved[] ",
(unsigned long) t->timeperframe, 
(unsigned long) t->extendedmode);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_v4l2_outputparm (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_outputparm *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "capability=%lu "
"outputmode=%lu "
"timeperframe=%lu "
"extendedmode=%lu "
"reserved[] ",
(unsigned long) t->capability, 
(unsigned long) t->outputmode, 
(unsigned long) t->timeperframe, 
(unsigned long) t->extendedmode);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_v4l2_streamparm (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_streamparm *t)
{
size_t n = 0;
n += strlcpy (str + n, "type=", size - n);
n = MIN (n, size);
n += snprint_symbol_v4l2_buf_type_ (str + n, size - n, rw, t->type);
n += strlcpy (str + n, " ", size - n);
n = MIN (n, size);
if (1 & rw) {
n += strlcpy (str + n, "parm={", size - n);
n = MIN (n, size);
}
if ((1 & rw) && V4L2_BUF_TYPE_CAPTURE == t->type) {
n += strlcpy (str + n, "capture={", size - n);
n = MIN (n, size);
n += snprint_struct_v4l2_captureparm (str + n, size - n, rw, &t->parm.capture);
n += strlcpy (str + n, "} ", size - n);
n = MIN (n, size);
}
if ((1 & rw) && V4L2_BUF_TYPE_VIDEOOUT == t->type) {
n += strlcpy (str + n, "output={", size - n);
n = MIN (n, size);
n += snprint_struct_v4l2_outputparm (str + n, size - n, rw, &t->parm.output);
n += strlcpy (str + n, "} ", size - n);
n = MIN (n, size);
}
if (1 & rw) {
n += strlcpy (str + n, "raw_data[]=? ", size - n);
n = MIN (n, size);
n += strlcpy (str + n, "} ", size - n);
n = MIN (n, size);
}
return n;
}

static size_t
snprint_struct_v4l2_fxdesc (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_fxdesc *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "index=%ld ",
(long) t->index);
n = MIN (n, size);
if (1 & rw) {
n += snprintf (str + n, size - n, "name=\"%.*s\" "
"flags=0x%lx "
"inputs=%lu "
"controls=%lu "
"reserved[] ",
32, (const char *) t->name, 
(unsigned long) t->flags, 
(unsigned long) t->inputs, 
(unsigned long) t->controls);
n = MIN (n, size);
}
return n;
}

static size_t
snprint_struct_v4l2_querymenu (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_querymenu *t)
{
size_t n = 0;
n += strlcpy (str + n, "id=", size - n);
n = MIN (n, size);
n += snprint_symbol_v4l2_cid_ (str + n, size - n, rw, t->id);
n += snprintf (str + n, size - n, " index=%ld ",
(long) t->index);
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
snprint_struct_v4l2_audioout (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_audioout *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "audio=%ld ",
(long) t->audio);
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
n += snprintf (str + n, size - n, "count=%ld "
"type=",
(long) t->count);
n = MIN (n, size);
n += snprint_symbol_v4l2_buf_type_ (str + n, size - n, rw, t->type);
n += strlcpy (str + n, " reserved[] ", size - n);
n = MIN (n, size);
return n;
}

static size_t
snprint_symbol_v4l2_audmode_ (char *str, size_t size, int rw __attribute__ ((unused)), unsigned long value)
{
return snprint_symbolic (str, size, 0, value,
"LOUDNESS", (unsigned long) V4L2_AUDMODE_LOUDNESS,
"AVL", (unsigned long) V4L2_AUDMODE_AVL,
"STEREO_field", (unsigned long) V4L2_AUDMODE_STEREO_field,
"STEREO_LINEAR", (unsigned long) V4L2_AUDMODE_STEREO_LINEAR,
"STEREO_PSEUDO", (unsigned long) V4L2_AUDMODE_STEREO_PSEUDO,
"STEREO_SPATIAL30", (unsigned long) V4L2_AUDMODE_STEREO_SPATIAL30,
"STEREO_SPATIAL50", (unsigned long) V4L2_AUDMODE_STEREO_SPATIAL50,
(void *) 0);
}

static size_t
snprint_struct_v4l2_audio (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_audio *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "audio=%ld ",
(long) t->audio);
n = MIN (n, size);
if (1 & rw) {
n += snprintf (str + n, size - n, "name=\"%.*s\" "
"capability=",
32, (const char *) t->name);
n = MIN (n, size);
n += snprint_symbolic (str + n, size - n, 0, t->capability,
"EFFECTS", (unsigned long) V4L2_AUDCAP_EFFECTS,
"LOUDNESS", (unsigned long) V4L2_AUDCAP_LOUDNESS,
"AVL", (unsigned long) V4L2_AUDCAP_AVL,
(void *) 0);
n += strlcpy (str + n, " mode=", size - n);
n = MIN (n, size);
n += snprint_symbol_v4l2_audmode_ (str + n, size - n, rw, t->mode);
n += strlcpy (str + n, " reserved[] ", size - n);
n = MIN (n, size);
}
return n;
}

static size_t
snprint_struct_v4l2_output (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_output *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "index=%ld ",
(long) t->index);
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
n += strlcpy (str + n, " capability=", size - n);
n = MIN (n, size);
n += snprint_symbolic (str + n, size - n, 0, t->capability,
"AUDIO", (unsigned long) V4L2_OUTPUT_CAP_AUDIO,
(void *) 0);
n += snprintf (str + n, size - n, " assoc_audio=%ld "
"reserved[] ",
(long) t->assoc_audio);
n = MIN (n, size);
}
return n;
}

static size_t
snprint_struct_v4l2_window (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_window *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "x=%ld "
"y=%ld "
"width=%ld "
"height=%ld "
"chromakey=%lu "
"clips=%p "
"clipcount=%ld "
"bitmap=%p ",
(long) t->x, 
(long) t->y, 
(long) t->width, 
(long) t->height, 
(unsigned long) t->chromakey, 
(const void *) t->clips, 
(long) t->clipcount, 
(const void *) t->bitmap);
n = MIN (n, size);
return n;
}

static size_t
snprint_symbol_v4l2_fbuf_cap_ (char *str, size_t size, int rw __attribute__ ((unused)), unsigned long value)
{
return snprint_symbolic (str, size, 0, value,
"EXTERNOVERLAY", (unsigned long) V4L2_FBUF_CAP_EXTERNOVERLAY,
"CHROMAKEY", (unsigned long) V4L2_FBUF_CAP_CHROMAKEY,
"CLIPPING", (unsigned long) V4L2_FBUF_CAP_CLIPPING,
"SCALEUP", (unsigned long) V4L2_FBUF_CAP_SCALEUP,
"SCALEDOWN", (unsigned long) V4L2_FBUF_CAP_SCALEDOWN,
"BITMAP_CLIPPING", (unsigned long) V4L2_FBUF_CAP_BITMAP_CLIPPING,
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
n += snprint_symbolic (str + n, size - n, 2, t->flags,
"PRIMARY", (unsigned long) V4L2_FBUF_FLAG_PRIMARY,
"OVERLAY", (unsigned long) V4L2_FBUF_FLAG_OVERLAY,
"CHROMAKEY", (unsigned long) V4L2_FBUF_FLAG_CHROMAKEY,
(void *) 0);
n += strlcpy (str + n, " base[]=? "
"fmt={", size - n);
n = MIN (n, size);
n += snprint_struct_v4l2_pix_format (str + n, size - n, rw, &t->fmt);
n += strlcpy (str + n, "} ", size - n);
n = MIN (n, size);
return n;
}

static size_t
snprint_ioctl_arg (char *str, size_t size, unsigned int cmd, int rw, void *arg)
{
size_t n;
assert (size > 0);
switch (cmd) {
case VIDIOC_G_PERF:
if (!arg) { return strlcpy (str, "VIDIOC_G_PERF", size); }
 return snprint_struct_v4l2_performance (str, size, rw, arg);
case VIDIOC_PREVIEW:
if (!arg) { return strlcpy (str, "VIDIOC_PREVIEW", size); }
case VIDIOC_STREAMON:
if (!arg) { return strlcpy (str, "VIDIOC_STREAMON", size); }
case VIDIOC_STREAMOFF:
if (!arg) { return strlcpy (str, "VIDIOC_STREAMOFF", size); }
case VIDIOC_G_FREQ:
if (!arg) { return strlcpy (str, "VIDIOC_G_FREQ", size); }
case VIDIOC_S_FREQ:
if (!arg) { return strlcpy (str, "VIDIOC_S_FREQ", size); }
case VIDIOC_G_INPUT:
if (!arg) { return strlcpy (str, "VIDIOC_G_INPUT", size); }
case VIDIOC_S_INPUT:
if (!arg) { return strlcpy (str, "VIDIOC_S_INPUT", size); }
case VIDIOC_G_OUTPUT:
if (!arg) { return strlcpy (str, "VIDIOC_G_OUTPUT", size); }
case VIDIOC_S_OUTPUT:
if (!arg) { return strlcpy (str, "VIDIOC_S_OUTPUT", size); }
case VIDIOC_G_EFFECT:
if (!arg) { return strlcpy (str, "VIDIOC_G_EFFECT", size); }
case VIDIOC_S_EFFECT:
if (!arg) { return strlcpy (str, "VIDIOC_S_EFFECT", size); }
 n = snprintf (str, size, "%ld", (long) * (int *) arg);
return MIN (size - 1, n);
case VIDIOC_G_COMP:
if (!arg) { return strlcpy (str, "VIDIOC_G_COMP", size); }
case VIDIOC_S_COMP:
if (!arg) { return strlcpy (str, "VIDIOC_S_COMP", size); }
 return snprint_struct_v4l2_compression (str, size, rw, arg);
case VIDIOC_ENUM_PIXFMT:
if (!arg) { return strlcpy (str, "VIDIOC_ENUM_PIXFMT", size); }
case VIDIOC_ENUM_FBUFFMT:
if (!arg) { return strlcpy (str, "VIDIOC_ENUM_FBUFFMT", size); }
 return snprint_struct_v4l2_fmtdesc (str, size, rw, arg);
case VIDIOC_G_TUNER:
if (!arg) { return strlcpy (str, "VIDIOC_G_TUNER", size); }
case VIDIOC_S_TUNER:
if (!arg) { return strlcpy (str, "VIDIOC_S_TUNER", size); }
 return snprint_struct_v4l2_tuner (str, size, rw, arg);
case VIDIOC_QUERYCAP:
if (!arg) { return strlcpy (str, "VIDIOC_QUERYCAP", size); }
 return snprint_struct_v4l2_capability (str, size, rw, arg);
case VIDIOC_ENUMSTD:
if (!arg) { return strlcpy (str, "VIDIOC_ENUMSTD", size); }
 return snprint_struct_v4l2_enumstd (str, size, rw, arg);
case VIDIOC_QUERYCTRL:
if (!arg) { return strlcpy (str, "VIDIOC_QUERYCTRL", size); }
 return snprint_struct_v4l2_queryctrl (str, size, rw, arg);
case VIDIOC_G_MODULATOR:
if (!arg) { return strlcpy (str, "VIDIOC_G_MODULATOR", size); }
case VIDIOC_S_MODULATOR:
if (!arg) { return strlcpy (str, "VIDIOC_S_MODULATOR", size); }
 return snprint_struct_v4l2_modulator (str, size, rw, arg);
case VIDIOC_ENUMINPUT:
if (!arg) { return strlcpy (str, "VIDIOC_ENUMINPUT", size); }
 return snprint_struct_v4l2_input (str, size, rw, arg);
case VIDIOC_G_FMT:
if (!arg) { return strlcpy (str, "VIDIOC_G_FMT", size); }
case VIDIOC_S_FMT:
if (!arg) { return strlcpy (str, "VIDIOC_S_FMT", size); }
 return snprint_struct_v4l2_format (str, size, rw, arg);
case VIDIOC_QUERYBUF:
if (!arg) { return strlcpy (str, "VIDIOC_QUERYBUF", size); }
case VIDIOC_QBUF:
if (!arg) { return strlcpy (str, "VIDIOC_QBUF", size); }
case VIDIOC_DQBUF:
if (!arg) { return strlcpy (str, "VIDIOC_DQBUF", size); }
 return snprint_struct_v4l2_buffer (str, size, rw, arg);
case VIDIOC_ENUMCVT:
if (!arg) { return strlcpy (str, "VIDIOC_ENUMCVT", size); }
 return snprint_struct_v4l2_cvtdesc (str, size, rw, arg);
case VIDIOC_G_CTRL:
if (!arg) { return strlcpy (str, "VIDIOC_G_CTRL", size); }
case VIDIOC_S_CTRL:
if (!arg) { return strlcpy (str, "VIDIOC_S_CTRL", size); }
 return snprint_struct_v4l2_control (str, size, rw, arg);
case VIDIOC_G_PARM:
if (!arg) { return strlcpy (str, "VIDIOC_G_PARM", size); }
case VIDIOC_S_PARM:
if (!arg) { return strlcpy (str, "VIDIOC_S_PARM", size); }
 return snprint_struct_v4l2_streamparm (str, size, rw, arg);
case VIDIOC_ENUMFX:
if (!arg) { return strlcpy (str, "VIDIOC_ENUMFX", size); }
 return snprint_struct_v4l2_fxdesc (str, size, rw, arg);
case VIDIOC_QUERYMENU:
if (!arg) { return strlcpy (str, "VIDIOC_QUERYMENU", size); }
 return snprint_struct_v4l2_querymenu (str, size, rw, arg);
case VIDIOC_G_AUDOUT:
if (!arg) { return strlcpy (str, "VIDIOC_G_AUDOUT", size); }
case VIDIOC_S_AUDOUT:
if (!arg) { return strlcpy (str, "VIDIOC_S_AUDOUT", size); }
 return snprint_struct_v4l2_audioout (str, size, rw, arg);
case VIDIOC_REQBUFS:
if (!arg) { return strlcpy (str, "VIDIOC_REQBUFS", size); }
 return snprint_struct_v4l2_requestbuffers (str, size, rw, arg);
case VIDIOC_G_AUDIO:
if (!arg) { return strlcpy (str, "VIDIOC_G_AUDIO", size); }
case VIDIOC_S_AUDIO:
if (!arg) { return strlcpy (str, "VIDIOC_S_AUDIO", size); }
 return snprint_struct_v4l2_audio (str, size, rw, arg);
case VIDIOC_ENUMOUTPUT:
if (!arg) { return strlcpy (str, "VIDIOC_ENUMOUTPUT", size); }
 return snprint_struct_v4l2_output (str, size, rw, arg);
case VIDIOC_G_WIN:
if (!arg) { return strlcpy (str, "VIDIOC_G_WIN", size); }
case VIDIOC_S_WIN:
if (!arg) { return strlcpy (str, "VIDIOC_S_WIN", size); }
 return snprint_struct_v4l2_window (str, size, rw, arg);
case VIDIOC_G_FBUF:
if (!arg) { return strlcpy (str, "VIDIOC_G_FBUF", size); }
case VIDIOC_S_FBUF:
if (!arg) { return strlcpy (str, "VIDIOC_S_FBUF", size); }
 return snprint_struct_v4l2_framebuffer (str, size, rw, arg);
case VIDIOC_G_STD:
if (!arg) { return strlcpy (str, "VIDIOC_G_STD", size); }
case VIDIOC_S_STD:
if (!arg) { return strlcpy (str, "VIDIOC_S_STD", size); }
 return snprint_struct_v4l2_standard (str, size, rw, arg);
default:
if (!arg) { return snprint_unknown_ioctl (str, size, cmd, arg); }
str[0] = 0; return 0;
}
return 0; }

static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_QUERYCAP (struct v4l2_capability *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_ENUM_PIXFMT (struct v4l2_fmtdesc *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_ENUM_FBUFFMT (struct v4l2_fmtdesc *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_FMT (struct v4l2_format *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_FMT (struct v4l2_format *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_COMP (struct v4l2_compression *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_COMP (const struct v4l2_compression *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_REQBUFS (struct v4l2_requestbuffers *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_QUERYBUF (struct v4l2_buffer *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_FBUF (struct v4l2_framebuffer *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_FBUF (const struct v4l2_framebuffer *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_WIN (struct v4l2_window *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_WIN (const struct v4l2_window *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_PREVIEW (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_QBUF (struct v4l2_buffer *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_DQBUF (struct v4l2_buffer *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_STREAMON (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_STREAMOFF (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_PERF (struct v4l2_performance *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_PARM (struct v4l2_streamparm *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_PARM (const struct v4l2_streamparm *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_STD (struct v4l2_standard *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_STD (const struct v4l2_standard *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_ENUMSTD (struct v4l2_enumstd *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_ENUMINPUT (struct v4l2_input *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_CTRL (struct v4l2_control *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_CTRL (const struct v4l2_control *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_TUNER (struct v4l2_tuner *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_TUNER (const struct v4l2_tuner *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_FREQ (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_FREQ (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_AUDIO (struct v4l2_audio *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_AUDIO (const struct v4l2_audio *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_QUERYCTRL (struct v4l2_queryctrl *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_QUERYMENU (struct v4l2_querymenu *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_INPUT (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_INPUT (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_ENUMCVT (struct v4l2_cvtdesc *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_OUTPUT (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_OUTPUT (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_ENUMOUTPUT (struct v4l2_output *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_AUDOUT (struct v4l2_audioout *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_AUDOUT (const struct v4l2_audioout *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_ENUMFX (struct v4l2_fxdesc *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_EFFECT (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_EFFECT (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_G_MODULATOR (struct v4l2_modulator *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_S_MODULATOR (const struct v4l2_modulator *arg __attribute__ ((unused))) {}

