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
 * This code is used to communicate with the VBI device (usually
 * /dev/vbi), so multiple plugins can access to it simultaneously.
 * The code uses libvbi, written by Michael Schimek.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libvbi.h>
#include <pthread.h>
#include <ctype.h>
#include <time.h>
#include <pthread.h>

#include "tveng.h"
/* Manages config values for zconf (it saves me some typing) */
#define ZCONF_DOMAIN "/zapping/options/vbi/"
#include "zconf.h"
#include "zvbi.h"
#include "zmisc.h"

/*
 * TODO:
 *	- protect against vbi=NULL
 */

#undef TRUE
#undef FALSE
#include "../common/fifo.h"

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

static struct vbi *vbi=NULL; /* holds the vbi object */
static pthread_t zvbi_thread_id; /* Just a dummy thread to select() */
static volatile gboolean exit_thread = FALSE; /* just an atomic flag
						 to tell the thread to exit */

struct ttx_client {
  fifo		mqueue;
  int		page, subpage; /* monitored page, subpage */
  int		id; /* of the client */
  pthread_mutex_t mutex;
  struct fmt_page fp; /* formatted page */
  GdkPixbuf	*unscaled; /* unscaled version of the page */
  GdkPixbuf	*scaled; /* scaled version of the page */
  int		w, h;
  int		freezed; /* do not refresh the current page */
};

static GList *ttx_clients = NULL;
static pthread_mutex_t clients_mutex;

/* handler for a vbi event */
static void event(struct dl_head *reqs, vbi_event *ev);

/* thread function (just a loop that selects on the VBI device) */
static void * zvbi_thread(void * vbi);

/* Some info about the last processed header, protected by a mutex */
static struct {
  pthread_mutex_t mutex;

  /* Generic info */
  /* xpacket condition, signalled when the station name is parsed */
  pthread_cond_t xpacket_cond;
  char xpacket[32];
  char header[64];
  char *name; /* usually, something like station-Teletext or similar
		 (Antena3-Teletexto, for example) */
  /* Pre-processed info from the header, for convenience */
  int hour, min, sec;
} last_info;

/* Open the configured VBI device, FALSE on error */
gboolean
zvbi_open_device(void)
{
  gint finetune;
  gboolean erc;
  gchar *device;
  gint index;
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

  zcc_bool(FALSE, "Enable VBI decoding", "enable_vbi");
  zcc_bool(TRUE, "Use VBI for getting station names", "use_vbi");
  zcc_char("/dev/vbi", "VBI device", "vbi_device");
  zcc_bool(TRUE, "Error correction", "erc");
  zcc_int(999, "Finetune range", "finetune");
  zcc_int(0, "Default TTX region", "default_region");
  srand(time(NULL));

  if ((vbi) || (!zcg_bool(NULL, "enable_vbi")))
    return FALSE; /* this code isn't reentrant */

  device = zcg_char(NULL, "vbi_device");
  finetune = zcg_int(NULL, "finetune");
  erc = zcg_bool(NULL, "erc");

  if (!(vbi = vbi_open(device, cache_open(), finetune)))
    {
      g_warning("cannot open %s, vbi services will be disabled",
		device);
      return FALSE;
    }

  if (vbi->cache)
    vbi->cache->op->mode(vbi->cache, CACHE_MODE_ERC, erc);

  vbi_add_handler(vbi, event, NULL);

  last_info.name = NULL;
  last_info.hour = last_info.min = last_info.sec = -1;

  pthread_mutex_init(&(last_info.mutex), NULL);
  pthread_cond_init(&(last_info.xpacket_cond), NULL);
  if (pthread_create(&zvbi_thread_id, NULL, zvbi_thread, vbi))
    {
      vbi_del_handler(vbi, event, NULL);
      vbi_close(vbi);
      vbi = NULL;
      return FALSE;
    }

  index = zcg_int(NULL, "default_region");
  if (index < 0)
    index = 0;
  if (index > 7)
    index = 7;
  vbi_set_default_region(vbi, region_mapping[index]);
  pthread_mutex_init(&clients_mutex, NULL);

  return TRUE;
}

static void
send_ttx_message(struct ttx_client *client,
		 enum ttx_message message)
{
  buffer *b = wait_empty_buffer(&client->mqueue);
  b->data = GINT_TO_POINTER(message);
  send_full_buffer(&client->mqueue, b);
}

static void
remove_client(struct ttx_client *client)
{
  uninit_fifo(&client->mqueue);
  pthread_mutex_destroy(&client->mutex);
  gdk_pixbuf_unref(client->unscaled);
  if (client->scaled)
    gdk_pixbuf_unref(client->scaled);
  g_free(client);
}

/* returns the id */
int
register_ttx_client(void)
{
  static int id;
  struct ttx_client *client;
  gchar *filename;
  int w, h; /* of the unscaled image */
  GdkPixbuf *simple;

  client = g_malloc(sizeof(struct ttx_client));
  memset(client, 0, sizeof(struct ttx_client));
  client->id = id++;
  pthread_mutex_init(&client->mutex, NULL);
  filename = g_strdup_printf("%s/%s%d.jpeg", PACKAGE_DATA_DIR,
			     "../pixmaps/zapping/vt_loading",
			     (rand()%2)+1);
  vbi_get_rendered_size(&w, &h);
  client->unscaled = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, w,
				  h);
  g_assert(client->unscaled != NULL);
  
  simple = gdk_pixbuf_new_from_file(filename);
  g_free(filename);
  if (simple)
    {
      gdk_pixbuf_scale(simple,
		       client->unscaled, 0, 0, w, h,
		       0, 0,
		       (double) w / gdk_pixbuf_get_width(simple),
		       (double) h / gdk_pixbuf_get_height(simple),
		       INTERP_MODE);
      gdk_pixbuf_unref(simple);
    }

  g_assert(init_buffered_fifo(&client->mqueue, NULL, 16, 0) > 0);
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

struct fmt_page*
get_ttx_fmt_page(int id)
{
  struct ttx_client *client;
  struct fmt_page *result=NULL;

  pthread_mutex_lock(&clients_mutex);
  if ((client = find_client(id)))
    result = &client->fp;
  pthread_mutex_unlock(&clients_mutex);

  return result;
}

enum ttx_message
peek_ttx_message(int id)
{
  struct ttx_client *client;
  buffer *b;
  enum ttx_message message;

  pthread_mutex_lock(&clients_mutex);

  if ((client = find_client(id)))
    {
      b = recv_full_buffer(&client->mqueue);
      if (b)
	{
	  message = GPOINTER_TO_INT(b->data);
	  send_empty_buffer(&client->mqueue, b);
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
get_ttx_message(int id)
{
  struct ttx_client *client;
  buffer *b;
  enum ttx_message message;

  pthread_mutex_lock(&clients_mutex);

  if ((client = find_client(id)))
    {
      b = wait_full_buffer(&client->mqueue);
      g_assert(b != NULL);
      message = GPOINTER_TO_INT(b->data);
      send_empty_buffer(&client->mqueue, b);
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

  while ((b=recv_full_buffer(&client->mqueue)))
    send_empty_buffer(&client->mqueue, b);
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

static int
build_client_page(struct ttx_client *client, int page, int subpage)
{
  GdkPixbuf *simple;
  gchar *filename;

  g_assert(client != NULL);

  pthread_mutex_lock(&client->mutex);
  if (page > 0)
    {
      if (!vbi_fetch_page(vbi, &client->fp, page, subpage, 25))
        {
	  pthread_mutex_unlock(&client->mutex);
	  return 0;
	}
      vbi_draw_page(&client->fp,
		    gdk_pixbuf_get_pixels(client->unscaled), 0);
    }
  else if (page == 0)
    {
      memset(&client->fp, 0, sizeof(struct fmt_page));
      filename = g_strdup_printf("%s/%s%d.jpeg", PACKAGE_DATA_DIR,
				 "../pixmaps/zapping/vt_loading",
				 (rand()%2)+1);
  
      simple = gdk_pixbuf_new_from_file(filename);
      g_free(filename);
      if (simple)
	{
	  gdk_pixbuf_scale(simple,
			   client->unscaled, 0, 0,
			   gdk_pixbuf_get_width(client->unscaled),
			   gdk_pixbuf_get_height(client->unscaled),
			   0, 0,
			   (double)
			   gdk_pixbuf_get_width(client->unscaled) /
			   gdk_pixbuf_get_width(simple),
			   (double)
			   gdk_pixbuf_get_height(client->unscaled) /
			   gdk_pixbuf_get_height(simple),
			   INTERP_MODE);
	  gdk_pixbuf_unref(simple);
	}
    }

  if ((!client->scaled) &&
      (client->w > 0) &&
      (client->h > 0))
    client->scaled = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
				    client->w, client->h);
  if (client->scaled)
    gdk_pixbuf_scale(client->unscaled,
		     client->scaled, 0, 0, client->w, client->h,
		     0, 0,
		     (double) client->w /
		     gdk_pixbuf_get_width(client->unscaled),
		     (double) client->h /
		      gdk_pixbuf_get_height(client->unscaled),
		     INTERP_MODE);

  pthread_mutex_unlock(&client->mutex);
  return 1; /* success */
}

void
monitor_ttx_page(int id/*client*/, int page, int subpage)
{
  struct ttx_client *client;

  pthread_mutex_lock(&clients_mutex);
  client = find_client(id);
  if (client)
    {
      client->freezed = FALSE;
      client->page = page;
      client->subpage = subpage;
      if ((page >= 0x100) && (page <= 0x899)) {
        if (build_client_page(client, page, subpage))
	  {
	    clear_message_queue(client);
	    send_ttx_message(client, TTX_PAGE_RECEIVED);
	  }
      } else {
	build_client_page(client, 0, 0);
	clear_message_queue(client);
	send_ttx_message(client, TTX_PAGE_RECEIVED);
      }
    }
  pthread_mutex_unlock(&clients_mutex);
}

void monitor_ttx_this(int id, struct fmt_page *pg)
{
  struct ttx_client *client;

  if (!pg)
    return;

  pthread_mutex_lock(&clients_mutex);
  if ((client = find_client(id)))
    {
      client->page = pg->pgno;
      client->subpage = pg->subno;
      client->freezed = TRUE;
      memcpy(&client->fp, pg, sizeof(struct fmt_page));
      vbi_draw_page(&client->fp,
		    gdk_pixbuf_get_pixels(client->unscaled), 0);
      build_client_page(client, -1, -1);
      clear_message_queue(client);
      send_ttx_message(client, TTX_PAGE_RECEIVED);
    }
  pthread_mutex_unlock(&clients_mutex);
}

void
ttx_freeze (int id)
{
  struct ttx_client *client;

  pthread_mutex_lock(&clients_mutex);
  if ((client = find_client(id)))
    client->freezed = TRUE;
  pthread_mutex_unlock(&clients_mutex);  
}

void
ttx_unfreeze (int id)
{
  struct ttx_client *client;

  pthread_mutex_lock(&clients_mutex);
  if ((client = find_client(id)))
    client->freezed = FALSE;
  pthread_mutex_unlock(&clients_mutex);  
}

static void
notify_clients(int page, int subpage)
{
  GList *p;
  struct ttx_client *client;

  pthread_mutex_lock(&clients_mutex);
  p = g_list_first(ttx_clients);
  while (p)
    {
      client = (struct ttx_client*)p->data;
      if ((client->page == page) && (!client->freezed) &&
	  ((client->subpage == subpage) || (client->subpage == ANY_SUB)))
	{
	  build_client_page(client, page, subpage);
	  send_ttx_message(client, TTX_PAGE_RECEIVED);
	}
      p = p->next;
    }
  pthread_mutex_unlock(&clients_mutex);
}

void resize_ttx_page(int id, int w, int h)
{
  struct ttx_client *client;

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
	    gdk_pixbuf_scale(client->unscaled,
			     client->scaled, 0, 0, w, h,
			     0, 0,
			     (double) w /
			     gdk_pixbuf_get_width(client->unscaled),
			     (double) h /
			     gdk_pixbuf_get_height(client->unscaled),
			     INTERP_MODE);
	  
	  client->w = w;
	  client->h = h;
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

/* Closes the VBI device */
void
zvbi_close_device(void)
{
  GList * destruction;

  if (!vbi) /* disabled */
    return;

  exit_thread = TRUE;

  pthread_join(zvbi_thread_id, NULL);
  pthread_mutex_destroy(&(last_info.mutex));
  pthread_cond_destroy(&(last_info.xpacket_cond));

  vbi_del_handler(vbi, event, NULL);
  pthread_mutex_lock(&clients_mutex);
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
  vbi_close(vbi);
  vbi = NULL;
}

/*
  Returns the global vbi object, or NULL if vbi isn't enabled or
  doesn't work. You can safely use this function to test if VBI works
  (it will return NULL if it doesn't).
*/
struct vbi *
zvbi_get_object(void)
{
  return vbi;
}

/* this is called when we receive a page, header, etc. */
static void
event(struct dl_head *reqs, vbi_event *ev)
{
    unsigned char *p;
    int hour=0, min=0, sec=0;
    char *name, *h;
    
    switch (ev->type) {
    case VBI_EVENT_HEADER:
	p = ev->p1;
	// printv("header %.32s\n", p+8);
	pthread_mutex_lock(&(last_info.mutex));
	memcpy(last_info.header,p+8,32);
	last_info.header[32] = 0;
	pthread_mutex_unlock(&(last_info.mutex));
	/* Parse the time from the header */
	/* first check that we are getting what we expect */
	if ((!(isdigit(last_info.header[25]))) ||
	    (!(isdigit(last_info.header[27]))) ||
	    (!(isdigit(last_info.header[28]))) ||
	    (!(isdigit(last_info.header[30]))) ||
	    (!(isdigit(last_info.header[31]))))
	  {
	    last_info.hour = last_info.min = last_info.sec = -1;
	    break;
	  }
	if (isdigit(last_info.header[24]))
	  hour = last_info.header[24]-'0';
	hour = hour * 10 + (last_info.header[25]-'0');
	min = (last_info.header[27]-'0')*10+(last_info.header[28]-'0');
	sec = (last_info.header[30]-'0')*10+(last_info.header[31]-'0');
	if ((hour >= 24) || (hour < 0) || (min >= 60) || (min < 0) ||
	    (sec >= 60) || (sec < 0))
	  {
	    last_info.hour = last_info.min = last_info.sec = -1;
	    break;
	  }
	pthread_mutex_lock(&(last_info.mutex));
	last_info.hour = hour;
	last_info.min = min;
	last_info.sec = sec;
	pthread_mutex_unlock(&(last_info.mutex));
	break;
    case VBI_EVENT_PAGE:
	printv("vtx page %x.%02x \r", ev->pgno,
	       ev->subno & 0xFF);
	
	/* Set the dirty flag on the page */
	notify_clients(ev->pgno, ev->subno);
	break;
    case VBI_EVENT_XPACKET:
	p = ev->p1;
	//printv("xpacket %x %x %x %x - %.20s\n",
	// 			p[0],p[1],p[3],p[5],p+20);
	pthread_mutex_lock(&(last_info.mutex));
	memcpy(last_info.xpacket,p+20,20);
	last_info.xpacket[20] = 0;
	
	/* parse the VBI name of the broadcaster */
	last_info.name = NULL;
	if (last_info.xpacket[0]) {
	  for (h = last_info.xpacket+19; h >= last_info.xpacket; h--) {
	    if (' ' != *h)
	      break;
	    *h = 0;
	  }
	  for (name = last_info.xpacket; *name == ' '; name++)
	    ;
	  if (*name)
	    {
	      last_info.name = name;
	      /* signal the condition */
	      pthread_cond_broadcast(&(last_info.xpacket_cond));
	    }
	}
	pthread_mutex_unlock(&(last_info.mutex));
	break;

    default:
    }
}

/*
  Returns a pointer to the name of the Teletext provider, or NULL if
  this name is unknown. You must g_free the returned value.
*/
gchar*
zvbi_get_name(void)
{
  gchar * p = NULL;
  struct timeval now;
  struct timespec timeout;
  int retcode;

  if (!vbi)
    return NULL;

  pthread_mutex_lock(&(last_info.mutex));
  gettimeofday(&now, NULL);
  timeout.tv_sec = now.tv_sec+1; /* Wait one second, then fail */
  timeout.tv_nsec = now.tv_usec * 1000;
  retcode = pthread_cond_timedwait(&(last_info.xpacket_cond),
				   &(last_info.mutex), &timeout);
  if ((retcode != ETIMEDOUT) && (last_info.name))
    p = g_strdup(last_info.name);
  pthread_mutex_unlock(&(last_info.mutex));

  return p;
}

/*
  Fills in the given pointers with the time as it appears in the
  header. The pointers can be NULL.
  If the time isn't known, -1 will be returned in all the fields
*/
void
zvbi_get_time(gint * hour, gint * min, gint * sec)
{
  if (!vbi)
    {
      if (hour)
	*hour = -1;
      if (min)
	*min = -1;
      if (sec)
	*sec = -1;

      return;
    }

  pthread_mutex_lock(&(last_info.mutex));

  if (hour)
    *hour = last_info.hour;
  if (min)
    *min = last_info.min;
  if (sec)
    *sec = last_info.sec;

  pthread_mutex_unlock(&(last_info.mutex));
}

static void * zvbi_thread(void *p)
{
  extern void vbi_teletext(struct vbi *vbi, buffer *b);
  struct vbi *vbi = p;
  buffer *b;

  while (!exit_thread) {
    b = wait_full_buffer(vbi->fifo);

    if (!b) {
      fprintf(stderr, "Oops! VBI read error and "
              "I don't know how to handle it.\n");
      exit(EXIT_FAILURE);
    }

    vbi_teletext(vbi, b);

    send_empty_buffer(vbi->fifo, b);
  }

  return NULL;
}
