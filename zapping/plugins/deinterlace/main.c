/*
 *  Zapping TV viewer
 *
 *  Copyright (C) 2004 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id: main.c,v 1.5 2005-03-30 21:29:33 mschimek Exp $ */

#include "site_def.h"

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common/intl-priv.h"
#include "libtv/image_format.h"	/* tv_memcpy() */
#include "src/properties.h"
#include "src/plugin_common.h"
#include "src/zgconf.h"
#include "libtv/cpu.h"
#include "preferences.h"
#include "main.h"

#define GCONF_DIR "/apps/zapping/plugins/deinterlace"

#ifndef DI_MAIN_HEIGHT_DIV
/* For tests. My CPU is too slow to run
   all methods at full height. */
#  define DI_MAIN_HEIGHT_DIV 1
#endif

/* See windows.h */
const int64_t vsplat8_m1[2] = { -1, -1 };
const int64_t vsplat8_1[2] = { 0x0101010101010101LL, 0x0101010101010101LL };
const int64_t vsplat8_127[2] = { 0x7F7F7F7F7F7F7F7FLL, 0x7F7F7F7F7F7F7F7FLL };
const int64_t vsplat16_255[2] = { 0x00FF00FF00FF00FFLL, 0x00FF00FF00FF00FFLL };
const int64_t vsplat32_1[2] = { 0x0000000100000001LL, 0x0000000100000001LL };
const int64_t vsplat32_2[2] = { 0x0000000200000002LL, 0x0000000200000002LL };

DEINTERLACE_METHOD *		deinterlace_methods[30];

static zf_consumer		consumer;
static gint			capture_format_id = -1;

static gboolean			capturing;
static gboolean			active;

static gboolean			reverse_fields;

typedef struct {
  TPicture			tpicture;
  zf_buffer *			buffer;
} picture;

static picture			pictures[MAX_PICTURE_HISTORY];
static TDeinterlaceInfo		info;
static guint			queue_len2;
static guint			field_count;

static long			cpu_feature_flags;

static DEINTERLACE_METHOD *	method;

DEINTERLACE_METHOD *
deinterlace_find_method		(const gchar *		name)
{
  guint i;

  g_return_val_if_fail (NULL != name, NULL);

  for (i = 0; i < G_N_ELEMENTS (deinterlace_methods); ++i)
    if ((method = deinterlace_methods[i]))
      if (0 == g_ascii_strcasecmp (name, method->szName))
	{
	  return method;
	}

  return NULL;
}

static guint
deinterlace			(zimage *		dst0,
				 zimage *		dst1)
{
  const tv_video_standard *std;
  gboolean bottom_first;
  zf_buffer *b1;
  zf_buffer *b2;
  TPicture *tp;
  picture *p;
  capture_frame *cf;
  const zimage *src;
  guint n_frames;

  if (0)
    {
      fputc ('=', stderr);
      fflush (stderr);
    }

  std = tv_cur_video_standard (zapping->info);
  bottom_first = (std && (std->videostd_set & TV_VIDEOSTD_SET_NTSC));
  bottom_first ^= reverse_fields;

  b1 = NULL;
  b2 = NULL;

  for (;;)
    {
      if (b2)
	{
	  zf_send_empty_buffer (&consumer, b2);
	}

      b2 = b1;

      if (!(b1 = zf_recv_full_buffer (&consumer)))
	break;
    }

  if (!b2)
    return 0;

  cf = PARENT (b2, capture_frame, b);
  src = retrieve_frame (cf, TV_PIXFMT_YUYV, /* copy */ FALSE);

  if (!src)
    {
      zf_send_empty_buffer (&consumer, b2);
      return 0;
    }

  n_frames = 0;

  /* First field. */

  tp = info.PictureHistory[queue_len2 - 1];
  p = PARENT (tp, picture, tpicture);

  if (p->buffer)
    {
      zf_send_empty_buffer (&consumer, p->buffer);
      p->buffer = NULL;
    }

  memmove (info.PictureHistory + 1,
	   info.PictureHistory + 0,
	   ((G_N_ELEMENTS (info.PictureHistory) - 1)
	    * sizeof (info.PictureHistory[0])));

  info.PictureHistory[0] = tp;

  p->buffer = b2;

  if (bottom_first)
    {
      tp->pData = ((uint8_t *) src->img) + info.FrameWidth * 2;
      tp->Flags = PICTURE_INTERLACED_ODD; /* sic */
    }
  else
    {
      tp->pData = src->img;
      tp->Flags = PICTURE_INTERLACED_EVEN; /* sic */
    }

  tp->IsFirstInSeries = (0 == field_count);

  ++field_count;

  info.Overlay = dst0->img;

  if (field_count >= (guint) method->nFieldsRequired)
    {
      if (method->pfnAlgorithm (&info))
	n_frames = 1;
    }

  /* Second field. */

  tp = info.PictureHistory[queue_len2 - 1];
  p = PARENT (tp, picture, tpicture);

  memmove (info.PictureHistory + 1,
	   info.PictureHistory + 0,
	   ((G_N_ELEMENTS (info.PictureHistory) - 1)
	    * sizeof (info.PictureHistory[0])));

  info.PictureHistory[0] = tp;

  if (bottom_first)
    {
      tp->pData = src->img;
      tp->Flags = PICTURE_INTERLACED_EVEN; /* sic */
    }
  else
    {
      tp->pData = ((uint8_t *) src->img) + info.FrameWidth * 2;
      tp->Flags = PICTURE_INTERLACED_ODD; /* sic */
    }

  tp->IsFirstInSeries = FALSE;

  ++field_count;

  if (n_frames > 0)
    info.Overlay = dst1->img;

  if (field_count >= (guint) method->nFieldsRequired)
    {
      if (method->pfnAlgorithm (&info))
	++n_frames;
    }

  return n_frames;
}

static gboolean
stop_thread			(void)
{
  zf_buffer *b;
  guint i;

  for (i = 0; i < G_N_ELEMENTS (pictures); ++i)
    if (pictures[i].buffer)
      {
	zf_send_empty_buffer (&consumer, pictures[i].buffer);
	pictures[i].buffer = NULL;
      }

  field_count = 0;

  while ((b = zf_recv_full_buffer (&consumer)))
    zf_send_empty_buffer (&consumer, b);

  zf_rem_consumer (&consumer);

  if (-1 != capture_format_id)
    release_capture_format (capture_format_id);
  capture_format_id = -1;

  remove_display_filter (deinterlace);

  return TRUE;
}

static gboolean
start_thread1			(void)
{
  gchar *s;
  capture_mode old_mode;
  guint resolution;
  const tv_video_standard *std;
  guint display_height;
  guint capture_width;
  guint capture_height;
  guint i;

  if (0)
    fprintf (stderr, "Start deinterlace thread\n");

  s = NULL;
  z_gconf_get_string (&s, GCONF_DIR "/method");

  if (!s)
    return FALSE;

  method = deinterlace_find_method (s);

  g_free (s);
  s = NULL;

  if (!method)
    return FALSE;

  s = NULL;
  z_gconf_get_string (&s, GCONF_DIR "/resolution");

  resolution = 4;

  if (s)
    {
      /* Error ignored. */
      gconf_string_to_enum (resolution_enum, s, &resolution);
      resolution = SATURATE (resolution, 1, 4);
    }

  if (0)
    fprintf (stderr, "Method %s (%s), resolution %u\n",
	     _(method->szName), method->szName, resolution);

  old_mode = tv_get_capture_mode (zapping->info);
  if (CAPTURE_MODE_READ != old_mode)
    if (-1 == zmisc_switch_mode (zapping->display_mode,
				 CAPTURE_MODE_READ,
				 zapping->info))
      {
	goto failure;
      }

  if ((std = tv_cur_video_standard (zapping->info)))
    {
      capture_width = (std->frame_width * resolution) >> 2;
      capture_height = std->frame_height;
    }
  else
    {
      capture_width = (768 * resolution) >> 2;
      capture_height = 576;
    }

  display_height = capture_height;
  if (method->bIsHalfHeight)
    display_height >>= 1;

  if (!add_display_filter (deinterlace,
			   TV_PIXFMT_YUYV,
			   capture_width,
			   display_height / DI_MAIN_HEIGHT_DIV))
    {
      g_warning ("Cannot add display filter\n");
      goto failure1;
    }

  capture_format_id = request_capture_format (zapping->info,
					      capture_width,
					      capture_height,
					      TV_PIXFMT_SET (TV_PIXFMT_YUYV),
					      REQ_SIZE | REQ_PIXFMT);
  if (-1 == capture_format_id)
    {
      z_show_non_modal_message_dialog
	(/* parent */ NULL,
	 GTK_MESSAGE_ERROR,
	 _("Could not start deinterlace plugin"),
	 _("Could not switch to capture format YUYV %u x %u."),
	 capture_width, capture_height);

      goto failure2;
    }

  zf_add_consumer (&capture_fifo, &consumer);

  CLEAR (info);
  
  info.Version = DEINTERLACE_INFO_CURRENT_VERSION;

  for (i = 0; i < MAX_PICTURE_HISTORY; ++i)
    info.PictureHistory[i] = &pictures[i].tpicture;

  info.Overlay = NULL;
  info.OverlayPitch = capture_width * 2;
  info.LineLength = capture_width * 2;
  info.FrameWidth = capture_width;
  info.FrameHeight = capture_height / DI_MAIN_HEIGHT_DIV;
  info.FieldHeight = capture_height / 2 / DI_MAIN_HEIGHT_DIV;
  info.CpuFeatureFlags = cpu_feature_flags;
  info.InputPitch = capture_width * 2 * 2;
  info.pMemcpy = (void *) tv_memcpy;

  /* XXX Not implemented yet. */
  assert (!method->bNeedFieldDiff);
  assert (!method->bNeedCombFactor);

  queue_len2 = (method->nFieldsRequired + 1) & -2;
  g_assert (queue_len2 <= MAX_PICTURE_HISTORY);

  field_count = 0;

  return TRUE;

 failure2:
  remove_display_filter (deinterlace);

 failure1:
  /* Error ignored. */
  zmisc_switch_mode (zapping->display_mode, old_mode, zapping->info);

 failure:
  return FALSE;
}

static void
plugin_capture_stop		(void)
{
  if (active)
    stop_thread ();

  active = FALSE;
  capturing = FALSE;
}

static void
plugin_capture_start		(void)
{
  capturing = TRUE;

  if (!active)
    active = start_thread1 ();
}

static void
notify				(GConfClient *		client _unused_,
				 guint			cnxn_id _unused_,
				 GConfEntry *		entry _unused_,
				 gpointer		user_data _unused_)
{
  if (0)
    fprintf(stderr, "Notify %d\n", capturing);

  if (capturing)
    {
      if (active)
	stop_thread ();

      active = start_thread1 ();
    }
}

static void
plugin_close			(void)
{
}

static void
properties_add			(GtkDialog *		dialog)
{
  static SidebarEntry se = {
    .label		= N_("Deinterlace"),
    .icon_name		= "interlace48.png",
    .create		= deinterlace_prefs_new,
    .cancel		= (void (*)(GtkWidget *)) deinterlace_prefs_cancel,
    .help_link_id	= "zapping-settings-deinterlace",
  };
  static const SidebarGroup sg = {
    N_("Plugins"), &se, 1
  };

  standard_properties_add (dialog, &sg, 1, /* glade */ NULL);
}

static gboolean
plugin_init			(PluginBridge		bridge _unused_,
				 tveng_device_info *	info _unused_)
{
  static const property_handler ph = {
    .add = properties_add,
  };
  long cpu_feature_flags;

  if (!(cpu_features & CPU_FEATURE_MMX))
    return FALSE;

  append_property_handler (&ph);

  cpu_feature_flags = 0;

  if (cpu_features & CPU_FEATURE_CMOV)
    cpu_feature_flags |= FEATURE_CMOV;
  if (cpu_features & CPU_FEATURE_MMX)
    cpu_feature_flags |= FEATURE_MMX;
  if (cpu_features & CPU_FEATURE_SSE)
    cpu_feature_flags |= FEATURE_SSE;
  if (cpu_features & CPU_FEATURE_SSE2)
    cpu_feature_flags |= FEATURE_SSE2;
  if (cpu_features & CPU_FEATURE_3DNOW)
    cpu_feature_flags |= FEATURE_3DNOW;
  if (cpu_features & CPU_FEATURE_3DNOW_EXT)
    cpu_feature_flags |= FEATURE_3DNOWEXT;

#undef GET
#define GET(x, y)							\
  {									\
    extern DEINTERLACE_METHOD *DI_##y##_GetDeinterlacePluginInfo (long); \
    deinterlace_methods[INDEX_##x] =					\
      DI_##y##_GetDeinterlacePluginInfo (cpu_feature_flags);		\
  }

  GET (VIDEO_BOB, VideoBob);
  GET (VIDEO_WEAVE, VideoWeave);
  GET (VIDEO_2FRAME, TwoFrame);
  GET (WEAVE, Weave);
  GET (BOB, Bob);
  GET (SCALER_BOB, ScalerBob);
  GET (EVEN_ONLY, EvenOnly);
  GET (ODD_ONLY, OddOnly);
  /* No longer supported  GET (BLENDED_CLIP, BlendedClip); */
  /* To do  GET (ADAPTIVE, Adaptive); */
  GET (VIDEO_GREEDY, Greedy);
  GET (VIDEO_GREEDY2FRAME, Greedy2Frame);
  GET (VIDEO_GREEDYH, GreedyH);
  /* To do - bNeedCombFactor
     GET (OLD_GAME, OldGame); */
  GET (VIDEO_TOMSMOCOMP, TomsMoComp);
  GET (VIDEO_MOCOMP2, MoComp2);

  z_gconf_notify_add (GCONF_DIR "/method", notify, NULL);
  z_gconf_notify_add (GCONF_DIR "/resolution", notify, NULL);

  z_gconf_auto_update_bool (&reverse_fields, GCONF_DIR "/reverse_fields");

  return TRUE;
}

static void
plugin_get_info			(const gchar **		canonical_name,
				 const gchar **		descriptive_name,
				 const gchar **		description,
				 const gchar **		short_description,
				 const gchar **		author,
				 const gchar **		version)
{
  if (canonical_name)
    *canonical_name = "deinterlace";
  if (descriptive_name)
    *descriptive_name = N_("Deinterlace plugin");
  if (description)
    *description = "";
  if (short_description)
    *short_description = "";
  if (author)
    *author = "";
  if (version)
    *version = "1.0";
}

static struct plugin_misc_info *
plugin_get_misc_info		(void)
{
  static struct plugin_misc_info returned_struct = {
    sizeof (struct plugin_misc_info),
    6, /* plugin priority */
    0 /* category */
  };

  return &returned_struct;
}

gboolean
plugin_get_symbol		(const gchar *		name,
				 gint			hash,
				 gpointer *		ptr)
{
  static const struct plugin_exported_symbol symbols [] = {
    SYMBOL (plugin_close, 0x1234),
    SYMBOL (plugin_capture_stop, 0x1234),
    SYMBOL (plugin_capture_start, 0x1234),
    SYMBOL (plugin_get_info, 0x1234),
    SYMBOL (plugin_get_misc_info, 0x1234),
    SYMBOL (plugin_init, 0x1234),
  };
  guint i;

  for (i = 0; i < N_ELEMENTS (symbols); ++i)
    if (0 == strcmp (symbols[i].symbol, name))
      {
	if (symbols[i].hash != hash)
	  {
	    if (ptr)
	      *ptr = GINT_TO_POINTER(0x3); /* hash collision code */
	    g_warning("Check error: \"%s\" in plugin %s "
		      "has hash 0x%x vs. 0x%x",
		      name, "teletext", symbols[i].hash, hash);
	    return FALSE;
	  }
	if (ptr)
	  *ptr = symbols[i].ptr;
	return TRUE;
      }

  if (ptr)
    *ptr = GINT_TO_POINTER(0x2);

  return FALSE;
}

gint
plugin_get_protocol		(void)
{
  return PLUGIN_PROTOCOL;
}
