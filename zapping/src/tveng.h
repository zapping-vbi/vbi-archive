/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000 Iñaki García Etxebarria
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

#ifndef __TVENG_H__
#define __TVENG_H__

#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include <inttypes.h>
#include <assert.h>

#ifndef DISABLE_X_EXTENSIONS
#ifdef HAVE_XV_EXTENSION
#ifndef USE_XV /* avoid redefinition */
#define USE_XV 1
#endif
#endif
#endif

/* We need video extensions (DGA) */
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xfuncs.h>
#ifdef USE_XV
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#endif

/* The video device capabilities flags */
#define TVENG_CAPS_CAPTURE 1 /* Can capture to memory */
#define TVENG_CAPS_TUNER (1 << 1) /* Has some tuner */
#define TVENG_CAPS_TELETEXT (1 << 2) /* Has teletext */
#define TVENG_CAPS_OVERLAY (1 << 3) /* Can overlay to the framebuffer */
#define TVENG_CAPS_CHROMAKEY (1 << 4) /* Overlay chromakeyed */
#define TVENG_CAPS_CLIPPING (1 << 5) /* Overlay clipping supported */
#define TVENG_CAPS_FRAMERAM (1 << 6) /* Overlay overwrites framebuffer
				      mem */
#define TVENG_CAPS_SCALES (1 << 7) /* HW image scaling supported */
#define TVENG_CAPS_MONOCHROME (1 << 8) /* greyscale only */
#define TVENG_CAPS_SUBCAPTURE (1 << 9) /* Can capture only part of the
					image */

/* The valid modes for opening the video device */
enum tveng_attach_mode
{
  /*
    Attachs the device so you can only control it, not read
    frames. This way you can do more than one opens per device. This
    will only work if V4L2 is present, otherwise this call means the
    same as TVENG_ATTACH_READ
  */
  TVENG_ATTACH_CONTROL,
  /*
    Attachs the device so you can read data from it and control
    it. You cannot attach the same device twice with this type of
    attachment.
  */
  TVENG_ATTACH_READ,
  /*
    Attachs the device to a XVideo virtual device, use this mode if
    you would prefer the X server to take care of the video. This mode
    only supports preview mode, and falls back to the previous attach
    modes if XVideo isn't present or it isn't functional.
  */
  TVENG_ATTACH_XV
};

/* The capture structure */
struct tveng_caps{
  char name[32]; /* canonical name for this interface */
  int flags; /* OR'ed flags, see the above #defines */
  int channels; /* Number of radio/tv channels */
  int audios; /* Number of audio devices */
  int maxwidth, maxheight; /* Maximum capture dimensions */
  int minwidth, minheight; /* minimum capture dimensions */
};

typedef struct _tveng_device_info tveng_device_info;







typedef int tv_bool;

#undef TRUE
#define TRUE 1
#undef FALSE
#define FALSE 0

/*
 *  Callbacks
 */

typedef struct _tv_callback tv_callback;

typedef void
tv_callback_fn			(void *			object,
				 void *			user_data);

extern tv_callback *
tv_callback_add			(tv_callback **		list,
				 tv_callback_fn *	notify,
				 tv_callback_fn *	destroy,
				 void *			user_data);
extern void
tv_callback_remove		(tv_callback *		cb);
extern void
tv_callback_destroy		(void *			object,
				 tv_callback **		list);
extern void
tv_callback_block		(tv_callback *		cb);
extern void
tv_callback_unblock		(tv_callback *		cb);

#define TV_CALLBACK_BLOCK(cb, statement)				\
do {									\
	tv_callback_block (cb);						\
	statement;							\
	tv_callback_unblock (cb);					\
} while (0)

/*
 *  Devices
 */

typedef struct _tv_device_node tv_device_node;

/* This is a model (in the MVC sense) for devices we encounter
   and want to present to the user to make a choice or whatever. */
struct _tv_device_node {
	tv_device_node *	next;

	char *			label;		/* localized */
	char *			bus;		/* localized */

	char *			driver;		/* localized */
	char *			version;	/* localized */

	char *			device;		/* interface specific, usually "/dev/some" */

	void *			user_data;	/* for tveng clients */

	void			(* destroy)(tv_device_node *, tv_bool restore);
};

typedef tv_device_node *
tv_device_node_filter_fn	(const char *		name);

extern tv_device_node *
tv_device_node_add		(tv_device_node **	list,
				 tv_device_node *	node);
extern tv_device_node *
tv_device_node_remove		(tv_device_node **	list,
				 tv_device_node *	node);
extern tv_device_node *
tv_device_node_find		(tv_device_node *	list,
				 const char *		device);
extern void
tv_device_node_delete		(tv_device_node **	list,
				 tv_device_node *	node,
				 tv_bool		restore);
static __inline__ void
tv_device_node_delete_list	(tv_device_node **	list)
{
	assert (list != NULL);

	while (*list)
		tv_device_node_delete (list, *list, FALSE);
}
extern tv_device_node *
tv_device_node_new		(const char *		label,
				 const char *		bus,
				 const char *		driver,
				 const char *		version,
				 const char *		device,
				 unsigned int		size);

/*
 *  Video lines (input or output)
 */

typedef enum {
	TV_VIDEO_LINE_TYPE_NONE,
	TV_VIDEO_LINE_TYPE_BASEBAND,		/* CVBS, Y/C, RGB */
	TV_VIDEO_LINE_TYPE_TUNER,
	TV_VIDEO_LINE_TYPE_SATELLITE
} tv_video_line_type;

typedef enum {
	TV_VIDEO_LINE_ID_NONE,
	TV_VIDEO_LINE_ID_UNKNOWN = TV_VIDEO_LINE_ID_NONE
} tv_video_line_id;

typedef struct _tv_video_line tv_video_line;

struct _tv_video_line {
	tv_video_line *		_next;
	void *			_parent;
	tv_callback *		_callback;

	char *			label;
	unsigned int		hash;

	tv_video_line_type	type;
	tv_video_line_id	id;

// tv_videostd_id ?

	union {
		struct {
			unsigned int		minimum;
			unsigned int		maximum;
			unsigned int		step;

			unsigned int		frequency;
		}			tuner;
	}			u;
};

static __inline__ tv_callback *
tv_video_line_add_callback	(tv_video_line *	line,
				 void			(* notify)(tv_video_line *, void *),
				 void			(* destroy)(tv_video_line *, void *),
				 void *			user_data)
{
	assert (line != NULL);

	return tv_callback_add (&line->_callback,
				(tv_callback_fn *) notify,
				(tv_callback_fn *) destroy,
				user_data);
}

/*
 *  Audio lines (input or output)
 */

typedef enum {
	TV_AUDIO_LINE_TYPE_NONE
} tv_audio_line_type;

typedef enum {
	TV_AUDIO_LINE_ID_NONE,
	TV_AUDIO_LINE_ID_UNKNOWN = TV_AUDIO_LINE_ID_NONE
} tv_audio_line_id;

typedef struct _tv_audio_line tv_audio_line;

struct _tv_audio_line {
	tv_audio_line *		_next;
	void *			_parent;
	tv_callback *		_callback;

	char *			label;
	unsigned int		hash;

	tv_audio_line_type	type;
	tv_audio_line_id	id;

	int			minimum;	/* volume */
	int			maximum;
	int			step;
	int			reset;

	/* A mixer input which can be routed to the ADC.
	   Only these are valid args for set_rec_line().
	   Meaningless for tv audio inputs. */
	unsigned		recordable	: 1;

	unsigned		stereo		: 1;

	/* Each audio line has a built-in volume control. Or at least that's
	   what we pretend, only mixers really do. Further we assume it
	   has a mute switch, independent of volume, emulated when not. These
	   values reflect the last known left (0) and right (1) volume and
	   mute state. */
	unsigned		muted		: 1;
	int			volume[2];
};

extern tv_bool
tv_audio_line_update		(tv_audio_line *	line);
extern tv_bool
tv_audio_line_get_volume	(tv_audio_line *	line,
				 int *			left,
				 int *			right);
extern tv_bool
tv_audio_line_set_volume	(tv_audio_line *	line,
				 int			left,
				 int			right);
extern tv_bool
tv_audio_line_get_mute		(tv_audio_line *	line,
				 tv_bool *		mute);
extern tv_bool
tv_audio_line_set_mute		(tv_audio_line *	line,
				 tv_bool		mute);

static __inline__ tv_callback *
tv_audio_line_add_callback	(tv_audio_line *	line,
				 void			(* notify)(tv_audio_line *, void *),
				 void			(* destroy)(tv_audio_line *, void *),
				 void *			user_data)
{
	assert (line != NULL);

	return tv_callback_add (&line->_callback,
				(tv_callback_fn *) notify,
				(tv_callback_fn *) destroy,
				user_data);
}

/*
 *  Video standards
 */

/* Copied from V4L2 2.5, keep order. */

typedef enum {
     	TV_VIDEOSTD_PAL_B = 0, /* none, unknown? FIXME */
	TV_VIDEOSTD_PAL_B1,
	TV_VIDEOSTD_PAL_G,
	TV_VIDEOSTD_PAL_H,

	TV_VIDEOSTD_PAL_I,
	TV_VIDEOSTD_PAL_D,
	TV_VIDEOSTD_PAL_D1,
	TV_VIDEOSTD_PAL_K,

	TV_VIDEOSTD_PAL_M = 8,
	TV_VIDEOSTD_PAL_N,
	TV_VIDEOSTD_PAL_NC,

	TV_VIDEOSTD_NTSC_M = 12,
	TV_VIDEOSTD_NTSC_M_JP,

	TV_VIDEOSTD_SECAM_B = 16,
	TV_VIDEOSTD_SECAM_D,
	TV_VIDEOSTD_SECAM_G,
	TV_VIDEOSTD_SECAM_H,

	TV_VIDEOSTD_SECAM_K,
	TV_VIDEOSTD_SECAM_K1,
	TV_VIDEOSTD_SECAM_L,

	TV_VIDEOSTD_CUSTOM_BEGIN = 32,
	TV_VIDEOSTD_CUSTOM_END = 64
} tv_videostd;

#define TV_MAX_VIDEOSTDS 64

typedef uint64_t tv_videostd_set;

#define TV_VIDEOSTD_SET(videostd) (((tv_videostd_set) 1) << (videostd))

#define TV_VIDEOSTD_SET_UNKNOWN 0
#define TV_VIDEOSTD_SET_EMPTY 0
#define TV_VIDEOSTD_SET_PAL_BG (+ TV_VIDEOSTD_SET (TV_VIDEOSTD_PAL_B)	\
				+ TV_VIDEOSTD_SET (TV_VIDEOSTD_PAL_B1)	\
				+ TV_VIDEOSTD_SET (TV_VIDEOSTD_PAL_G))
#define TV_VIDEOSTD_SET_PAL_DK (+ TV_VIDEOSTD_SET (TV_VIDEOSTD_PAL_D)	\
				+ TV_VIDEOSTD_SET (TV_VIDEOSTD_PAL_D1)	\
				+ TV_VIDEOSTD_SET (TV_VIDEOSTD_PAL_K))
#define TV_VIDEOSTD_SET_PAL    (+ TV_VIDEOSTD_SET_PAL_BG		\
				+ TV_VIDEOSTD_SET_PAL_DK		\
				+ TV_VIDEOSTD_SET (TV_VIDEOSTD_PAL_H)	\
				+ TV_VIDEOSTD_SET (TV_VIDEOSTD_PAL_I))
#define TV_VIDEOSTD_SET_NTSC   (+ TV_VIDEOSTD_SET (TV_VIDEOSTD_NTSC_M)	\
				+ TV_VIDEOSTD_SET (TV_VIDEOSTD_NTSC_M_JP))
#define TV_VIDEOSTD_SET_SECAM  (+ TV_VIDEOSTD_SET (TV_VIDEOSTD_SECAM_B)	\
				+ TV_VIDEOSTD_SET (TV_VIDEOSTD_SECAM_D)	\
				+ TV_VIDEOSTD_SET (TV_VIDEOSTD_SECAM_G)	\
				+ TV_VIDEOSTD_SET (TV_VIDEOSTD_SECAM_H)	\
				+ TV_VIDEOSTD_SET (TV_VIDEOSTD_SECAM_K)	\
				+ TV_VIDEOSTD_SET (TV_VIDEOSTD_SECAM_K1)\
				+ TV_VIDEOSTD_SET (TV_VIDEOSTD_SECAM_L))
#define TV_VIDEOSTD_SET_525_60 (+ TV_VIDEOSTD_SET (TV_VIDEOSTD_PAL_M)	\
				+ TV_VIDEOSTD_SET_NTSC)
#define TV_VIDEOSTD_SET_625_50 (+ TV_VIDEOSTD_SET_PAL			\
				+ TV_VIDEOSTD_SET (TV_VIDEOSTD_PAL_N)	\
				+ TV_VIDEOSTD_SET (TV_VIDEOSTD_PAL_NC)	\
				+ TV_VIDEOSTD_SET_SECAM)
#define TV_VIDEOSTD_SET_ALL    (+ TV_VIDEOSTD_SET_525_60		\
				+ TV_VIDEOSTD_SET_625_50)

#define TV_VIDEOSTD_SET_CUSTOM						\
	((~TV_VIDEOSTD_SET_EMPTY) << TV_VIDEOSTD_CUSTOM_BEGIN)

extern const char *
tv_videostd_name		(tv_videostd		videostd);

typedef struct _tv_video_standard tv_video_standard;

struct _tv_video_standard {
	tv_video_standard *	_next;
	void *			_parent;
	tv_callback *		_callback;

	char *			label;		/* localized */
	unsigned int		hash;

	/* Note multiple bits can be set if the driver doesn't know
	   exactly, or doesn't care about the difference (hardware
	   switches automatically, difference not applicable to
	   baseband input, etc). */
	tv_videostd_set		videostd_set;

	/* Nominal frame size, e.g. 640 x 480, assuming square
	   pixel sampling. */
	unsigned int		frame_width;
	unsigned int		frame_height;

	/* Nominal frame rate, either 25 or 30000 / 1001 Hz. */
	double			frame_rate;
};

static __inline__ tv_callback *
tv_video_standard_add_callback	(tv_video_standard *	standard,
				 void			(* destroy)(tv_video_standard *, void *),
				 void *			user_data)
{
	assert (standard != NULL);

	return tv_callback_add (&standard->_callback,
				NULL, /* video standards will not change */
				(tv_callback_fn *) destroy,
				user_data);
}

extern tv_callback *
tv_add_audio_callback		(tveng_device_info *	info,
				 void			(* notify)(tveng_device_info *, void *),
				 void			(* destroy)(tveng_device_info *, void *),
				 void *			user_data);

/*
 *  Controls
 */

/* Programmatically accessable controls. Other controls
   are anonymous, only the user knows what they do. Keep
   the list short. */
typedef enum {
	TV_CONTROL_ID_NONE,
	TV_CONTROL_ID_UNKNOWN = TV_CONTROL_ID_NONE,
	TV_CONTROL_ID_BRIGHTNESS,
	TV_CONTROL_ID_CONTRAST,
	TV_CONTROL_ID_SATURATION,
	TV_CONTROL_ID_HUE,
	TV_CONTROL_ID_MUTE,
	TV_CONTROL_ID_VOLUME,
	TV_CONTROL_ID_BASS,
	TV_CONTROL_ID_TREBLE
} tv_control_id;

typedef enum {
	TV_CONTROL_TYPE_NONE,
	TV_CONTROL_TYPE_INTEGER,	/* integer [min, max] */
	TV_CONTROL_TYPE_BOOLEAN,	/* integer [0, 1] */
	TV_CONTROL_TYPE_CHOICE,		/* multiple choice */
	TV_CONTROL_TYPE_ACTION,		/* setting has one-time effect */
	TV_CONTROL_TYPE_COLOR		/* RGB color entry */
} tv_control_type;

typedef struct _tv_control tv_control;

struct _tv_control {
  // XXX this is private because tveng combines control lists and we don't want
  // the client to see this. Actually the tv_x_next functions are a pain, I
  // must think of something else to permit public next pointers everywhere.
	tv_control *		_next;		/* private, use tv_control_next() */

	void *			_parent;
	tv_callback *		_callback;

	char *			label;		/* localized */
	unsigned int		hash;

	tv_control_type		type;
	tv_control_id		id;

	char **			menu;		/* localized; last entry NULL */
  // add?	unsigned int		selectable;	/* menu item 1 << n */
  // control enabled/disabled flag?

	int			minimum;
	int			maximum;
	int			step;
	int			reset;

	int			value;		/* last known, not current value */

  	tv_bool		_ignore; /* preliminary, private */
};

// XXX should not take info argument
int
tveng_update_control(tv_control *control, tveng_device_info * info);
int
tveng_set_control(tv_control * control, int value,
		  tveng_device_info * info);

static __inline__ tv_callback *
tv_control_add_callback		(tv_control *		control,
				 void			(* notify)(tv_control *, void *),
				 void			(* destroy)(tv_control *, void *),
				 void *			user_data)
{
	assert (control != NULL);

	return tv_callback_add (&control->_callback,
				(tv_callback_fn *) notify,
				(tv_callback_fn *) destroy,
				user_data);
}

/*
 *  Audio modes
 */

/* XXX this is a set, no enum */
typedef enum {
	TV_AUDIO_CAPABILITY_NONE,
	TV_AUDIO_CAPABILITY_AUTO	= (1 << 0),
	TV_AUDIO_CAPABILITY_MONO	= (1 << 1),
	TV_AUDIO_CAPABILITY_STEREO	= (1 << 2),
	/* Note SAP and BILINGUAL are mutually exclusive. */
	TV_AUDIO_CAPABILITY_SAP		= (1 << 3),
	TV_AUDIO_CAPABILITY_BILINGUAL	= (1 << 4),
} tv_audio_capability;

typedef enum {
	TV_AUDIO_MODE_AUTO		= 0,
	TV_AUDIO_MODE_UNKNOWN		= TV_AUDIO_MODE_AUTO,
	TV_AUDIO_MODE_MONO		= 1,
	TV_AUDIO_MODE_LANG1_MONO	= TV_AUDIO_MODE_MONO,
	TV_AUDIO_MODE_STEREO		= 2,
	TV_AUDIO_MODE_LANG1_STEREO	= TV_AUDIO_MODE_STEREO,
	TV_AUDIO_MODE_LANG2		= 3,
	TV_AUDIO_MODE_LANG2_MONO	= TV_AUDIO_MODE_LANG2
	/* LANG2_STEREO: There are no 4 channel systems. Digital and
	   satellite are a different matter. */
} tv_audio_mode;

extern tv_bool
tv_set_audio_mode		(tveng_device_info *	info,
				 tv_audio_mode		mode);
extern tv_bool
tv_audio_update			(tveng_device_info *	info);

/*
 *  Pixel format
 */

typedef enum {
	TV_PIXFMT_NONE,
	TV_PIXFMT_UNKNOWN = TV_PIXFMT_NONE,

	TV_PIXFMT_RESERVED0,

	/* Planar YUV formats */

	TV_PIXFMT_YUV444,		/* 4x4 4x4 4x4 */
	TV_PIXFMT_YVU444,
	TV_PIXFMT_YUV422,		/* 4x4 2x4 2x4 */
	TV_PIXFMT_YVU422,
	TV_PIXFMT_YUV411,		/* 4x4 1x4 1x4 */
	TV_PIXFMT_YVU411,
	TV_PIXFMT_YUV420,		/* 4x4 2x2 2x2 */
	TV_PIXFMT_YVU420,
	TV_PIXFMT_YUV410,		/* 4x4 1x1 1x1 */
	TV_PIXFMT_YVU410,

	/* Packed YUV formats */	/* reg msb..lsb -> memory byte 0..3 */

	TV_PIXFMT_YUVA24_LE,		/* AVUY -> LE  Y U V A  BE  A V U Y */
	TV_PIXFMT_YUVA24_BE,
	TV_PIXFMT_YVUA24_LE,		/* AUVY -> LE  Y V U A  BE  A U V Y */
	TV_PIXFMT_YVUA24_BE,

	TV_PIXFMT_AVUY24_BE = TV_PIXFMT_YUVA24_LE,
	TV_PIXFMT_AVUY24_LE,		/* YUVA -> LE  A V U Y  BE  Y U V A */
	TV_PIXFMT_AUVY24_BE,
	TV_PIXFMT_AUVY24_LE,		/* YVUA -> LE  A U V Y  BE  Y V U A */

	TV_PIXFMT_YUV24_LE,		/*  VUY -> LE  Y U V  BE  V U Y */
	TV_PIXFMT_YUV24_BE,
	TV_PIXFMT_YVU24_LE,		/*  UVY -> LE  Y V U  BE  U V Y */
	TV_PIXFMT_YVU24_BE,

	TV_PIXFMT_VUY24_BE = TV_PIXFMT_YUV24_LE,
	TV_PIXFMT_VUY24_LE,		/*  YUV -> LE  V U Y  BE  Y U V */
	TV_PIXFMT_UVY24_BE,
	TV_PIXFMT_UVY24_LE,		/*  YVU -> LE  U V Y  BE  Y V U */

	TV_PIXFMT_YUYV,			/* Y0 U Y1 V in memory */
	TV_PIXFMT_YVYU,			/* Y0 V Y1 U */
	TV_PIXFMT_UYVY,			/* U Y0 V Y1 */
	TV_PIXFMT_VYUY,			/* V Y0 U Y1 */

	TV_PIXFMT_RESERVED1,
	TV_PIXFMT_Y8,			/* Y */

	TV_PIXFMT_RESERVED2,
	TV_PIXFMT_RESERVED3,

	/* Packed RGB formats */

	TV_PIXFMT_RGBA24_LE,		/* ABGR -> LE  R G B A  BE  A B G R */
	TV_PIXFMT_RGBA24_BE,
	TV_PIXFMT_BGRA24_LE,		/* ARGB -> LE  B G R A  BE  A R G B */
	TV_PIXFMT_BGRA24_BE,

	TV_PIXFMT_ABGR24_BE = TV_PIXFMT_RGBA24_LE,
	TV_PIXFMT_ABGR24_LE,		/* RGBA -> LE  A B G R  BE  R G B A */
	TV_PIXFMT_ARGB24_BE,
	TV_PIXFMT_ARGB24_LE,		/* BGRA -> LE  A R G B  BE  B G R A */

	TV_PIXFMT_RGB24_LE,		/*  BGR -> LE  R G B  BE  B G R */
	TV_PIXFMT_BGR24_LE,		/*  RGB -> LE  B G R  BE  R G B */

	TV_PIXFMT_BGR24_BE = TV_PIXFMT_RGB24_LE,
	TV_PIXFMT_RGB24_BE,

	TV_PIXFMT_RGB16_LE,		/* bbbbbggggggrrrrr msb..lsb */
	TV_PIXFMT_RGB16_BE,
	TV_PIXFMT_BGR16_LE,		/* rrrrrggggggbbbbb */
	TV_PIXFMT_BGR16_BE,

	TV_PIXFMT_RGBA15_LE,		/* abbbbbgggggrrrrr */
	TV_PIXFMT_RGBA15_BE,
	TV_PIXFMT_BGRA15_LE,		/* arrrrrgggggbbbbb */
	TV_PIXFMT_BGRA15_BE,
	TV_PIXFMT_ARGB15_LE,		/* bbbbbgggggrrrrra */
	TV_PIXFMT_ARGB15_BE,
	TV_PIXFMT_ABGR15_LE,		/* rrrrrgggggbbbbba */
	TV_PIXFMT_ABGR15_BE,

	TV_PIXFMT_RGBA12_LE,		/* aaaabbbbggggrrrr */
	TV_PIXFMT_RGBA12_BE,
	TV_PIXFMT_BGRA12_LE,		/* aaaarrrrggggbbbb */
	TV_PIXFMT_BGRA12_BE,
	TV_PIXFMT_ARGB12_LE,		/* bbbbggggrrrraaaa */
	TV_PIXFMT_ARGB12_BE,
	TV_PIXFMT_ABGR12_LE,		/* rrrrggggbbbbaaaa */
	TV_PIXFMT_ABGR12_BE,

	TV_PIXFMT_RGB8,			/* bbgggrrr */
	TV_PIXFMT_BGR8,			/* rrrgggbb */

	TV_PIXFMT_RGBA7,		/* abbgggrr */
	TV_PIXFMT_BGRA7,		/* arrgggbb */
	TV_PIXFMT_ARGB7,		/* bbgggrra */
	TV_PIXFMT_ABGR7			/* rrgggbba */
} tv_pixfmt;

#define TV_MAX_PIXFMTS 64

typedef uint64_t tv_pixfmt_set;

#define TV_PIXFMT_SET(pixfmt) (((tv_pixfmt_set) 1) << (pixfmt))

#define TV_PIXFMT_SET_UNKNOWN 0
#define TV_PIXFMT_SET_EMPTY 0
#define TV_PIXFMT_SET_YUV_PLANAR (+ TV_PIXFMT_SET (TV_PIXFMT_YUV444)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YVU444)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YUV422)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YVU422)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YUV411)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YVU411)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YUV420)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YVU420)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YUV410)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YVU410))
#define TV_PIXFMT_SET_YUVA24     (+ TV_PIXFMT_SET (TV_PIXFMT_YUVA24_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YUVA24_BE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YVUA24_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YVUA24_BE))
#define TV_PIXFMT_SET_YUV24	 (+ TV_PIXFMT_SET (TV_PIXFMT_YUV24_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YUV24_BE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YVU24_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YVU24_BE))
#define TV_PIXFMT_SET_YUV16	 (+ TV_PIXFMT_SET (TV_PIXFMT_YUYV)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_YVYU)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_UYVY)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_VYUY))
#define TV_PIXFMT_SET_YUV_PACKED (+ TV_PIXFMT_SET_YUVA24		\
				  + TV_PIXFMT_SET_YUV24			\
				  + TV_PIXFMT_SET_YUV16			\
				  + TV_PIXFMT_SET (TV_PIXFMT_Y8))
#define TV_PIXFMT_SET_YUV	 (+ TV_PIXFMT_SET_YUV_PLANAR		\
				  + TV_PIXFMT_SET_YUV_PACKED)
#define TV_PIXFMT_SET_RGBA24	 (+ TV_PIXFMT_SET (TV_PIXFMT_RGBA24_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_RGBA24_BE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_BGRA24_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_BGRA24_BE))
#define TV_PIXFMT_SET_RGB24	 (+ TV_PIXFMT_SET (TV_PIXFMT_RGB24_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_BGR24_LE))
#define TV_PIXFMT_SET_RGB16	 (+ TV_PIXFMT_SET (TV_PIXFMT_RGB16_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_RGB16_BE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_BGR16_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_BGR16_BE))
#define TV_PIXFMT_SET_RGB15	 (+ TV_PIXFMT_SET (TV_PIXFMT_RGBA15_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_RGBA15_BE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_BGRA15_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_BGRA15_BE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_ARGB15_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_ARGB15_BE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_ABGR15_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_ABGR15_BE))
#define TV_PIXFMT_SET_RGB12	 (+ TV_PIXFMT_SET (TV_PIXFMT_RGBA12_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_RGBA12_BE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_BGRA12_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_BGRA12_BE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_ARGB12_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_ARGB12_BE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_ABGR12_LE)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_ABGR12_BE))
#define TV_PIXFMT_SET_RGB8	 (+ TV_PIXFMT_SET (TV_PIXFMT_RGB8)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_BGR8))
#define TV_PIXFMT_SET_RGB7	 (+ TV_PIXFMT_SET (TV_PIXFMT_RGBA7)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_BGRA7)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_ARGB7)	\
				  + TV_PIXFMT_SET (TV_PIXFMT_ABGR7))
#define TV_PIXFMT_SET_RGB_PACKED (+ TV_PIXFMT_SET_RGBA24		\
				  + TV_PIXFMT_SET_RGB24			\
				  + TV_PIXFMT_SET_RGB16			\
				  + TV_PIXFMT_SET_RGB15			\
				  + TV_PIXFMT_SET_RGB12			\
				  + TV_PIXFMT_SET_RGB8			\
				  + TV_PIXFMT_SET_RGB7)
#define TV_PIXFMT_SET_RGB	    TV_PIXFMT_SET_RGB_PACKED
#define TV_PIXFMT_SET_PLANAR	    TV_PIXFMT_SET_YUV_PLANAR
#define TV_PIXFMT_SET_PACKED	 (+ TV_PIXFMT_SET_YUV_PACKED		\
				  + TV_PIXFMT_SET_RGB_PACKED)
#define TV_PIXFMT_SET_ALL	 (+ TV_PIXFMT_SET_YUV			\
				  + TV_PIXFMT_SET_RGB)

#define TV_PIXFMT_IS_YUV(pixfmt)					\
	(0 != (TV_PIXFMT_SET (pixfmt) & TV_PIXFMT_SET_YUV))
#define TV_PIXFMT_IS_RGB(pixfmt)					\
	(0 != (TV_PIXFMT_SET (pixfmt) & TV_PIXFMT_SET_RGB))
#define TV_PIXFMT_IS_PLANAR(pixfmt)					\
	(0 != (TV_PIXFMT_SET (pixfmt) & TV_PIXFMT_SET_PLANAR))
#define TV_PIXFMT_IS_PACKED(pixfmt)					\
	(0 != (TV_PIXFMT_SET (pixfmt) & TV_PIXFMT_SET_PACKED))

#ifdef __GNUC__
#define TV_PIXFMT_BYTES_PER_PIXEL(pixfmt)				\
	(!__builtin_constant_p (pixfmt) ?				\
	 tv_pixfmt_bytes_per_pixel (pixfmt) :				\
	 ((TV_PIXFMT_SET (pixfmt) & TV_PIXFMT_SET_YUVA24) ? 4 :		\
	  ((TV_PIXFMT_SET (pixfmt) & TV_PIXFMT_SET_RGBA24) ? 4 :	\
	   ((TV_PIXFMT_SET (pixfmt) & TV_PIXFMT_SET_YUV24) ? 3 :	\
	    ((TV_PIXFMT_SET (pixfmt) & TV_PIXFMT_SET_RGB24) ? 3 :	\
	     ((TV_PIXFMT_SET (pixfmt) & TV_PIXFMT_SET_YUV16) ? 2 :	\
	      ((TV_PIXFMT_SET (pixfmt) & TV_PIXFMT_SET_RGB16) ? 2 :	\
	       ((TV_PIXFMT_SET (pixfmt) & TV_PIXFMT_SET_RGB15) ? 2 :	\
	        ((TV_PIXFMT_SET (pixfmt) & TV_PIXFMT_SET_RGB12) ? 2 :	\
		 1))))))))
#else
#define TV_PIXFMT_BYTES_PER_PIXEL(pixfmt)				\
	(tv_pixfmt_bytes_per_pixel (pixfmt))
#endif

extern const char *
tv_pixfmt_name			(tv_pixfmt		pixfmt);
extern unsigned int
tv_pixfmt_bytes_per_pixel	(tv_pixfmt		pixfmt);

/*
 *  Broken-down pixel format
 */

typedef struct _tv_pixel_format tv_pixel_format;

struct _tv_pixel_format {
	tv_pixfmt		pixfmt;
	unsigned int		_reserved1;		/* color space */

	/* Number of bits per pixel. For packed YUV 4:2:2 this is 16.
	   For planar formats this refers to the Y plane only. */
	unsigned int		bits_per_pixel;

	/* Number of red, green and blue, or luma and chroma bits
	   per pixel. Averaged if U and V plane are smaller than Y plane. */
	unsigned int		color_depth;

	/* Width and height of the U and V plane must be multiplied
	   by these values to get the size of the Y plane. Will be
           1, 2 or 4. */
	unsigned int		uv_hscale;
	unsigned int		uv_vscale;

	/* Format is packed and pixels are stored in 16, 24 or 32 bit
	   (bits_per_pixel) quantities with most significant byte
	   first in memory. */
	unsigned		big_endian	: 1;

	/* Y, U and V color components are stored in separate arrays,
	   first Y, then U and V. */
	unsigned		planar		: 1;

	/* For packed YUV 4:2:2, V pixel is stored before U pixel
	   in memory. For planar YUV formats, V plane is stored
	   before U plane in memory. */
	unsigned		vu_order	: 1;

	unsigned		_reserved2	: 29;

	/* Bit masks describing size and position of color components
	   in a in 8, 16, 24 or 32 bit (bits_per_pixel) quantity, as
	   seen when reading from memory with proper endianess.
	   For packed YUV 4:2:2 and planar formats y, u and v will be
	   0xFF. The a (alpha) component can be zero. */
	union {
		struct {
			unsigned int		r;
			unsigned int		g;
			unsigned int		b;
			unsigned int		a;
		}			rgb;
		struct {
			unsigned int		y;
			unsigned int		u;
			unsigned int		v;
			unsigned int		a;
		}			yuv;
	}			mask;
};

extern tv_bool
tv_pixfmt_to_pixel_format	(tv_pixel_format *	format,
				 tv_pixfmt		pixfmt,
				 unsigned int		reserved);
extern tv_bool
tv_pixel_format_to_pixfmt	(tv_pixel_format *	format);

/*
 *  Image format
 */

typedef struct _tv_image_format tv_image_format;

struct _tv_image_format {
	/* Image width in pixels, for planar formats this refers to
	   the Y plane and must be a multiple of tv_pixel_format.uv_hscale. */
	unsigned int		width;

	/* Image height in pixels, for planar formats this refers to
	   the Y plane and must be a multiple of tv_pixel_format.uv_vscale. */
	unsigned int		height;

	/* For packed formats bytes_per_line >= (width * tv_pixel_format
	   .bits_per_pixel + 7) / 8. For planar formats this refers to
	   the Y plane only, with implied y_size = bytes_per_line * height. */
	unsigned int		bytes_per_line;

	/* For planar formats only, refers to the U and V plane. */
	unsigned int		uv_bytes_per_line;

/* ATTENTION [u_|v_]offset and [uv_]bytes_per_line aren't
   fully supported yet, don't use them. */

	/* For packed formats the image offset in bytes from the buffer
	   start. For planar formats this refers to the Y plane. */
	unsigned int		offset;

	/* For planar formats only, the byte offset of the U and V
	   plane from the start of the buffer. */
	unsigned int		u_offset;
	unsigned int		v_offset;

	/* Buffer size. For packed formats size >= offset + height
	   * bytes_per_line. For planar formats size >=
	   MAX (offset + y_size, u_offset + uv_size, v_offset + uv_size). */
	unsigned int		size;

	tv_pixfmt		pixfmt;
	unsigned int		_reserved;		/* color space */
};

extern tv_bool
tv_image_format_init		(tv_image_format *	format,
				 unsigned int		width,
				 unsigned int		height,
				 tv_pixfmt		pixfmt,
				 unsigned int		reserved);
extern tv_bool
tv_image_format_is_valid	(const tv_image_format *format);

/*
 *  Video capture
 */

/* Convenience construction for managing image data */
typedef union {
  struct {
    void*	data; /* Data, usually in rgb or yuyv formats */
    int		stride; /* bytes per line */
  } linear;
  struct {
    void	*y, *u, *v; /* Pointers to the different fields */
    int		y_stride; /* bytes per line of the Y field */
    int		uv_stride; /* bytes per line of U or V fields */
  } planar;
} tveng_image_data;


/*
 *  Video overlay, see ROADMAP
 */

typedef struct _tv_overlay_buffer tv_overlay_buffer;

/* This is the target of DMA overlay, a continuous chunk of physical memory.
   Usually it describes the visible portion of the graphics card's video
   memory. */
struct _tv_overlay_buffer {
  	/* Memory address as seen by the video capture device, without
	   virtual address translation by the CPU. Actually this assumes
           graphic card and capture device share an address space, which is
	   not necessarily true if the devices connect to different busses,
	   but I'm not aware of any driver APIs considering this either. */
	unsigned long		base;

	tv_image_format		format;
};

/* Overlay clipping rectangle. These are regions you don't want
   overlaid, with clipping coordinates relative to the overlay
   window origin (not the overlay buffer). */

typedef struct _tv_clip tv_clip;

struct _tv_clip {
	unsigned int		x		: 16;
	unsigned int		y		: 16;
	unsigned int		width		: 16;
	unsigned int		height		: 16;
};

static __inline__ tv_bool
tv_clip_equal			(const tv_clip *	clip1,
				 const tv_clip *	clip2)
{
	if (sizeof (tv_clip) == 8)
		return (* (uint64_t *) clip1) == (* (uint64_t *) clip2);
	else
		return (0 == ((clip1->x ^ clip2->x) |
			      (clip1->y ^ clip2->y) |
			      (clip1->width ^ clip2->width) |
			      (clip1->height ^ clip2->height)));
}

typedef struct _tv_clip_vector tv_clip_vector;

struct _tv_clip_vector {
	tv_clip *		vector;
	unsigned int		size;
	unsigned int		capacity;
};

extern tv_bool
tv_clip_vector_equal		(const tv_clip_vector *	vector1,
				 const tv_clip_vector *	vector2);
extern tv_bool
tv_clip_vector_copy		(tv_clip_vector *	dst,
				 const tv_clip_vector *	src);
static __inline__ void
tv_clip_vector_clear		(tv_clip_vector *	vector)
{
	vector->size = 0;
}

extern tv_bool
tv_clip_vector_add_clip_xy	(tv_clip_vector *	vector,
				 unsigned int		x1,
				 unsigned int		y1,
				 unsigned int		x2,
				 unsigned int		y2);
static __inline__ tv_bool
tv_clip_vector_add_clip_wh	(tv_clip_vector *	vector,
				 unsigned int		x,
				 unsigned int		y,
				 unsigned int		width,
				 unsigned int		height)
{
	return tv_clip_vector_add_clip_xy
	  (vector, x, y, x + width, y + height);
}

extern uint8_t *
tv_clip_vector_to_clip_mask	(tv_clip_vector *	vector,
				 unsigned int		width,
				 unsigned int		height);

static __inline__ void
tv_clip_vector_init		(tv_clip_vector *	vector)
{
	memset (vector, 0, sizeof (*vector));
}

static __inline__ void
tv_clip_vector_destroy		(tv_clip_vector *	vector)
{
	free (vector->vector);
	tv_clip_vector_init (vector);
}

/* Overlay rectangle. This is what gets DMAed into the overlay
   buffer, minus clipping rectangles. Coordinates are relative
   to the overlay buffer origin. */

typedef struct _tv_window tv_window;

struct _tv_window {
	int			x;	// sic, can be negative
	int			y;
	unsigned int		width;
	unsigned int		height;

	/* Invisible regions of window, coordinates relative x, y above. */
	tv_clip_vector		clip_vector;

	// XXX to be removed
  Window win; /* window we are previewing to (only needed in XV mode) */
  GC gc; /* gc associated with win */
};

static __inline__ void
tv_window_destroy		(tv_window *		window)
{
	tv_clip_vector_destroy (&window->clip_vector);
	memset (window, 0, sizeof (*window));
}

static __inline__ void
tv_window_init			(tv_window *		window)
{
	memset (window, 0, sizeof (*window));
}








enum tveng_capture_mode
{
  /* FIXME this describes window contents
     and doesn't belong here since tveng maintains no windows.
     Moreover some drivers [appear to, from client POV] support
     capture and overlay simultaneously. */

  TVENG_NO_CAPTURE,		/* Capture isn't active */
  TVENG_CAPTURE_READ,		/* Capture is through a read() call */
  TVENG_CAPTURE_PREVIEW,       	/* Capture is through fullscreen overlays */
  TVENG_CAPTURE_WINDOW,		/* Capture is through windowed overlays */

  /* Doesn't belong here either, but I don't want to break
     too many things at once. */
  TVENG_TELETEXT
};

/* The controller we are using for this device */
enum tveng_controller
{
  TVENG_CONTROLLER_NONE, /* No controller set */
  TVENG_CONTROLLER_V4L1, /* V4L1 controller (old V4l spec) */
  TVENG_CONTROLLER_V4L2, /* V4L2 controller (new v4l spec) */
  TVENG_CONTROLLER_XV,	 /* XVideo controller */
  TVENG_CONTROLLER_EMU,	 /* Emulation controller */
  TVENG_CONTROLLER_MOTHER /* The wrapper controller (tveng.c) */
};

/* The structure used to hold info about a video_device */
struct _tveng_device_info
{
  char * file_name; /* The name used to open() this fd */
  int fd; /* Video device file descriptor */
  enum tveng_capture_mode current_mode; /* Current capture mode */
  enum tveng_attach_mode attach_mode; /* Mode this was attached with
				       */
  enum tveng_controller current_controller; /* Controller used */
  struct tveng_caps caps; /* Video system capabilities */

  /* the format about this capture */
  tv_image_format format; /* pixel format of this device */

	/* All internal communication with the device is logged
	   through this fp when non-NULL. */
	FILE *			log_fp;

	/* Panel properties */

	/* Video inputs of the device, invariable. */
	tv_video_line *		video_inputs;
	/* Can be NULL only when no list exists. */
	tv_video_line *		cur_video_input;

	/* Audio inputs of the device, invariable. Not supported yet.
	   Need a function telling which video and audio inputs combine. */
	tv_audio_line *		audio_inputs;
	/* Can be NULL only when no list exists. */
	tv_audio_line *		cur_audio_input;

	/* Video standards supported by the current video input. Note
	   videostd_ids are bitwise mutually exclusive, i.e. no two listed
	   standards can have the same videostd_id bit set. */
	tv_video_standard *	video_standards;
	/* This can be NULL if we don't know. If it matters,
	   and video_standards is not NULL, clients should ask the user. */
	tv_video_standard *	cur_video_standard;

	/* Controls */
	tv_control *		controls;
	unsigned		audio_mutable : 1;

	/* Audio mode, input & videostd dependant. Not implemented yet. */
	tv_audio_capability	audio_capability;
	tv_audio_mode		audio_mode;
	/* lang1/2: 0-none/unknown 1-mono 2-stereo */
	unsigned int		audio_reception[2];

	/* Overlay device properties */

	tv_overlay_buffer	overlay_buffer;
	tv_window		overlay_window;

	tv_bool			overlay_active; // XXX internal



  /* Unique integer that indentifies this device */
  int signature;

  /* Debugging/error reporting stuff */
  int tveng_errno; /* Numerical id of the last error, 0 == success */
  char * error; /* points to the last error message */
  int debug_level; /* 0 for no errors, increase for greater verbosity */

  struct tveng_private * priv; /* private stuff */
};

/* Video inputs */

extern const tv_video_line *
tv_next_video_input		(const tveng_device_info *info,
				 const tv_video_line *	line);
extern const tv_video_line *
tv_nth_video_input		(tveng_device_info *	info,
				 unsigned int		hash);
extern unsigned int
tv_video_input_position		(tveng_device_info *	info,
				 const tv_video_line *	line);
extern const tv_video_line *
tv_video_input_by_hash		(tveng_device_info *	info,
				 unsigned int		hash);
extern const tv_video_line *
tv_get_video_input		(tveng_device_info *	info);
extern tv_bool
tv_set_video_input		(tveng_device_info *	info,
				 const tv_video_line *	line);
/* Current video input, frequencies in Hz */
extern tv_bool
tv_get_tuner_frequency		(tveng_device_info *	info,
				 unsigned int *		frequency);
extern tv_bool
tv_set_tuner_frequency		(tveng_device_info *	info,
				 unsigned int		frequency);
/* Note this refers to the cur_video_input pointer: notify is called after
   it changed, possibly to NULL. Since this pointer always points to a video
   input list member NULL implies the list has been destroyed. The pointer
   itself is never destroyed until info is. */
extern tv_callback *
tv_add_video_input_callback	(tveng_device_info *	info,
				 void			(* notify)(tveng_device_info *, void *),
				 void			(* destroy)(tveng_device_info *, void *),
				 void *			user_data);

/* Audio inputs */

extern const tv_audio_line *
tv_next_audio_input		(const tveng_device_info *info,
				 const tv_audio_line *	line);
extern const tv_audio_line *
tv_nth_audio_input		(tveng_device_info *	info,
				 unsigned int		hash);
extern unsigned int
tv_audio_input_position		(tveng_device_info *	info,
				 const tv_audio_line *	line);
extern const tv_audio_line *
tv_audio_input_by_hash		(tveng_device_info *	info,
				 unsigned int		hash);
extern const tv_audio_line *
tv_get_audio_input		(tveng_device_info *	info);
extern tv_bool
tv_set_audio_input		(tveng_device_info *	info,
				 const tv_audio_line *	line);
extern tv_callback *
tv_add_audio_input_callback	(tveng_device_info *	info,
				 void			(* notify)(tveng_device_info *, void *),
				 void			(* destroy)(tveng_device_info *, void *),
				 void *			user_data);

/* Video standards */

extern const tv_video_standard *
tv_next_video_standard		(const tveng_device_info *info,
				 const tv_video_standard *standard);
extern const tv_video_standard *
tv_nth_video_standard		(tveng_device_info *	info,
				 unsigned int		index);
extern unsigned int
tv_video_standard_position	(tveng_device_info *	info,
				 const tv_video_standard *standard);
extern const tv_video_standard *
tv_video_standard_by_hash	(tveng_device_info *	info,
				 unsigned int		hash);
extern const tv_video_standard *
tv_get_video_standard		(tveng_device_info *	info);
extern tv_bool
tv_set_video_standard		(tveng_device_info *	info,
				 const tv_video_standard *standard);
extern tv_bool
tv_set_video_standard_by_id	(tveng_device_info *	info,
				 tv_videostd_set	videostd_set);
/* See add_video_input_callback note. The standard list can change with
   a video input change. If so, the entire list will be rebuilt, calling
   notify at least once after the pointer changed to NULL. */
extern tv_callback *
tv_add_video_standard_callback	(tveng_device_info *	info,
				 void			(* notify)(tveng_device_info *, void *),
				 void			(* destroy)(tveng_device_info *, void *),
				 void *			user_data);

/* Controls */

extern tv_control *
tv_next_control			(const tveng_device_info *info,
				 const tv_control *	control);
extern tv_control *
tv_nth_control			(tveng_device_info *	info,
				 unsigned int		index);
extern unsigned int
tv_control_position		(tveng_device_info *	info,
				 const tv_control *	control);
extern tv_control *
tv_control_by_hash		(tveng_device_info *	info,
				 unsigned int		hash);
extern tv_control *
tv_control_by_id		(tveng_device_info *	info,
				 tv_control_id		id);
/*
  Gets the current value of the controls, fills in info->controls
  appropiately. After this (and if it succeeds) you can look in
  info->controls to get the values for each control. -1 on error
*/
int
tveng_update_controls(tveng_device_info * info);
/*
  Gets the value of a control, given its name. Returns -1 on
  error. The comparison is performed disregarding the case. The value
  read is stored in cur_value.
*/
int
tveng_get_control_by_name(const char * control_name,
			  int * cur_value,
			  tveng_device_info * info);
/*
  Sets the value of a control, given its name. Returns -1 on
  error. The comparison is performed disregarding the case.
  new_value holds the new value given to the control, and it is
  clipped as neccessary.
*/
int
tveng_set_control_by_name(const char * control_name,
			  int new_value,
			  tveng_device_info * info);



/* Starts a tveng_device_info object, returns a pointer to the object
   or NULL on error. Display is the display we are connected to, bpp
   is the current X display's depth in Bits Per Pixel, or -1 if TVeng
   should try to detect it.
*/
tveng_device_info * tveng_device_info_new(Display * display, int bpp);

/* Destroys a tveng_device_info object */
void tveng_device_info_destroy(tveng_device_info * info);

/*
  Associates the given tveng_device_info with the given video
  device. On error it returns -1 and sets info->errno, info->error to
  the correct values.
  device_file: The file used to access the video device (usually
  /dev/video)
  attach_mode: Specifies the mode to open the device file
  depth: The color depth the capture will be in, -1 means let tveng
  decide based on the current display depth.
  info: The structure to be associated with the device
*/
int tveng_attach_device(const char* device_file,
			enum tveng_attach_mode attach_mode,
			tveng_device_info * info);

/*
  Stores in short_str and long_str (if they are non-null) the
  description of the current controller. The enum value can be found in
  info->current_controller.
  For example, V4L2 controller would say:
  short_str: 'V4L2'
  long_str: 'Video4Linux 2'
  info->current_controller: TVENG_CONTROLLER_V4L2
  This function always succeeds.
  The returned pointer are statically allocated, i.e., they don't need
  to be freed.
*/
void
tveng_describe_controller(char ** short_str, char ** long_str,
			  tveng_device_info * info);

/*
  Closes the video device asocciated to the device info object. Should
  be called before reattaching a video device to the same object, but
  there is no need to call this before calling tveng_device_info_destroy.
*/
void tveng_close_device(tveng_device_info* info);

/*
  Functions for controlling the video capture. All of them return -1
  in case of error, so any value != -1 should be considered valid
  (unless explicitly stated in the description of the function) 
*/


/* Updates the current capture format info. -1 if failed */
int
tveng_update_capture_format(tveng_device_info * info);

/* -1 if failed. Sets the format and fills in info -> format
   with the correct values  */
int
tveng_set_capture_format(tveng_device_info * info);


/* Audio interface */

extern void
tv_quiet_set			(tveng_device_info *	info,
				 tv_bool		quiet);
extern int
tv_mute_get			(tveng_device_info *	info,
				 tv_bool		update);
extern int
tv_mute_set			(tveng_device_info *	info,
				 tv_bool		mute);
extern tv_callback *
tv_mute_add_callback		(tveng_device_info *	info,
				 void			(* notify)(tveng_device_info *, void *),
				 void			(* destroy)(tveng_device_info *, void *),
				 void *			user_data);


int
tveng_set_input_by_name(const char * input_name,
			tveng_device_info * info);
int
tveng_set_standard_by_name(const char * name, tveng_device_info * info);

/*
  Gets the signal strength and the afc code. The afc code indicates
  how to get a better signal, if negative, tune higher, if negative,
  tune lower. 0 means no idea or feature not present in the current
  controller (i.e. V4L1). Strength and/or afc can be NULL pointers,
  that would mean ignore that parameter.
*/
int
tveng_get_signal_strength (int *strength, int * afc,
			   tveng_device_info * info);

/*
  Sets up the capture device so any read() call after this one
  succeeds. Returns -1 on error.
*/
int
tveng_start_capturing(tveng_device_info * info);

/* Tries to stop capturing. -1 on error. */
int
tveng_stop_capturing(tveng_device_info * info);

/* 
   Reads a frame from the video device, storing the read data in dest.
   time: time to wait using select() in miliseconds
   info: pointer to the video device info structure
   Returns -1 on error, anything else on success.
   Note: if you want this call to be non-blocking, call it with time=0
*/
int tveng_read_frame(tveng_image_data * dest,
		     unsigned int time, tveng_device_info * info);

/*
  Gets the timestamp of the last read frame in seconds.
*/
double tveng_get_timestamp(tveng_device_info * info);

/* 
   Sets the capture buffer to an specific size. returns -1 on
   error. Remember to check the value of width and height in the
   format struct since it can be different to the one requested. 
*/
int tveng_set_capture_size(int width, int height, tveng_device_info *
			   info);

/* 
   Gets the actual size of the capture buffer in width and height.
   -1 on error
*/
int tveng_get_capture_size(int *width, int *height, tveng_device_info * info);

/* XF86 Frame Buffer routines */

extern tv_bool
tv_get_overlay_buffer		(tveng_device_info *	info,
				 tv_overlay_buffer *	target);
extern tv_bool
tv_set_overlay_buffer		(tveng_device_info *	info,
				 tv_overlay_buffer *	target);
extern tv_bool
tv_set_overlay_xwindow		(tveng_device_info *	info,
				 Window			window,
				 GC			gc);

/* 
   This is a convenience function, it returns the real screen depth in
   BPP (bits per pixel). This one is quite important for 24 and 32 bit
   modes, since the default X visual may be 24 bit and the real screen
   depth 32, thus an expensive RGB -> RGBA conversion must be
   performed for each frame.
*/
int
tveng_get_display_depth(tveng_device_info * info);

/*
  Sets the preview window dimensions to the given window.
  Returns -1 on error, something else on success.
  Success doesn't mean that the requested dimensions are used, maybe
  they are different, check the returned fields to see if they are suitable
  info   : Device we are controlling
  The current chromakey value is used, the caller doesn't need to fill
  it in.
*/
int
tveng_set_preview_window(tveng_device_info * info);

/*
  Gets the current overlay window parameters.
  Returns -1 on error, and any other value on success.
  info   : The device to use
*/
int
tveng_get_preview_window(tveng_device_info * info);

/* Some useful macros for the following function */
#define ON 1
#define OFF 0
#define tveng_set_preview_on(INFO) tveng_set_preview (ON, INFO)
#define tveng_set_preview_off(INFO) tveng_set_preview (OFF, INFO)

/* 
   Sets the previewing on/off.
   on : if 1, set preview on, if 0 off, other values are silently ignored
   info  : device to use for previewing
   Returns -1 on error, anything else on success
*/
int
tveng_set_preview (int on, tveng_device_info * info);

/*
 * Adjusts the verbosity value passed to zapping_setup_fb, cannot fail
 */
void
tveng_set_zapping_setup_fb_verbosity(int level, tveng_device_info * info);

/*
 * A value of TRUE forces dword-aligning of X coords and widths in
 * preview mode (workaround for some buggy drivers).
 */
void tveng_set_dword_align(int dword_align, tveng_device_info *info);

/*
 * Sets the chroma value to the given one, has only effect if the
 * driver supports it.
 */
void tveng_set_chromakey(uint32_t chroma, tveng_device_info *info);

/*
 * Returns the current chromakey value as a pixel value. If the driver
 * doesn't support this -1 is returned and chroma is left untouched.
 */
int tveng_get_chromakey (uint32_t *chroma, tveng_device_info *info);

/* Returns the current verbosity value passed to zapping_setup_fb */
int
tveng_get_zapping_setup_fb_verbosity(tveng_device_info * info);

/* build hash for the given string, normalized */
int
tveng_build_hash(const char *string);

/* get the current debug level */
int tveng_get_debug_level(tveng_device_info * info);

/* set the debug level. The value will be clipped to valid values */
void tveng_set_debug_level(int level, tveng_device_info * info);

/* sets xv support on/off, 1 means off */
void tveng_set_xv_support(int disabled, tveng_device_info * info);

#ifdef USE_XV
/* Add special XV controls to the device */
void tveng_set_xv_port(XvPortID port, tveng_device_info * info);
/* Tell that the given XV port isn't valid any more */
void tveng_unset_xv_port(tveng_device_info *info);
#endif

/*
  OV511 specific code:
  The camera has a clickable button, detect this button's state.
  Returns:
  * -1 on error (no OV51* or the appropiate /proc entry nonexistant)
  * 0, 1 on success.
*/
int tveng_ov511_get_button_state (tveng_device_info *info);

/* Aquire the (recursive) mutex on the device, TVeng functions already
   locks it when called. */
void tveng_mutex_lock(tveng_device_info *info);

/* Releases the mutex */
void tveng_mutex_unlock(tveng_device_info * info);

/*
 *  AUDIO MIXER INTERFACE
 */

typedef struct _tv_mixer tv_mixer;
typedef struct _tv_mixer_interface tv_mixer_interface;

/*
 *  Assumptions, for now:
 *
 *  A mixer has zero or more analog inputs listed in
 *  tv_mixer.inputs. All inputs are summed and routed to one or
 *  more outputs. The volume and mute control affect each input line
 *  individually before summation and have no effect on recording.
 *
 *  Sound devices can have one ADC. If so, of a subset of inputs one
 *  (tv_mixer.rec_line) can be routed to the only ADC. Optional a
 *  gain control (tv_mixer.rec_gain) exists on the internal line
 *  between the input multiplexer and ADC.
 *
 *  Sound devices can have one DAC, somehow routed to one or more
 *  outputs. Optional a gain control (tv_mixer.play_gain) exists
 *  on the internal line between the DAC and summation or a
 *  multiplexer connecting to the outputs.
 *
 *  Outputs may have volume controls associated with them, but we
 *  leave them alone, to be changed with a mixer application.
 *  Likewise we need not care about the routing of inputs and DAC to
 *  outputs, at least until we have to deal with multichannel sound
 *  or user complaints. :-)
 *
 *  Some mixers can select more than one input for recording.
 *  We don't need that, but won't interfere if the user insists.
 *  Except to reset when muted at startup the playback gain should
 *  be left alone, because hardware or software may sum PCM audio
 *  before converted by the DAC. Output volume can be easily
 *  implemented in our codecs.
 *
 *  Major flaw remains the assumption of a single ADC and DAC. Some
 *  devices have more, have digital inputs and outputs, and not all
 *  of them routed through the mixer.
 */
struct _tv_mixer {
	tv_device_node		node;

	/*
	 *  Routes from inputs to output sum/mux. Mute/volume
	 *  does not affect recording.
	 */
	tv_audio_line *		inputs;

	/*
	 *  Last known recording source, this points to one of the
	 *  'inputs' or NULL.
	 *
	 *  Note the user can select multiple sources with a mixer
	 *  application, then this is only one of them, usually the
	 *  one requested with set_rec_line(). Point is this may
	 *  change asynchronously, use of callback recommended.
	 */
	tv_audio_line *		rec_line;

	/*
	 *  Route from rec mux to ADC or NULL.
	 */
	tv_audio_line *		rec_gain;

	/*
	 *  Route from DAC to output sum/mux or NULL.
	 */
	tv_audio_line *		play_gain;

	/* private */

	const tv_mixer_interface *
				_interface;

	FILE *			_log;		/* if non-zero log all driver i/o */

	/*
	 *  Called by interface when tv_mixer.rec_line changed.
	 */
	tv_callback *		_callback;
};

extern tv_bool
tv_mixer_line_update		(tv_audio_line *	line);

extern tv_bool
tv_mixer_line_get_volume	(tv_audio_line *	line,
				 unsigned int *		left,
				 unsigned int *		right);
extern tv_bool
tv_mixer_line_set_volume	(tv_audio_line *	line,
				 unsigned int		left,
				 unsigned int		right);
extern tv_bool
tv_mixer_line_get_mute		(tv_audio_line *	line,
				 tv_bool *		mute);
extern tv_bool
tv_mixer_line_set_mute		(tv_audio_line *	line,
				 tv_bool		mute);
extern tv_bool
tv_mixer_line_record		(tv_audio_line *	line,
				 tv_bool		exclusive);
static __inline__ tv_callback *
tv_mixer_line_add_callback	(tv_audio_line *	line,
				 void			(* notify)(tv_audio_line *, void *),
				 void			(* destroy)(tv_audio_line *, void *),
				 void *			user_data)
{
	assert (line != NULL);

	return tv_callback_add (&line->_callback,
				(tv_callback_fn *) notify,
				(tv_callback_fn *) destroy,
				user_data);
}
extern tv_bool
tv_mixer_update			(tv_mixer *		mixer);
static __inline__ tv_callback *
tv_mixer_add_callback		(tv_mixer *		mixer,
				 void			(* notify)(tv_mixer *, void *),
				 void			(* destroy)(tv_mixer *, void *),
				 void *			user_data)
{
	assert (mixer != NULL);

	return tv_callback_add (&mixer->_callback,
				(tv_callback_fn *) notify,
				(tv_callback_fn *) destroy,
				user_data);
}
extern tv_mixer *
tv_mixer_open			(FILE *			log,
				 const char *		device);
static __inline__ void
tv_mixer_close			(tv_mixer *		mixer)
{
	tv_device_node_delete (NULL, &mixer->node, FALSE);
}
extern tv_mixer *
tv_mixer_scan			(FILE *			log);

/* open, scan? */

extern void
tveng_attach_mixer_line		(tveng_device_info *	info,
				 tv_mixer *		mixer,
				 tv_audio_line *	line);

/* Sanity checks should use this */
#define t_assert(condition) if (!(condition)) { \
fprintf(stderr, _("%s (%d): %s: assertion (%s) failed\n"), __FILE__, \
__LINE__, __PRETTY_FUNCTION__, #condition); \
exit(1);}

#define t_warn(templ, args...)						\
  fprintf (stderr, "%s:%u: " templ, __FILE__, __LINE__ ,##args );

/* Builds a custom error message, doesn't use errno */
#define t_error_msg(str_error, msg_error, info, args...) \
do { \
  char temp_error_buffer[256]; \
  temp_error_buffer[255] = 0; \
  snprintf(temp_error_buffer, 255, "[%s] %s (line %d)\n%s failed: %s", \
	   __FILE__, __PRETTY_FUNCTION__, __LINE__, str_error, msg_error); \
  (info)->error[255] = 0; \
  snprintf((info)->error, 255, temp_error_buffer ,##args); \
  if ((info)->debug_level) \
    fprintf(stderr, "TVeng: %s\n", (info)->error); \
} while (0)

#define tv_error_msg(info, template, args...)				\
do {									\
  snprintf ((info)->error, 255, template ,##args );			\
  if ((info)->debug_level > 0)						\
    fprintf (stderr, "%s:%u:%s:%s", __FILE__, __LINE__,			\
	     __PRETTY_FUNCTION__, (info)->error);			\
} while (0)

/* Builds an error message that lets me debug much better */
#define t_error(str_error, info) \
t_error_msg((str_error), strerror((info)->tveng_errno), (info));

/* Defines a point that should never be reached */
#define t_assert_not_reached() do {\
fprintf(stderr, \
_("[%s: %d: %s] This should have never been reached\n" ), __FILE__, \
__LINE__, __PRETTY_FUNCTION__); \
exit(1); \
} while (0)

#endif /* TVENG.H */
