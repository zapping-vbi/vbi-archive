/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000-2001 Iñaki García Etxebarria
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
 * This code is used to communicate with the VBI device (usually
 * /dev/vbi), so multiple plugins can access it simultaneously.
 * Libvbi, written by Michael Schimek, is used.
 */

/* XXX gdk_pixbuf_render_to_drawable -> gdk_draw_pixbuf() */
#undef GDK_DISABLE_DEPRECATED

#include <site_def.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <pthread.h>
#include <ctype.h>
#include <time.h>
#include <pthread.h>
#include <math.h>
#include "common/ucs-2.h"

#include "tveng.h"
/* Manages config values for zconf (it saves me some typing) */
#define ZCONF_DOMAIN "/zapping/options/vbi/"
#include "zconf.h"
#include "zvbi.h"
#include "zmisc.h"
#include "interface.h"
#include "v4linterface.h"
#include "ttxview.h"
#include "osd.h"
#include "callbacks.h"
#include "remote.h"
#include "globals.h"

#undef TRUE
#undef FALSE
#include "common/fifo.h"

#ifdef HAVE_LIBZVBI

/*
  Quality-speed tradeoff when scaling+antialiasing the page:
  - GDK_INTERP_NEAREST: Very fast scaling but visually pure rubbish
  - GDK_INTERP_TILES: Slower, and doesn't look very good at high (>2x)
			scalings
  - GDK_INTERP_BILINEAR: Very good quality, but slower than nearest
			and tiles.
  - GDK_INTERP_HYPER: Slower than Bilinear, slightly better at very
			high (>3x) scalings
*/
#define INTERP_MODE GDK_INTERP_BILINEAR

static vbi_decoder *vbi=NULL; /* holds the vbi object */
static ZModel * vbi_model=NULL; /* notify to clients the open/closure
				   of the device */

static vbi_wst_level teletext_level = VBI_WST_LEVEL_1p5;
static pthread_mutex_t network_mutex;
static pthread_mutex_t prog_info_mutex;
static gboolean station_name_known = FALSE;
static gchar station_name[256];
vbi_network current_network; /* current network info */
vbi_program_info program_info[2]; /* current and next program */

double zvbi_ratio = 4.0/3.0;

/* used by osd.c; use python caption_page to change this var */
vbi_pgno		zvbi_caption_pgno	= 0;

/**
 * The blink of items in the page is done by applying the patch once
 * every second (whenever the client wishes) to the appropiate places
 * in the screen.
 */
struct ttx_patch {
  int		col, row, width, height; /* geometry of the patch */
  GdkPixbuf	*unscaled_on;
  GdkPixbuf	*unscaled_off;
  GdkPixbuf	*scaled_on;
  GdkPixbuf	*scaled_off;
  gint		phase; /* flash phase 0 ... 19, 0..4 == off */
  gboolean	vanilla_only; /* apply just once */
  gboolean	dirty; /* needs reapplying */
};

struct ttx_client {
  zf_fifo		mqueue;
  zf_producer	mqueue_prod;
  zf_consumer	mqueue_cons;
  int		page, subpage; /* monitored page, subpage */
  int		id; /* of the client */
  pthread_mutex_t mutex;
  vbi_page	fpg; /* formatted page */
  GdkPixbuf	*unscaled_on; /* unscaled version of the page (on) */
  GdkPixbuf	*unscaled_off;
  GdkPixbuf	*scaled; /* scaled version of the page */
  int		w, h;
  int		freezed; /* do not refresh the current page */
  int		num_patches;
  int		reveal; /* whether to show hidden chars */
  struct ttx_patch *patches; /* patches to be applied */
  gboolean	waiting; /* waiting for the index page */
};

static GList *ttx_clients = NULL;
static pthread_mutex_t clients_mutex; /* FIXME: A rwlock is better for
				       this */

/* Corresponding to enum vbi_audio_mode */
gchar *zvbi_audio_mode_str[] =
{
  /* TRANSLATORS: VBI audio mode */
  N_("No Audio"),
  N_("Mono"),
  N_("Stereo"),
  N_("Stereo Surround"),
  N_("Simulated Stereo"),
  /* TRANSLATORS: Video description for the blind,
     on a secondary audio channel. */
  N_("Video Descriptions"),
  /* TRANSLATORS: Audio unrelated to the current
     program. */
  N_("Non-program Audio"),
  N_("Special Effects"),
  N_("Data Service"),
  N_("Unknown Audio"),
};

/*
 *  VBI core
 */

static pthread_t	decoder_id;
static pthread_t	capturer_id;
static gboolean		vbi_quit;
static gboolean		decoder_quit_ack;
static gboolean		capturer_quit_ack;
static zf_fifo		sliced_fifo;
static vbi_capture *	capture;

/* Attn: must be pthread_cancel-safe */

static void *
decoding_thread (void *p)
{
  zf_consumer c;

  D();

  assert (zf_add_consumer (&sliced_fifo, &c));

  /* setpriority (PRIO_PROCESS, 0, 5); */

  while (!vbi_quit) {
    zf_buffer *b;
    struct timeval now;
    struct timespec timeout;

    gettimeofday (&now, NULL);
    timeout.tv_sec = now.tv_sec + 1;
    timeout.tv_nsec = now.tv_usec * 1000;

    if (!(b = zf_wait_full_buffer_timeout (&c, &timeout)))
      continue;

    if (b->used <= 0) {
      zf_send_empty_buffer (&c, b);
/* FIXME this hurts
      if (b->used < 0)
	fprintf (stderr, "I/O error in decoding thread, aborting.\n");
      break;
*/
      continue;
    }

    pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, NULL);

    vbi_decode (vbi, (vbi_sliced *) b->data,
		b->used / sizeof(vbi_sliced), b->time);

    pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, NULL);

    zf_send_empty_buffer(&c, b);
  }

  zf_rem_consumer(&c);

  decoder_quit_ack = TRUE;

  return NULL;
}

/* Attn: must be pthread_cancel-safe */

static void *
capturing_thread (void *x)
{
  struct timeval timeout;  
  zf_producer p;
#if 0
  list stack;
  int stacked;

  init_list(&stack);
  glitch_time = v4l->time_per_frame * 1.25;
  stacked_time = 0.0;
  last_time = 0.0;
  stacked = 0;
#endif

  D();

  timeout.tv_sec = 1;
  timeout.tv_usec = 0;

  assert (zf_add_producer (&sliced_fifo, &p));

  while (!vbi_quit) {
    zf_buffer *b;
    int lines;

    b = zf_wait_empty_buffer (&p);

    switch (vbi_capture_read_sliced (capture, (vbi_sliced *) b->data,
				     &lines, &b->time, &timeout))
      {
      case 1: /* ok */
	break;

      case 0: /* timeout */
#if 0
	for (; stacked > 0; stacked--)
	  send_full_buffer (&p, PARENT (rem_head(&stack), buffer, node));
#endif
	b->used = -1;
	b->error = errno;
	b->errorstr = _("VBI interface timeout");

	zf_send_full_buffer (&p, b);

	goto abort;

      default: /* error */
#if 0
	for (; stacked > 0; stacked--)
	  send_full_buffer (&p, PARENT (rem_head(&stack), buffer, node));
#endif
	b->used = -1;
	b->error = errno;
	b->errorstr = _("VBI interface: Failed to read from the device");

	zf_send_full_buffer (&p, b);

	goto abort;
      }

    if (lines == 0) {
      ((vbi_sliced *) b->data)->id = VBI_SLICED_NONE;
      b->used = sizeof(vbi_sliced); /* zero means EOF */
    } else {
      b->used = lines * sizeof(vbi_sliced);
    }

#if 0 /* L8R */
    /*
     *  This curious construct compensates temporary shifts
     *  caused by an unusual delay between read() and
     *  the execution of gettimeofday(). A complete loss
     *  remains lost.
     */
    if (last_time > 0 &&
	(b->time - (last_time + stacked_time)) > glitch_time) {
      if (stacked >= (f->buffers.members >> 2)) {
	/* Not enough space &| hopeless desynced */
	for (stacked_time = 0.0; stacked > 0; stacked--) {
	  buffer *b = PARENT(rem_head(&stack), buffer, node);
	  send_full_buffer(&p, b);
	}
      } else {
	add_tail(&stack, &b->node);
	stacked_time += v4l->time_per_frame;
	stacked++;
	continue;
      }
    } else { /* (back) on track */ 
      for (stacked_time = 0.0; stacked > 0; stacked--) {
	buffer *b = PARENT(rem_head(&stack), buffer, node);
	b->time = last_time += v4l->time_per_frame; 
	send_full_buffer(&p, b);
      }
    }

    last_time = b->time;
#endif

    zf_send_full_buffer(&p, b);
  }

 abort:

  zf_rem_producer (&p);

  capturer_quit_ack = TRUE;

  return NULL;
}

#define SERVICES (VBI_SLICED_TELETEXT_B | VBI_SLICED_VPS | \
		  VBI_SLICED_CAPTION_625 | VBI_SLICED_CAPTION_525 | \
		  VBI_SLICED_WSS_625 | VBI_SLICED_WSS_CPR1204)

static gint
join (char *who, pthread_t id, gboolean *ack, gint timeout)
{
  vbi_quit = TRUE;

  /* Dirty. Where is pthread_try_join()? */
  for (; (!*ack) && timeout > 0; timeout--) {
    usleep (100000);
  }

  /* Ok, you asked for it */
  if (timeout == 0) {
    int r;

    printv("Unfriendly termination\n");
    r = pthread_cancel (id);
    if (r != 0)
      {
	printv("Cancellation of %s failed: %d\n", who, r);
	return 0;
      }
  }

  pthread_join (id, NULL);

  return timeout;
}

/* XXX vbi must be restarted on video std change. is it?*/

static gboolean
threads_init (const gchar *dev_name, int given_fd)
{
  gchar *failed = _("VBI initialization failed.\n%s");
  gchar *memory = _("Ran out of memory.");
  gchar *thread = _("Out of resources to start a new thread.");
#warning Linux only
  gchar *mknod_hint = _(
	"This probably means that the required driver isn't loaded. "
	"Add to your /etc/modules.conf the line:\n"
	"alias char-major-81-224 bttv (replace bttv by the name "
	"of your video driver)\n"
        "and with mknod create /dev/vbi0 appropriately. If this "
	"doesn't work, you can disable VBI in Settings/Preferences/VBI "
	"options/Enable VBI decoding.");
  unsigned int services = SERVICES;
  char *_errstr;
  vbi_raw_decoder *raw;
  int buffer_size;

  D();

  if (!(vbi = vbi_decoder_new()))
    {
      RunBox(failed, GTK_MESSAGE_ERROR, memory);
      return FALSE;
    }

  D();

#warning FIXME
  if (!(capture = vbi_capture_bktr_new (dev_name, /* XXX */ 625,
					&services, /* strict */ -1,
					&_errstr, !!debug_msg)))
    {
      if (_errstr)
	free (_errstr);

      if (!(capture = vbi_capture_v4l2_new (dev_name, 20 /* buffers */,
					    &services, /* strict */ -1,
					    &_errstr, !!debug_msg)))
	{
	  if (_errstr)
	    free (_errstr);

	  if (!(capture = vbi_capture_v4l_sidecar_new (dev_name, given_fd,
						       &services, /* strict */ -1,
						       &_errstr, !!debug_msg)))
	    {
	      gchar *t;

	      t = g_locale_to_utf8 (_errstr, -1, NULL, NULL, NULL);
	      g_assert (t != NULL);

	      if (errno == ENOENT || errno == ENXIO || errno == ENODEV)
		{
		  gchar *s = g_strconcat(t, "\n", mknod_hint, NULL);
	      
		  RunBox(failed, GTK_MESSAGE_ERROR, s);
		  g_free (s);
		}
	      else
		{
		  RunBox(failed, GTK_MESSAGE_ERROR, t);
		}

	      g_free (t);
	      free (_errstr);
	      vbi_decoder_delete (vbi);
	      vbi = NULL;
	      return FALSE;
	    }
	}
    }

  D();

  /* XXX when we have WSS625, disable video sampling*/

  raw = vbi_capture_parameters (capture);
  buffer_size = (raw->count[0] + raw->count[1]) * sizeof(vbi_sliced);

  if (!zf_init_buffered_fifo (&sliced_fifo, "vbi-sliced", 20, buffer_size))
    {
      ShowBox(failed, GTK_MESSAGE_ERROR, memory);
      vbi_capture_delete (capture);
      vbi_decoder_delete (vbi);
      vbi = NULL;
      return FALSE;
    }

  D();

  vbi_quit = FALSE;
  decoder_quit_ack = FALSE;
  capturer_quit_ack = FALSE;

  if (pthread_create (&decoder_id, NULL, decoding_thread, NULL))
    {
      ShowBox(failed, GTK_MESSAGE_ERROR, thread);
      zf_destroy_fifo (&sliced_fifo);
      vbi_capture_delete (capture);
      vbi_decoder_delete (vbi);
      vbi = NULL;
      return FALSE;
    }

  D();

  if (pthread_create (&capturer_id, NULL, capturing_thread, NULL))
    {
      ShowBox(failed, GTK_MESSAGE_ERROR, thread);
      join ("dec0", decoder_id, &decoder_quit_ack, 15);
      zf_destroy_fifo (&sliced_fifo);
      vbi_capture_delete (capture);
      vbi_decoder_delete (vbi);
      vbi = NULL;
      return FALSE;
    }

  D();

  return TRUE;
}

static void
threads_destroy (void)
{
  D();

  if (vbi)
    {
      D();
      join ("cap", capturer_id, &capturer_quit_ack, 15);
      D();
      join ("dec", decoder_id, &decoder_quit_ack, 15);
      D();
      zf_destroy_fifo (&sliced_fifo);
      D();
      vbi_capture_delete (capture);
      D();
      vbi_decoder_delete (vbi);

      vbi = NULL;
    }

  D();
}

/* handler for a vbi event */
static void event(vbi_event *ev, void *unused);

static void
on_vbi_prefs_changed		(const gchar *key,
				 gboolean *new_value,
				 gpointer data)
{
  /* Try to open the device */
  if (!vbi && *new_value)
    {
      D();
      if (!zvbi_open_device(zcg_char(NULL, "vbi_device")))
	{
	  /* Define in site_def.h if you don't want this to happen */
#ifndef DO_NOT_DISABLE_VBI_ON_FAILURE
	  zcs_bool(FALSE, "enable_vbi");
#endif	  
	}
      D();
    }
  if (vbi && !*new_value)
    {
      D();
      /* TTX mode */
      if (main_info->current_mode == TVENG_TELETEXT
	  || main_info->current_mode == TVENG_NO_CAPTURE)
	zmisc_switch_mode(TVENG_CAPTURE_WINDOW, main_info);
      zvbi_close_device();
      D();
    }

  /* disable VBI if needed */
  vbi_gui_sensitive(!!zvbi_get_object());
}

int osd_pipe[2];

static void cc_event(vbi_event *ev, void *data)
{
  int *pipe = data;

  switch (ev->type)
    {
      case VBI_EVENT_TTX_PAGE:
        if (ev->ev.ttx_page.pgno != zvbi_caption_pgno)
          return;
	break;

      case VBI_EVENT_CAPTION:
        if (ev->ev.caption.pgno != zvbi_caption_pgno)
          return;
	break;

      default:
        return;
    }

  /* Shouldn't block when the pipe buffer is full... ? */
  write(pipe[1], "", 1);
}

#if 0 /* temporarily disabled */

/* Trigger handling */
static gint trigger_timeout_id = -1;
static gint trigger_client_id = -1;
static vbi_link last_trigger;

static void
on_trigger_clicked		(GtkWidget *		widget,
				 vbi_link *		trigger)
{
  GError *err = NULL;
  switch (trigger->type)
    {
    case VBI_LINK_HTTP:
    case VBI_LINK_FTP:
    case VBI_LINK_EMAIL:
      gnome_url_show(trigger->url, &err);
      if (err)
	{
	  /* TRANSLATORS: "Cannot open <URL>" */
	  ShowBox (_("Cannot open %s:\n%s"), GTK_MESSAGE_ERROR,
	  trigger->url, err->message);
	  g_error_free (err);
	}
      break;

    case VBI_LINK_PAGE:
    case VBI_LINK_SUBPAGE:
      python_command_printf (widget,
			     "zapping.ttx_open_new(%x, %d)",
			     trigger->pgno,
			     (trigger->subno <= 0 || trigger->subno > 0x99) ?
			     vbi_bcd2dec (trigger->subno) : -1);
      break;

    case VBI_LINK_LID:
    case VBI_LINK_TELEWEB:
    case VBI_LINK_MESSAGE:
      /* ignore */
      break;
      
    default:
      ShowBox("Unhandled trigger type %d, please contact the maintainer",
	      GTK_MESSAGE_WARNING, trigger->type);
      break;
    }
}

static void
acknowledge_trigger			(vbi_link	*link)
{
  GtkWidget *button;
  gchar *buffer;
  GtkWidget *pix;
  gint filter_level = 9;
  gint action = zcg_int(NULL, "trigger_default");

  switch (zcg_int(NULL, "filter_level"))
    {
    case 0: /* High priority */
      filter_level = 2;
      break;
    case 1: /* Medium, high */
      filter_level = 5;
      break;
    default:
      break;
    }

  if (link->priority > filter_level)
    return;

  switch (link->itv_type)
    {
    case VBI_WEBLINK_PROGRAM_RELATED:
      action = zcg_int(NULL, "pr_trigger");
      break;
    case VBI_WEBLINK_NETWORK_RELATED:
      action = zcg_int(NULL, "nw_trigger");
      break;
    case VBI_WEBLINK_STATION_RELATED:
      action = zcg_int(NULL, "st_trigger");
      break;
    case VBI_WEBLINK_SPONSOR_MESSAGE:
      action = zcg_int(NULL, "sp_trigger");
      break;
    case VBI_WEBLINK_OPERATOR:
      action = zcg_int(NULL, "op_trigger");
      break;
    default:
      break;
    }

  if (link->autoload)
    action = 2;

  if (!action) /* ignore */
    return;

  if (action == 2) /* open automagically */
    {
      on_trigger_clicked(NULL, link);
      return;
    }
#warning
  /*  pix = gtk_image_new_from_stock (link->eacem ?
  				  "zapping-eacem-icon" : "zapping-atvef-icon",
  				  GTK_ICON_SIZE_BUTTON);*/
  pix=0;

  if (pix)
    {
      gtk_widget_show (pix);
      button = gtk_button_new ();
      gtk_container_add (GTK_CONTAINER (button), pix);
    }
  else /* pixmaps not installed */
    {
      /* click me, tsk tsk. */
      /*      button = gtk_button_new_with_label (_("Click me"));*/
      button = gtk_button_new_with_label ("  ");
    }

  /* FIXME: Show more fields (type, itv...)
   * {mhs}:
   * type is not for users.
   * nuid identifies the network (or 0), same code as vbi_network.nuid.
   *   Only for PAGE/SUBPAGE links or triggers.
   * expires is the time (seconds and fractions since epoch (-> time_t/timeval))
   *   until the target is valid.
   * autoload requests to open the target without user
   *   confirmation (ATVEF flag).
   * itv_type (ATVEF) and priority (EACEM) are intended for filtering
   *   -> preferences, and display priority. EACEM priorities are:
   *	 emergency = 0 (never blocked)
   *     high = 1 or 2
   *     medium = 3, 4 or 5
   *     low = 6, 7, 8 or 9 (default 9)
   */
  memcpy(&last_trigger, link, sizeof(last_trigger));
  g_signal_connect(G_OBJECT(button), "clicked",
		     G_CALLBACK(on_trigger_clicked),
		     &last_trigger);

  gtk_widget_show(button);
  switch (link->type)
    {
    case VBI_LINK_HTTP:
    case VBI_LINK_FTP:
    case VBI_LINK_EMAIL:
      if (link->name[0])
        buffer = g_strdup_printf(" %s", link->name /* , link->url */);
      else
        buffer = g_strdup_printf(" %s", link->url);
      z_tooltip_set(button,
		  _("Open this link with the predetermined Web browser.\n"
		    "You can configure this in the GNOME Control Center, "
		    "under Handlers/Url Navigator"));
      break;
    case VBI_LINK_PAGE:
    case VBI_LINK_SUBPAGE:
      if (link->name[0])
        buffer = g_strdup_printf(_(" %s: Teletext Page %x"),
				 link->name, link->pgno);
      else
        buffer = g_strdup_printf(_(" Teletext Page %x"), link->pgno);
      z_tooltip_set(button, _("Open this page with Zapzilla"));
      break;
    case VBI_LINK_MESSAGE:
      buffer = g_strdup_printf(" %s", link->name);
      gtk_widget_set_sensitive(button, FALSE);
      break;
    default:
      ShowBox("Unhandled link type %d, please contact the maintainer",
	      GTK_MESSAGE_WARNING, link->type);
      buffer = g_strdup_printf("%s", link->name);
      break;
    }
  z_status_set_widget(button);
  z_status_print(buffer, /* markup */ FALSE, -1);
  g_free(buffer);
}

#endif /* 0 */

#if 0 /* temporarily disabled */

static void
update_main_title (void)
{
  tveng_tuned_channel *channel;
  gchar *name = NULL;

  if (*current_network.name)
    {
      name = current_network.name;
      /* FIXME the nn encoding should be UTF-8, but isn't
	 really defined. g_convert returns NULL on failure. */
      name = g_convert (name, strlen (name),
			"UTF-8", "ISO-8859-1",
			NULL, NULL, NULL);
    }
  else
    {
      /* switch away from known network */
    }

  channel = tveng_tuned_channel_nth (global_channel_list,
				     cur_tuned_channel);
  if (!channel)
    z_set_main_title (NULL, name);
  else if (!channel->name)
    z_set_main_title (channel, name);

  g_free (name);
}

#endif

#if 0 /* temporarily disabled */

static gint trigger_timeout		(gint	client_id)
{
  enum ttx_message msg;
  ttx_message_data data;

  while ((msg = peek_ttx_message(client_id, &data)))
    switch(msg)
      {
      case TTX_PAGE_RECEIVED:
      case TTX_CHANNEL_SWITCHED:
	break;
      case TTX_NETWORK_CHANGE:
      case TTX_PROG_INFO_CHANGE:
	update_main_title();
	break;
      case TTX_BROKEN_PIPE:
	g_warning("Broken TTX pipe");
	trigger_timeout_id = -1;
	return FALSE;
      case TTX_TRIGGER:
	acknowledge_trigger((vbi_link*)(&data.data));
	break;
      default:
	g_warning("Unknown message: %d", msg);
	break;
      }

  return TRUE;
}

#endif /* 0 */

/* Open the configured VBI device, FALSE on error */
gboolean
zvbi_open_device(const char *device)
{
  gint index;
  int given_fd;
  static int region_mapping[8] = {
    0, /* WCE */
    8, /* EE */
    16, /* WET */
    24, /* CSE */
    32, /* C */
    48, /* GC */
    64, /* A */
    80 /* I */
  };

#ifdef HAVE_LIBZVBI
  D();
  if (main_info)
    given_fd = main_info->fd;
  else
    given_fd = -1;
  D();
  if (!threads_init (device, given_fd))
    return FALSE;
  D();
  /* Enter something valid or accept the default */
  index = zcg_int(NULL, "default_region");
  if (index >= 0 && index <= 7)
    vbi_teletext_set_default_region(vbi, region_mapping[index]);
  index = zcg_int(NULL, "teletext_level");
  if (index >= 0 && index <= 3)
    {
      vbi_teletext_set_level(vbi, index);
      teletext_level = index;
    }
  D();
  /* Send all events to our main event handler */
  g_assert(vbi_event_handler_add(vbi, ~0, event, NULL) != 0);
  D();
  pthread_mutex_init(&clients_mutex, NULL);
  D();
  zmodel_changed(vbi_model);
  D();
  /* Send OSD relevant events to our OSD event handler */
  g_assert(vbi_event_handler_add(vbi,
				 VBI_EVENT_CAPTION | VBI_EVENT_TTX_PAGE,
				 cc_event, osd_pipe) != 0);
  D();
#if 0 /* temporarily disabled */
  if (trigger_client_id >= 0)
    unregister_ttx_client(trigger_client_id);
  if (trigger_timeout_id >= 0)
    gtk_timeout_remove(trigger_timeout_id);
  trigger_client_id = register_ttx_client();
  trigger_timeout_id = gtk_timeout_add(100, (GtkFunction)trigger_timeout,
				       GINT_TO_POINTER(trigger_client_id));
#endif
  return TRUE;
#endif

  return FALSE;
}

/* down the road */
static void remove_client(struct ttx_client* client);

/* Closes the VBI device */
void
zvbi_close_device(void)
{
  GList * destruction;

  D();

  if (!vbi) /* disabled */
    return;

#if 0 /* temporarily disabled */
  if (trigger_timeout_id >= 0)
    gtk_timeout_remove(trigger_timeout_id);
  if (trigger_client_id >= 0)
    unregister_ttx_client(trigger_client_id);

  D();

  trigger_client_id = trigger_timeout_id = -1;
#endif

  pthread_mutex_lock(&clients_mutex);

  D();

  destruction = g_list_first(ttx_clients);
  while (destruction)
    {
      remove_client((struct ttx_client*) destruction->data);
      destruction = destruction->next;
    }
  g_list_free(ttx_clients);
  ttx_clients = NULL;
  pthread_mutex_unlock(&clients_mutex);
  pthread_mutex_destroy(&clients_mutex);

  D();
 
  threads_destroy ();

  D();

  zmodel_changed(vbi_model);

  D();
}

static void
send_ttx_message(struct ttx_client *client,
		 enum ttx_message message,
		 void *data, int bytes)
{
  ttx_message_data *d;
  zf_buffer *b;
  
  b = zf_wait_empty_buffer(&client->mqueue_prod);

  d = (ttx_message_data*)b->data;
  d->msg = message;
  g_assert(bytes <= sizeof(ttx_message_data));

#if 0 /* temporarily disabled */
  if (data)
    memcpy(&(d->data), data, bytes);
#endif

  zf_send_full_buffer(&client->mqueue_prod, b);
}

static void
remove_client(struct ttx_client *client)
{
  gint i;

  zf_destroy_fifo(&client->mqueue);
  pthread_mutex_destroy(&client->mutex);
  g_object_unref(client->unscaled_on);
  g_object_unref(client->unscaled_off);
  if (client->scaled)
    g_object_unref(client->scaled);
  for (i = 0; i<client->num_patches; i++)
    {
      g_object_unref(client->patches[i].unscaled_on);
      g_object_unref(client->patches[i].unscaled_off);
      if (client->patches[i].scaled_on)
	g_object_unref(client->patches[i].scaled_on);
      if (client->patches[i].scaled_off)
	g_object_unref(client->patches[i].scaled_off);
    }
  if (client->num_patches)
    g_free(client->patches);
  g_free(client);
}

static GdkPixbuf *
vt_loading			(void)
{
  gchar *filename;
  GdkPixbuf *pixbuf;

  filename = g_strdup_printf("%s/vt_loading%d.jpeg",
			     PACKAGE_PIXMAPS_DIR,
			     (rand() % 2) + 1);

  /* Errors will be detected later on */
  pixbuf = gdk_pixbuf_new_from_file (filename, NULL);

  g_free (filename);

  return pixbuf;
}

/* returns the id */
int
register_ttx_client(void)
{
  static int id;
  struct ttx_client *client;
  int w, h; /* of the unscaled image */
  GdkPixbuf *simple;

  client = g_malloc0(sizeof(*client));

  client->id = id++;
  client->reveal = 0;
  client->waiting = TRUE;
  pthread_mutex_init(&client->mutex, NULL);
  vbi_get_max_rendered_size(&w, &h);
  client->unscaled_on = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, w,
				    h);
  client->unscaled_off = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, w,
				    h);
  
  g_assert(client->unscaled_on != NULL);
  g_assert(client->unscaled_off != NULL);
  
  if ((simple = vt_loading ()))
    {
      gdk_pixbuf_scale(simple,
		       client->unscaled_on, 0, 0, w, h,
		       0, 0,
		       (double) w / gdk_pixbuf_get_width(simple),
		       (double) h / gdk_pixbuf_get_height(simple),
		       zcg_int(NULL, "qstradeoff"));
      z_pixbuf_copy_area(client->unscaled_on, 0, 0, w, h,
			   client->unscaled_off, 0, 0);
      g_object_unref(simple);
    }

  g_assert(zf_init_buffered_fifo(
           &client->mqueue, "zvbi-mqueue",
	   16, sizeof(ttx_message_data)) > 0);

  zf_add_producer(&client->mqueue, &client->mqueue_prod);
  zf_add_consumer(&client->mqueue, &client->mqueue_cons);

  pthread_mutex_lock(&clients_mutex);
  ttx_clients = g_list_append(ttx_clients, client);
  pthread_mutex_unlock(&clients_mutex);
  return client->id;
}

static inline struct ttx_client*
find_client(int id)
{
  GList *find;

  find= g_list_first(ttx_clients);
  while (find)
    {
      if (((struct ttx_client*)find->data)->id == id)
	return ((struct ttx_client*)find->data);

      find = find->next;
    }

  return NULL; /* not found */
}

void
set_ttx_parameters(int id, int reveal)
{
  struct ttx_client *client;

  pthread_mutex_lock(&clients_mutex);
  if ((client = find_client(id)))
    {
      client->reveal = reveal;
    }
  pthread_mutex_unlock(&clients_mutex);
}

vbi_page *
get_ttx_fmt_page(int id)
{
  struct ttx_client *client;
  vbi_page *pg = NULL;

  pthread_mutex_lock(&clients_mutex);

  if ((client = find_client(id)))
    pg = &client->fpg;

  pthread_mutex_unlock(&clients_mutex);

  return pg;
}

GdkPixbuf *
get_scaled_ttx_page (int id)
{
  GdkPixbuf *result = NULL;
  struct ttx_client *client;

  pthread_mutex_lock(&clients_mutex);
  if ((client = find_client(id)))
    result = client->scaled;
  pthread_mutex_unlock(&clients_mutex);

  return result;
}

enum ttx_message
peek_ttx_message(int id, ttx_message_data *data)
{
  struct ttx_client *client;
  zf_buffer *b;
  enum ttx_message message;
  ttx_message_data *d;

  pthread_mutex_lock(&clients_mutex);

  if ((client = find_client(id)))
    {
      b = zf_recv_full_buffer(&client->mqueue_cons);
      if (b)
	{
	  d = (ttx_message_data*)b->data;
	  message = d->msg;
	  memcpy(data, d, sizeof(ttx_message_data));
	  zf_send_empty_buffer(&client->mqueue_cons, b);
	}
      else
	message = TTX_NONE;
    }
  else
    message = TTX_BROKEN_PIPE;

  pthread_mutex_unlock(&clients_mutex);

  return message;
}

enum ttx_message
get_ttx_message(int id, ttx_message_data *data)
{
  struct ttx_client *client;
  zf_buffer *b;
  enum ttx_message message;
  ttx_message_data *d;

  pthread_mutex_lock(&clients_mutex);

  if ((client = find_client(id)))
    {
      b = zf_wait_full_buffer(&client->mqueue_cons);
      g_assert(b != NULL);
      d = (ttx_message_data*)b->data;
      message = d->msg;
      memcpy(data, d, sizeof(ttx_message_data));
      zf_send_empty_buffer(&client->mqueue_cons, b);
    }
  else
    message = TTX_BROKEN_PIPE;

  pthread_mutex_unlock(&clients_mutex);

  return message;
}

static void
clear_message_queue(struct ttx_client *client)
{
  zf_buffer *b;

  while ((b=zf_recv_full_buffer(&client->mqueue_cons)))
    zf_send_empty_buffer(&client->mqueue_cons, b);
}

void
unregister_ttx_client(int id)
{
  GList *search;

  pthread_mutex_lock(&clients_mutex);
  search = g_list_first(ttx_clients);
  while (search)
    {
      if (((struct ttx_client*)search->data)->id == id)
	{
	  remove_client((struct ttx_client*)search->data);
	  ttx_clients = g_list_remove(ttx_clients, search->data);
	  break;
	}
      search = search->next;
    }
  pthread_mutex_unlock(&clients_mutex);
}

/* Won't change */
#define CW		12		
#define CH		10

static void
add_patch(struct ttx_client *client, int col, int row,
	  vbi_char *ac, gboolean vanilla_only)
{
  struct ttx_patch patch, *destiny = NULL;
  gint sw, sh; /* scaled dimensions */
  gint i;

  /* avoid duplicate patches */
  for (i = 0; i < client->num_patches; i++)
    if (client->patches[i].col == col &&
	client->patches[i].row == row)
      {
	destiny = &client->patches[i];
	if (destiny->scaled_on)
	  g_object_unref(G_OBJECT (destiny->scaled_on));
	if (destiny->scaled_off)
	  g_object_unref(G_OBJECT (destiny->scaled_off));
	g_object_unref(G_OBJECT (destiny->unscaled_on));
	g_object_unref(G_OBJECT (destiny->unscaled_off));
	break;
      }
      
  CLEAR (patch);
  patch.width = patch.height = 1;
  patch.col = col;
  patch.row = row;
  patch.vanilla_only = vanilla_only;
  patch.dirty = TRUE;

  switch (ac->size)
    {
    case VBI_DOUBLE_WIDTH:
      patch.width = 2;
      break;
    case VBI_DOUBLE_HEIGHT:
      patch.height = 2;
      break;
    case VBI_DOUBLE_SIZE:
      patch.width = 2;
      patch.height = 2;
      break;
    default:
      break;
    }

  sw = (((double) client->w * (patch.width * CW + 10))
	/ gdk_pixbuf_get_width(client->unscaled_on));
  sh = (((double) client->h * (patch.height * CH + 10))
	/ gdk_pixbuf_get_height(client->unscaled_on));

  patch.unscaled_on = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
				     patch.width*CW+10, patch.height*CH+10);
  patch.unscaled_off = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
				      patch.width*CW+10, patch.height*CH+10);
  g_assert(patch.unscaled_on != NULL);
  g_assert(patch.unscaled_off != NULL);

  z_pixbuf_copy_area(client->unscaled_on, patch.col*CW-5,
		       patch.row*CH-5, patch.width*CW+10,
		       patch.height*CH+10, patch.unscaled_on, 0, 0);
  z_pixbuf_copy_area(client->unscaled_off, patch.col*CW-5,
		     patch.row*CH-5, patch.width*CW+10,
		     patch.height*CH+10, patch.unscaled_off, 0, 0);

  if (client->w > 0  &&  client->h > 0  && sh > 0)
    {
      patch.scaled_on =
	z_pixbuf_scale_simple(patch.unscaled_on,
			      sw, sh, zcg_int(NULL, "qstradeoff"));
      patch.scaled_off =
	z_pixbuf_scale_simple(patch.unscaled_off, sw, sh,
			      zcg_int(NULL, "qstradeoff"));
    }

  if (!destiny)
    {
      client->patches = g_realloc(client->patches, sizeof(struct ttx_patch)*
				  (client->num_patches+1));
      memcpy(client->patches+client->num_patches, &patch,
	     sizeof(struct ttx_patch));
      client->num_patches++;
    }
  else
    memcpy(destiny, &patch, sizeof(struct ttx_patch));
}

/**
 * Resizes the set of patches to fit in the new geometry
 */
static void
resize_patches(struct ttx_client *client)
{
  gint i;
  gint sw, sh;

  for (i=0; i<client->num_patches; i++)
    {
      sw = (((double)client->w*(client->patches[i].width*CW+10))/
	gdk_pixbuf_get_width(client->unscaled_on));
      sh = (((double)client->h*(client->patches[i].height*CH+10))/
	gdk_pixbuf_get_height(client->unscaled_on));

      if (client->patches[i].scaled_on)
	g_object_unref(G_OBJECT (client->patches[i].scaled_on));
      client->patches[i].scaled_on =
	z_pixbuf_scale_simple(client->patches[i].unscaled_on,
				sw, sh, zcg_int(NULL, "qstradeoff"));
      if (client->patches[i].scaled_off)
	g_object_unref(G_OBJECT (client->patches[i].scaled_off));
      client->patches[i].scaled_off =
	z_pixbuf_scale_simple(client->patches[i].unscaled_off,
				sw, sh, zcg_int(NULL, "qstradeoff"));
      client->patches[i].dirty = TRUE;
    }
}

/**
 * Scans the current page of the client and builds the appropiate set
 * of patches.
 */
static void
build_patches(struct ttx_client *client)
{
  gint i;
  gint col, row;
  vbi_char *ac;

  for (i = 0; i<client->num_patches; i++)
    {
      if (client->patches[i].scaled_on)
	g_object_unref(G_OBJECT (client->patches[i].scaled_on));
      if (client->patches[i].scaled_off)
	g_object_unref(G_OBJECT (client->patches[i].scaled_off));
      g_object_unref(G_OBJECT (client->patches[i].unscaled_on));
      g_object_unref(G_OBJECT (client->patches[i].unscaled_off));
    }
  g_free(client->patches);
  client->patches = NULL;
  client->num_patches = 0;

  /* FIXME: This is too cumbersome, something more smart is needed */
  for (col = 0; col < client->fpg.columns; col++)
    for (row = 0; row < client->fpg.rows; row++)
      {
	ac = &client->fpg.text[row * client->fpg.columns + col];
	if ((ac->flash) && (ac->size <= VBI_DOUBLE_SIZE))
	  add_patch(client, col, row, ac, FALSE);
      }
}

/**
 * Applies the patches into the page.
 */
static void
refresh_ttx_page_intern(struct ttx_client *client, GtkWidget *drawable)
{
  int i;
  struct ttx_patch *p;
  GdkPixbuf *scaled;
  gint x, y;
  double sx, sy;

  for (i=0; i<client->num_patches; i++)
    {
      p = &(client->patches[i]);
      if (!p->vanilla_only)
	{
	  p->phase = (p->phase + 1) % 20;
	  if ((p->phase != 0) && (p->phase != 5))
	    continue;

	  if (p->phase == 5)
	    scaled = p->scaled_on;
	  else /* phase == 0 */
	    scaled = p->scaled_off;
	}
      else
	{
	  if (!p->dirty || !p->scaled_on)
	    continue;
	  p->dirty = FALSE;
	  scaled = p->scaled_on;
	}
      
      /* Update the scaled version of the page */
      if ((scaled) && (client->scaled) && (client->w > 0)
	  && (client->h > 0))
	{
	  x = (((double)client->w*p->col)/client->fpg.columns);
	  y = (((double)client->h*p->row)/client->fpg.rows);
	  sx = ((double)gdk_pixbuf_get_width(scaled)/
	    gdk_pixbuf_get_width(p->unscaled_on))*5.0;
	  sy = ((double)gdk_pixbuf_get_height(scaled)/
	    gdk_pixbuf_get_height(p->unscaled_on))*5.0;
	  
	  z_pixbuf_copy_area(scaled, (int)(sx), (int)(sy),
			     (int)(gdk_pixbuf_get_width(scaled)-sx*2),
			     (int)(gdk_pixbuf_get_height(scaled)-sy*2),
			     client->scaled,
			     x, y);
	  
	  if (drawable)
	    gdk_window_clear_area_e(drawable->window, x, y,
				    (int)(gdk_pixbuf_get_width(scaled)-sx*2),
				    (int)(gdk_pixbuf_get_height(scaled)-sy*2));
	}
    }
}

void
refresh_ttx_page(int id, GtkWidget *drawable)
{
  struct ttx_client *client;

  pthread_mutex_lock(&clients_mutex);
  if ((client = find_client(id)))
    {
      pthread_mutex_lock(&client->mutex);
      refresh_ttx_page_intern(client, drawable);
      pthread_mutex_unlock(&client->mutex);
    }
  pthread_mutex_unlock(&clients_mutex);
}

static int
build_client_page(struct ttx_client *client, vbi_page *pg)
{
  GdkPixbuf *simple;

  if (!vbi)
    return 0;

  pthread_mutex_lock(&client->mutex);
  if (pg && pg != (vbi_page *) -1)
    {
      memcpy(&client->fpg, pg, sizeof(client->fpg));
      vbi_draw_vt_page(&client->fpg,
		       VBI_PIXFMT_RGBA32_LE,
		       (uint32_t *) gdk_pixbuf_get_pixels(client->unscaled_on),
		       client->reveal, 1 /* flash_on */);
      vbi_draw_vt_page_region(&client->fpg,
			      VBI_PIXFMT_RGBA32_LE,
			      (uint32_t *) gdk_pixbuf_get_pixels(client->unscaled_off),
			      -1 /* rowstride */,
			      0 /* column */, 0 /* row */,
			      client->fpg.columns, client->fpg.rows,
			      client->reveal, 0 /* flash_on */);
      client->waiting = FALSE;
    }
  else if (!pg)
    {
      CLEAR (client->fpg);

      if ((simple = vt_loading ()))
	{
	  gdk_pixbuf_scale(simple,
			   client->unscaled_on, 0, 0,
			   gdk_pixbuf_get_width(client->unscaled_on),
			   gdk_pixbuf_get_height(client->unscaled_on),
			   0, 0,
			   (double)
			   gdk_pixbuf_get_width(client->unscaled_on) /
			   gdk_pixbuf_get_width(simple),
			   (double)
			   gdk_pixbuf_get_height(client->unscaled_on) /
			   gdk_pixbuf_get_height(simple),
			   zcg_int(NULL, "qstradeoff"));
	  z_pixbuf_copy_area(client->unscaled_on, 0, 0,
			       gdk_pixbuf_get_width(client->unscaled_on),
			       gdk_pixbuf_get_height(client->unscaled_off),
			       client->unscaled_off, 0, 0);
	  g_object_unref(G_OBJECT (simple));
	}
    }

  if ((!client->scaled) &&
      (client->w > 0) &&
      (client->h > 0))
    client->scaled = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
				    client->w, client->h);
  if (client->scaled)
    gdk_pixbuf_scale(client->unscaled_on,
		     client->scaled, 0, 0, client->w, client->h,
		     0, 0,
		     (double) client->w /
		     gdk_pixbuf_get_width(client->unscaled_on),
		     (double) client->h /
		      gdk_pixbuf_get_height(client->unscaled_on),
		     zcg_int(NULL, "qstradeoff"));

  build_patches(client);
  refresh_ttx_page_intern(client, NULL);

  pthread_mutex_unlock(&client->mutex);
  return 1; /* success */
}

static void
rolling_headers(struct ttx_client *client, vbi_page *pg)
{
  gint col;
  vbi_char *ac;
  const gint first_col = 32; /* 8 needs more thoughts */
  uint8_t *drcs_clut;
  uint8_t *drcs[32];

/* To debug page formatting (site_def.h) */
#if ZVBI_DISABLE_ROLLING
  return;
#endif

  if (client->waiting)
    return;

  pthread_mutex_lock(&client->mutex);

  if (pg->pgno < 0x100)
    goto abort;

  /* FIXME due to a channel change between the time this page
     was fetched and this function is called these pointers
     may no longer be valid. We likely don't need drcs here,
     so the pointers are temporarily cleared. The real fix
     requires proper reference counting in libzvbi and/or page
     invalidating on channel change in zapping. */
  drcs_clut = client->fpg.drcs_clut;
  client->fpg.drcs_clut = NULL;
  memcpy (drcs, client->fpg.drcs, sizeof (drcs));
  CLEAR (client->fpg.drcs);

  for (col = first_col; col < 40; col++)
    {
      ac = client->fpg.text + col;
      
      if (ac->unicode != pg->text[col].unicode
	  && ac->size <= VBI_DOUBLE_SIZE)
	{
	  ac->unicode = pg->text[col].unicode;

	  vbi_draw_vt_page_region(&client->fpg,
		VBI_PIXFMT_RGBA32_LE,
	  	(uint32_t *) gdk_pixbuf_get_pixels(client->unscaled_off) + col * CW,
		gdk_pixbuf_get_rowstride(client->unscaled_off),
		col, 0, 1, 1, client->reveal, 0 /* flash_off */);
	  vbi_draw_vt_page_region(&client->fpg,
		VBI_PIXFMT_RGBA32_LE,
		(uint32_t *) gdk_pixbuf_get_pixels(client->unscaled_on) + col * CW,
		gdk_pixbuf_get_rowstride(client->unscaled_on),
		col, 0, 1, 1, client->reveal, 1 /* flash_on */);
	  add_patch(client, col, 0, ac, !ac->flash);
	}
    }

  client->fpg.drcs_clut = drcs_clut;
  memcpy (client->fpg.drcs, drcs, sizeof (drcs));

 abort:
  pthread_mutex_unlock(&client->mutex);
}

void
monitor_ttx_page(int id/*client*/, int page, int subpage)
{
  struct ttx_client *client;
  vbi_page pg;

  if (!vbi)
    return;

  pthread_mutex_lock(&clients_mutex);
  client = find_client(id);
  if (client)
    {
      client->freezed = FALSE;
      client->page = page;
      client->subpage = subpage;

      /* 0x900 is our TOP index page */
      if ((page >= 0x100) && (page <= 0x900))
        {
	  if (vbi_fetch_vt_page(vbi, &pg, page, subpage,
				teletext_level, 25, 1))
	    {
	      build_client_page(client, &pg);
	      clear_message_queue(client);
	      send_ttx_message(client, TTX_PAGE_RECEIVED, NULL, 0);
	    }
	} 
      else 
	{
	  build_client_page(client, NULL);
	  clear_message_queue(client);
	  send_ttx_message(client, TTX_PAGE_RECEIVED, NULL, 0);
	}
    }
  pthread_mutex_unlock(&clients_mutex);
}

void monitor_ttx_this(int id, vbi_page *pg)
{
  struct ttx_client *client;

  if (!vbi)
    return;

  if (!pg)
    return;

  pthread_mutex_lock(&clients_mutex);
  if ((client = find_client(id)))
    {
      client->page = pg->pgno;
      client->subpage = pg->subno;
      client->freezed = TRUE;
      memcpy(&client->fpg, pg, sizeof(client->fpg));
      vbi_draw_vt_page(&client->fpg,
		VBI_PIXFMT_RGBA32_LE,
		(uint32_t *) gdk_pixbuf_get_pixels(client->unscaled_on),
		client->reveal, 1 /* flash_on */);
      vbi_draw_vt_page_region(&client->fpg,
		VBI_PIXFMT_RGBA32_LE,
      		(uint32_t *) gdk_pixbuf_get_pixels(client->unscaled_off),
		-1 /* rowstride */, 0 /* column */, 0 /* row */,
		pg->columns, pg->rows, client->reveal, 0 /* flash_on */);
      build_client_page(client, (vbi_page *) -1);
      clear_message_queue(client);
      send_ttx_message(client, TTX_PAGE_RECEIVED, NULL, 0);
    }
  pthread_mutex_unlock(&clients_mutex);
}

void
ttx_freeze (int id)
{
  struct ttx_client *client;

  if (!vbi)
    return;

  pthread_mutex_lock(&clients_mutex);
  if ((client = find_client(id)))
    client->freezed = TRUE;
  pthread_mutex_unlock(&clients_mutex);  
}

void
ttx_unfreeze (int id)
{
  struct ttx_client *client;

  if (!vbi)
    return;

  pthread_mutex_lock(&clients_mutex);
  if ((client = find_client(id)))
    client->freezed = FALSE;
  pthread_mutex_unlock(&clients_mutex);  
}

static void
scan_header(vbi_page *pg)
{
  gint col, i=0;
  vbi_char *ac;
  ucs2_t ucs2[256];
  char *buf;

  /* Station name usually goes here */
  for (col = 7; col < 16; col++)
    {
      ac = pg->text + col;

      if (!ac->unicode || !vbi_is_print(ac->unicode))
	/* prob. bad reception, abort */
	return;

      if (ac->unicode == 0x0020 && i == 0)
	continue;

      ucs2[i++] = ac->unicode;
    }

  if (!i)
    return;

  /* remove spaces in the end */
  for (col = i-1; col >= 0 && ucs2[col] == 0x0020; col--)
    i = col;

  if (!i)
    return;

  ucs2[i] = 0;

  buf = ucs22local(ucs2);

  if (!buf || !*buf)
    return;

  /* enhance */
  if (station_name_known)
    {
      col = strlen(station_name);
      for (i=0; i<strlen(buf) && i<255; i++)
	if (col <= i || station_name[i] == ' ' ||
	    (!isalnum(station_name[i]) && isalnum(buf[i])))
	  station_name[i] = buf[i];
      station_name[i] = 0;
    }
  /* just copy */
  else
    {
      strncpy(station_name, buf, 255);
      station_name[255] = 0;
    }

  free(buf);

  station_name_known = TRUE;
}

static void
notify_clients(int page, int subpage,
	       gboolean roll_header,
	       gboolean header_update)
{
  GList *p;
  struct ttx_client *client;
  vbi_page pg;

  if (!vbi)
    return;

  pg.rows = 0;

  pthread_mutex_lock(&clients_mutex);

  for (p = g_list_first(ttx_clients); p; p = p->next)
    {
      client = (struct ttx_client*)p->data;
      
      if ((client->page == page)
	  && (!client->freezed)
	  && ((client->subpage == subpage)
	      || (client->subpage == VBI_ANY_SUBNO)))
	{
	  if (pg.rows < 25
	      && !vbi_fetch_vt_page(vbi, &pg, page, subpage,
				    teletext_level, 25, 1))
	    {
	      pthread_mutex_unlock(&clients_mutex);
	      return;
	    }
	  build_client_page(client, &pg);
	  send_ttx_message(client, TTX_PAGE_RECEIVED, NULL, 0);
	}
      else if (roll_header)
	{
	  if (pg.rows < 1
	      && !vbi_fetch_vt_page(vbi, &pg, page, subpage,
				    teletext_level, 1, 0))
	    {
	      pthread_mutex_unlock(&clients_mutex);
	      return;
	    }

	  rolling_headers(client, &pg);
	}
    }

  pthread_mutex_unlock(&clients_mutex);

  if (header_update)
    {
      if (pg.rows < 1
	  && vbi_fetch_vt_page(vbi, &pg, page, subpage,
			       teletext_level, 1, 0))
	scan_header(&pg);
    }
}

static void
notify_clients_generic (enum ttx_message msg)
{
  GList *p;
  struct ttx_client *client;

  if (!vbi)
    return;

  pthread_mutex_lock (&clients_mutex);

  for (p = g_list_first (ttx_clients); p; p = p->next)
    {
      client = (struct ttx_client *) p->data;
      send_ttx_message (client, msg, NULL, 0);
    }

  pthread_mutex_unlock(&clients_mutex);
}

#if 0 /* temporarily disabled */

static void
notify_clients_trigger (const vbi_link *ld)
{
  GList *p;
  struct ttx_client *client;

  if (!vbi)
    return;

  pthread_mutex_lock(&clients_mutex);
  p = g_list_first(ttx_clients);
  while (p)
    {
      client = (struct ttx_client*)p->data;
      send_ttx_message(client, TTX_TRIGGER, (ttx_message_data*)ld,
		       sizeof(vbi_link));
      p = p->next;
    }
  pthread_mutex_unlock(&clients_mutex);
}

#endif

void resize_ttx_page(int id, int w, int h)
{
  struct ttx_client *client;

  if (!vbi)
    return;

  if ((w<=0) || (h<=0))
    return;

  pthread_mutex_lock(&clients_mutex);
  if ((client = find_client(id)))
    {
      pthread_mutex_lock(&client->mutex);
      if ((client->w != w) ||
	  (client->h != h))
	{
	  if (client->scaled)
	    g_object_unref(G_OBJECT (client->scaled));
	  client->scaled = NULL;
	  client->scaled = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
					  w, h);

	  if (client->scaled)
	    gdk_pixbuf_scale(client->unscaled_on,
			     client->scaled, 0, 0, w, h,
			     0, 0,
			     (double) w /
			     gdk_pixbuf_get_width(client->unscaled_on),
			     (double) h /
			     gdk_pixbuf_get_height(client->unscaled_on),
			     zcg_int(NULL, "qstradeoff"));
	  client->w = w;
	  client->h = h;

	  resize_patches(client);
	}
      pthread_mutex_unlock(&client->mutex);
    }
  pthread_mutex_unlock(&clients_mutex);
}

void render_ttx_page(int id, GdkDrawable *drawable,
		     GdkGC *gc,
		     gint src_x, gint src_y,
		     gint dest_x, gint dest_y,
		     gint w, gint h)
{
  struct ttx_client *client;

  if (!vbi)
    return;

  pthread_mutex_lock(&clients_mutex);

  if ((client = find_client(id)))
    {
      pthread_mutex_lock(&client->mutex);

      if (client->scaled)
	{
	  gint pw;
	  gint ph;

	  /* Kludge to prevent a hickup w/"Loading" image.
	     XXX Why do we need this? */
	  pw = gdk_pixbuf_get_width (client->scaled);
	  ph = gdk_pixbuf_get_height (client->scaled);

	  gdk_pixbuf_render_to_drawable (client->scaled,
					 drawable, gc,
					 src_x, src_y,
					 dest_x, dest_y,
					 MIN (w, pw), MIN (h, ph),
					 GDK_RGB_DITHER_NORMAL,
					 src_x, src_y);
	}

      pthread_mutex_unlock(&client->mutex);
    }

  pthread_mutex_unlock(&clients_mutex);
}

void
render_ttx_mask(int id, GdkBitmap *mask)
{
  struct ttx_client *client;

  if (!vbi)
    return;

  pthread_mutex_lock(&clients_mutex);
  if ((client = find_client(id)))
    {
      pthread_mutex_lock(&client->mutex);
      if (client->scaled)
	gdk_pixbuf_render_threshold_alpha(client->scaled, mask,
					  0, 0, 0, 0,
					  client->w, client->h, 127);
      pthread_mutex_unlock(&client->mutex);
    }
  pthread_mutex_unlock(&clients_mutex);
}

/*
  Returns the global vbi object, or NULL if vbi isn't enabled or
  doesn't work. You can safely use this function to test if VBI works
  (it will return NULL if it doesn't).
*/
vbi_decoder *
zvbi_get_object(void)
{
  return vbi;
}

ZModel *
zvbi_get_model(void)
{
  return vbi_model;
}

/* this is called when we receive a page, header, etc. */
static void
event(vbi_event *ev, void *unused)
{
    switch (ev->type) {
    case VBI_EVENT_TTX_PAGE:
      {
	static gboolean receiving_pages = FALSE;
	if (!receiving_pages)
	  {
	    receiving_pages = TRUE;
	    printv("Received vtx page %x.%02x\n",
		   ev->ev.ttx_page.pgno, ev->ev.ttx_page.subno & 0xFF);
	  }
      }

      /* Set the dirty flag on the page */
      notify_clients(ev->ev.ttx_page.pgno, ev->ev.ttx_page.subno,
		     ev->ev.ttx_page.roll_header,
		     ev->ev.ttx_page.header_update);

#ifdef BLACK_MOON_IS_ON
      if (ev->ev.ttx_page.pgno == 0x300)
	{
	  vbi_link link;
	  snprintf(link.name, 256, "Programación T5");
	  snprintf(link.url, 256, "ttx://300");
	  link.type = VBI_LINK_PAGE;
	  link.page = 0x300;
	  link.subpage = ANY_SUB;
	  link.priority = 9;
	  link.itv_type = link.autoload = 0;
	  notify_clients_trigger (&link);
	}
#endif

      break;
    case VBI_EVENT_NETWORK:
      pthread_mutex_lock(&network_mutex);
      memcpy(&current_network, &ev->ev.network, sizeof(vbi_network));
      if (*current_network.name)
	{
	  strncpy(station_name, current_network.name, 255);
	  station_name[255] = 0;
	  station_name_known = TRUE;
	}
      else if (*current_network.call)
	{
	  strncpy(station_name, current_network.call, 255);
	  station_name[255] = 0;
	  station_name_known = TRUE;
	}
      else
	  station_name_known = FALSE;

      notify_clients_generic (TTX_NETWORK_CHANGE);
      pthread_mutex_unlock(&network_mutex);
      break;

#if 0 /* temporarily disabled */
    case VBI_EVENT_TRIGGER:
      notify_clients_trigger (ev->ev.trigger);
      break;
#endif

    case VBI_EVENT_ASPECT:
      if (zconf_get_integer(NULL, "/zapping/options/main/ratio") == 3)
	zvbi_ratio = ev->ev.aspect.ratio;
      break;

    default:
      break;
    }
}

#endif /* HAVE_LIBZVBI */

void
vbi_gui_sensitive (gboolean on)
{
  static const gchar *widgets [] = {
    "separador5",
    "videotext1",
#if 0 /* temporarily disabled */
    "vbi_info1",
    "program_info1",
#endif
    "toolbar-teletext",
    "toolbar-subtitle",
    "new_ttxview",
    "menu-subtitle",
    NULL
  };
  const gchar **sp;

  for (sp = widgets; *sp; ++sp)
    {
      GtkWidget *widget;

      widget = lookup_widget (main_window, *sp);

      if (on)
	gtk_widget_show (widget);
      else
	gtk_widget_hide (widget);

      gtk_widget_set_sensitive (widget, on);
    }

  if (on)
    {
      printv("VBI enabled, showing GUI items\n");
    }
  else
    {
      printv("VBI disabled, removing GUI items\n");

      /* Set the capture mode to a default value and disable VBI */
      if (zcg_int(NULL, "capture_mode") == TVENG_TELETEXT)
	zcs_int(TVENG_CAPTURE_READ, "capture_mode");
    }
}

#ifndef ZVBI_CAPTION_DEBUG
#define ZVBI_CAPTION_DEBUG 0
#endif

#ifdef HAVE_LIBZVBI

static vbi_pgno
find_subtitle_page		(void)
{
  vbi_decoder *vbi;
  vbi_pgno pgno;

  if (!(vbi = zvbi_get_object ()))
    return 0;

  for (pgno = 1; pgno <= 0x899;
       pgno = (pgno == 4) ? 0x100 : vbi_add_bcd (pgno, 0x001))
    {
      vbi_page_type classf;

      classf = vbi_classify_page (vbi, pgno, NULL, NULL);

      if (VBI_SUBTITLE_PAGE == classf)
	return pgno;
    }

  return 0;
}

static PyObject *
py_closed_caption		(PyObject *		self,
				 PyObject *		args)
{
  static const char *key = "/zapping/internal/callbacks/closed_caption";
  int active;

  active = -1; /* toggle */

  if (!PyArg_ParseTuple (args, "|i", &active))
    g_error ("zapping.closed_caption(|i)");

  if (-1 == active)
    active = !zconf_get_boolean (NULL, key);

  if (ZVBI_CAPTION_DEBUG)
    fprintf (stderr, "CC enable %d\n", active);

  zconf_set_boolean (active, key);

  if (active)
    {
      vbi_subno dummy;

      /* In Teletext mode, overlay currently displayed page. */
      if (get_ttxview_page (main_window, &zvbi_caption_pgno, &dummy))
	{
	  if (ZVBI_CAPTION_DEBUG)
	    fprintf (stderr, "CC Teletext pgno %x\n", zvbi_caption_pgno);

	  zmisc_restore_previous_mode (main_info);
	}
      /* In video mode, use previous page or find subtitles. */
      else if (zvbi_caption_pgno <= 0)
	{
	  zvbi_caption_pgno = find_subtitle_page ();

	  if (ZVBI_CAPTION_DEBUG)
	    fprintf (stderr, "CC lookup pgno %x\n", zvbi_caption_pgno);

	  if (zvbi_caption_pgno <= 0)
	    {
	      /* Bad luck. */
	      zconf_set_boolean (FALSE, key);
	    }
	}
      else
	{
	  if (ZVBI_CAPTION_DEBUG)
	    fprintf (stderr, "CC previous pgno %x\n", zvbi_caption_pgno);
	}
    }
  else
    {
      osd_clear ();
    }

  py_return_true;
}

void
startup_zvbi(void)
{
#ifdef HAVE_LIBZVBI
  zcc_bool(TRUE, "Enable VBI decoding", "enable_vbi");
#else
  zcc_bool(FALSE, "Enable VBI decoding", "enable_vbi");
#endif
  /* No longer used */
  zcc_bool(TRUE, "Use VBI for getting station names", "use_vbi");
  zcc_bool(FALSE, "Overlay subtitle pages automagically", "auto_overlay");

  zcc_char("/dev/vbi0", "VBI device", "vbi_device");
  zcc_int(0, "Default TTX region", "default_region");
  zcc_int(3, "Teletext implementation level", "teletext_level");
  zcc_int(2, "ITV filter level", "filter_level");
  zcc_int(1, "Default action for triggers", "trigger_default");
  zcc_int(1, "Program related links", "pr_trigger");
  zcc_int(1, "Network related links", "nw_trigger");
  zcc_int(1, "Station related links", "st_trigger");
  zcc_int(1, "Sponsor messages", "sp_trigger");
  zcc_int(1, "Operator messages", "op_trigger");
  zcc_int(INTERP_MODE, "Quality speed tradeoff", "qstradeoff");

  zconf_create_boolean (FALSE, "Display subtitles",
			"/zapping/internal/callbacks/closed_caption");

  cmd_register ("closed_caption", py_closed_caption, METH_VARARGS,
		("Closed Caption on/off"), "zapping.closed_caption()");

#ifdef HAVE_LIBZVBI
  zconf_add_hook("/zapping/options/vbi/enable_vbi",
		 (ZConfHook)on_vbi_prefs_changed,
		 (gpointer)0xdeadbeef);

  vbi_model = ZMODEL(zmodel_new());

  vbi_reset_prog_info(&program_info[0]);
  vbi_reset_prog_info(&program_info[1]);

  pthread_mutex_init(&network_mutex, NULL);
  pthread_mutex_init(&prog_info_mutex, NULL);

  CLEAR (current_network);

  if (pipe(osd_pipe)) {
    g_warning("Cannot create osd pipe");
    exit(EXIT_FAILURE);
  }
#endif
}

void shutdown_zvbi(void)
{
  pthread_mutex_destroy(&prog_info_mutex);
  pthread_mutex_destroy(&network_mutex);

  D();

  if (vbi)
    zvbi_close_device();

  D();

  if (vbi_model)
    g_object_unref(G_OBJECT(vbi_model));

  D();

  close(osd_pipe[0]);
  close(osd_pipe[1]);
}

gchar *
zvbi_get_name(void)
{
  if (!station_name_known || !vbi)
    return NULL;

  return g_convert (station_name, strlen (station_name),
		    "ISO-8859-1","UTF-8",
		    NULL, NULL, NULL);
}

void
zvbi_name_unknown(void)
{
  station_name_known = FALSE;
}

void
zvbi_channel_switched(void)
{
  GList *p;
  struct ttx_client *client;

  if (!vbi)
    return;

  vbi_channel_switched(vbi, 0);

  osd_clear();

  pthread_mutex_lock(&clients_mutex);
  p = g_list_first(ttx_clients);
  while (p)
    {
      client = (struct ttx_client*)p->data;
      send_ttx_message(client, TTX_CHANNEL_SWITCHED, NULL, 0);
      p = p->next;
    }
  pthread_mutex_unlock(&clients_mutex);

#if 0 /* Temporarily removed. */
  zvbi_reset_network_info ();
  zvbi_reset_program_info ();
#endif
}

vbi_wst_level
zvbi_teletext_level (void)
{
  return teletext_level;
}

#endif /* HAVE_LIBZVBI */
