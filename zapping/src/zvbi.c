/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000 I�aki Garc�a Etxebarria
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
 * The code uses libvbi, a nearly verbatim copy of alevt.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#ifdef HAVE_GDKPIXBUF
#include <gdk-pixbuf/gdk-pixbuf.h>
#endif
#include <libvbi.h>
#include <pthread.h>
#include <ctype.h>
#include <time.h>

#include "tveng.h"
#include "zconf.h"
/* Manages config values for zconf (it saves me some typing) */
#define ZCONF_DOMAIN "/zapping/options/vbi/"
#include "zmisc.h"
#include "zvbi.h"
#include "interface.h"

static struct vbi *vbi=NULL; /* holds the vbi object */

static pthread_t zvbi_thread_id; /* Just a dummy thread to select() */
static gboolean exit_thread = FALSE; /* just an atomic flag to tell the
					thread to exit*/

static gboolean vbi_mode=FALSE; /* VBI mode, default off */

/* Go to the index by default */
static gint cur_page=0x100, cur_subpage=ANY_SUB;

/* handler for a vbi event */
static void event(struct dl_head *reqs, struct vt_event *ev);

/* thread function (just a loop that selects on the VBI device) */
static void * zvbi_thread(void * unused);

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

  zcc_bool(TRUE, "Enable VBI decoding", "enable_vbi");
  zcc_bool(TRUE, "Use VBI for getting station names", "use_vbi");
  zcc_char("/dev/vbi", "VBI device", "vbi_device");
  zcc_bool(TRUE, "Error correction", "erc");
  zcc_int(999, "Finetune range", "finetune");

  if ((vbi) || (!zcg_bool(NULL, "enable_vbi")))
    return FALSE; /* this code isn't reentrant */

  device = zcg_char(NULL, "vbi_device");
  finetune = zcg_int(NULL, "finetune");
  erc = zcg_bool(NULL, "erc");

  fdset_init(fds);

  if (!(vbi = vbi_open(device, cache_open(), finetune, -1)))
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
  if (pthread_create(&zvbi_thread_id, NULL, zvbi_thread, NULL))
    {
      vbi_del_handler(vbi, event, NULL);
      vbi_close(vbi);
      vbi = NULL;
      return FALSE;
    }

  zvbi_set_current_page(cur_page, cur_subpage);

  return TRUE;
}

/* Closes the VBI device */
void
zvbi_close_device(void)
{
  if (!vbi) /* disabled */
    return;

  exit_thread = TRUE;

  pthread_join(zvbi_thread_id, NULL);
  pthread_mutex_destroy(&(last_info.mutex));
  pthread_cond_destroy(&(last_info.xpacket_cond));

  vbi_del_handler(vbi, event, NULL);
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

/* renders the formatted and mem versions of the page if it's being
   monitored */
void
zvbi_render_monitored_page(struct vt_page * vtp);

/* this is called when we receive a page, header, etc. */
static void
event(struct dl_head *reqs, struct vt_event *ev)
{
    unsigned char *p;
    struct vt_page *vtp;
    int hour=0, min=0, sec=0;
    char *name, *h;
    
    switch (ev->type) {
    case EV_HEADER:
	p = ev->p1;
	if (ev->i2 & PG_OUTOFSEQ)
	  break;
	//	fprintf(stderr,"header %.32s\n", p+8);
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
    case EV_PAGE:
	vtp = ev->p1;
	//	fprintf(stderr,"vtx page %x.%02x  \r", vtp->pgno,
	//		vtp->subno);
	/* Set the dirty flag on the page */
	zvbi_set_page_state(vtp->pgno, vtp->subno, TRUE, time(NULL));
	/* Now render the mem version of the page */
	zvbi_render_monitored_page(vtp);
	break;
    case EV_XPACKET:
	p = ev->p1;
	//	fprintf(stderr,"xpacket %x %x %x %x - %.20s\n",
	//			p[0],p[1],p[3],p[5],p+20);
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

/* thread function (just a loop that selects on the VBI device) */
static void * zvbi_thread(void * unused)
{
  while (!exit_thread)
    fdset_select(fds, 200); /* 0.2s timeout */

  return NULL;
}

/*
  Returns the specified page. Use ANY_SUB for accessing the most
  recent subpage, its number otherwise.
  Returns the page on success, NULL otherwise
  The returned struct doesn't need to be freed
*/
struct vt_page *
zvbi_get_page(gint page, gint subpage)
{
  struct vt_page * result;

  if ((!vbi) || (!vbi->cache))
    return NULL;

  result = vbi->cache->op->get(vbi->cache, page, subpage);

  if (result)
    zvbi_set_page_state(result->pgno, result->subno, FALSE, time(NULL));

  return result;
}

/*
  Stores in the given location the formatted version of the given
  page. use ANY_SUB for accessing the most recent subpage, its
  number otherwise.
  Returns TRUE if the page has been rendered properly, FALSE if it
  wasn't in the cache.
  Reveal specifies whether to render hidden text or not, usually FALSE
*/
gboolean
zvbi_format_page(gint page, gint subpage, gboolean reveal,
		 struct fmt_page *pg)
{
  struct vt_page *vtp;

  if ((!vbi) || (!vbi->cache))
    return FALSE;

  vtp = zvbi_get_page(page, subpage);
  if (!vtp)
    return FALSE;

  zvbi_set_page_state(vtp->pgno, vtp->subno, FALSE, time(NULL));

  fmt_page(reveal, pg, vtp);

  return TRUE;
}

/*
  Renders the given formatted page into a paletted buffer. Each byte
  in the buffer represents 1-byte, and the palette is as follows:
                      {  0,  0,  0},
		      {255,  0,  0},
		      {  0,255,  0},
		      {255,255,  0},
		      {  0,  0,255},
		      {255,  0,255},
		      {  0,255,255},
		      {255,255,255}
   width and height are pointers where the dimensions of the rendered
   buffer will be stored. They cannot be NULL.
   Returns the allocated buffer (width*height bytes), or NULL on
   error.
   The allocated buffer should be free'd, not g_free'd, since it's
   allocated with malloc. NULL on error.
*/
unsigned char*
zvbi_render_page(struct fmt_page *pg, gint *width, gint *height)
{
  if ((!vbi) || (!width) || (!height) || (!pg))
    return NULL;

  return mem_output(pg, width, height);
}

/*
  Renders the given formatted page into a RGBA buffer.
  alpha is a 8-entry unsigned char array specifying the alpha values
  for the 8 different colours.
  The same is just like in zvbi_render_page.
*/
unsigned char*
zvbi_render_page_rgba(struct fmt_page *pg, gint *width, gint *height,
		      unsigned char * alpha)
{
  unsigned char * mem;
  unsigned char * extra_mem;
  int x, y;
  gint rowstride;
  unsigned char rgb8[][4]={{  0,  0,  0,  0},
			   {255,  0,  0,  0},
			   {  0,255,  0,  0},
			   {255,255,  0,  0},
			   {  0,  0,255,  0},
			   {255,  0,255,  0},
			   {  0,255,255,  0},
			   {255,255,255,  0}};

  if ((!vbi) || (!width) || (!height) || (!pg))
    return NULL;

  mem = zvbi_render_page(pg, width, height);

  if (!mem)
    return NULL;

  extra_mem = malloc((*width)*(*height)*4);

  if (!extra_mem)
    {
      free(mem);
      return NULL;
    }

  rowstride = *width;

  for (x=0;x<8;x++)
    rgb8[x][3] = alpha[x];

  for (y=0;y<*height;y++)
    for (x=0; x<*width;x++)
      ((gint32*)extra_mem)[y*rowstride+x] =
	((gint32*)rgb8)[mem[y*rowstride+x]];

  free(mem);
  return extra_mem;
}

/*
  This is for monitoring the state of pages (to know if they have
  been updated in the cache). subpage can be ANY_SUB, in that case we
  monitor just the page, not the page.subpage.
*/
struct monitor_page {
  gint page, subpage;
  gboolean dirty; /* changed since last zvbi_clean_page() */
  char * mem; /* rgba rendered page */
  pthread_mutex_t mutex; /* Mutex for protecting the mem */
  struct fmt_page pg; /* The formatted page */
  gint width, height; /* Width and height of the rendered image */
  time_t last_change; /* Time this page got dirty, as returned by
			 time() */
};

pthread_mutex_t monitor_mutex = PTHREAD_MUTEX_INITIALIZER;
static GList *monitor=NULL; /* List of all the pages marked to monitor */

/*
  Tells the VBI engine to monitor a page. This way you can know if the
  page has been updated since the last format_page, get_page.
*/
void
zvbi_monitor_page(gint page, gint subpage)
{
  struct monitor_page * mp;

  mp = malloc(sizeof(struct monitor_page));
  if (!mp)
    return;

  mp->page = page;
  mp->subpage = subpage;
  mp->dirty = TRUE;
  mp->last_change = time(NULL);
  mp->mem = NULL;

  pthread_mutex_init(&(mp->mutex), NULL);

  pthread_mutex_lock(&(monitor_mutex));
  monitor = g_list_append(monitor, mp);
  pthread_mutex_unlock(&(monitor_mutex));
}

/*
  Tells the VBI engine to stop monitoring this page
*/
void
zvbi_forget_page(gint page, gint subpage)
{
  struct monitor_page *mp;
  GList *p;

  pthread_mutex_lock(&(monitor_mutex));
  p = g_list_first(monitor);
  while (p)
    {
      mp = (struct monitor_page*)p->data;
      if ((mp->page == page) && (mp->subpage == subpage))
	break;
      p=p->next;
    }

  if (!p)
    {
      pthread_mutex_unlock(&(monitor_mutex));
      return; /* not found, but fail gracefuly */
    }

  g_list_remove(monitor, mp);
  pthread_mutex_destroy(&(mp->mutex));
  if (mp->mem)
    free(mp->mem);
  free(mp);
  pthread_mutex_unlock(&(monitor_mutex));
}

/*
  Asks whether the page has been updated since the last format_page,
  get_page.
  TRUE means that the page has been updated.
*/
gboolean
zvbi_get_page_state(gint page, gint subpage)
{
  struct monitor_page *mp;
  gboolean result;
  GList *p;

  pthread_mutex_lock(&(monitor_mutex));
  p = g_list_first(monitor);
  while (p)
    {
      mp = (struct monitor_page*)p->data;
      if ((mp->page == page) && (mp->subpage == subpage))
	break;
      p=p->next;
    }

  if (!p)
    {
      pthread_mutex_unlock(&(monitor_mutex));
      return FALSE; /* not found, but fail gracefuly */
    }

  result = mp->dirty;

  pthread_mutex_unlock(&(monitor_mutex));

  return result;
}

/*
  Sets the state of the given page.
  TRUE means set it to dirty.
  If subpage is set to ANY_SUB, then all subpages of page being
  monitored are set.
  last_change is ignored if dirty == FALSE
*/
void
zvbi_set_page_state(gint page, gint subpage, gboolean dirty, time_t
		    last_change)
{
  struct monitor_page *mp;
  GList *p;

  pthread_mutex_lock(&(monitor_mutex));
  p = g_list_first(monitor);
  while (p)
    {
      mp = (struct monitor_page*)p->data;
      if ((mp->page == page) && ((mp->subpage == ANY_SUB) ||
				 (subpage == ANY_SUB) ||
				 (mp->subpage == subpage)))
	{
	  mp->dirty = dirty;
	  if (dirty)
	    mp->last_change = last_change;
	}
      p=p->next;
    }
  pthread_mutex_unlock(&(monitor_mutex));
}

/* renders the formatted and mem versions of the page if it's being
   monitored */
void
zvbi_render_monitored_page(struct vt_page * vtp)
{
  struct monitor_page *mp;
  GList *p;
  unsigned char alphas[8] = {255, 255, 255, 255, 255, 255, 255, 255};
  gint page = vtp->pgno, subpage = vtp->subno;

  pthread_mutex_lock(&(monitor_mutex));
  p = g_list_first(monitor);
  while (p)
    {
      mp = (struct monitor_page*)p->data;
      if ((mp->page == page) && ((mp->subpage == ANY_SUB) ||
				 (subpage == ANY_SUB) ||
				 (mp->subpage == subpage)))
	{
	  pthread_mutex_lock(&(mp->mutex));
	  fmt_page(FALSE, &(mp->pg), vtp);
	  if (mp->mem)
	    free(mp->mem);
	  mp->mem = zvbi_render_page_rgba(&(mp->pg), &(mp->width),
				      &(mp->height), alphas);
	  pthread_mutex_unlock(&(mp->mutex));
	}
      p=p->next;
    }
  pthread_mutex_unlock(&(monitor_mutex));
}

#ifdef HAVE_GDKPIXBUF
/* the current unscaled teletext page */
static GdkPixbuf * teletext_page = NULL;
/* The scaled version of the above */
static GdkPixbuf * scaled_teletext_page = NULL;
/* frees the pixbuf memory */
static void zvbi_free_pixbuf_mem(guchar *pixels, gpointer data)
{
  free(pixels);
}

/* Rebuilds teletext page and scaled_teletext_page if neccesary,
   returns TRUE if scaled_teletext_page was modified */
static gboolean
zvbi_update_pixbufs(gint page, gint subpage, gint w, gint h)
{
  struct monitor_page *mp;
  GList *p;
  gboolean do_update = FALSE;
  gchar * filename;

  pthread_mutex_lock(&(monitor_mutex));
  p = g_list_first(monitor);
  while (p)
    {
      mp = (struct monitor_page*)p->data;
      if ((mp->page == page) && ((mp->subpage == ANY_SUB) ||
				 (subpage == ANY_SUB) ||
				 (mp->subpage == subpage)))
	break; /* found */
      p=p->next;
    }
  if (!p)
    {
      pthread_mutex_unlock(&(monitor_mutex));
      return FALSE;
    }
  
  pthread_mutex_lock(&(mp->mutex));
  
  if (((mp->dirty) || (!teletext_page)) && (mp->mem))
    {
      if (teletext_page)
	gdk_pixbuf_unref(teletext_page);

      teletext_page = 
	gdk_pixbuf_new_from_data(mp->mem, GDK_COLORSPACE_RGB, TRUE, 8,
				 mp->width, mp->height, (mp->width)*4,
				 zvbi_free_pixbuf_mem, NULL);
      /*
	if the pixbuf is being drawn (in the main thread) and we free
	it from the zvbi thread, a SIGSEGV occurs. This fixes it.
      */
      mp->mem = NULL;
      mp->dirty = FALSE;
      do_update = TRUE;
    }

  pthread_mutex_unlock(&(mp->mutex));
  pthread_mutex_unlock(&(monitor_mutex));

  if (!teletext_page)
    {
      /* try to load something, so the program doesn't look like it's
	 freezed */
      srand(time(NULL));
      filename =
	g_strdup_printf("%s/%s%d.jpeg", PACKAGE_DATA_DIR,
			"../pixmaps/zapping/vt_loading",
			(rand()%2)+1);
      teletext_page = gdk_pixbuf_new_from_file(filename);
      g_free(filename);
      if (!teletext_page)
	return FALSE;
    }

  if ((!scaled_teletext_page) || (do_update) ||
      (gdk_pixbuf_get_width(scaled_teletext_page) != w) ||
      (gdk_pixbuf_get_height(scaled_teletext_page) != h))
    {
      if (scaled_teletext_page)
	gdk_pixbuf_unref(scaled_teletext_page);

      scaled_teletext_page =
	gdk_pixbuf_scale_simple(teletext_page,
				w, h, GDK_INTERP_BILINEAR);

      return TRUE;
    }

  return FALSE;
}
#endif /* HAVE_GDKPIXBUF */

/* Called when the tv screen changes size */
void
zvbi_window_updated(GtkWidget * widget)
{
#ifdef HAVE_GDKPIXBUF
  gint width, height;
#endif

  if (!vbi_mode)
    return;

#ifdef HAVE_GDKPIXBUF
  if (!widget->window)
    return;

  gdk_window_get_size(widget->window, &width, &height);
  zvbi_update_pixbufs(cur_page, cur_subpage, width, height);

  /* Draw the TXT */
  zvbi_exposed(widget, 0, 0, width, height);
#endif
}

/* called when the tv_screen receives an expose event */
void
zvbi_exposed(GtkWidget * widget, gint x, gint y, gint w, gint h)
{
#ifdef HAVE_GDKPIXBUF
  if ((!scaled_teletext_page) || (!vbi_mode))
    return;

  /* Some clipping, the pixbuf might not be the same size as the window */
  if (x>gdk_pixbuf_get_width(scaled_teletext_page))
    x = gdk_pixbuf_get_width(scaled_teletext_page);

  if (y>gdk_pixbuf_get_height(scaled_teletext_page))
    y = gdk_pixbuf_get_height(scaled_teletext_page);

  if ((x+w) > gdk_pixbuf_get_width(scaled_teletext_page))
    w = gdk_pixbuf_get_width(scaled_teletext_page)-x;

  if ((y+h) > gdk_pixbuf_get_height(scaled_teletext_page))
    h = gdk_pixbuf_get_height(scaled_teletext_page)-y;

  gdk_pixbuf_render_to_drawable(scaled_teletext_page,
				widget->window,
				widget -> style -> white_gc,
				x, y, x, y,
				w, h,
				GDK_RGB_DITHER_NONE, x, y);
#endif
}

/*
  Builds a GdkPixbuf version of the current teletext page, and updates
  it if neccesary.
*/
void zvbi_build_current_teletext_page(GtkWidget *widget)
{
#ifdef HAVE_GDKPIXBUF
  gint w, h;
#endif

  if (!vbi_mode) /* Just do nothing */
    return;

#ifdef HAVE_GDKPIXBUF
  gdk_window_get_size(widget->window, &w, &h);

  if (zvbi_update_pixbufs(cur_page, cur_subpage, w, h))
    zvbi_window_updated(widget);
#endif /* HAVE_GDKPIXBUF */
}

/* Sets the current page/subpage */
void zvbi_set_current_page(gint page, gint subpage)
{
  zvbi_forget_page(cur_page, cur_subpage);

  cur_page = page;
  cur_subpage = subpage;

  zvbi_monitor_page(cur_page, cur_subpage);
  /* In the next zmisc_build_current_page the change will occur */
}

/* Gets the current page/subpage. Any of the pointers can be NULL */
void zvbi_get_current_page(gint* page, gint* subpage)
{
  if (page)
    *page = cur_page;
  if (subpage)
    *subpage = cur_subpage;
}

/*
  Sets VBI mode. TRUE (on) makes the VBI engine start drawing on the
  given window when required.
*/
void zvbi_set_mode(gboolean on)
{
  g_assert((on == TRUE) || (on == FALSE));

  if ((vbi_mode == on) || (!vbi))
    return; /* Nothing to do */

  vbi_mode = on;
}

/*
  Gets the current VBI mode. TRUE means on
*/
gboolean zvbi_get_mode(void)
{
  return vbi_mode;
}
