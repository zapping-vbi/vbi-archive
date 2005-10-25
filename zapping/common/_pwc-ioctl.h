/* Generated file, do not edit! */

#include <stdio.h>
#include "common/device.h"

#ifndef __GNUC__
#undef __attribute__
#define __attribute__(x)
#endif

static void
fprint_struct_pwc_serial (FILE *fp, int rw __attribute__ ((unused)), const struct pwc_serial *t)
{
fprintf (fp, "serial=\"%.*s\" ",
30, (const char *) t->serial);
}

static void
fprint_struct_pwc_probe (FILE *fp, int rw __attribute__ ((unused)), const struct pwc_probe *t)
{
fprintf (fp, "name=\"%.*s\" "
"type=%ld ",
32, (const char *) t->name, 
(long) t->type);
}

static void
fprint_struct_pwc_wb_speed (FILE *fp, int rw __attribute__ ((unused)), const struct pwc_wb_speed *t)
{
fprintf (fp, "control_speed=%ld "
"control_delay=%ld ",
(long) t->control_speed, 
(long) t->control_delay);
}

static void
fprint_symbol_pwc_wb_ (FILE *fp, int rw __attribute__ ((unused)), unsigned long value)
{
fprint_symbolic (fp, 0, value,
"INDOOR", (unsigned long) PWC_WB_INDOOR,
"OUTDOOR", (unsigned long) PWC_WB_OUTDOOR,
"FL", (unsigned long) PWC_WB_FL,
"MANUAL", (unsigned long) PWC_WB_MANUAL,
"AUTO", (unsigned long) PWC_WB_AUTO,
0);
}

static void
fprint_struct_pwc_whitebalance (FILE *fp, int rw __attribute__ ((unused)), const struct pwc_whitebalance *t)
{
fputs ("mode=", fp);
fprint_symbol_pwc_wb_ (fp, rw, t->mode);
fprintf (fp, " manual_red=%ld "
"manual_blue=%ld "
"read_red=%ld "
"read_blue=%ld ",
(long) t->manual_red, 
(long) t->manual_blue, 
(long) t->read_red, 
(long) t->read_blue);
}

static void
fprint_struct_pwc_mpt_status (FILE *fp, int rw __attribute__ ((unused)), const struct pwc_mpt_status *t)
{
fprintf (fp, "status=%ld "
"time_pan=%ld "
"time_tilt=%ld ",
(long) t->status, 
(long) t->time_pan, 
(long) t->time_tilt);
}

static void
fprint_struct_pwc_video_command (FILE *fp, int rw __attribute__ ((unused)), const struct pwc_video_command *t)
{
fprintf (fp, "type=%ld "
"release=%ld "
"size=%ld "
"alternate=%ld "
"command_len=%ld "
"command_buf[]=? "
"bandlength=%ld "
"frame_size=%ld ",
(long) t->type, 
(long) t->release, 
(long) t->size, 
(long) t->alternate, 
(long) t->command_len, 

(long) t->bandlength, 
(long) t->frame_size);
}

static void
fprint_struct_pwc_leds (FILE *fp, int rw __attribute__ ((unused)), const struct pwc_leds *t)
{
fprintf (fp, "led_on=%ld "
"led_off=%ld ",
(long) t->led_on, 
(long) t->led_off);
}

static void
fprint_struct_pwc_imagesize (FILE *fp, int rw __attribute__ ((unused)), const struct pwc_imagesize *t)
{
fprintf (fp, "width=%ld "
"height=%ld ",
(long) t->width, 
(long) t->height);
}

static void
fprint_struct_pwc_mpt_angles (FILE *fp, int rw __attribute__ ((unused)), const struct pwc_mpt_angles *t)
{
fprintf (fp, "absolute=%ld "
"pan=%ld "
"tilt=%ld ",
(long) t->absolute, 
(long) t->pan, 
(long) t->tilt);
}

static void
fprint_struct_pwc_mpt_range (FILE *fp, int rw __attribute__ ((unused)), const struct pwc_mpt_range *t)
{
fprintf (fp, "pan_min=%ld "
"pan_max=%ld "
"tilt_min=%ld "
"tilt_max=%ld ",
(long) t->pan_min, 
(long) t->pan_max, 
(long) t->tilt_min, 
(long) t->tilt_max);
}

static void
fprint_pwc_ioctl_arg (FILE *fp, unsigned int cmd, int rw, void *arg)
{
switch (cmd) {
case VIDIOCPWCGSERIAL:
if (!arg) { fputs ("VIDIOCPWCGSERIAL", fp); return; }
 fprint_struct_pwc_serial (fp, rw, arg);
break;
case VIDIOCPWCPROBE:
if (!arg) { fputs ("VIDIOCPWCPROBE", fp); return; }
 fprint_struct_pwc_probe (fp, rw, arg);
break;
case VIDIOCPWCSAWBSPEED:
if (!arg) { fputs ("VIDIOCPWCSAWBSPEED", fp); return; }
case VIDIOCPWCGAWBSPEED:
if (!arg) { fputs ("VIDIOCPWCGAWBSPEED", fp); return; }
 fprint_struct_pwc_wb_speed (fp, rw, arg);
break;
case VIDIOCPWCSCQUAL:
if (!arg) { fputs ("VIDIOCPWCSCQUAL", fp); return; }
case VIDIOCPWCGCQUAL:
if (!arg) { fputs ("VIDIOCPWCGCQUAL", fp); return; }
case VIDIOCPWCSAGC:
if (!arg) { fputs ("VIDIOCPWCSAGC", fp); return; }
case VIDIOCPWCGAGC:
if (!arg) { fputs ("VIDIOCPWCGAGC", fp); return; }
case VIDIOCPWCSSHUTTER:
if (!arg) { fputs ("VIDIOCPWCSSHUTTER", fp); return; }
case VIDIOCPWCSCONTOUR:
if (!arg) { fputs ("VIDIOCPWCSCONTOUR", fp); return; }
case VIDIOCPWCGCONTOUR:
if (!arg) { fputs ("VIDIOCPWCGCONTOUR", fp); return; }
case VIDIOCPWCSBACKLIGHT:
if (!arg) { fputs ("VIDIOCPWCSBACKLIGHT", fp); return; }
case VIDIOCPWCGBACKLIGHT:
if (!arg) { fputs ("VIDIOCPWCGBACKLIGHT", fp); return; }
case VIDIOCPWCSFLICKER:
if (!arg) { fputs ("VIDIOCPWCSFLICKER", fp); return; }
case VIDIOCPWCGFLICKER:
if (!arg) { fputs ("VIDIOCPWCGFLICKER", fp); return; }
case VIDIOCPWCSDYNNOISE:
if (!arg) { fputs ("VIDIOCPWCSDYNNOISE", fp); return; }
case VIDIOCPWCGDYNNOISE:
if (!arg) { fputs ("VIDIOCPWCGDYNNOISE", fp); return; }
case VIDIOCPWCMPTRESET:
if (!arg) { fputs ("VIDIOCPWCMPTRESET", fp); return; }
 fprintf (fp, "%ld", (long) * (int *) arg);
break;
case VIDIOCPWCSAWB:
if (!arg) { fputs ("VIDIOCPWCSAWB", fp); return; }
case VIDIOCPWCGAWB:
if (!arg) { fputs ("VIDIOCPWCGAWB", fp); return; }
 fprint_struct_pwc_whitebalance (fp, rw, arg);
break;
case VIDIOCPWCMPTSTATUS:
if (!arg) { fputs ("VIDIOCPWCMPTSTATUS", fp); return; }
 fprint_struct_pwc_mpt_status (fp, rw, arg);
break;
case VIDIOCPWCGVIDCMD:
if (!arg) { fputs ("VIDIOCPWCGVIDCMD", fp); return; }
 fprint_struct_pwc_video_command (fp, rw, arg);
break;
case VIDIOCPWCSLED:
if (!arg) { fputs ("VIDIOCPWCSLED", fp); return; }
case VIDIOCPWCGLED:
if (!arg) { fputs ("VIDIOCPWCGLED", fp); return; }
 fprint_struct_pwc_leds (fp, rw, arg);
break;
case VIDIOCPWCGREALSIZE:
if (!arg) { fputs ("VIDIOCPWCGREALSIZE", fp); return; }
 fprint_struct_pwc_imagesize (fp, rw, arg);
break;
case VIDIOCPWCMPTSANGLE:
if (!arg) { fputs ("VIDIOCPWCMPTSANGLE", fp); return; }
case VIDIOCPWCMPTGANGLE:
if (!arg) { fputs ("VIDIOCPWCMPTGANGLE", fp); return; }
 fprint_struct_pwc_mpt_angles (fp, rw, arg);
break;
case VIDIOCPWCMPTGRANGE:
if (!arg) { fputs ("VIDIOCPWCMPTGRANGE", fp); return; }
 fprint_struct_pwc_mpt_range (fp, rw, arg);
break;
	default:
		if (!arg) { fprint_unknown_ioctl (fp, cmd, arg); return; }
		break;
	}
}

static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCPWCSCQUAL (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCPWCGCQUAL (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCPWCGSERIAL (struct pwc_serial *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCPWCPROBE (struct pwc_probe *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCPWCSAGC (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCPWCGAGC (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCPWCSSHUTTER (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCPWCSAWB (const struct pwc_whitebalance *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCPWCGAWB (struct pwc_whitebalance *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCPWCSAWBSPEED (const struct pwc_wb_speed *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCPWCGAWBSPEED (struct pwc_wb_speed *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCPWCSLED (const struct pwc_leds *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCPWCGLED (struct pwc_leds *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCPWCSCONTOUR (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCPWCGCONTOUR (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCPWCSBACKLIGHT (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCPWCGBACKLIGHT (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCPWCSFLICKER (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCPWCGFLICKER (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCPWCSDYNNOISE (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCPWCGDYNNOISE (int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCPWCGREALSIZE (struct pwc_imagesize *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCPWCMPTRESET (const int *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCPWCMPTGRANGE (struct pwc_mpt_range *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCPWCMPTSANGLE (const struct pwc_mpt_angles *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCPWCMPTGANGLE (struct pwc_mpt_angles *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCPWCMPTSTATUS (struct pwc_mpt_status *arg __attribute__ ((unused))) {}
static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOCPWCGVIDCMD (struct pwc_video_command *arg __attribute__ ((unused))) {}

