/*
 * (C) 2001 I�aki Garc�a Etxebarria
 * Just a convenience wrapper
 */
#ifndef __LIBVBI_H__
#define __LIBVBI_H__

#include <vt.h>
#include <vbi.h>	/* XXX */
//#include <lang.h>
#include <dllist.h>
#include <export.h>

/*
 *  BCD arithmetic for page numbers
 */

static inline unsigned int
dec2bcd(unsigned int dec)
{
	return (dec % 10) + ((dec / 10) % 10) * 16 + (dec / 100) * 256;
}

static inline unsigned int
bcd2dec(unsigned int bcd)
{
	return (bcd & 15) + ((bcd >> 4) & 15) * 10 + (bcd >> 8) * 100;
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
	const unsigned int x = 0x06666666;

	return (((bcd + x) ^ (bcd ^ x)) & 0x110) == 0;
}

/*
 *  Teletext (teletext.c, packet.c)
 */

struct vbi; /* opaque type */

extern int		vbi_fetch_page(struct vbi *vbi, struct fmt_page *pg,
				       int pgno, int subno, int display_rows, int navigation);
extern void		vbi_set_default_region(struct vbi *vbi, int default_region);

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
 *  Event
 */

typedef enum {
	VBI_EVENT_NONE = 0,
	VBI_EVENT_CLOSE,
	VBI_EVENT_PAGE,		// p1:vt_page	i1:query-flag
	VBI_EVENT_HEADER,	// i1:pgno  i2:subno  i3:flags  p1:data
	VBI_EVENT_XPACKET,	// i1:mag  i2:pkt  i3:errors  p1:data
	VBI_EVENT_RESET,	// ./.
	VBI_EVENT_TIMER,	// ./.
} vbi_event_type;

typedef struct
{
	vbi_event_type		type;
	int			pgno;
	int			subno;

    /* old cruft */
    void *resource;	/* struct xio_win *, struct vbi *, ... */
    int i1, i2, i3, i4;
    void *p1;
} vbi_event;

struct vbi_client
{
    struct dl_node node[1];
    void (*handler)(void *data, vbi_event *ev);
    void *data;
};

#endif /* __LIBVBI_H__ */
