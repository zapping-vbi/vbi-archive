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
/*
 * Private stuff, we can play freely with this without losing binary
 * or source compatibility.
 */
#ifndef __TVENG_PRIVATE_H__
#define __TVENG_PRIVATE_H__
#include <tveng.h>
#include <pthread.h>

/* i18n */
#ifndef _
#ifdef ENABLE_NLS
#include <libintl.h>
#define _(String) gettext (String)
#ifndef N_
#define N_(String) (String)
#endif
#else /* ENABLE_NLS */
#define _(String) (String)
#ifndef N_
#define N_(String) (String)
#endif
#endif /* ENABLE_NLS */
#endif /* _ */

#include "x11stuff.h"

typedef struct _tv_dev_control tv_dev_control;

struct _tv_dev_control {
	tv_control		pub;		/* attn keep this first */
	tveng_device_info *	device;		/* owner */	
	tv_callback_node *	callback;
};

/*
  Function prototypes for modules, NULL means not implemented or not
  pertinent.
  For the descriptions, see tveng.h
*/
struct tveng_module_info {
  int	(*attach_device)(const char* device_file,
			 enum tveng_attach_mode  attach_mode,
			 tveng_device_info * info);
  void	(*describe_controller)(char **short_str, char **long_str,
			       tveng_device_info *info);
  void	(*close_device)(tveng_device_info *info);
  int	(*get_inputs)(tveng_device_info *info);
  int	(*set_input)(struct tveng_enum_input *input,
		     tveng_device_info *info);
  int	(*get_standards)(tveng_device_info *info);
  int	(*set_standard)(struct tveng_enumstd *std,
			tveng_device_info *info);
  int	(*update_capture_format)(tveng_device_info *info);
  int	(*set_capture_format)(tveng_device_info *info);

	/*
	 *  Update tv_control.value to notice asynchronous changes
	 *  by other applications, may call tv_dev_control.callback.
	 *  May also update other properties if we come across that
	 *  information in the course. If the control is NULL update
	 *  all controls, this may be faster than individual updates.
	 */
	tv_bool			(* update_control)	(tveng_device_info *,
							 tv_dev_control *);

	/*
	 *  Set the value of a control, this implies update_control
	 *  with all side effects mentioned.
	 */
  	tv_bool			(*set_control)		(tveng_device_info *,
							 tv_dev_control *,
							 int);

  int	(*tune_input)(uint32_t freq, tveng_device_info *info);
  int	(*get_signal_strength)(int *strength, int *afc,
			       tveng_device_info *info);
  int	(*get_tune)(uint32_t *freq, tveng_device_info *info);
  int	(*get_tuner_bounds)(uint32_t *min, uint32_t *max,
			    tveng_device_info *info);
  int	(*start_capturing)(tveng_device_info *info);
  int	(*stop_capturing)(tveng_device_info *info);
  int	(*read_frame)(tveng_image_data *where,
		      unsigned int time, tveng_device_info *info);
  double (*get_timestamp)(tveng_device_info *info);
  int	(*set_capture_size)(int width, int height,
			    tveng_device_info *info);
  int	(*get_capture_size)(int *width, int *height,
			    tveng_device_info *info);

  int	(*detect_preview)(tveng_device_info *info);

  int	(*set_preview_window)(tveng_device_info *info);
  int	(*get_preview_window)(tveng_device_info *info);
  int	(*set_preview)(int on, tveng_device_info *info);

  int	(*start_previewing)(tveng_device_info *info,
			    x11_dga_parameters *dga);
  int	(*stop_previewing)(tveng_device_info *info);

  void	(*set_chromakey)(uint32_t pixel, tveng_device_info *info);
  int	(*get_chromakey)(uint32_t *pixel, tveng_device_info *info);

  /* Device specific stuff */
  int	(*ov511_get_button_state)(tveng_device_info *info);

  /* size of the private data of the module */
  int	private_size;
};

#include "x11stuff.h"

struct tveng_private {
  Display	*display;
  int		save_x, save_y;
  int		bpp;
  int		current_bpp;
  struct tveng_module_info module;

  x11_vidmode_state old_mode;

  int		zapping_setup_fb_verbosity;
  gchar *	mode; /* vidmode */
  int		assume_yvu; /* assume destination is YVU */
  int		disable_xv; /* 1 if XVideo should be disabled */
  int		dword_align; /* 1 if x and w should be dword aligned */

  pthread_mutex_t mutex; /* Thread safety */

  /* Controls managed directly by tveng.c */
#ifdef USE_XV
  XvPortID	port;
  Atom filter;
  Atom double_buffer;
  Atom colorkey; /* colorkey doesn't have min, max, it's defined by
		    RGB triplets */
#endif

  tv_control *		control_mute;
};

static inline void
free_control			(tv_control *		tc)
{
	tv_dev_control *tdc = (tv_dev_control *) tc; /* XXX */

	if (!tc)
		return;

	tv_callback_destroy (tc, &tdc->callback);

	if (tc->label) {
		free ((char *) tc->label);
	}

	if (tc->menu) {
	      unsigned int i;

	      for (i = 0; tc->menu[i]; i++) {
		      free ((char *) tc->menu[i]);
	      }

	      free (tc->menu);
	}

	free (tc);
}

static inline tv_control *
append_control			(tveng_device_info *	info,
				 tv_control *		tc,
				 unsigned int		size)
{
	tv_control **tcp;

	for (tcp = &info->controls; *tcp; tcp = &(*tcp)->next)
		;

	if (size > 0) {
		*tcp = malloc (size);

		if (!*tcp) {
			info->tveng_errno = errno;
			t_error("malloc", info);
			return NULL;
		}

		memcpy (*tcp, tc, size);

		tc = *tcp;
	} else {
		*tcp = tc;
	}

	tc->next = NULL;

	return tc;
}

#ifndef MAX
#define MAX(X, Y) (((X) < (Y)) ? (Y) : (X))
#endif
#ifndef MIN
#define MIN(X, Y) (((X) > (Y)) ? (Y) : (X))
#endif

/* check for hash collisions in info->inputs */
static inline void
input_collisions(tveng_device_info *info)
{
  int i, j, hash;

  for (i=0; i<info->num_inputs; i++)
    {
      hash = info->inputs[i].hash;
      for (j = i+1; j<info->num_inputs; j++)
	if (info->inputs[j].hash == hash)
	  fprintf(stderr,
		  "WARNING: TVENG: Hash collision between %s and %s (%x)\n"
		  "please send a bug report the maintainer!\n",
		  info->inputs[i].name, info->inputs[j].name, hash);
    }
}

/* check for hash collisions in info->standards */
static inline void
standard_collisions(tveng_device_info *info)
{
  int i, j, hash;

  for (i=0; i<info->num_standards; i++)
    {
      hash = info->standards[i].hash;
      for (j = i+1; j<info->num_standards; j++)
	if (info->standards[j].hash == hash)
	  fprintf(stderr,
		  "WARNING: TVENG: Hash collision between %s and %s (%x)\n"
		  "please send a bug report the maintainer!\n",
		  info->standards[i].name, info->standards[j].name, hash);
    }
}

static inline void
tveng_copy_block (void *_src, void *_dest,
		  int src_stride, int dest_stride,
		  int lines)
{
  int min_stride = MIN (src_stride, dest_stride);

  if (src_stride == dest_stride)
    memcpy (_dest, _src, src_stride * lines);
  else for (;lines; lines--, _src += src_stride, _dest += dest_stride)
    memcpy (_dest, _src, min_stride);
}

static void
tveng_copy_frame (unsigned char *src, tveng_image_data *where,
		  tveng_device_info *info) __attribute__ ((unused));
static void
tveng_copy_frame (unsigned char *src, tveng_image_data *where,
		  tveng_device_info *info)
{
  if (tveng_is_planar (info->format.pixformat))
    {
      unsigned char *y = where->planar.y, *u = where->planar.u,
	*v = where->planar.v;
      unsigned char *src_y, *src_u, *src_v;
      /* Assume that we are aligned */
      int bytes = info->format.height * info->format.width;
      t_assert (info->format.pixformat == TVENG_PIX_YUV420 ||
		info->format.pixformat == TVENG_PIX_YVU420);
      if (info->priv->assume_yvu)
	{
	  unsigned char *t = u;
	  u = v;
	  v = t;
	}
      src_y = src;
      src_u = src_y + bytes;
      src_v = src_u + (bytes>>2);
      if (info->format.pixformat == TVENG_PIX_YVU420)
	{
	  unsigned char *t = src_u;
	  src_u = src_v;
	  src_v = t;
	}
      tveng_copy_block (src_y, y, info->format.width,
			where->planar.y_stride, info->format.height);
      tveng_copy_block (src_u, u, info->format.width/2,
			where->planar.uv_stride,
			info->format.height/2);
      tveng_copy_block (src_v, v, info->format.width/2,
			where->planar.uv_stride,
			info->format.height/2);
    }
  else /* linear */
    tveng_copy_block (src, where->linear.data,
		      info->format.bytesperline, where->linear.stride,
		      info->format.height);
}

#endif /* tveng_private.h */
