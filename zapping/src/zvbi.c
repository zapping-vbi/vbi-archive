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

#include "site_def.h"

#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <pthread.h>
#include <ctype.h>
#include <time.h>
#include <pthread.h>
#include <math.h>

#include "tveng.h"
/* Manages config values for zconf (it saves me some typing) */
#define ZCONF_DOMAIN "/zapping/options/vbi/"
#include "zconf.h"
#include "zvbi.h"
#include "zmisc.h"
#include "interface.h"
#include "v4linterface.h"
#include "osd.h"
#include "remote.h"
#include "globals.h"
#include "subtitle.h"

#undef TRUE
#undef FALSE
#include "common/fifo.h"

#ifndef ZVBI_CAPTURE_THREAD_DEBUG
#define ZVBI_CAPTURE_THREAD_DEBUG 0
#endif

#ifdef HAVE_LIBZVBI

static vbi_decoder *	vbi;

static ZModel *		vbi_model;	/* notify clients about the
					   open/closure of the device */



static gboolean		station_name_known = FALSE;
static gchar		station_name[256];
vbi_network		current_network; /* current network info */
vbi_program_info	program_info[2]; /* current and next program */

/* Returns the global vbi object, or NULL if vbi isn't enabled or
   doesn't work. You can safely use this function to test if VBI works
   (it will return NULL if it doesn't). */
vbi_decoder *
zvbi_get_object			(void)
{
  return vbi;
}

ZModel *
zvbi_get_model			(void)
{
  return vbi_model;
}

gchar *
zvbi_get_name			(void)
{
  if (!station_name_known || !vbi)
    return NULL;

  return g_convert (station_name, NUL_TERMINATED,
		    "ISO-8859-1", "UTF-8",
		    NULL, NULL, NULL);
}

gchar *
zvbi_get_current_network_name	(void)
{
  gchar *name;

  if (*current_network.name)
    {
      name = current_network.name;
      /* FIXME the nn encoding should be UTF-8, but isn't
	 really defined. g_convert returns NULL on failure. */
      name = g_convert (name, strlen (name),
			"UTF-8", "ISO-8859-1",
			NULL, NULL, NULL);
      return name;
    }
  else
    {
      return NULL;
    }
}

void
zvbi_name_unknown		(void)
{
  station_name_known = FALSE;

  if (vbi)
    vbi_channel_switched (vbi, 0);
}

#if 0 /* temporarily disabled */

enum ttx_message {
  TTX_NONE=0, /* No messages */
  TTX_PAGE_RECEIVED, /* The monitored page has been received */
  TTX_NETWORK_CHANGE, /* New network info feeded into the decoder */
  TTX_PROG_INFO_CHANGE, /* New program info feeded into the decoder */
  TTX_TRIGGER, /* Trigger event, ttx_message_data.link filled */
  TTX_CHANNEL_SWITCHED, /* zvbi_channel_switched was called, the cache
			   has been cleared */
  TTX_BROKEN_PIPE /* No longer connected to the TTX decoder */
};

typedef struct {
  enum ttx_message msg;
  union {
    vbi_link	link; /* A trigger link */
  } data;
} ttx_message_data;

/* Trigger handling */
static gint		trigger_timeout_id = NO_SOURCE_ID;
static gint		trigger_client_id = -1;
static vbi_link		last_trigger;

static void
on_trigger_clicked		(GtkWidget *		widget,
				 vbi_link *		trigger)
{
  switch (trigger->type)
    {
    case VBI_LINK_HTTP:
    case VBI_LINK_FTP:
    case VBI_LINK_EMAIL:
      z_url_show (NULL, trigger->url);
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
acknowledge_trigger		(vbi_link *		link)
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
		   ("Open this link with the predetermined Web browser.\n"
		/* FIXME wrong */
		    "You can configure this in the Gnome Control Center "
		    "under Advanced/Preferred Applications/Web Browser"));
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

static void
update_main_title		(void)
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

static gint
trigger_timeout			(gint			client_id)
{
  enum ttx_message msg;
  ttx_message_data data;

  while ((msg = ttx_client_next_message (client_id, &data)))
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
	trigger_timeout_id = NO_SOURCE_ID;
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

static pthread_mutex_t	network_mutex;
static pthread_mutex_t	prog_info_mutex;

double			zvbi_ratio = 4.0 / 3.0;

#if 0

static void
scan_header			(vbi_page *		pg _unused_)
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
      g_strlcpy(station_name, buf, 255);
      station_name[255] = 0;
    }

  free(buf);

  station_name_known = TRUE;
}

#endif /* 0 */



int osd_pipe[2];

static void
cc_event_handler		(vbi_event *		ev,
				 void *			data)
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
  write (pipe[1], "x", 1);
}

int ttx_pipe[2];

/* This is called when we receive a page, header, etc. */
static void
event_handler			(vbi_event *		ev,
				 void *			unused _unused_)
{
  switch (ev->type) {

  case VBI_EVENT_NETWORK:
    pthread_mutex_lock(&network_mutex);

    memcpy(&current_network, &ev->ev.network, sizeof(vbi_network));

    if (*current_network.name)
      {
	g_strlcpy(station_name, current_network.name, 255);
	station_name[255] = 0;
	station_name_known = TRUE;
      }
    else if (*current_network.call)
      {
	g_strlcpy(station_name, current_network.call, 255);
	station_name[255] = 0;
	station_name_known = TRUE;
      }
    else
      {
	station_name_known = FALSE;
      }

    /* notify_clients_generic (TTX_NETWORK_CHANGE); */
    pthread_mutex_unlock(&network_mutex);

    break;

#if 0 /* temporarily disabled */
  case VBI_EVENT_TRIGGER:
    notify_clients_trigger (ev->ev.trigger);
    break;
#endif

  case VBI_EVENT_ASPECT:
    if (zconf_get_int(NULL, "/zapping/options/main/ratio") == 3)
      zvbi_ratio = ev->ev.aspect.ratio;
    break;

  default:
    break;
  }
}

/* ------------------------------------------------------------------------- */

/* Future stuff */
typedef struct _vbi_dvb_demux vbi_dvb_demux;
extern unsigned int
_vbi_dvb_demux_cor		(vbi_dvb_demux *	dx,
				 vbi_sliced *		sliced,
				 unsigned int 		sliced_lines,
				 int64_t *		pts,
				 const uint8_t **	buffer,
				 unsigned int *		buffer_left);
extern void
_vbi_dvb_demux_delete		(vbi_dvb_demux *	dx);
extern vbi_dvb_demux *
_vbi_dvb_demux_pes_new		(void *			callback,
				 void *			user_data);

static GList *			decoder_list;
static GList *			chsw_list;
static unsigned int		flush;
static pthread_t		capturer_id;
static gboolean			vbi_quit;
static gboolean			capturer_quit_ack;
static zf_fifo			sliced_fifo;
static vbi_capture *		capture;
static vbi_proxy_client *	proxy_client;
static GSource *		source;
static GIOChannel *		channel;
static guint			channel_id = NO_SOURCE_ID;
static zf_consumer		channel_consumer;
static int			pes_fd = -1;
static uint8_t			pes_buffer[8192];
static const uint8_t *		pes_bp;
static unsigned int		pes_left;
static vbi_dvb_demux *		pes_demux;
static guint			pes_timeout_id = NO_SOURCE_ID;

void
zvbi_channel_switched		(void)
{
  const tv_video_line *vi;
  const tv_video_standard *vs;
  tveng_tuned_channel *channel;
  guint scanning;
  GList *p;

  if (!vbi)
    return;

  channel = NULL;
  if ((vi = tv_cur_video_input (zapping->info))
      && vi->type == TV_VIDEO_LINE_TYPE_TUNER)
    channel = tveng_tuned_channel_nth (global_channel_list,
				       cur_tuned_channel);

  /* XXX */
  scanning = 625;
  if ((vs = tv_cur_video_standard (zapping->info)))
    if (vs->videostd_set & TV_VIDEOSTD_SET_525_60)
      scanning = 525;

  vbi_channel_switched (vbi, 0);

  for (p = chsw_list; p; p = p->next)
    {
      zvbi_chsw_fn *func = p->data;
      func (channel, scanning);
    }

  osd_clear ();

#if 0 /* Temporarily removed. */
  zvbi_reset_network_info ();
  zvbi_reset_program_info ();
#endif

  /* XXX better ask libzvbi to flush. */
  flush = 2 * 25;
}

void
zvbi_add_decoder		(zvbi_decoder_fn *	decoder,
				 zvbi_chsw_fn *		chsw)
{
  decoder_list = g_list_append (decoder_list, decoder);
  chsw_list = g_list_append (chsw_list, chsw);
}

void
zvbi_remove_decoder		(zvbi_decoder_fn *	decoder,
				 zvbi_chsw_fn *		chsw)
{
  decoder_list = g_list_remove (decoder_list, decoder);
  chsw_list = g_list_remove (chsw_list, chsw);
}

static gboolean
decoder_giofunc			(GIOChannel *		source _unused_,
				 GIOCondition		condition _unused_,
				 gpointer		data _unused_)
{
  char dummy[16];
  zf_buffer *b;

  if (read (ttx_pipe[0], dummy, 16 /* flush */) <= 0)
    return TRUE;

  while ((b = zf_recv_full_buffer (&channel_consumer)))
    {
      unsigned int n_lines;
      GList *p;

      n_lines = b->used / sizeof (vbi_sliced);

      if (flush > 0)
	{
	  --flush;
	}
      else
	{
	  for (p = decoder_list; p; p = p->next)
	    {
	      zvbi_decoder_fn *func = p->data;
	      func ((vbi_sliced *) b->data, n_lines, b->time);
	    }

	  if (vbi)
	    vbi_decode (vbi, (vbi_sliced *) b->data, (int) n_lines, b->time);
	}

      zf_send_empty_buffer (&channel_consumer, b);
    }

  return TRUE; /* call again */
}

static gboolean
pes_source_timeout		(gpointer		user_data _unused_)
{
  static vbi_sliced sliced[50];
  unsigned int n_lines;
  int64_t pts;
  GList *p;

  if (0 == pes_left) {
    ssize_t actual;

    actual = read (pes_fd, pes_buffer, sizeof (pes_buffer));
    switch (actual) {
    case -1:
      perror ("PES read");
      return TRUE; /* call again */

    case 0: /* EOF */
      fprintf (stderr, "PES rewind\n");
      lseek (pes_fd, 0, SEEK_SET);
      return TRUE;

    default:
      pes_bp = pes_buffer;
      pes_left = actual;
      break;
    }
  }

  n_lines = _vbi_dvb_demux_cor (pes_demux, sliced, N_ELEMENTS (sliced),
				&pts, &pes_bp, &pes_left);
  if (0 == n_lines)
    return TRUE;

  for (p = decoder_list; p; p = p->next)
    {
      zvbi_decoder_fn *func = p->data;
      func (sliced, n_lines, pts);
    }

  if (vbi)
    vbi_decode (vbi, sliced, n_lines, pts);

  return TRUE;
}

typedef struct {
  GSource		source;  
  GPollFD		poll_fd;
  zf_producer		producer;
} proxy_source;

static gboolean
proxy_source_prepare		(GSource *		source _unused_,
				 gint *			timeout)
{
  *timeout = -1; /* infinite */

  return FALSE; /* go poll */
}

static gboolean
proxy_source_check		(GSource *		source)
{
  proxy_source *ps = PARENT (source, proxy_source, source);

  return !!(ps->poll_fd.revents & G_IO_IN);
}

static gboolean
proxy_source_dispatch		(GSource *		source _unused_,
				 GSourceFunc		callback _unused_,
				 gpointer		user_data _unused_)
{
  struct timeval timeout;  
  vbi_sliced sliced[50];
  int n_lines;
  double time;
  GList *p;

  timeout.tv_sec = 0;
  timeout.tv_usec = 0;

  switch (vbi_capture_read_sliced (capture, sliced, &n_lines, &time, &timeout))
    {
    case 1: /* ok */
      if (ZVBI_CAPTURE_THREAD_DEBUG)
	{
	  fprintf (stdout, ",");
	  fflush (stdout);
	}

      if (flush > 0)
	{
	  --flush;
	  break;
	}

      for (p = decoder_list; p; p = p->next)
	{
	  zvbi_decoder_fn *func = p->data;
	  func (sliced, n_lines, time);
	}

      if (vbi)
	vbi_decode (vbi, sliced, n_lines, time);

      break;

    default:
      /* What now? */
      break;
    }

  return TRUE;
}

static GSourceFuncs
proxy_source_funcs = {
  proxy_source_prepare,
  proxy_source_check,
  proxy_source_dispatch,
  /* finalize */ NULL,
};

/* Attn: must be pthread_cancel-safe */

static void *
capturing_thread (void *x _unused_)
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

  if (ZVBI_CAPTURE_THREAD_DEBUG)
    fprintf (stderr, "VBI capture thread started\n");

  D();

  timeout.tv_sec = 1;
  timeout.tv_usec = 0;

  assert (zf_add_producer (&sliced_fifo, &p));

  while (!vbi_quit) {
    zf_buffer *b;
    int lines;

    b = zf_wait_empty_buffer (&p);

  retry:
    switch (vbi_capture_read_sliced (capture, (vbi_sliced *) b->data,
				     &lines, &b->time, &timeout))
      {
      case 1: /* ok */
	if (ZVBI_CAPTURE_THREAD_DEBUG)
	  {
	    fprintf (stdout, ".");
	    fflush (stdout);
	  }
	break;

      case 0: /* timeout */
	if (ZVBI_CAPTURE_THREAD_DEBUG)
	  fprintf (stderr, "Timeout in VBI capture thread %d %d\n",
		   (int) timeout.tv_sec, (int) timeout.tv_usec);
#if 0
	for (; stacked > 0; stacked--)
	  send_full_buffer (&p, PARENT (rem_head(&stack), buffer, node));
#endif
	b->used = -1;
	b->error = errno;
	b->errorstr = _("VBI interface timeout");

	zf_send_full_buffer (&p, b);

	if (channel)
	  write (ttx_pipe[1], "x", 1);

	goto abort;

      default: /* error */
	if (ZVBI_CAPTURE_THREAD_DEBUG)
	  fprintf (stderr, "Error %d, %s in VBI capture thread\n",
		   errno, strerror (errno));
	if (EIO == errno) {
	  usleep (10000); /* prevent busy loop */
	  goto retry; /* XXX */
	}
#if 0
	for (; stacked > 0; stacked--)
	  send_full_buffer (&p, PARENT (rem_head(&stack), buffer, node));
#endif
	b->used = -1;
	b->error = errno;
	b->errorstr = _("VBI interface: Failed to read from the device");

	zf_send_full_buffer (&p, b);

	if (channel)
	  write (ttx_pipe[1], "x", 1);

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

    if (channel)
      write (ttx_pipe[1], "x", 1);
  }

 abort:
  if (ZVBI_CAPTURE_THREAD_DEBUG)
    fprintf (stderr, "VBI capture thread terminates\n");

  zf_rem_producer (&p);

  capturer_quit_ack = TRUE;

  return NULL;
}

#define SERVICES (VBI_SLICED_TELETEXT_B | \
		  VBI_SLICED_VPS | \
		  VBI_SLICED_CAPTION_625 | \
		  VBI_SLICED_CAPTION_525 | \
		  VBI_SLICED_WSS_625 | \
		  VBI_SLICED_WSS_CPR1204)

static gint
join_thread			(const char *		who,
				 pthread_t		id,
				 gboolean *		ack,
				 gint			timeout)
{
  vbi_quit = TRUE;

  /* Dirty. Where is pthread_try_join()? */
  for (; (!*ack) && timeout > 0; timeout--) {
    usleep (100000);
  }

  /* Ok, you asked for it */
  if (timeout == 0) {
    int r;

    printv("Unfriendly vbi capture termination\n");
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

static void
destroy_capture			(void)
{
  D();

  vbi_capture_delete (capture);
  capture = NULL;

  D();

  if (-1 != pes_fd)
    {
      _vbi_dvb_demux_delete (pes_demux);
      pes_demux = NULL;

      close (pes_fd);
      pes_fd = -1;
    }
  else if (proxy_client)
    {
      vbi_proxy_client_destroy (proxy_client);
      proxy_client = NULL;
    }

  D();

  vbi_decoder_delete (vbi);
  vbi = NULL;

  D();
}

static void
destroy_threads			(void)
{
  D();

  if (vbi)
    {
      D();

      if (NO_SOURCE_ID != pes_timeout_id)
	{
	  g_source_remove (pes_timeout_id);
	}
      else if (proxy_client)
	{
	  proxy_source *ps = PARENT (source, proxy_source, source);

	  zf_rem_producer (&ps->producer);

	  g_source_destroy (source);
	  g_source_unref (source);
	  source = NULL;
	}
      else
	{
	  join_thread ("cap", capturer_id, &capturer_quit_ack, 15);
	}

      D();

	{
	  /* Undo g_io_add_watch(). */
	  g_source_remove (channel_id);

	  g_io_channel_unref (channel);
	  channel = NULL;

	  zf_rem_consumer (&channel_consumer);
	}

      D();

      zf_destroy_fifo (&sliced_fifo);

      destroy_capture ();
    }

  D();
}

static gboolean
open_pes			(const gchar *		dev_name)
{
  pes_fd = open (dev_name, O_RDONLY | O_NOCTTY | O_NONBLOCK, 0);
  if (-1 == pes_fd)
    return FALSE;

  pes_bp = pes_buffer;
  pes_left = 0;

  pes_demux = _vbi_dvb_demux_pes_new (NULL, NULL);
  g_assert (NULL != pes_demux);

  return TRUE;
}

static gboolean
open_proxy			(const gchar *		dev_name,
				 unsigned int		scanning,
				 unsigned int *		services)
{
  char *errstr = NULL;

  proxy_client = vbi_proxy_client_create (dev_name,
					  PACKAGE,
					  /* client flags */ 0,
					  &errstr,
					  !!debug_msg);
  if (!proxy_client || errstr)
    {
      g_assert (!proxy_client);

      if (errstr)
	{
	  printv ("Cannot create proxy client: %s\n", errstr);
	  free (errstr);
	  errstr = NULL;
	}

      return FALSE;
    }

  capture = vbi_capture_proxy_new (proxy_client,
				   /* buffers */ 20,
				   (int) scanning,
				   services,
				   /* strict */ -1,
				   &errstr);
  if (!capture || errstr)
    {
      g_assert (!capture);

      vbi_proxy_client_destroy (proxy_client);
      proxy_client = NULL;

      if (errstr)
	{
	  printv ("Cannot create proxy device: %s\n", errstr);
	  free (errstr);
	  errstr = NULL;
	}

      return FALSE;
    }

  return TRUE;
}

static gboolean
init_threads			(const gchar *		dev_name,
				 int			given_fd)
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
  char *errstr;
  int buffer_size;
  unsigned int scanning;
  const tv_video_standard *vs;

  D();

  if (!(vbi = vbi_decoder_new ()))
    {
      RunBox (failed, GTK_MESSAGE_ERROR, memory);
      return FALSE;
    }

  D();

  /* XXX */
  scanning = 625;
  if (zapping->info)
    if ((vs = tv_cur_video_standard (zapping->info)))
      if (vs->videostd_set & TV_VIDEOSTD_SET_525_60)
	scanning = 525;

  if (g_file_test (dev_name, G_FILE_TEST_IS_REGULAR))
    {
      if (!open_pes (dev_name))
	{
	  RunBox (failed, GTK_MESSAGE_ERROR, "");

	  vbi_decoder_delete (vbi);
	  vbi = NULL;

	  return FALSE;
	}
    }
  else
    {

#ifdef ENABLE_BKTR

      if (!(capture = vbi_capture_bktr_new (dev_name,
					    scanning,
					    &services,
					    /* strict */ -1,
					    &errstr,
					    !!debug_msg)))
	{
	  gchar *t;

	  t = g_locale_to_utf8 (errstr, NUL_TERMINATED, NULL, NULL, NULL);
	  g_assert (t != NULL);

	  RunBox (failed, GTK_MESSAGE_ERROR, t);

	  g_free (t);

	  free (errstr);

	  vbi_decoder_delete (vbi);
	  vbi = NULL;

	  return FALSE;
	}

#else /* !ENABLE_BKTR */

      if (!(open_proxy (dev_name,
			scanning,
			&services)))
	{
	  if (!(capture = vbi_capture_v4l2_new (dev_name,
						/* buffers */ 20,
						&services,
						/* strict */ -1,
						&errstr,
						!!debug_msg)))
	    {
	      printv ("vbi_capture_v4l2_new error: %s\n", errstr);

	      if (errstr)
		free (errstr);

	      if (!(capture = vbi_capture_v4l_sidecar_new (dev_name,
							   given_fd,
							   &services,
							   /* strict */ -1,
							   &errstr,
							   !!debug_msg)))
		{
		  gchar *t;
		  gchar *s;

		  t = g_locale_to_utf8 (errstr, NUL_TERMINATED, NULL, NULL, NULL);
		  g_assert (t != NULL);

		  switch (errno)
		    {
		    case ENOENT:
		    case ENXIO:
		    case ENODEV:
		      s = g_strconcat (t, "\n", mknod_hint, NULL);
		      RunBox (failed, GTK_MESSAGE_ERROR, s);
		      g_free (s);
		      break;

		    default:
		      RunBox (failed, GTK_MESSAGE_ERROR, t);
		      break;
		    }

		  g_free (t);

		  free (errstr);
	  
		  vbi_decoder_delete (vbi);
		  vbi = NULL;

		  return FALSE;
		}
	    }
	}

#endif /* !ENABLE_BKTR */

    }

  D();

  /* XXX when we have WSS625, disable video sampling*/

  if (-1 != pes_fd)
    {
      buffer_size = 50 * sizeof (vbi_sliced);
    }
  else
    {
      vbi_raw_decoder *raw;

      raw = vbi_capture_parameters (capture);
      buffer_size = (raw->count[0] + raw->count[1]) * sizeof (vbi_sliced);
    }

  if (!zf_init_buffered_fifo (&sliced_fifo, "vbi-sliced", 20, buffer_size))
    {
      ShowBox(failed, GTK_MESSAGE_ERROR, memory);
      destroy_capture ();
      return FALSE;
    }

  D();

  vbi_quit = FALSE;
  capturer_quit_ack = FALSE;

  assert (zf_add_consumer (&sliced_fifo, &channel_consumer));

  channel = g_io_channel_unix_new (ttx_pipe[0]);

  channel_id =
    g_io_add_watch (channel, G_IO_IN, decoder_giofunc, /* user_data */ NULL);

  D();

  if (-1 != pes_fd)
    {
      /* XXX this is terrible inaccurate. */
      pes_timeout_id = g_timeout_add (40, pes_source_timeout,
				      /* user_data */ NULL);
    }
  else if (proxy_client)
    {
      proxy_source *ps;

      /* We can avoid a thread because the proxy buffers for us.
         XXX should also work with mmapped reads, provided the
	 read call does not block since we poll() already. */

      /* Attn: source_funcs must be static. */
      source = g_source_new (&proxy_source_funcs, sizeof (proxy_source));

      ps = PARENT (source, proxy_source, source);

      ps->poll_fd.fd = vbi_capture_fd (capture);
      ps->poll_fd.events = G_IO_IN;
      ps->poll_fd.revents = 0;

      g_source_add_poll (source, &ps->poll_fd);

      assert (zf_add_producer (&sliced_fifo, &ps->producer));

      g_source_attach (source, /* context = default */ NULL);
    }
  else
    {
      if (pthread_create (&capturer_id, NULL, capturing_thread, NULL))
	{
	  ShowBox(failed, GTK_MESSAGE_ERROR, thread);
	  zf_destroy_fifo (&sliced_fifo);
	  destroy_capture ();
	  return FALSE;
	}
    }

  D();

  return TRUE;
}

/**
 * Closes the VBI device.
 */
void
zvbi_close_device		(void)
{
  D();

  if (!vbi) /* disabled */
    return;

#if 0 /* temporarily disabled */
  if (trigger_timeout_id >= 0)
    gtk_timeout_remove(trigger_timeout_id);
  if (trigger_client_id >= 0)
    unregister_ttx_client(trigger_client_id);

  D();

  trigger_client_id = -1;
  trigger_timeout_id = NO_SOURCE_ID;
#endif

  D();
 
  destroy_threads ();

  D();

  zmodel_changed (vbi_model);

  D();
}

/**
 * Open the configured VBI device, FALSE on error.
 */
gboolean
zvbi_open_device		(const char *		dev_name)
{
  int given_fd;
  vbi_bool success;

  D();

  given_fd = -1;
  if (zapping->info
      && tv_get_fd (zapping->info) >= 0
      && tv_get_fd (zapping->info) < 100)
    given_fd = tv_get_fd (zapping->info);

  D();

  if (!init_threads (dev_name, given_fd))
    return FALSE;

  D();

  /* Send all events to our main event handler */
  success = vbi_event_handler_add (vbi,
				   ~0 /* all events */,
				   event_handler,
				   /* user_data */ NULL);
  g_assert (success);

  D();

  zmodel_changed (vbi_model);

  D();

  /* Send OSD relevant events to our OSD event handler */
  success = vbi_event_handler_add (vbi,
				   VBI_EVENT_CAPTION | VBI_EVENT_TTX_PAGE,
				   cc_event_handler,
				   osd_pipe);
  g_assert (success);

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
}

#endif /* HAVE_LIBZVBI */

void
vbi_gui_sensitive		(gboolean		on)
{
  gtk_action_group_set_visible (zapping->vbi_action_group, on);

  if (on)
    {
      printv ("VBI enabled, showing GUI items\n");
    }
  else
    {
      printv ("VBI disabled, removing GUI items\n");

      /* Set the capture mode to a default value and disable VBI */
      if (zcg_int (NULL, "capture_mode") == OLD_TVENG_TELETEXT)
	zcs_int (OLD_TVENG_CAPTURE_READ, "capture_mode");
    }
}

#ifdef HAVE_LIBZVBI

static void
on_vbi_prefs_changed		(const gchar *		key _unused_,
				 gboolean *		new_value,
				 gpointer		data _unused_)
{
  /* Try to open the device */
  if (!vbi && *new_value)
    {
      D();

      if (!zvbi_open_device (zcg_char (NULL, "vbi_device")))
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
      if (CAPTURE_MODE_NONE == tv_get_capture_mode (zapping->info)
	  || CAPTURE_MODE_TELETEXT == tv_get_capture_mode (zapping->info))
	zmisc_switch_mode (DISPLAY_MODE_WINDOW,
			   CAPTURE_MODE_OVERLAY,
			   zapping->info);

      zvbi_close_device ();

      D();
    }

  /* disable VBI if needed */
  vbi_gui_sensitive (!!zvbi_get_object ());
}

static void
on_vbi_device_changed		(const gchar *		key _unused_,
				 const gchar **		new_value _unused_,
				 gpointer		data _unused_)
{
  gboolean enable_vbi;

  enable_vbi = zcg_bool (NULL, "enable_vbi");
  if (enable_vbi && NULL != vbi)
    {
      int given_fd;

      /* Restart with new device. */

      D();

      destroy_threads ();

      D();

      given_fd = -1;
      if (zapping->info
	  && tv_get_fd (zapping->info) >= 0
	  && tv_get_fd (zapping->info) < 100)
	given_fd = tv_get_fd (zapping->info);

      D();

      if (!init_threads (zcg_char (NULL, "vbi_device"), given_fd))
	{
	  /* Define in site_def.h if you don't want this to happen */
#ifndef DO_NOT_DISABLE_VBI_ON_FAILURE
	  zcs_bool(FALSE, "enable_vbi");
	  vbi_gui_sensitive (FALSE);
#endif	  
	}
      else
	{
	  vbi_bool success;

	  /* Send all events to our main event handler */
	  success = vbi_event_handler_add (vbi,
					   ~0 /* all events */,
					   event_handler,
					   /* user_data */ NULL);
	  g_assert (success);

	  D();

	  /* Send OSD relevant events to our OSD event handler */
	  success = vbi_event_handler_add (vbi,
					   VBI_EVENT_CAPTION |
					   VBI_EVENT_TTX_PAGE,
					   cc_event_handler,
					   osd_pipe);
	  g_assert (success);

	  D();
	}
    }
}

#endif /* HAVE_LIBZVBI */

void
shutdown_zvbi			(void)
{

#ifdef HAVE_LIBZVBI

  pthread_mutex_destroy (&prog_info_mutex);
  pthread_mutex_destroy (&network_mutex);

  D();

  if (vbi)
    zvbi_close_device ();

  D();

  if (vbi_model)
    g_object_unref (G_OBJECT (vbi_model));

  D();

  close (ttx_pipe[1]);
  close (ttx_pipe[0]);

  close (osd_pipe[1]);
  close (osd_pipe[0]);

#endif /* HAVE_LIBZVBI */

}

void
startup_zvbi			(void)
{
#ifdef HAVE_LIBZVBI
  zcc_bool (TRUE, "Enable VBI decoding", "enable_vbi");
#else
  zcc_bool (FALSE, "Enable VBI decoding", "enable_vbi");
#endif

  zcc_char ("/dev/vbi0", "VBI device", "vbi_device");

  /* Currently unused. */
  zcc_int (2, "ITV filter level", "filter_level");
  zcc_int (1, "Default action for triggers", "trigger_default");
  zcc_int (1, "Program related links", "pr_trigger");
  zcc_int (1, "Network related links", "nw_trigger");
  zcc_int (1, "Station related links", "st_trigger");
  zcc_int (1, "Sponsor messages", "sp_trigger");
  zcc_int (1, "Operator messages", "op_trigger");

#ifdef HAVE_LIBZVBI

  zconf_add_hook ("/zapping/options/vbi/enable_vbi",
		  (ZConfHook) on_vbi_prefs_changed,
		  (gpointer) 0xdeadbeef);

  zconf_add_hook ("/zapping/options/vbi/vbi_device",
		  (ZConfHook) on_vbi_device_changed,
		  (gpointer) 0xdeadbeef);

  vbi_model = ZMODEL (zmodel_new ());

  vbi_reset_prog_info (&program_info[0]);
  vbi_reset_prog_info (&program_info[1]);

  pthread_mutex_init (&network_mutex, NULL);
  pthread_mutex_init (&prog_info_mutex, NULL);

  CLEAR (current_network);

  if (pipe (osd_pipe))
    {
      g_warning ("Cannot create osd pipe");
      exit (EXIT_FAILURE);
    }

  if (pipe (ttx_pipe))
    {
      g_warning ("Cannot create ttx pipe");
      exit (EXIT_FAILURE);
    }

#endif /* HAVE_LIBZVBI */

}
