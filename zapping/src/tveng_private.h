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
  int	(*update_controls)(tveng_device_info *info);
  int	(*set_control)(struct tveng_control *control,
		       int value, tveng_device_info *info);
  int	(*get_mute)(tveng_device_info *info);
  int	(*set_mute)(int value, tveng_device_info *info);
  int	(*tune_input)(uint32_t freq, tveng_device_info *info);
  int	(*get_signal_strength)(int *strength, int *afc,
			       tveng_device_info *info);
  int	(*get_tune)(uint32_t *freq, tveng_device_info *info);
  int	(*get_tuner_bounds)(uint32_t *min, uint32_t *max,
			    tveng_device_info *info);
  int	(*start_capturing)(tveng_device_info *info);
  int	(*stop_capturing)(tveng_device_info *info);
  int	(*read_frame)(void *where, unsigned int bytesperline,
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
  int	(*start_previewing)(tveng_device_info *info);
  int	(*stop_previewing)(tveng_device_info *info);
  /* size of the private data of the module */
  int	private_size;
};

struct tveng_private {
  Display	*display;
  int		save_x, save_y;
  int		bpp;
  int		current_bpp;
  char		*default_standard;
  struct tveng_module_info module;

#ifndef DISABLE_X_EXTENSIONS
  XF86VidModeModeInfo modeinfo;
  int		restore_mode;
  int		xf86vm_enabled;
#endif

  int		zapping_setup_fb_verbosity;
  int		change_mode;
  int		assume_yvu; /* assume destination is YVU */
  int		disable_xv; /* 1 if XVideo should be disabled */
  int		chromakey; /* RGB32 */
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
};

static int
p_tveng_append_control(struct tveng_control * new_control, 
		       tveng_device_info * info) __attribute__ ((unused));

static struct tveng_control *
find_control_by_id(tveng_device_info *info, int id) __attribute__ ((unused));

static struct tveng_control *
find_control_by_id(tveng_device_info *info, int id)
{
  int i;

  for (i = 0; i<info->num_controls; i++)
    if (info->controls[i].id == id)
      return &(info->controls[i]);

  return NULL;
};

static int
p_tveng_append_control(struct tveng_control * new_control, 
		       tveng_device_info * info)
{
  struct tveng_control * new_pointer;

  if (find_control_by_id(info, new_control->id))
    return 0;

  new_pointer = (struct tveng_control*)
    realloc(info->controls, (info->num_controls+1)*
	    sizeof(struct tveng_control));

  if (!new_pointer)
    {
      info->tveng_errno = errno;
      t_error("realloc", info);
      return -1;
    }
  info->controls = new_pointer;

  memcpy(&info->controls[info->num_controls], new_control,
	 sizeof(struct tveng_control));
  info->num_controls++;
  return 0;
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

/* Copies from src to dest "pixels" RGB/BGR pixels swapping R and B */
static inline
void endian3(char *dest, const char *src, int pixels)
     __attribute__ ((unused));

static inline
void endian3(char *dest, const char *src, int pixels)
{
  for (;pixels;pixels--)
    {
      *(dest++) = src[2];
      *(dest++) = src[1];
      *(dest++) = src[0];

      src+=3;
    }
}

/* Copies from src to dest "pixels" RGBA/BGRA pixels swapping R and B */
static inline
void endian4(char *dest, const char *src, int pixels)
     __attribute__ ((unused));

static inline
void endian4(char *dest, const char *src, int pixels)
{
  for (;pixels;pixels--)
    {
	dest[0] = src[2];
	dest[1] = src[1];
	dest[2] = src[0];

	dest += 4;
	src += 4;
    }
}

#endif /* tveng_private.h */
