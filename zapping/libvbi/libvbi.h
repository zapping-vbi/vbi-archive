/*
 *  Zapzilla/libvbi public interface
 *
 *  Copyright (C) 2001 Iñaki García Etxebarria
 *  Copyright (C) 2001 Michael H. Schimek
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

/* $Id: libvbi.h,v 1.25 2001-03-09 17:39:01 mschimek Exp $ */

#ifndef __LIBVBI_H__
#define __LIBVBI_H__

#include <stdint.h>

//#include <vt.h>
//#include <lang.h>
//#include "dllist.h"
#include "export.h"

/*
    public interface:
	libvbi.h: bcd stuff, struct vbi, vbi_*
	format.h: attr_*, struct fmt_page
	export.h: 2do
 */

/*
 *  BCD arithmetic for page numbers
 */

static inline unsigned int
dec2bcd(unsigned int dec)
{
	return (dec % 10) + ((dec / 10) % 10) * 16 + ((dec / 100) % 10) * 256;
}

static inline unsigned int
bcd2dec(unsigned int bcd)
{
	return (bcd & 15) + ((bcd >> 4) & 15) * 10 + ((bcd >> 8) & 15) * 100;
}

static inline unsigned int
add_bcd(unsigned int a, unsigned int b)
{
	unsigned int t;

	a += 0x06666666;
	t  = a + b;
	b ^= a ^ t;
	b  = (~b & 0x11111110) >> 3;
	b |= b * 2;

	return (t - b) & 0xFFF;
}

static inline int
is_bcd(unsigned int bcd)
{
	static const unsigned int x = 0x06666666;

	return (((bcd + x) ^ (bcd ^ x)) & 0x110) == 0;
}

/*
 *  Teletext (teletext.c, packet.c)
 */

struct vbi; /* opaque type */

extern int		vbi_fetch_vt_page(struct vbi *vbi, struct fmt_page *pg,
				          int pgno, int subno, int display_rows, int navigation);
extern void		vbi_set_default_region(struct vbi *vbi, int default_region);
extern void		vbi_set_teletext_level(struct vbi *vbi, int level);

/*
 *  Closed Caption (caption.c)
 */

extern int		vbi_fetch_cc_page(struct vbi *vbi, struct fmt_page *pg, int pgno);

/*
 *  Navigation (teletext.c)
 */

typedef enum {
	VBI_LINK_NONE = 0,
	VBI_LINK_PAGE,
	VBI_LINK_SUBPAGE,
	VBI_LINK_HTTP,
	VBI_LINK_FTP,
	VBI_LINK_EMAIL,
} vbi_link_type;

typedef struct {
	vbi_link_type		type;
	int			pgno;
	int			subno;
	unsigned char		text[42];
} vbi_link;

extern void		vbi_resolve_link(struct fmt_page *pg, int column, int row, vbi_link *ld);
extern void		vbi_resolve_home(struct fmt_page *pg, vbi_link *ld);

extern int		vbi_page_title(struct vbi *vbi, int pgno, int subno, char *buf);

/*
 *  Teletext search (search.c)
 */

#include "../common/ucs-2.h"

extern void		vbi_delete_search(void *p);
extern void *		vbi_new_search(struct vbi *vbi, int pgno, int subno,
				ucs2_t *pattern, int casefold, int (* progress)(struct fmt_page *pg));
extern int		vbi_next_search(void *p, struct fmt_page **pg, int dir);

/*
 *  Rendering (exp_gfx.c)
 */

extern void		vbi_draw_vt_page_region(struct fmt_page *pg, uint32_t *canvas,
				int column, int row, int width, int height, unsigned int rowstride,
				int reveal, int flash_on);
extern void		vbi_draw_cc_page_region(struct fmt_page *pg, uint32_t *canvas,
				int column, int row, int width, int height, unsigned int rowstride);

#define vbi_draw_vt_page(pg, canvas, reveal, flash_on) \
	vbi_draw_vt_page_region(pg, canvas, 0, 0, 40, 25, -1, reveal, flash_on)

/*
 *  Network identification.
 *
 *  All strings are ISO 8859-1, local language, and NUL terminated.
 *  Prepare for empty strings.
 */

typedef struct {
	char			name[33];		/* descriptive name */
	char			label[9];		/* short name (TTX/VPS) */
	char			call[33];		/* network call letters (XDS) */

	int			tape_delay;		/* tape delay, minutes (XDS) */

	/* Private */

	void *			unique;
	int			cni_vps;
	int			cni_8301;
	int			cni_8302;
	int			cni_x26;

	int			cycle;
} vbi_network;

/*
 *  Event (vbi.c)
 */

#define VBI_EVENT_NONE		0
#define	VBI_EVENT_CLOSE		(1 << 0)
#define	VBI_EVENT_PAGE		(1 << 1)	// p1:vt_page	i1:query-flag
#define VBI_EVENT_CAPTION	(1 << 2)
#define	VBI_EVENT_HEADER	(1 << 3)	// i1:pgno  i2:subno  i3:flags  p1:data
#define	VBI_EVENT_NETWORK	(1 << 4)
/*
 *  Some station/network identifier has been received, vbi_event.p1 is
 *  a vbi_network pointer. The event will not repeat*) unless a different
 *  identifier has been received and confirmed.
 *
 *  Minimum time for recognition
 *
 *  VPS (DE/AT/CH only):	0.08 s
 *  Teletext PDC, 8/30:		2 s
 *  Teletext X/26:		unknown
 *  XDS (US only):		unknown, between 0.1x to 10x seconds
 *
 *  *) VPS/TTX and XDS will not combine in real life, sample insertion
 *     can confuse the logic.
 */

#define VBI_EVENT_IO_ERROR	(1 << 5)
/*
 *  A fatal I/O error occured, as reported by the vbi fifo producer.
 *  On return from the event handler the vbi mainloop will terminate
 *  and must be restarted after the problem has been solved.
 *
 *  (XXX this should include an error code and message) 
 */

#define	VBI_EVENT_XPACKET	(1 << 6)	// i1:mag  i2:pkt  i3:errors  p1:data
#define	VBI_EVENT_RESET		(1 << 7)	// ./.
#define	VBI_EVENT_TIMER		(1 << 8)	// ./.

typedef struct {
	int			type;
	int			pgno;
	int			subno;

    void *p1;
} vbi_event;

extern int		vbi_event_handler(struct vbi *vbi, int event_mask, void (* handler)(vbi_event *, void *), void *user_data); 


struct cache;
/* given_fd points to an opened video device, or -1, ignored for V4L2 */
struct vbi *vbi_open(char *vbi_dev_name, struct cache *ca, int given_fd);
void vbi_close(struct vbi *vbi);
extern void *	vbi_mainloop(void *p);

#include "vbi.h" /* XXX */

#endif /* __LIBVBI_H__ */
