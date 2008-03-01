/* Generated file, do not edit! */

#include <stdio.h>
#include "io.h"

#ifndef __GNUC__
#undef __attribute__
#define __attribute__(x)
#endif

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
n += snprint_symbolic (str + n, size - n, 2, t->flags,
(void *) 0);
n += strlcpy (str + n, " ", size - n);
n = MIN (n, size);
}
n += strlcpy (str + n, "mode=", size - n);
n = MIN (n, size);
n += snprint_symbolic (str + n, size - n, 0, t->mode,
(void *) 0);
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
(void *) 0);
n += strlcpy (str + n, " type=", size - n);
n = MIN (n, size);
n += snprint_symbolic (str + n, size - n, 0, t->type,
"TV", (unsigned long) VIDEO_TYPE_TV,
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
snprint_struct_video_capability (char *str, size_t size, int rw __attribute__ ((unused)), const struct video_capability *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "name=\"%.*s\" "
"type=",
32, (const char *) t->name);
n = MIN (n, size);
n += snprint_symbolic (str + n, size - n, 0, t->type,
"CAPTURE", (unsigned long) VID_TYPE_CAPTURE,
"TELETEXT", (unsigned long) VID_TYPE_TELETEXT,
(void *) 0);
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
case VIDIOCGVBIFMT:
if (!arg) { return strlcpy (str, "VIDIOCGVBIFMT", size); }
case VIDIOCSVBIFMT:
if (!arg) { return strlcpy (str, "VIDIOCSVBIFMT", size); }
 return snprint_struct_vbi_format (str, size, rw, arg);
case VIDIOCGFREQ:
if (!arg) { return strlcpy (str, "VIDIOCGFREQ", size); }
case VIDIOCSFREQ:
if (!arg) { return strlcpy (str, "VIDIOCSFREQ", size); }
 snprintf (str, size, "%lu", (unsigned long) * (unsigned long *) arg);
break;
case VIDIOCGUNIT:
if (!arg) { return strlcpy (str, "VIDIOCGUNIT", size); }
 return snprint_struct_video_unit (str, size, rw, arg);
case VIDIOCGTUNER:
if (!arg) { return strlcpy (str, "VIDIOCGTUNER", size); }
case VIDIOCSTUNER:
if (!arg) { return strlcpy (str, "VIDIOCSTUNER", size); }
 return snprint_struct_video_tuner (str, size, rw, arg);
case VIDIOCGCHAN:
if (!arg) { return strlcpy (str, "VIDIOCGCHAN", size); }
case VIDIOCSCHAN:
if (!arg) { return strlcpy (str, "VIDIOCSCHAN", size); }
 return snprint_struct_video_channel (str, size, rw, arg);
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
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCGFREQ (unsigned long *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCSFREQ (const unsigned long *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCGUNIT (struct video_unit *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCGVBIFMT (struct vbi_format *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCSVBIFMT (const struct vbi_format *arg __attribute__ ((unused))) {}

