/*
 * (C) 2001 Iñaki García Etxebarria
 * Just a convenience wrapper
 */
#ifndef __LIBVBI_H__
#define __LIBVBI_H__

#include <vt.h>
#include <misc.h>
#include <vbi.h>
//#include <lang.h>
#include <dllist.h>
#include <export.h>
#include "../common/ucs-2.h"

/*
 *  Navigation
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
 *  Search
 */

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
