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
#include <libvbi.h>
#include <pthread.h>
#include <ctype.h>

#include "tveng.h"
#include "zconf.h"
/* Manages config values for zconf (it saves me some typing) */
#define ZCONF_DOMAIN "/zapping/internal/zvbi/"
#include "zmisc.h"
#include "zvbi.h"

static struct vbi *vbi=NULL; /* holds the vbi object */

static pthread_t zvbi_thread_id; /* Just a dummy thread to select() */
static gboolean exit_thread = FALSE; /* just an atomic flag to tell the
					thread to exit*/

/* handler for a vbi event */
static void event(struct dl_head *reqs, struct vt_event *ev);

/* thread function (just a loop that selects on the VBI device) */
static void * zvbi_thread(void * unused);

/* Some info about the last processed header, protected by a mutex */
static struct {
  pthread_mutex_t mutex;

  /* Generic info */
  char xpacket[32];
  char header[64];
  char *name; /* usually, something like station-Teletext or similar
		 (Antena3-Teletexto, for example) */

  /* Pre-processed info from the header, for convenience */
  int hour, min, sec;
} last_info;

/* Open the given VBI device, FALSE on error */
gboolean
zvbi_open_device(gchar * device, gint finetune, gboolean erc)
{
  g_assert(device != NULL);

  if (vbi)
    return FALSE; /* this code isn't reentrant */

  fdset_init(fds);

  /* fixme: make this real code */
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

  if (pthread_create(&zvbi_thread_id, NULL, zvbi_thread, NULL))
    {
      vbi_del_handler(vbi, event, NULL);
      vbi_close(vbi);
      vbi = NULL;
      return FALSE;
    }

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
	//	vtp->subno);
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
	    last_info.name = name;
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

  if (!vbi)
    return NULL;

  pthread_mutex_lock(&(last_info.mutex));
  if (last_info.name)
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
    fdset_select(fds, 100); /* 0.1s timeout */

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
  if ((!vbi) || (!vbi->cache))
    return NULL;

  return vbi->cache->op->get(vbi->cache, page, subpage);
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
