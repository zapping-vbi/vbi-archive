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
#include "callbacks.h"
#include "zvbi.h"
#include "interface.h"

/* fixme: The history_stack should keep track of subpages too */

static struct vbi *vbi=NULL; /* holds the vbi object */
static GtkWidget* txtcontrols=NULL; /* GUI controller for the TXT */
static GList *history=NULL; /* visited pages, no duplicates, sorted by
			     time of visit (most recent first) */
static GList *history_stack=NULL; /* visited pages in order of time (oldest
				     first), duplicates allowed */
static gint history_stack_size=0; /* Number of items in history_stack */
static gint history_sp=0; /* Pointer in the stack */
static pthread_t zvbi_thread_id; /* Just a dummy thread to select() */
static gboolean exit_thread = FALSE; /* just an atomic flag to tell the
					thread to exit*/

static gboolean vbi_mode=FALSE; /* VBI mode, default off */

/* Go to the index by default */
static gint cur_page=0x100, cur_subpage=ANY_SUB;

/* handler for a vbi event */
static void event(struct dl_head *reqs, struct vt_event *ev);

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

#ifdef HAVE_GDKPIXBUF
/* the current unscaled teletext page */
static GdkPixbuf * teletext_page = NULL;
/* The scaled version of the above */
static GdkPixbuf * scaled_teletext_page = NULL;
#endif

/* Open the configured VBI device, FALSE on error */
gboolean
zvbi_open_device(gint newbttv)
{
  gint finetune;
  gboolean erc;
  gchar *device;

  zcc_bool(FALSE, "Enable VBI decoding", "enable_vbi");
  zcc_bool(TRUE, "Use VBI for getting station names", "use_vbi");
  zcc_char("/dev/vbi", "VBI device", "vbi_device");
  zcc_bool(TRUE, "Error correction", "erc");
  zcc_int(999, "Finetune range", "finetune");
  zcc_int(0, "Glyphs used to render TTX", "glyphs");
  srand(time(NULL));

  if ((vbi) || (!zcg_bool(NULL, "enable_vbi")))
    return FALSE; /* this code isn't reentrant */

  device = zcg_char(NULL, "vbi_device");
  finetune = zcg_int(NULL, "finetune");
  erc = zcg_bool(NULL, "erc");
  vbi_set_glyphs(zcg_int(NULL, "glyphs"));

/* fifo - OBSOLETE */
/*  fdset_init(fds);*/

  if (!(vbi = vbi_open(device, cache_open(), finetune, newbttv)))
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
  vbi_mode = FALSE;

  pthread_join(zvbi_thread_id, NULL);
  pthread_mutex_destroy(&(last_info.mutex));
  pthread_cond_destroy(&(last_info.xpacket_cond));

  vbi_del_handler(vbi, event, NULL);
  vbi_close(vbi);
  vbi = NULL;

  zcs_int(vbi_get_glyphs(), "glyphs");
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
    case EV_PAGE:
	vtp = ev->p1;
	printv("vtx page %x.%02x  \r", vtp->pgno,
	       vtp->subno);
	/* Set the dirty flag on the page */
	zvbi_set_page_state(vtp->pgno, vtp->subno, TRUE, time(NULL));
	/* Now render the mem version of the page */
	if (vbi_mode)
	  zvbi_render_monitored_page(vtp);
	break;
    case EV_XPACKET:
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

#undef TRUE
#undef FALSE // duh.
#include "../common/fifo.h"

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
  /* obsolete, fmt_page provides a colour map */
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

  for (x=0;x<32;x++) {
    rgb8[x & 7][3] = alpha[x & 7];
    pg->colour_map[x] |= alpha[x & 7] << 24;
  }
  
  for (y=0;y<*height;y++)
    for (x=0; x<*width;x++)
      ((gint32*)extra_mem)[y*rowstride+x] =
//	((gint32*)rgb8)[mem[y*rowstride+x]];
	pg->colour_map[mem[y*rowstride+x]]; /* 0x00BBGGRR */

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
  struct vt_page vtp; /* unformatted page */
  struct fmt_page pg; /* The formatted page */
  gint width, height; /* Width and height of the rendered image */
  time_t last_change; /* Time this page got dirty, as returned by
			 time() */
};

pthread_mutex_t monitor_mutex = PTHREAD_MUTEX_INITIALIZER;
static GList *monitor=NULL; /* List of all the pages marked to monitor */

/*
  FLOF (fast text) navigation. Given a x, tells which page/subpage it
  corresponds to. It returns a code of -1 in page if it couldn't be
  resolved, and FALSE.
  Fast text navigation is the "color" navigation, emitted in the
  bottom line of the teletext page.
*/
static gboolean
zvbi_resolve_flof(gint x, struct vt_page *vtp, gint *page, gint *subpage)
{
  gint code= 7, i, c;
  if ((!vbi) || (!vtp) || (!page) || (!subpage))
    return FALSE;

  if (!(vtp->flof))
    return FALSE;

  for (i=0; (i <= x) && (i<40); i++)
    if ((c = vtp->data[24][i]) < 8) /* color code */
      code = c; /* Store it for later on */

  if (code >= 8) /* not found ... weird */
    return FALSE;

  code = " \0\1\2\3 \3 "[code]; /* color->link conversion table */
  
  if ((code > 6) || ((vtp->link[code].pgno & 0xff) == 0xff))
    return FALSE;

  *page = vtp->link[code].pgno;
  *subpage = vtp->link[code].subno;

  return TRUE;
}

#define notdigit(x) (!isdigit(x))

static gboolean
zvbi_check_subpage(gchar *p, int x, gint *n1, gint *n2)
{
    p += x;

    if (x >= 0 && x < 42-5)
	if (notdigit(p[1]) || notdigit(p[0]))
	    if (isdigit(p[2]))
		if (p[3] == '/' || p[3] == ':')
		    if (isdigit(p[4]))
			if (notdigit(p[5]) || notdigit(p[6]))
			{
			    *n1 = p[2] % 16;
			    if (isdigit(p[1]))
				*n1 += p[1] % 16 * 16;
			    *n2 = p[4] % 16;
			    if (isdigit(p[5]))
				*n2 = *n2 * 16 + p[5] % 16;
			    if ((*n2 > 0x99) || (*n1 > 0x99) ||
				(*n1 > *n2))
			      return FALSE;
			    return TRUE;
			}
    return FALSE;
}

static gboolean
zvbi_check_page(gchar *p, int x, gint *pgno, gint *subno)
{
    p += x;

    if (x >= 0 && x < 42-4)
      if (notdigit(p[0]) && notdigit(p[4]))
	if (isdigit(p[1]))
	  if (isdigit(p[2]))
	    if (isdigit(p[3]))
	      {
		*pgno = p[1] % 16 * 256 + p[2] % 16 * 16 + p[3] % 16;
		*subno = ANY_SUB;
		if (*pgno >= 0x100 && *pgno <= 0x899)
		  return TRUE;
	      }
    return FALSE;
}

/*
  Text navigation.
  Given the page, the x and y, tries to find a page number in that
  position. In success, returns TRUE
*/
static gint
zvbi_resolve_page(gint x, gint y, struct vt_page * vtp, gint *page,
		  gint *subpage)
{
  struct fmt_page pg;
  gint i, n1, n2;
  gchar buffer[42]; /* The line and two spaces on the sides */

  if ((y > 24) || (y < 0) || (x < 0) || (x > 39) || (!vbi) || (!vtp)
      || (!page) || (!subpage))
    return FALSE;

  fmt_page(FALSE, &pg, vtp); /* It's easier to parse this way */
// {mhs}
//  if (pg.hid & (1 << y))
//    y--;
// XXX new .ch:  123  123  112233  112233
//                    123          112233
// propose a new link approach:
//  * at render time, scan fmt_page for page numbers and create
//    a link list: row, start_col, end_col, page_number, channel
//    including FLOF links.
//  * on left button, lookup link and jump to page
//  * on right button, ... page in new window
//  * on mouse move, lookup and change pointer image when over link

  if (y < 0)
    return FALSE;

  buffer[0] = buffer[41] = ' ';

  for (i=1; i<41; i++)
    buffer[i] = pg.data[y][i-1].ch;

  for (i = -2; i < 1; i++)
    if (zvbi_check_page(buffer, x+i, page, subpage))
      return TRUE;

  /* try to resolve subpage */
  for (i = -4; i < 1; i++)
    if (zvbi_check_subpage(buffer, x+i, &n1, &n2))
      {
	if ((cur_subpage != n1) && (cur_subpage != ANY_SUB))
	  return FALSE; /* mismatch */
	if (cur_subpage != ANY_SUB)
	  n1 = cur_subpage;
	n1 = dec2hex(hex2dec(n1)+1);
	if (n1 > n2)
	  n1 = 1;
	*page = cur_page;
	*subpage = n1;
	return TRUE;
      }

  return FALSE;
}

/*
  Tells the VBI engine to monitor a page. This way you can know if the
  page has been updated since the last format_page, get_page.
*/
void
zvbi_monitor_page(gint page, gint subpage)
{
  struct monitor_page * mp;
  struct vt_page *vtp;

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
  vtp = zvbi_get_page(page, subpage);
  if (vtp)
    {
      /* Set the dirty flag on the page */
      zvbi_set_page_state(vtp->pgno, vtp->subno, TRUE, time(NULL));
      /* Now render the mem version of the page */
      zvbi_render_monitored_page(vtp);
    }
  /* set the loading ... page if the requested page cannot be found */
#ifdef HAVE_GDKPIXBUF
  if (!vtp)
    {
      if (teletext_page)
	gdk_pixbuf_unref(teletext_page);
      teletext_page = NULL;
    }
#endif
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

  monitor = g_list_remove(monitor, mp);
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
	  memcpy(&(mp->vtp), vtp, sizeof(struct vt_page));
	  fmt_page(FALSE, &(mp->pg), &(mp->vtp));
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
      filename =
	g_strdup_printf("%s/%s%d.jpeg", PACKAGE_DATA_DIR,
			"../pixmaps/zapping/vt_loading",
			(rand()%2)+1);
      teletext_page = gdk_pixbuf_new_from_file(filename);
      g_free(filename);
      do_update = TRUE;
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

static void
zvbi_do_redraw(GtkWidget * widget, gint x, gint y, gint w, gint h)
{
#ifdef HAVE_GDKPIXBUF
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

/* called when the tv_screen receives an expose event */
static void
on_zvbi_expose_event		(GtkWidget	*widget,
				 GdkEvent	*event,
				 gpointer	user_data)
{
  if ((!vbi) || (!vbi_mode) || (event->type != GDK_EXPOSE))
    return;

#ifdef HAVE_GDKPIXBUF
  if (!scaled_teletext_page)
    return;
#endif

  zvbi_do_redraw(widget, event->expose.area.x, event->expose.area.y,
		 event->expose.area.width, event->expose.area.height);
}

static void
on_zvbi_size_allocate			(GtkWidget	*widget,
					 GtkAllocation	*allocation,
					 gpointer	user_data)
{
  if ((!vbi_mode) || (!vbi))
    return;

#ifdef HAVE_GDKPIXBUF
  if (!widget->window)
    return;

  zvbi_update_pixbufs(cur_page, cur_subpage, allocation->width,
		      allocation->height);

  /* Draw the TXT */
  zvbi_do_redraw(widget, 0, 0, allocation->width, allocation->height);
#endif  
}

static gboolean
on_zvbi_button_press_event		(GtkWidget	*widget,
					 GdkEvent	*event,
					 gpointer	user_data)
{
  GdkEventButton * bevent = (GdkEventButton *)event;

  struct vt_page vtp; /* This is a local copy of the current monitored
		       page, to avoid locking for a long time */
  gint w=0, h=0;
  gint page, subpage;
  gint x, y; /* coords in the 40*25 buffer */
  gboolean result;

  if ((event->type != GDK_BUTTON_PRESS) ||
      (bevent->button != 1) ||
      (!vbi) || (!vbi_mode) || (!monitor) || (!widget))
    return FALSE;


  pthread_mutex_lock(&(monitor_mutex));
  memcpy(&vtp,
	 &(((struct monitor_page*)g_list_first(monitor)->data)->vtp),
	 sizeof(struct vt_page));
  pthread_mutex_unlock(&(monitor_mutex));

  /* Convert (scaled) image coords to 40*25 coords */
  gdk_window_get_size(widget->window, &w, &h);

  if ((!w) || (!h))
    return FALSE;

  x = (bevent->x*40)/w;
  y = (bevent->y*25)/h;

  if ((x<0) || (x>39) || (y < 0) || (y > 24))
    return FALSE;

  switch (y)
    {
    case 0: /* Header, no useful info there*/
      break;
    case 1 ... 23: /* body of the page */
      result = zvbi_resolve_page(x, y, &vtp, &page, &subpage);
      if (result)
	zvbi_set_current_page(page, subpage);
      break;
    default: /* Bottom line, fast navigation */
      if (!zvbi_resolve_flof(x, &vtp, &page, &subpage))
	break;
      zvbi_set_current_page(page, subpage);
      break;
    }

  return FALSE;
}

/*
  Builds a GdkPixbuf version of the current teletext page, and updates
  it if neccesary.
  If there's a teletext page rendered (even the loading... page),
  returns a pointer to that data, and fills in the struct with data
  about that image.
*/
gpointer zvbi_build_current_teletext_page(GtkWidget *widget, struct
					  tveng_frame_format * format)
{
#ifdef HAVE_GDKPIXBUF
  gint w, h;
  GtkAllocation dummy_alloc;
#endif

  if ((!vbi) || (!vbi_mode) || (!format)) /* Just do nothing */
    return NULL;

#ifdef HAVE_GDKPIXBUF
  gdk_window_get_size(widget->window, &w, &h);

  dummy_alloc.width = w;
  dummy_alloc.height = h;

  if (zvbi_update_pixbufs(cur_page, cur_subpage, w, h))
    on_zvbi_size_allocate(widget, &dummy_alloc, NULL);

  if (!scaled_teletext_page)
    return NULL;

  format->width = gdk_pixbuf_get_width(scaled_teletext_page);
  format->height = gdk_pixbuf_get_height(scaled_teletext_page);
  format->bytesperline =
    gdk_pixbuf_get_rowstride(scaled_teletext_page);
  if (gdk_pixbuf_get_has_alpha(scaled_teletext_page))
    {
      format->depth = 32;
      format->pixformat = TVENG_PIX_RGB32;
      format->bpp = 4;
    }
  else
    {
      format->depth = 24;
      format->pixformat = TVENG_PIX_RGB24;
      format->bpp = 3;
    }
  format->sizeimage = format->bytesperline*format->height;

  return (gdk_pixbuf_get_pixels(scaled_teletext_page));

#else
  return NULL;
#endif /* HAVE_GDKPIXBUF */
}

/* Set the given history buttons to their correct state */
static void history_gui_setup(GtkWidget *prev,
			      GtkWidget *next)
{
      if (history_stack_size > (history_sp+1))
	gtk_widget_set_sensitive(next, TRUE);
      else
	gtk_widget_set_sensitive(next, FALSE);	

      if (history_sp > 0)
	gtk_widget_set_sensitive(prev, TRUE);
      else
	gtk_widget_set_sensitive(prev, FALSE);	
}

/* Sets the current page/subpage, no history keeping */
static void zvbi_set_current_page_pure(gint page, gint subpage)
{
  GtkWidget * widget;
  zvbi_forget_page(cur_page, cur_subpage);

  cur_page = page;
  cur_subpage = subpage;

  /* [GUI] update the spinbuttons so they reflect the new pos */
  if (txtcontrols)
    {
      /* tell the widget to ignore the next value_changed call */
      widget = lookup_widget(txtcontrols, "manual_subpage");
      gtk_object_set_user_data(GTK_OBJECT(widget), (gpointer)0xdeadbeef);
      widget = lookup_widget(txtcontrols, "manual_page");
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), hex2dec(page));
      widget = lookup_widget(txtcontrols, "manual_subpage");
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget),
				(subpage == ANY_SUB) ? -1 : hex2dec(subpage));
      gtk_object_set_user_data(GTK_OBJECT(widget), NULL);
    }

  zvbi_monitor_page(cur_page, cur_subpage);
  /* In the next zmisc_build_current_page the change will occur */
}

/* Sets the current page/subpage */
void zvbi_set_current_page(gint page, gint subpage)
{
  /* history, txtcontrols */
  gchar * clist_entry[1];
  GtkCList *history_clist=NULL;
  GtkWidget *vtx_history_previous=NULL;
  GtkWidget *vtx_history_next=NULL;
  GList *p;
  gint page_code, pc_subpage;

  if (txtcontrols)
    {
      history_clist = GTK_CLIST(lookup_widget(txtcontrols, "history"));
      vtx_history_previous = lookup_widget(txtcontrols,
					   "vtx_history_previous");
      vtx_history_next = lookup_widget(txtcontrols,
				       "vtx_history_next");
    }

  /* Append the current page to the history stack and set the history
     pointer to the last entry */
  pc_subpage = (subpage == ANY_SUB) ? 0xffff : (subpage & 0xffff);
  page_code = (page<<16)+pc_subpage;
  history_stack = g_list_append(history_stack, GINT_TO_POINTER(page_code));
  history_stack_size++;
  history_sp = history_stack_size-1;

  /* [GUI] enable/disable navigation controls, as needed */
  if (txtcontrols)
    history_gui_setup(vtx_history_previous, vtx_history_next);

  /* Add this entry to the time sorted list */
  if (g_list_find(history, GINT_TO_POINTER(page)))
    history = g_list_remove(history, GINT_TO_POINTER(page));

  history = g_list_prepend(history, GINT_TO_POINTER(page));
  /* [GUI] update the view */
  if (txtcontrols)
    {
      gtk_clist_freeze(history_clist);
      gtk_clist_clear(history_clist);
      p = g_list_first(history);
      while (p)
	{
	  clist_entry[0]=g_strdup_printf("%x", GPOINTER_TO_INT(p->data));
	  gtk_clist_append(history_clist, clist_entry);
	  g_free(clist_entry[0]);
	  p = p->next;
	}
      /*
	Set the ignore select_row flag to FALSE
	Rationale: The gtk_clist_select_row will raise a
	<<select_row>> signal, that will call zvbi_set_current_page
	again, hanging the program. This is a bit hackish, but avoids
	the problem.
      */
      gtk_object_set_user_data(GTK_OBJECT(history_clist),
			       GINT_TO_POINTER(TRUE));
      gtk_clist_select_row(history_clist, 0, 0);
      gtk_clist_thaw(history_clist);
    }

  zvbi_set_current_page_pure(page, subpage);
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
  GtkCList * history_clist;
  GtkWidget *vtx_history_previous=NULL;
  GtkWidget *vtx_history_next=NULL;
  GList *p;
  gchar * clist_entry[1];
  GtkSpinButton *spin;
  g_assert((on == TRUE) || (on == FALSE));

  if (!vbi)
    return; /* Nothing to do */

  if (on && (txtcontrols == NULL))
    {
      txtcontrols = create_txtcontrols();
      /* So the delete event can set the pointer to NULL */
      gtk_object_set_user_data(GTK_OBJECT(txtcontrols), &txtcontrols);
      /* Set up the manual page/subpage setting callbacks */
      spin = GTK_SPIN_BUTTON(lookup_widget(txtcontrols, "manual_page"));
      gtk_signal_connect(GTK_OBJECT(spin->adjustment),
			 "value_changed",
			 on_manual_page_value_changed, spin);
      spin = GTK_SPIN_BUTTON(lookup_widget(txtcontrols, "manual_subpage"));
      gtk_signal_connect(GTK_OBJECT(spin->adjustment),
			 "value_changed",
			 on_manual_subpage_value_changed, spin);
      gtk_object_set_user_data(GTK_OBJECT(spin), NULL);
      /* Make the history buttons show the real state */
      vtx_history_previous = lookup_widget(txtcontrols,
					   "vtx_history_previous");
      vtx_history_next = lookup_widget(txtcontrols,
				       "vtx_history_next");
      history_gui_setup(vtx_history_previous, vtx_history_next);
      /* Set the history CList to its correct value */
      history_clist =
	GTK_CLIST(lookup_widget(txtcontrols, "history"));
      p = g_list_first(history);
      while (p)
	{
	  clist_entry[0]=g_strdup_printf("%x", GPOINTER_TO_INT(p->data));
	  gtk_clist_append(history_clist, clist_entry);
	  g_free(clist_entry[0]);
	  p = p->next;
	}
      /* Set the ignore select_row flag to FALSE */
      gtk_object_set_user_data(GTK_OBJECT(history_clist),
			       GINT_TO_POINTER(FALSE));
      gtk_widget_show(txtcontrols);
    }
  else if ((!on) && (txtcontrols))
    {
      GtkWidget *p = txtcontrols;
      on_txtcontrols_delete_event(txtcontrols, NULL, NULL);
      gtk_widget_destroy(p);
    }

  if (on) /* force an update so the newest page appears */
    zvbi_set_current_page_pure(cur_page, cur_subpage);

  vbi_mode = on;
}

/*
  Gets the current VBI mode. TRUE means on
*/
gboolean zvbi_get_mode(void)
{
  return vbi_mode;
}

/*
  Sets the given (visited page) from the visited pages index. This
  should only be called by on_history_select_row (txtcontrols.c)
*/
void zvbi_select_visited_page(gint index)
{
  gint page = GPOINTER_TO_INT(g_list_nth(history, index)->data);

  zvbi_set_current_page(page, ANY_SUB);
}

/*
  Goes to the next page in the history (just like 'Next' in most
  navigators)
*/
void zvbi_history_next(void)
{
  gint page, page_code, pc_subpage;
  GtkWidget *vtx_history_previous, *vtx_history_next;

  if (history_stack_size == (history_sp+1))
    return;
  history_sp++;

  page_code = GPOINTER_TO_INT(g_list_nth(history_stack, history_sp)->data);
  page = page_code >> 16;
  pc_subpage = (page_code & 0xffff);
  pc_subpage = (pc_subpage == 0xffff) ? ANY_SUB : pc_subpage;

  zvbi_set_current_page_pure(page, pc_subpage);

  if (txtcontrols)
    {
      vtx_history_previous = lookup_widget(txtcontrols,
					   "vtx_history_previous");
      vtx_history_next = lookup_widget(txtcontrols,
				       "vtx_history_next");
      history_gui_setup(vtx_history_previous, vtx_history_next);
    }
}

/*
 Goes to the previous page in history
*/
void zvbi_history_previous(void)
{
  gint page, page_code, pc_subpage;
  GtkWidget *vtx_history_previous, *vtx_history_next;

  if (history_sp == 0)
    return;

  history_sp--;

  page_code = GPOINTER_TO_INT(g_list_nth(history_stack, history_sp)->data);
  page = page_code >> 16;
  pc_subpage = (page_code & 0xffff);
  pc_subpage = (pc_subpage == 0xffff) ? ANY_SUB : pc_subpage;

  zvbi_set_current_page_pure(page, pc_subpage);

  if (txtcontrols)
    {
      vtx_history_previous = lookup_widget(txtcontrols,
					   "vtx_history_previous");
      vtx_history_next = lookup_widget(txtcontrols,
				       "vtx_history_next");
      history_gui_setup(vtx_history_previous, vtx_history_next);
    }
}

void
zvbi_set_widget(GtkWidget * widget)
{
  gtk_widget_add_events(widget, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK);

  gtk_signal_connect(GTK_OBJECT(widget), "expose-event",
		     GTK_SIGNAL_FUNC(on_zvbi_expose_event),
		     NULL);

  gtk_signal_connect(GTK_OBJECT(widget), "button-press-event",
		     GTK_SIGNAL_FUNC(on_zvbi_button_press_event),
		     NULL);

  gtk_signal_connect(GTK_OBJECT(widget), "size-allocate",
		     GTK_SIGNAL_FUNC(on_zvbi_size_allocate),
		     NULL);
}
