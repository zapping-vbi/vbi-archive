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
 * The code uses libvbi, written by Michael Schimek.
 */

#include <site_def.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef HAVE_LIBZVBI

#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <pthread.h>
#include <ctype.h>
#include <time.h>
#include <pthread.h>
#include <math.h>
#include "common/ucs-2.h"
//#include "common/errstr.h"

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

#undef TRUE
#undef FALSE
#include "common/fifo.h"

extern GtkWidget *main_window;
extern tveng_device_info *main_info;

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

/* symbols used by osd.c */
vbi_pgno		zvbi_page = 1;
vbi_subno		zvbi_subpage = VBI_ANY_SUBNO;

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
  fifo		mqueue;
  producer	mqueue_prod;
  consumer	mqueue_cons;
  int		page, subpage; /* monitored page, subpage */
  int		id; /* of the client */
  pthread_mutex_t mutex;
  vbi_page	fp; /* formatted page */
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
  /* vbi audio mode */
  N_("No Audio"),
  N_("Mono"),
  N_("Stereo"),
  N_("Stereo Surround"),
  N_("Simulated Stereo"),
  N_("Video Descriptions"),
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
static fifo		sliced_fifo;
static vbi_capture *	capture;

/* Attn: must be pthread_cancel-safe */

static void *
decoding_thread (void *p)
{
  consumer c;

  D();

  assert (add_consumer (&sliced_fifo, &c));

  /* setpriority (PRIO_PROCESS, 0, 5); */

  while (!vbi_quit) {
    buffer *b;
    struct timeval now;
    struct timespec timeout;

    gettimeofday (&now, NULL);
    timeout.tv_sec = now.tv_sec + 1;
    timeout.tv_nsec = now.tv_usec * 1000;

    if (!(b = wait_full_buffer_timeout (&c, &timeout)))
      continue;

    if (b->used <= 0) {
      send_empty_buffer (&c, b);
      if (b->used < 0)
	fprintf (stderr, "I/O error in decoding thread, aborting.\n");
      break;
    }

    pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, NULL);

    vbi_decode (vbi, (vbi_sliced *) b->data,
		b->used / sizeof(vbi_sliced), b->time);

    pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, NULL);

    send_empty_buffer(&c, b);
  }

  rem_consumer(&c);

  decoder_quit_ack = TRUE;

  return NULL;
}

/* Attn: must be pthread_cancel-safe */

static void *
capturing_thread (void *x)
{
  struct timeval timeout;  
  producer p;
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

  assert (add_producer (&sliced_fifo, &p));

  while (!vbi_quit) {
    buffer *b;
    int lines;

    b = wait_empty_buffer (&p);

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
	b->errorstr = _("V4L/V4L2 VBI interface timeout");

	send_full_buffer (&p, b);

	goto abort;

      default: /* error */
#if 0
	for (; stacked > 0; stacked--)
	  send_full_buffer (&p, PARENT (rem_head(&stack), buffer, node));
#endif
	b->used = -1;
	b->error = errno;
	b->errorstr = _("V4L/V4L2 VBI interface: Failed to read from the device");

	send_full_buffer (&p, b);

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

    send_full_buffer(&p, b);
  }

 abort:

  rem_producer (&p);

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

// XXX vbi must be restarted on video std change. is it?

static gboolean
threads_init (gchar *dev_name, int given_fd)
{
  gchar *failed = _("VBI initialization failed.\n%s");
  gchar *memory = _("Ran out of memory.");
  gchar *thread = _("Out of resources to start a new thread.");
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
      ShowBox(failed, GNOME_MESSAGE_BOX_ERROR, memory);
      return FALSE;
    }

  D();

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
	  if (errno == ENOENT || errno == ENXIO || errno == ENODEV)
	    {
	      gchar *s = g_strconcat(_errstr, "\n", mknod_hint);
	      
	      ShowBox(failed, GNOME_MESSAGE_BOX_ERROR, s);
	      g_free (s);
	    }
	  else
	    {
	      ShowBox(failed, GNOME_MESSAGE_BOX_ERROR, _errstr);
	    }
	  free (_errstr);
	  vbi_decoder_delete (vbi);
	  vbi = NULL;
	  return FALSE;
	}
    }

  D();

  // XXX when we have WSS625, disable video sampling

  raw = vbi_capture_parameters (capture);
  buffer_size = (raw->count[0] + raw->count[1]) * sizeof(vbi_sliced);

  if (!init_buffered_fifo (&sliced_fifo, "vbi-sliced", 20, buffer_size))
    {
      ShowBox(failed, GNOME_MESSAGE_BOX_ERROR, memory);
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
      ShowBox(failed, GNOME_MESSAGE_BOX_ERROR, thread);
      destroy_fifo (&sliced_fifo);
      vbi_capture_delete (capture);
      vbi_decoder_delete (vbi);
      vbi = NULL;
      return FALSE;
    }

  D();

  if (pthread_create (&capturer_id, NULL, capturing_thread, NULL))
    {
      ShowBox(failed, GNOME_MESSAGE_BOX_ERROR, thread);
      join ("dec0", decoder_id, &decoder_quit_ack, 15);
      destroy_fifo (&sliced_fifo);
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
      destroy_fifo (&sliced_fifo);
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
      if (main_info->current_mode == TVENG_NO_CAPTURE)
	zmisc_switch_mode(TVENG_CAPTURE_WINDOW, main_info);
      zvbi_close_device();
      D();
    }

  /* disable VBI if needed */
  vbi_gui_sensitive(!!zvbi_get_object());
}

int osd_pipe[2];

void
startup_zvbi(void)
{
#ifdef ENABLE_V4L
  zcc_bool(TRUE, "Enable VBI decoding", "enable_vbi");
#else
  zcc_bool(FALSE, "Enable VBI decoding", "enable_vbi");
#endif
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

#ifdef ENABLE_V4L
  zconf_add_hook("/zapping/options/vbi/enable_vbi",
		 (ZConfHook)on_vbi_prefs_changed,
		 (gpointer)0xdeadbeef);

  vbi_model = ZMODEL(zmodel_new());

  vbi_reset_prog_info(&program_info[0]);
  vbi_reset_prog_info(&program_info[1]);

  pthread_mutex_init(&network_mutex, NULL);
  pthread_mutex_init(&prog_info_mutex, NULL);

  memset(&current_network, 0, sizeof(current_network));

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
    gtk_object_destroy(GTK_OBJECT(vbi_model));

  D();

  close(osd_pipe[0]);
  close(osd_pipe[1]);
}

static void cc_event(vbi_event *ev, void *data)
{
  int *pipe = data;

  switch (ev->type)
    {
      case VBI_EVENT_TTX_PAGE:
        if (ev->ev.ttx_page.pgno != zvbi_page)
          return;
	break;

      case VBI_EVENT_CAPTION:
        if (ev->ev.caption.pgno != zvbi_page)
          return;
	break;

      default:
        return;
    }

  /* Shouldn't block when the pipe buffer is full... ? */
  write(pipe[1], "", 1);
}

/* Trigger handling */
static gint trigger_timeout_id = -1;
static gint trigger_client_id = -1;
static vbi_link last_trigger;

static void
on_trigger_clicked			(gpointer	ignored,
					 vbi_link	*trigger)
{
  switch (trigger->type)
    {
    case VBI_LINK_HTTP:
    case VBI_LINK_FTP:
    case VBI_LINK_EMAIL:
      gnome_url_show(trigger->url);
      break;

    case VBI_LINK_PAGE:
    case VBI_LINK_SUBPAGE:
      cmd_execute_printf (NULL, "ttx_open_new %x %x",
			  trigger->pgno, trigger->subno);
      break;

    case VBI_LINK_LID:
    case VBI_LINK_TELEWEB:
    case VBI_LINK_MESSAGE:
      /* ignore */
      break;
      
    default:
      ShowBox("Unhandled trigger type %d, please contact the maintainer",
	      GNOME_MESSAGE_BOX_WARNING, trigger->type);
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

  if ((pix = z_load_pixmap (link->eacem ?
			    "eacem_icon.png" : "atvef_icon.png")))
    {
      button = gtk_button_new ();
      gtk_container_add (GTK_CONTAINER (button), pix);
    }
  else /* pixmaps not installed */
    {
      button = gtk_button_new_with_label (_("Click me"));
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
  gtk_signal_connect(GTK_OBJECT(button), "clicked",
		     GTK_SIGNAL_FUNC(on_trigger_clicked),
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
	      GNOME_MESSAGE_BOX_WARNING, link->type);
      buffer = g_strdup_printf("%s", link->name);
      break;
    }
  z_status_set_widget(button);
  z_status_print(buffer);
  g_free(buffer);
}

static void
update_main_title (void)
{
  extern tveng_tuned_channel * global_channel_list;
  extern int cur_tuned_channel;
  tveng_tuned_channel *channel;
  gchar *name = NULL;

  if (*current_network.name)
    name = current_network.name;
  /* else switch away from known network */

  channel = tveng_retrieve_tuned_channel_by_index(
	      cur_tuned_channel, global_channel_list);
  if (!channel)
    z_set_main_title(NULL, name);
  else if (!channel->name)
    z_set_main_title(channel, name);
}

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

/* Open the configured VBI device, FALSE on error */
gboolean
zvbi_open_device(char *device)
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

#ifdef ENABLE_V4L
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
  if (trigger_client_id >= 0)
    unregister_ttx_client(trigger_client_id);
  if (trigger_timeout_id >= 0)
    gtk_timeout_remove(trigger_timeout_id);
  trigger_client_id = register_ttx_client();
  trigger_timeout_id = gtk_timeout_add(100, (GtkFunction)trigger_timeout,
				       GINT_TO_POINTER(trigger_client_id));
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

  if (trigger_timeout_id >= 0)
    gtk_timeout_remove(trigger_timeout_id);
  if (trigger_client_id >= 0)
    unregister_ttx_client(trigger_client_id);

  D();

  trigger_client_id = trigger_timeout_id = -1;

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
  buffer *b;
  
  b = wait_empty_buffer(&client->mqueue_prod);

  d = (ttx_message_data*)b->data;
  d->msg = message;
  g_assert(bytes <= sizeof(ttx_message_data));

  if (data)
    memcpy(&(d->data), data, bytes);

  send_full_buffer(&client->mqueue_prod, b);
}

static void
remove_client(struct ttx_client *client)
{
  gint i;

  destroy_fifo(&client->mqueue);
  pthread_mutex_destroy(&client->mutex);
  gdk_pixbuf_unref(client->unscaled_on);
  gdk_pixbuf_unref(client->unscaled_off);
  if (client->scaled)
    gdk_pixbuf_unref(client->scaled);
  for (i = 0; i<client->num_patches; i++)
    {
      gdk_pixbuf_unref(client->patches[i].unscaled_on);
      gdk_pixbuf_unref(client->patches[i].unscaled_off);
      if (client->patches[i].scaled_on)
	gdk_pixbuf_unref(client->patches[i].scaled_on);
      if (client->patches[i].scaled_off)
	gdk_pixbuf_unref(client->patches[i].scaled_off);
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

  pixbuf = gdk_pixbuf_new_from_file (filename);

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

  client = g_malloc(sizeof(struct ttx_client));
  memset(client, 0, sizeof(struct ttx_client));
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
      gdk_pixbuf_unref(simple);
    }

  g_assert(init_buffered_fifo(
           &client->mqueue, "zvbi-mqueue",
	   16, sizeof(ttx_message_data)) > 0);

  add_producer(&client->mqueue, &client->mqueue_prod);
  add_consumer(&client->mqueue, &client->mqueue_cons);

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
    pg = &client->fp;

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
  buffer *b;
  enum ttx_message message;
  ttx_message_data *d;

  pthread_mutex_lock(&clients_mutex);

  if ((client = find_client(id)))
    {
      b = recv_full_buffer(&client->mqueue_cons);
      if (b)
	{
	  d = (ttx_message_data*)b->data;
	  message = d->msg;
	  memcpy(data, d, sizeof(ttx_message_data));
	  send_empty_buffer(&client->mqueue_cons, b);
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
  buffer *b;
  enum ttx_message message;
  ttx_message_data *d;

  pthread_mutex_lock(&clients_mutex);

  if ((client = find_client(id)))
    {
      b = wait_full_buffer(&client->mqueue_cons);
      g_assert(b != NULL);
      d = (ttx_message_data*)b->data;
      message = d->msg;
      memcpy(data, d, sizeof(ttx_message_data));
      send_empty_buffer(&client->mqueue_cons, b);
    }
  else
    message = TTX_BROKEN_PIPE;

  pthread_mutex_unlock(&clients_mutex);

  return message;
}

static void
clear_message_queue(struct ttx_client *client)
{
  buffer *b;

  while ((b=recv_full_buffer(&client->mqueue_cons)))
    send_empty_buffer(&client->mqueue_cons, b);
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
	  gdk_pixbuf_unref(destiny->scaled_on);
	if (destiny->scaled_off)
	  gdk_pixbuf_unref(destiny->scaled_off);
	gdk_pixbuf_unref(destiny->unscaled_on);
	gdk_pixbuf_unref(destiny->unscaled_off);
	break;
      }
      
  memset(&patch, 0, sizeof(struct ttx_patch));
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

  sw = rint(((double) client->w * (patch.width * CW + 10))
	    / gdk_pixbuf_get_width(client->unscaled_on));
  sh = rint(((double) client->h * (patch.height * CH + 10))
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
      sw = rint(((double)client->w*(client->patches[i].width*CW+10))/
	gdk_pixbuf_get_width(client->unscaled_on));
      sh = rint(((double)client->h*(client->patches[i].height*CH+10))/
	gdk_pixbuf_get_height(client->unscaled_on));

      if (client->patches[i].scaled_on)
	gdk_pixbuf_unref(client->patches[i].scaled_on);
      client->patches[i].scaled_on =
	z_pixbuf_scale_simple(client->patches[i].unscaled_on,
				sw, sh, zcg_int(NULL, "qstradeoff"));
      if (client->patches[i].scaled_off)
	gdk_pixbuf_unref(client->patches[i].scaled_off);
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
	gdk_pixbuf_unref(client->patches[i].scaled_on);
      if (client->patches[i].scaled_off)
	gdk_pixbuf_unref(client->patches[i].scaled_off);
      gdk_pixbuf_unref(client->patches[i].unscaled_on);
      gdk_pixbuf_unref(client->patches[i].unscaled_off);
    }
  g_free(client->patches);
  client->patches = NULL;
  client->num_patches = 0;

  /* FIXME: This is too cumbersome, something more smart is needed */
  for (col = 0; col < client->fp.columns; col++)
    for (row = 0; row < client->fp.rows; row++)
      {
	ac = &client->fp.text[row * client->fp.columns + col];
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
	  x = rint(((double)client->w*p->col)/client->fp.columns);
	  y = rint(((double)client->h*p->row)/client->fp.rows);
	  sx = ((double)gdk_pixbuf_get_width(scaled)/
	    gdk_pixbuf_get_width(p->unscaled_on))*5.0;
	  sy = ((double)gdk_pixbuf_get_height(scaled)/
	    gdk_pixbuf_get_height(p->unscaled_on))*5.0;
	  
	  z_pixbuf_copy_area(scaled, rint(sx), rint(sy),
			     rint(gdk_pixbuf_get_width(scaled)-sx*2),
			     rint(gdk_pixbuf_get_height(scaled)-sy*2),
			     client->scaled,
			     x, y);
	  
	  if (drawable)
	    gdk_window_clear_area_e(drawable->window, x, y,
				    rint(gdk_pixbuf_get_width(scaled)-sx*2),
				    rint(gdk_pixbuf_get_height(scaled)-sy*2));
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
      memcpy(&client->fp, pg, sizeof(client->fp));
      vbi_draw_vt_page(&client->fp,
		       VBI_PIXFMT_RGBA32_LE,
		       (uint32_t *) gdk_pixbuf_get_pixels(client->unscaled_on),
		       client->reveal, 1 /* flash_on */);
      vbi_draw_vt_page_region(&client->fp,
			      VBI_PIXFMT_RGBA32_LE,
			      (uint32_t *) gdk_pixbuf_get_pixels(client->unscaled_off),
			      -1 /* rowstride */,
			      0 /* column */, 0 /* row */,
			      client->fp.columns, client->fp.rows,
			      client->reveal, 0 /* flash_on */);
      client->waiting = FALSE;
    }
  else if (!pg)
    {
      memset(&client->fp, 0, sizeof(client->fp));

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
	  gdk_pixbuf_unref(simple);
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

/* To debug page formatting (site_def.h) */
#if ZVBI_DISABLE_ROLLING
  return;
#endif

  if (client->waiting)
    return;

  pthread_mutex_lock(&client->mutex);

  if (pg->pgno < 0x100)
    goto abort;

  for (col = first_col; col < 40; col++)
    {
      ac = client->fp.text + col;
      
      if (ac->unicode != pg->text[col].unicode
	  && ac->size <= VBI_DOUBLE_SIZE)
	{
	  ac->unicode = pg->text[col].unicode;

	  vbi_draw_vt_page_region(&client->fp,
		VBI_PIXFMT_RGBA32_LE,
	  	(uint32_t *) gdk_pixbuf_get_pixels(client->unscaled_off) + col * CW,
		gdk_pixbuf_get_rowstride(client->unscaled_off),
		col, 0, 1, 1, client->reveal, 0 /* flash_off */);
	  vbi_draw_vt_page_region(&client->fp,
		VBI_PIXFMT_RGBA32_LE,
		(uint32_t *) gdk_pixbuf_get_pixels(client->unscaled_on) + col * CW,
		gdk_pixbuf_get_rowstride(client->unscaled_on),
		col, 0, 1, 1, client->reveal, 1 /* flash_on */);
	  add_patch(client, col, 0, ac, !ac->flash);
	}
    }

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
      memcpy(&client->fp, pg, sizeof(client->fp));
      vbi_draw_vt_page(&client->fp,
		VBI_PIXFMT_RGBA32_LE,
		(uint32_t *) gdk_pixbuf_get_pixels(client->unscaled_on),
		client->reveal, 1 /* flash_on */);
      vbi_draw_vt_page_region(&client->fp,
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
      
      if ((client->page == page) && (!client->freezed) &&
	  ((client->subpage == subpage) || (client->subpage == VBI_ANY_SUBNO)))
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
	    gdk_pixbuf_unref(client->scaled);
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
	gdk_pixbuf_render_to_drawable(client->scaled,
				      drawable,
				      gc,
				      src_x, src_y,
				      dest_x, dest_y,
				      w, h,
				      GDK_RGB_DITHER_NORMAL,
				      src_x, src_y);

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

    case VBI_EVENT_TRIGGER:
      notify_clients_trigger (ev->ev.trigger);
      break;

    case VBI_EVENT_ASPECT:
      if (zconf_get_integer(NULL, "/zapping/options/main/ratio") == 3)
	zvbi_ratio = ev->ev.aspect.ratio;
      break;

    case VBI_EVENT_PROG_INFO:
    {
      vbi_program_info *pi = ev->ev.prog_info;

#ifdef ZVBI_DUMP_PROG_INFO
      int i;

      fprintf(stderr, "Program Info %d\n"
	      "  %d/%d %d:%d T%d L%d:%d E%d:%d:%d\n"
	      "  title <%s> type <",
	      pi->future,
	      pi->month, pi->day, pi->hour, pi->min, pi->tape_delayed,
	      pi->length_hour, pi->length_min,
	      pi->elapsed_hour, pi->elapsed_min, pi->elapsed_sec,
	      pi->title);
      for (i = 0; pi->type_id[i]; i++)
	fprintf(stderr, "%s ",
		vbi_prog_type_str_by_id(pi->type_classf, pi->type_id[i]));
      fprintf(stderr, "> rating <%s> dlsv %d\n"
	      "  audio %d,%s / %d,%s\n"
	      "  caption %d lang ",
	      vbi_rating_str_by_id(pi->rating_auth, pi->rating_id), pi->rating_dlsv,
	      pi->audio[0].mode, pi->audio[0].language,
	      pi->audio[1].mode, pi->audio[1].language,
	      pi->caption_services);
      for (i = 0; i < 8; i++)
	fprintf(stderr, "%s, ", pi->caption_language[i]);
      fprintf(stderr, "\n"
	      "  cgms %d aspect %f description: \""
	      "  %s\\%s\\%s\\%s\\%s\\%s\\%s\\%s\"\n",
	      pi->cgms_a, (double) pi->aspect.ratio,
	      pi->description[0], pi->description[1],
	      pi->description[2], pi->description[3],
	      pi->description[4], pi->description[5],
	      pi->description[6], pi->description[7]);
#endif

      pthread_mutex_lock(&prog_info_mutex);

      program_info[!!pi->future] = *pi;

      pthread_mutex_unlock(&prog_info_mutex);

      notify_clients_generic (TTX_PROG_INFO_CHANGE);

      break;
    }
 
    default:
      break;
    }
}

/* Handling of the network_info and prog_info dialog (alias vbi info vi) */

typedef enum {
	VI_TYPE_NETWORK,
	VI_TYPE_PROGRAM,
} vi_type;

struct vi_data
{
  int		id;		/* ttx_client id */
  ZModel	*vbi_model;	/* monitor changes in the VBI device */
  GtkWidget	*dialog;
  GtkWidget	*rating;
  gint		timeout;
  vi_type	type;
};

static void destroy_vi(gpointer ignored, struct vi_data *data);

static void
remove_vi_instance			(struct vi_data	*data)
{
  unregister_ttx_client(data->id);
  gtk_signal_disconnect_by_func(GTK_OBJECT(data->vbi_model),
				GTK_SIGNAL_FUNC(destroy_vi),
				data);
  gtk_timeout_remove(data->timeout);
  g_free(data);
}

static void
destroy_vi				(gpointer	ignored,
					 struct vi_data	*data)
{
  gtk_widget_destroy(data->dialog);
  remove_vi_instance(data);
}

static gboolean
on_vi_delete_event			(GtkWidget	*widget,
					 GdkEvent	*event,
					 struct vi_data	*data)
{
  remove_vi_instance(data);

  return FALSE;
}

static void
update_vi_network			(struct vi_data *data)
{
  GtkWidget *vi = data->dialog;
  GtkWidget *name, *label, *call, *tape;
  gchar *buffer;
  gint td;

  name = lookup_widget(vi, "label204");
  label = lookup_widget(vi, "label205");
  call = lookup_widget(vi, "label206");
  tape = lookup_widget(vi, "label207");

  pthread_mutex_lock(&network_mutex);
  if (*current_network.name)
    gtk_label_set_text(GTK_LABEL(name), current_network.name);
  else /* NLS: Network info, network name */
    gtk_label_set_text(GTK_LABEL(name), _("Unknown"));

  if (*current_network.call)
    gtk_label_set_text(GTK_LABEL(call), current_network.call);
  else /* NLS: Network info, call letters (USA) - not applicable */
    gtk_label_set_text(GTK_LABEL(call), _("n/a"));

  td = current_network.tape_delay;
  if (td == 0) /* NLS: Network info, tape delay (USA) */
    buffer = g_strdup(_("none"));
  else if (td < 60) /* NLS: Network info, tape delay (USA) */
    buffer = g_strdup_printf((char *)
			     ngettext("%d minute", "%d minutes", td), td);
  else /* NLS: Network info, tape delay (USA) */
    buffer = g_strdup_printf(_("%d h %d m"), td / 60, td % 60);
  gtk_label_set_text(GTK_LABEL(tape), buffer);
  g_free(buffer);

  pthread_mutex_unlock(&network_mutex);
}

static void
update_vi_program			(struct vi_data *data)
{
  GtkWidget *vi = data->dialog;
  GtkWidget *title, *date, *audio, *caption;
  GtkWidget *type, *synopsis, *vbox, *widget;
  vbi_program_info *pi = &program_info[0];
  gchar buffer[300], buffer2[256], *s;
  gint i, n, r;

  title = lookup_widget(vi, "label906");
  date = lookup_widget(vi, "label914");
  audio = lookup_widget(vi, "label915");
  caption = lookup_widget(vi, "label916");
  type = lookup_widget(vi, "label911");
  synopsis = lookup_widget(vi, "label917");
  vbox = lookup_widget(vi, "vbox45");

  pthread_mutex_lock(&prog_info_mutex);

  /* Title */

  if (pi->title[0])
    gtk_label_set_text(GTK_LABEL(title), pi->title);
  else
    gtk_label_set_text(GTK_LABEL(title), _("Current program"));

  /* Date, Length, Elapsed */

  buffer[0] = 0;

  if (pi->month >= 0)
    {
      struct tm tm;

      memset(&tm, 0, sizeof(tm));
      tm.tm_min = pi->min;
      tm.tm_hour = pi->hour;
      tm.tm_mday = pi->day + 1;
      tm.tm_mon = pi->month;

      /*
       *  NLS: Program info date, see man strftime;
       *  Only month day, month, hour and min valid.
       */
      strftime(buffer, sizeof(buffer) - 1,
	       _("%d %b  %I:%M %p    "), &tm);
    }

  n = strlen(buffer);

  if (pi->length_hour >= 0)
    {
      gint rem_hour, rem_min;

      rem_hour = pi->length_hour - pi->elapsed_hour;
      rem_min = pi->length_min - pi->elapsed_min;
      if (rem_min < 0)
	{
	  rem_hour -= 1;
	  rem_min += 60;
	}

      if (pi->elapsed_hour >= 0 /* is valid */
	  && rem_hour >= 0 /* elapsed <= length */)
	{
	  /* NLS: Program info length, elapsed; hours, minutes */
	  snprintf(buffer + n, sizeof(buffer) - n - 1,
		   _("\nLength: %uh%02u (%uh%02u remaining)"),
		   pi->length_hour, pi->length_min,
		   rem_hour, rem_min);
	}
      else 
	{
	  /* NLS: Program info length; hours, minutes */
	  snprintf(buffer + n, sizeof(buffer) - n - 1,
		   _("Length: %uh%02u"), pi->length_hour, pi->length_min);
	}
    }
  else if (pi->elapsed_hour >= 0) 
    {
      /* NLS: Program info elapsed; hours, minutes */
      snprintf(buffer + n, sizeof(buffer) - n - 1,
	       _("Elapsed: %uh%02u"), pi->elapsed_hour, pi->elapsed_min);
    }

  gtk_label_set_text(GTK_LABEL(date), buffer);

  /* Audio */

  buffer[0] = 0;

  if (pi->audio[0].mode != VBI_AUDIO_MODE_NONE
      && pi->audio[1].mode != VBI_AUDIO_MODE_NONE
      && pi->audio[0].mode != VBI_AUDIO_MODE_UNKNOWN
      && pi->audio[1].mode != VBI_AUDIO_MODE_UNKNOWN)
    {
      gchar *l1, *l2;

      /* XXX Latin-1 */
      l1 = pi->audio[0].language;
      l2 = pi->audio[1].language;

      if (l1 || l2)
	{
	  if (!l1) /* Program info audio language */
	    l1 = _("unknown language");
	  if (!l2)
	    l2 = _("unknown language");
	  snprintf(buffer, sizeof(buffer) - 1,
		   /* Program info audio mode, language */
		   _("Channel 1: %s (%s)\nChannel 2: %s (%s)"),
		   _(zvbi_audio_mode_str[pi->audio[0].mode]), l1,
		   _(zvbi_audio_mode_str[pi->audio[1].mode]), l2);
	}
      else
	  snprintf(buffer, sizeof(buffer) - 1,
		   /* Program info audio mode 1&2
		      (eg. language 1&2: "mono", "mono") */
		   _("Channel 1: %s\nChannel 2: %s"),
		   _(zvbi_audio_mode_str[pi->audio[0].mode]),
		   _(zvbi_audio_mode_str[pi->audio[1].mode]));
    }
  else if (pi->audio[0].mode != VBI_AUDIO_MODE_NONE
	   && pi->audio[0].mode != VBI_AUDIO_MODE_UNKNOWN)
    {
      /* XXX Latin-1 */
      gchar *l1 = pi->audio[0].language;

      if (l1)
	snprintf(buffer, sizeof(buffer) - 1,
		 /* Program info audio mode, language */
		 _("Audio: %s (%s)"),
		 _(zvbi_audio_mode_str[pi->audio[0].mode]), l1);
      else
	snprintf(buffer, sizeof(buffer) - 1,
		 /* Program info audio mode */
		 _("Audio: %s"),
		 _(zvbi_audio_mode_str[pi->audio[0].mode]));
    }

  gtk_label_set_text(GTK_LABEL(audio), buffer);

  /* Caption */

  buffer[0] = 0;
  n = 0;

  if (pi->caption_services == -1)
    ;
  else if (pi->caption_services == 1)
    {
      gchar *l = pi->caption_language[0];

      snprintf(buffer, sizeof(buffer) - 1,
	      /* Program info: has subtitles [language if known] */
	      l ? _("Captioned %s") : _("Captioned"), l);
    }
  else
    for (i = 0; i < 8; i++)
      if (pi->caption_services & (1 << i))
	{
	  gchar *l = pi->caption_language[i];

	  if (n > (sizeof(buffer) - 3))
	    break;
	  if (i > 0)
	    {
	      buffer[n] = ',';
	      buffer[n + 1] = ' ';
	      n += 2;
	    }
	  /* Proper names, no i18n. */
	  if (l)
	    r = snprintf(buffer + n, sizeof(buffer) - n - 1,
			(i < 4) ? "Caption %d: %s" : "Text %d: %s",
			i & 3, l);
	  else
	    r = snprintf(buffer + n, sizeof(buffer) - n - 1,
			(i < 4) ? "Caption %d" : "Text %d", i & 3);
	  if (r < 0)
	    break;
	  
	  n += r;
	}

  gtk_label_set_text(GTK_LABEL(caption), buffer);

  /* Rating */

  s = vbi_rating_string(pi->rating_auth, pi->rating_id);
  if (!s)
    s = "";
  strncpy(buffer, s, sizeof(buffer) - 1);
  n = strlen(s);

  buffer2[0] = 0;

  switch (pi->rating_auth)
    {
    case VBI_RATING_AUTH_MPAA:
      snprintf(buffer2, sizeof(buffer2) - 1,
	       "rating_mpaa_%x.png",
	       pi->rating_id & 7);
      break;

    case VBI_RATING_AUTH_TV_US:
      snprintf(buffer2, sizeof(buffer2) - 1,
	       "rating_tv_us_%x%x.png",
	       pi->rating_id & 7, pi->rating_dlsv & 15);
      if (n > 0 && pi->rating_dlsv != 0)
	snprintf(buffer + n, sizeof(buffer) - n - 1,
		 " %s%s%s%s",
		 (pi->rating_dlsv & VBI_RATING_D) ? "D" : "",
		 (pi->rating_dlsv & VBI_RATING_L) ? "L" : "",
		 (pi->rating_dlsv & VBI_RATING_S) ? "S" : "",
		 (pi->rating_dlsv & VBI_RATING_V) ? "V" : "");
      break;

    case VBI_RATING_AUTH_TV_CA_EN:
      snprintf(buffer2, sizeof(buffer2) - 1,
	       "rating_tv_ca_en_%x.png",
	       pi->rating_id & 7);
      break;

    case VBI_RATING_AUTH_TV_CA_FR:
      snprintf(buffer2, sizeof(buffer2) - 1,
	       "rating_tv_ca_fr_%x.png",
	       pi->rating_id & 7);
      break;

    default:
      break;
    }

  if (buffer2[0] == 0 || !(widget = z_load_pixmap (buffer2)))
    {
      if (buffer[0] == 0)
	/* NLS: Current program rating
	   (not rated means we don't know, not it's exempt) */
	widget = gtk_label_new(_("Not rated"));
      else
	widget = gtk_label_new(buffer);

      gtk_widget_show(widget);
    }

  if (data->rating)
    gtk_container_remove(GTK_CONTAINER(vbox), data->rating);
  gtk_box_pack_start(GTK_BOX(vbox), data->rating = widget,
		     FALSE, FALSE, 0);
  gtk_box_reorder_child(GTK_BOX(vbox), widget, 0);

  /* Type */

  buffer[0] = 0;
  n = 0;

  for (i = 0; i < 32; i++)
    {
      s = vbi_prog_type_string(pi->type_classf, pi->type_id[i]);

      if (!s)
	break;

      r = snprintf(buffer + n, sizeof(buffer) - n - 1,
		   "%s%s", (i > 0) ? "\n" : "", s);
      if (r < 0)
	break;
	  
      n += r;

      if (pi->type_classf != VBI_PROG_CLASSF_EIA_608)
	break;
    }

  if (islower(buffer[0]))
      buffer[0] = toupper(buffer[0]);

  gtk_label_set_text(GTK_LABEL(type), buffer);

  /* Synopsis */

  buffer[0] = 0;

  if ((pi->description[0][0] | pi->description[1][0]
       | pi->description[2][0] | pi->description[3][0]
       | pi->description[4][0] | pi->description[5][0]
       | pi->description[6][0] | pi->description[7][0]))
    snprintf(buffer, sizeof(buffer) - 1,
	     "%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s",
	     pi->description[0], pi->description[1],
	     pi->description[2], pi->description[3],
	     pi->description[4], pi->description[5],
	     pi->description[6], pi->description[7]);

  gtk_label_set_text(GTK_LABEL(synopsis), buffer);

  pthread_mutex_unlock(&prog_info_mutex);
}

static gint
event_timeout				(struct vi_data	*data)
{
  enum ttx_message msg;
  ttx_message_data msg_data;

  while ((msg = peek_ttx_message(data->id, &msg_data)))
    {
      switch (msg)
	{
	case TTX_PAGE_RECEIVED:
	case TTX_TRIGGER:
	case TTX_CHANNEL_SWITCHED:
	  break;
	case TTX_NETWORK_CHANGE:
	  if (data->type == VI_TYPE_NETWORK)
	    update_vi_network(data);
	  break;
        case TTX_PROG_INFO_CHANGE:
	  if (data->type == VI_TYPE_PROGRAM)
	    update_vi_program(data);
	  break;
	case TTX_BROKEN_PIPE:
	  g_warning("Broken TTX pipe");
	  return FALSE;
	default:
	  g_warning("Unknown message: %d", msg);
	  break;
	}
    }

  return TRUE;
}

GtkWidget *
zvbi_build_network_info(void)
{
  GtkWidget * vbi_info = build_widget("vbi_info", NULL);
  struct vi_data *data;

  if (!zvbi_get_object())
    {
      ShowBox("VBI couldn't be opened, Teletext won't work",
	      GNOME_MESSAGE_BOX_ERROR);
      return vbi_info;
    }

  data = g_malloc(sizeof(struct vi_data));

  data->id = register_ttx_client();
  data->vbi_model = zvbi_get_model();
  data->dialog = vbi_info;
  data->type = VI_TYPE_NETWORK;
  data->timeout = gtk_timeout_add(5000, (GtkFunction)event_timeout, data);

  gtk_signal_connect(GTK_OBJECT(data->vbi_model), "changed",
		     GTK_SIGNAL_FUNC(destroy_vi), data);
  gtk_signal_connect(GTK_OBJECT(data->dialog), "delete-event",
		     GTK_SIGNAL_FUNC(on_vi_delete_event),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(data->dialog, "button38")),
		     "clicked",
		     GTK_SIGNAL_FUNC(destroy_vi), data);

  update_vi_network(data);

  return vbi_info;
}

GtkWidget *
zvbi_build_program_info(void)
{
  GtkWidget * prog_info = build_widget("program_info", NULL);
  struct vi_data *data;

  if (!zvbi_get_object())
    {
      ShowBox("VBI couldn't be opened", GNOME_MESSAGE_BOX_ERROR);
      return prog_info;
    }

  data = g_malloc(sizeof(struct vi_data));

  data->id = register_ttx_client();
  data->vbi_model = zvbi_get_model();
  data->dialog = prog_info;
  data->rating = lookup_widget(prog_info, "label919");
  data->type = VI_TYPE_PROGRAM;
  data->timeout = gtk_timeout_add(5000, (GtkFunction)event_timeout, data);

  gtk_signal_connect(GTK_OBJECT(data->vbi_model), "changed",
		     GTK_SIGNAL_FUNC(destroy_vi), data);
  gtk_signal_connect(GTK_OBJECT(data->dialog), "delete-event",
		     GTK_SIGNAL_FUNC(on_vi_delete_event),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(data->dialog, "button40")),
		     "clicked",
		     GTK_SIGNAL_FUNC(destroy_vi), data);

  update_vi_program(data);

  return prog_info;
}

gchar *
zvbi_get_name(void)
{
  if (!station_name_known || !vbi)
    return NULL;

  return g_strdup(station_name);
}

void
zvbi_name_unknown(void)
{
  station_name_known = FALSE;
}

void
zvbi_reset_program_info (void)
{
  pthread_mutex_lock(&prog_info_mutex);

  vbi_reset_prog_info(&program_info[0]);
  vbi_reset_prog_info(&program_info[1]);

  pthread_mutex_unlock(&prog_info_mutex);

  update_main_title();
  notify_clients_generic (TTX_PROG_INFO_CHANGE);
}

void
zvbi_reset_network_info (void)
{
  pthread_mutex_lock(&network_mutex);

  memset(&current_network, 0, sizeof(current_network));

  pthread_mutex_unlock(&network_mutex);

  update_main_title();
  notify_clients_generic (TTX_NETWORK_CHANGE);
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

  zvbi_reset_network_info ();
  zvbi_reset_program_info ();
}

gchar *
zvbi_current_title(void)
{
  gchar *s;

  pthread_mutex_lock(&prog_info_mutex);

  /* current program title */
  s = program_info[0].title[0] ? program_info[0].title : "";
  s = g_strdup(s);

  pthread_mutex_unlock(&prog_info_mutex);

  return s;
}

gchar *
zvbi_current_rating(void)
{
  gchar *s = NULL;

  pthread_mutex_lock(&prog_info_mutex);

  s = vbi_rating_string(program_info[0].rating_auth, program_info[0].rating_id);

  if (s == NULL)
  /* current program rating (not rated means we don't know, not it's exempt) */
    s = _("Not rated");

  pthread_mutex_unlock(&prog_info_mutex);

  return s;
}

vbi_wst_level
zvbi_teletext_level (void)
{
  return teletext_level;
}

#endif /* HAVE_LIBZVBI */
