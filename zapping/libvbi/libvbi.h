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
} vbi_link_descr;

extern void		vbi_resolve_link(struct fmt_page *pg, int column, int row, vbi_link_descr *ld);
extern void		vbi_resolve_home(struct fmt_page *pg, vbi_link_descr *ld);

extern int		vbi_page_title(struct vbi *vbi, int pgno, int subno, char *buf);

/*
 *  Search
 */

extern void		vbi_delete_search(void *p);
extern void *		vbi_new_search(struct vbi *vbi, int pgno, int subno,
				ucs2_t *pattern, int casefold, int (* progress)(struct fmt_page *pg));
extern int		vbi_next_search(void *p, struct fmt_page **pg, int dir);

#endif /* __LIBVBI_H__ */
