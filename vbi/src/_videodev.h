/* Generated file, do not edit! */

#include <stdio.h>
#include "io.h"

#ifndef __GNUC__
#undef __attribute__
#define __attribute__(x)
#endif

static size_t
snprint_struct_video_play_mode (char *str, size_t size, int rw __attribute__ ((unused)), const struct video_play_mode *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "mode=%ld "
"p1=%ld "
"p2=%ld ",
(long) t->mode, 
(long) t->p1, 
(long) t->p2);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_video_capture (char *str, size_t size, int rw __attribute__ ((unused)), const struct video_capture *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "x=%lu "
"y=%lu "
"width=%lu "
"height=%lu "
"decimation=%lu "
"flags=",
(unsigned long) t->x, 
(unsigned long) t->y, 
(unsigned long) t->width, 
(unsigned long) t->height, 
(unsigned long) t->decimation);
n = MIN (n, size);
n += snprint_symbolic (str + n, size - n, 2, t->flags,
"ODD", (unsigned long) VIDEO_CAPTURE_ODD,
"EVEN", (unsigned long) VIDEO_CAPTURE_EVEN,
(void *) 0);
n += strlcpy (str + n, " ", size - n);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_video_code (char *str, size_t size, int rw __attribute__ ((unused)), const struct video_code *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "loadwhat=\"%.*s\" "
"datasize=%ld "
"data=%p ",
16, (const char *) t->loadwhat, 
(long) t->datasize, 
(const void *) t->data);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_video_unit (char *str, size_t size, int rw __attribute__ ((unused)), const struct video_unit *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "video=%ld "
"vbi=%ld "
"radio=%ld "
"audio=%ld "
"teletext=%ld ",
(long) t->video, 
(long) t->vbi, 
(long) t->radio, 
(long) t->audio, 
(long) t->teletext);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_video_window (char *str, size_t size, int rw __attribute__ ((unused)), const struct video_window *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "x=%lu "
"y=%lu "
"width=%lu "
"height=%lu "
"chromakey=%lu "
"flags=",
(unsigned long) t->x, 
(unsigned long) t->y, 
(unsigned long) t->width, 
(unsigned long) t->height, 
(unsigned long) t->chromakey);
n = MIN (n, size);
n += snprint_symbolic (str + n, size - n, 2, t->flags,
"INTERLACE", (unsigned long) VIDEO_WINDOW_INTERLACE,
"CHROMAKEY", (unsigned long) VIDEO_WINDOW_CHROMAKEY,
(void *) 0);
n += snprintf (str + n, size - n, " clips=%p "
"clipcount=%ld ",
(const void *) t->clips, 
(long) t->clipcount);
n = MIN (n, size);
return n;
}

static size_t
snprint_symbol_video_palette_ (char *str, size_t size, int rw __attribute__ ((unused)), unsigned long value)
{
return snprint_symbolic (str, size, 0, value,
"GREY", (unsigned long) VIDEO_PALETTE_GREY,
"HI240", (unsigned long) VIDEO_PALETTE_HI240,
"RGB565", (unsigned long) VIDEO_PALETTE_RGB565,
"RGB24", (unsigned long) VIDEO_PALETTE_RGB24,
"RGB32", (unsigned long) VIDEO_PALETTE_RGB32,
"RGB555", (unsigned long) VIDEO_PALETTE_RGB555,
"YUV422", (unsigned long) VIDEO_PALETTE_YUV422,
"YUYV", (unsigned long) VIDEO_PALETTE_YUYV,
"UYVY", (unsigned long) VIDEO_PALETTE_UYVY,
"YUV420", (unsigned long) VIDEO_PALETTE_YUV420,
"YUV411", (unsigned long) VIDEO_PALETTE_YUV411,
"RAW", (unsigned long) VIDEO_PALETTE_RAW,
"YUV422P", (unsigned long) VIDEO_PALETTE_YUV422P,
"YUV411P", (unsigned long) VIDEO_PALETTE_YUV411P,
"YUV420P", (unsigned long) VIDEO_PALETTE_YUV420P,
"YUV410P", (unsigned long) VIDEO_PALETTE_YUV410P,
"PLANAR", (unsigned long) VIDEO_PALETTE_PLANAR,
"COMPONENT", (unsigned long) VIDEO_PALETTE_COMPONENT,
(void *) 0);
}

static size_t
snprint_struct_video_picture (char *str, size_t size, int rw __attribute__ ((unused)), const struct video_picture *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "brightness=%lu "
"hue=%lu "
"colour=%lu "
"contrast=%lu "
"whiteness=%lu "
"depth=%lu "
"palette=",
(unsigned long) t->brightness, 
(unsigned long) t->hue, 
(unsigned long) t->colour, 
(unsigned long) t->contrast, 
(unsigned long) t->whiteness, 
(unsigned long) t->depth);
n = MIN (n, size);
n += snprint_symbol_video_palette_ (str + n, size - n, rw, t->palette);
n += strlcpy (str + n, " ", size - n);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_video_mmap (char *str, size_t size, int rw __attribute__ ((unused)), const struct video_mmap *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "frame=%lu "
"height=%ld "
"width=%ld "
"format=%lu ",
(unsigned long) t->frame, 
(long) t->height, 
(long) t->width, 
(unsigned long) t->format);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_video_buffer (char *str, size_t size, int rw __attribute__ ((unused)), const struct video_buffer *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "base=%p "
"height=%ld "
"width=%ld "
"depth=%ld "
"bytesperline=%ld ",
(const void *) t->base, 
(long) t->height, 
(long) t->width, 
(long) t->depth, 
(long) t->bytesperline);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_video_mbuf (char *str, size_t size, int rw __attribute__ ((unused)), const struct video_mbuf *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "size=%ld "
"frames=%ld "
"offsets[]=? ",
(long) t->size, 
(long) t->frames);
n = MIN (n, size);
return n;
}

static size_t
snprint_symbol_video_audio_ (char *str, size_t size, int rw __attribute__ ((unused)), unsigned long value)
{
return snprint_symbolic (str, size, 2, value,
"MUTE", (unsigned long) VIDEO_AUDIO_MUTE,
"MUTABLE", (unsigned long) VIDEO_AUDIO_MUTABLE,
"VOLUME", (unsigned long) VIDEO_AUDIO_VOLUME,
"BASS", (unsigned long) VIDEO_AUDIO_BASS,
"TREBLE", (unsigned long) VIDEO_AUDIO_TREBLE,
(void *) 0);
}

static size_t
snprint_symbol_video_sound_ (char *str, size_t size, int rw __attribute__ ((unused)), unsigned long value)
{
return snprint_symbolic (str, size, 0, value,
"MONO", (unsigned long) VIDEO_SOUND_MONO,
"STEREO", (unsigned long) VIDEO_SOUND_STEREO,
"LANG1", (unsigned long) VIDEO_SOUND_LANG1,
"LANG2", (unsigned long) VIDEO_SOUND_LANG2,
(void *) 0);
}

static size_t
snprint_struct_video_audio (char *str, size_t size, int rw __attribute__ ((unused)), const struct video_audio *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "audio=%ld "
"volume=%lu "
"bass=%lu "
"treble=%lu "
"flags=",
(long) t->audio, 
(unsigned long) t->volume, 
(unsigned long) t->bass, 
(unsigned long) t->treble);
n = MIN (n, size);
n += snprint_symbol_video_audio_ (str + n, size - n, rw, t->flags);
n += snprintf (str + n, size - n, " name=\"%.*s\" "
"mode=",
16, (const char *) t->name);
n = MIN (n, size);
n += snprint_symbol_video_sound_ (str + n, size - n, rw, t->mode);
n += snprintf (str + n, size - n, " balance=%lu "
"step=%lu ",
(unsigned long) t->balance, 
(unsigned long) t->step);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_vbi_format (char *str, size_t size, int rw __attribute__ ((unused)), const struct vbi_format *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "sampling_rate=%lu "
"samples_per_line=%lu "
"sample_format=%lu "
"start[]=? "
"count[]=? "
"flags=",
(unsigned long) t->sampling_rate, 
(unsigned long) t->samples_per_line, 
(unsigned long) t->sample_format);
n = MIN (n, size);
n += snprint_symbolic (str + n, size - n, 2, t->flags,
"UNSYNC", (unsigned long) VBI_UNSYNC,
"INTERLACED", (unsigned long) VBI_INTERLACED,
(void *) 0);
n += strlcpy (str + n, " ", size - n);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_video_info (char *str, size_t size, int rw __attribute__ ((unused)), const struct video_info *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "frame_count=%lu "
"h_size=%lu "
"v_size=%lu "
"smpte_timecode=%lu "
"picture_type=%lu "
"temporal_reference=%lu "
"user_data[]=? ",
(unsigned long) t->frame_count, 
(unsigned long) t->h_size, 
(unsigned long) t->v_size, 
(unsigned long) t->smpte_timecode, 
(unsigned long) t->picture_type, 
(unsigned long) t->temporal_reference);
n = MIN (n, size);
return n;
}

static size_t
snprint_struct_video_channel (char *str, size_t size, int rw __attribute__ ((unused)), const struct video_channel *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "channel=%ld ",
(long) t->channel);
n = MIN (n, size);
if (1 & rw) {
n += snprintf (str + n, size - n, "name=\"%.*s\" "
"tuners=%ld "
"flags=",
32, (const char *) t->name, 
(long) t->tuners);
n = MIN (n, size);
n += snprint_symbolic (str + n, size - n, 2, t->flags,
"TUNER", (unsigned long) VIDEO_VC_TUNER,
"AUDIO", (unsigned long) VIDEO_VC_AUDIO,
(void *) 0);
n += strlcpy (str + n, " type=", size - n);
n = MIN (n, size);
n += snprint_symbolic (str + n, size - n, 0, t->type,
"TV", (unsigned long) VIDEO_TYPE_TV,
"CAMERA", (unsigned long) VIDEO_TYPE_CAMERA,
(void *) 0);
n += strlcpy (str + n, " ", size - n);
n = MIN (n, size);
}
n += snprintf (str + n, size - n, "norm=%lu ",
(unsigned long) t->norm);
n = MIN (n, size);
return n;
}

static size_t
snprint_symbol_video_tuner_ (char *str, size_t size, int rw __attribute__ ((unused)), unsigned long value)
{
return snprint_symbolic (str, size, 2, value,
"PAL", (unsigned long) VIDEO_TUNER_PAL,
"NTSC", (unsigned long) VIDEO_TUNER_NTSC,
"SECAM", (unsigned long) VIDEO_TUNER_SECAM,
"LOW", (unsigned long) VIDEO_TUNER_LOW,
"NORM", (unsigned long) VIDEO_TUNER_NORM,
"STEREO_ON", (unsigned long) VIDEO_TUNER_STEREO_ON,
"RDS_ON", (unsigned long) VIDEO_TUNER_RDS_ON,
"MBS_ON", (unsigned long) VIDEO_TUNER_MBS_ON,
(void *) 0);
}

static size_t
snprint_symbol_video_mode_ (char *str, size_t size, int rw __attribute__ ((unused)), unsigned long value)
{
return snprint_symbolic (str, size, 0, value,
"PAL", (unsigned long) VIDEO_MODE_PAL,
"NTSC", (unsigned long) VIDEO_MODE_NTSC,
"SECAM", (unsigned long) VIDEO_MODE_SECAM,
"AUTO", (unsigned long) VIDEO_MODE_AUTO,
(void *) 0);
}

static size_t
snprint_struct_video_tuner (char *str, size_t size, int rw __attribute__ ((unused)), const struct video_tuner *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "tuner=%ld ",
(long) t->tuner);
n = MIN (n, size);
if (1 & rw) {
n += snprintf (str + n, size - n, "name=\"%.*s\" "
"rangelow=%lu "
"rangehigh=%lu "
"flags=",
32, (const char *) t->name, 
(unsigned long) t->rangelow, 
(unsigned long) t->rangehigh);
n = MIN (n, size);
n += snprint_symbol_video_tuner_ (str + n, size - n, rw, t->flags);
n += strlcpy (str + n, " ", size - n);
n = MIN (n, size);
}
n += strlcpy (str + n, "mode=", size - n);
n = MIN (n, size);
n += snprint_symbol_video_mode_ (str + n, size - n, rw, t->mode);
n += strlcpy (str + n, " ", size - n);
n = MIN (n, size);
if (1 & rw) {
n += snprintf (str + n, size - n, "signal=%lu ",
(unsigned long) t->signal);
n = MIN (n, size);
}
return n;
}

static size_t
snprint_struct_video_key (char *str, size_t size, int rw __attribute__ ((unused)), const struct video_key *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "key[]=? "
"flags=0x%lx ",
(unsigned long) t->flags);
n = MIN (n, size);
return n;
}

static size_t
snprint_symbol_vid_type_ (char *str, size_t size, int rw __attribute__ ((unused)), unsigned long value)
{
return snprint_symbolic (str, size, 0, value,
"CAPTURE", (unsigned long) VID_TYPE_CAPTURE,
"TUNER", (unsigned long) VID_TYPE_TUNER,
"TELETEXT", (unsigned long) VID_TYPE_TELETEXT,
"OVERLAY", (unsigned long) VID_TYPE_OVERLAY,
"CHROMAKEY", (unsigned long) VID_TYPE_CHROMAKEY,
"CLIPPING", (unsigned long) VID_TYPE_CLIPPING,
"FRAMERAM", (unsigned long) VID_TYPE_FRAMERAM,
"SCALES", (unsigned long) VID_TYPE_SCALES,
"MONOCHROME", (unsigned long) VID_TYPE_MONOCHROME,
"SUBCAPTURE", (unsigned long) VID_TYPE_SUBCAPTURE,
"MPEG_DECODER", (unsigned long) VID_TYPE_MPEG_DECODER,
"MPEG_ENCODER", (unsigned long) VID_TYPE_MPEG_ENCODER,
"MJPEG_DECODER", (unsigned long) VID_TYPE_MJPEG_DECODER,
"MJPEG_ENCODER", (unsigned long) VID_TYPE_MJPEG_ENCODER,
(void *) 0);
}

static size_t
snprint_struct_video_capability (char *str, size_t size, int rw __attribute__ ((unused)), const struct video_capability *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "name=\"%.*s\" "
"type=",
32, (const char *) t->name);
n = MIN (n, size);
n += snprint_symbol_vid_type_ (str + n, size - n, rw, t->type);
n += snprintf (str + n, size - n, " channels=%ld "
"audios=%ld "
"maxwidth=%ld "
"maxheight=%ld "
"minwidth=%ld "
"minheight=%ld ",
(long) t->channels, 
(long) t->audios, 
(long) t->maxwidth, 
(long) t->maxheight, 
(long) t->minwidth, 
(long) t->minheight);
n = MIN (n, size);
return n;
}

static size_t
snprint_ioctl_arg (char *str, size_t size, unsigned int cmd, int rw, void *arg)
{
size_t n;
assert (size > 0);
switch (cmd) {
case VIDIOCSPLAYMODE:
if (!arg) { return strlcpy (str, "VIDIOCSPLAYMODE", size); }
 return snprint_struct_video_play_mode (str, size, rw, arg);
case VIDIOCGCAPTURE:
if (!arg) { return strlcpy (str, "VIDIOCGCAPTURE", size); }
case VIDIOCSCAPTURE:
if (!arg) { return strlcpy (str, "VIDIOCSCAPTURE", size); }
 return snprint_struct_video_capture (str, size, rw, arg);
case VIDIOCCAPTURE:
if (!arg) { return strlcpy (str, "VIDIOCCAPTURE", size); }
case VIDIOCSYNC:
if (!arg) { return strlcpy (str, "VIDIOCSYNC", size); }
case VIDIOCSWRITEMODE:
if (!arg) { return strlcpy (str, "VIDIOCSWRITEMODE", size); }
 n = snprintf (str, size, "%ld", (long) * (int *) arg);
return MIN (size - 1, n);
case VIDIOCSMICROCODE:
if (!arg) { return strlcpy (str, "VIDIOCSMICROCODE", size); }
 return snprint_struct_video_code (str, size, rw, arg);
case VIDIOCGFREQ:
if (!arg) { return strlcpy (str, "VIDIOCGFREQ", size); }
case VIDIOCSFREQ:
if (!arg) { return strlcpy (str, "VIDIOCSFREQ", size); }
 snprintf (str, size, "%lu", (unsigned long) * (unsigned long *) arg);
break;
case VIDIOCGUNIT:
if (!arg) { return strlcpy (str, "VIDIOCGUNIT", size); }
 return snprint_struct_video_unit (str, size, rw, arg);
case VIDIOCGWIN:
if (!arg) { return strlcpy (str, "VIDIOCGWIN", size); }
case VIDIOCSWIN:
if (!arg) { return strlcpy (str, "VIDIOCSWIN", size); }
 return snprint_struct_video_window (str, size, rw, arg);
case VIDIOCGPICT:
if (!arg) { return strlcpy (str, "VIDIOCGPICT", size); }
case VIDIOCSPICT:
if (!arg) { return strlcpy (str, "VIDIOCSPICT", size); }
 return snprint_struct_video_picture (str, size, rw, arg);
case VIDIOCMCAPTURE:
if (!arg) { return strlcpy (str, "VIDIOCMCAPTURE", size); }
 return snprint_struct_video_mmap (str, size, rw, arg);
case VIDIOCGFBUF:
if (!arg) { return strlcpy (str, "VIDIOCGFBUF", size); }
case VIDIOCSFBUF:
if (!arg) { return strlcpy (str, "VIDIOCSFBUF", size); }
 return snprint_struct_video_buffer (str, size, rw, arg);
case VIDIOCGMBUF:
if (!arg) { return strlcpy (str, "VIDIOCGMBUF", size); }
 return snprint_struct_video_mbuf (str, size, rw, arg);
case VIDIOCGAUDIO:
if (!arg) { return strlcpy (str, "VIDIOCGAUDIO", size); }
case VIDIOCSAUDIO:
if (!arg) { return strlcpy (str, "VIDIOCSAUDIO", size); }
 return snprint_struct_video_audio (str, size, rw, arg);
case VIDIOCGVBIFMT:
if (!arg) { return strlcpy (str, "VIDIOCGVBIFMT", size); }
case VIDIOCSVBIFMT:
if (!arg) { return strlcpy (str, "VIDIOCSVBIFMT", size); }
 return snprint_struct_vbi_format (str, size, rw, arg);
case VIDIOCGPLAYINFO:
if (!arg) { return strlcpy (str, "VIDIOCGPLAYINFO", size); }
 return snprint_struct_video_info (str, size, rw, arg);
case VIDIOCGCHAN:
if (!arg) { return strlcpy (str, "VIDIOCGCHAN", size); }
case VIDIOCSCHAN:
if (!arg) { return strlcpy (str, "VIDIOCSCHAN", size); }
 return snprint_struct_video_channel (str, size, rw, arg);
case VIDIOCGTUNER:
if (!arg) { return strlcpy (str, "VIDIOCGTUNER", size); }
case VIDIOCSTUNER:
if (!arg) { return strlcpy (str, "VIDIOCSTUNER", size); }
 return snprint_struct_video_tuner (str, size, rw, arg);
case VIDIOCKEY:
if (!arg) { return strlcpy (str, "VIDIOCKEY", size); }
 return snprint_struct_video_key (str, size, rw, arg);
case VIDIOCGCAP:
if (!arg) { return strlcpy (str, "VIDIOCGCAP", size); }
 return snprint_struct_video_capability (str, size, rw, arg);
default:
if (!arg) { return snprint_unknown_ioctl (str, size, cmd, arg); }
str[0] = 0; return 0;
}
return 0; }

static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCGCAP (struct video_capability *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCGCHAN (struct video_channel *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCSCHAN (const struct video_channel *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCGTUNER (struct video_tuner *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCSTUNER (const struct video_tuner *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCGPICT (struct video_picture *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCSPICT (const struct video_picture *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCCAPTURE (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCGWIN (struct video_window *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCSWIN (const struct video_window *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCGFBUF (struct video_buffer *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCSFBUF (const struct video_buffer *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCKEY (struct video_key *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCGFREQ (unsigned long *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCSFREQ (const unsigned long *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCGAUDIO (struct video_audio *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCSAUDIO (const struct video_audio *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCSYNC (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCMCAPTURE (const struct video_mmap *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCGMBUF (struct video_mbuf *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCGUNIT (struct video_unit *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCGCAPTURE (struct video_capture *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCSCAPTURE (const struct video_capture *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCSPLAYMODE (const struct video_play_mode *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCSWRITEMODE (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCGPLAYINFO (struct video_info *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCSMICROCODE (const struct video_code *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCGVBIFMT (struct vbi_format *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCSVBIFMT (const struct vbi_format *arg __attribute__ ((unused))) {}

