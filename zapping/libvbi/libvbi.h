/*
 * (C) 2001 I�aki Garc�a Etxebarria
 * Just a convenience wrapper
 */
#ifndef __LIBVBI_H__
#define __LIBVBI_H__

#include <vt.h>
#include <misc.h>
#include <vbi.h>
//#include <lang.h>
#include <../common/ucs-2.h>
#include <dllist.h>
#include <export.h>

extern void		vbi_delete_search(void *p);
extern void *		vbi_new_search(struct cache *ca, int pgno, int subno,
				ucs2_t *pattern, int casefold,
				int (* progress)(struct fmt_page *pg));
extern int		vbi_next_search(void *p, struct fmt_page **pg);

#endif
