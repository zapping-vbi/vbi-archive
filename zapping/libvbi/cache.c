#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "vbi.h"

/*
  AleVT:
    There are some subtleties in this cache.

    - Simple hash is used.
    - All subpages of a page are in the same hash chain.
    - The newest subpage is at the front.


    Hmm... maybe a tree would be better...
*/

//static struct cache_ops cops;

typedef struct {
	node			node;		/* hash chain */

	nuid			nuid;		/* network sending this page */
	int			priority;	/* cache purge priority */

	struct vt_page		page;

	/* dynamic size, no fields below */
} cache_page;


static inline int
hash(int pgno)
{
    // very simple...
    return pgno % HASH_SIZE;
}

void
vbi_cache_flush(struct vbi *vbi)
{
	struct cache *ca = vbi->cache;
	cache_page *cp;
	int h;

	for (h = 0; h < HASH_SIZE; h++)
		while ((cp = PARENT(rem_head(ca->hash + h), cache_page, node))) {
			free(cp);
		}

	memset(ca->hi_subno, 0, sizeof(ca->hi_subno));
}

void
vbi_cache_destroy(struct vbi *vbi)
{
	struct cache *ca = vbi->cache;

	vbi_cache_flush(vbi);

	free(ca);
}

/*
    Get a page from the cache.
    If subno is SUB_ANY, the newest subpage of that page is returned
*/

struct vt_page *
vbi_cache_get(struct vbi *vbi, int pgno, int subno, int subno_mask)
{
	struct cache *ca = vbi->cache;
	cache_page *cp;
	int h = hash(pgno);

	if (subno == ANY_SUB) {
		subno = 0;
		subno_mask = 0;
	}

	for_all_nodes (cp, ca->hash + h, node)
		if (cp->page.pgno == pgno
		    && (cp->page.subno & subno_mask) == subno) {
			/* found, move to front (make it 'new') */
			add_head(ca->hash + h, unlink_node(ca->hash + h, &cp->node));
			return &cp->page;
		}

	return NULL;
}

/* public */ int
vbi_is_cached(struct vbi *vbi, int pgno, int subno)
{
	return NULL != vbi_cache_get(vbi, pgno, subno, -1);
}

/*
    Put a page in the cache.
    If it's already there, it is updated.
*/

struct vt_page *
vbi_cache_put(struct vbi *vbi, struct vt_page *vtp)
{
	struct cache *ca = vbi->cache;
	cache_page *cp;
	int h = hash(vtp->pgno);
	int size = vtp_size(vtp);

	for_all_nodes (cp, ca->hash + h, node)
		if (cp->page.pgno == vtp->pgno
		    && cp->page.subno == vtp->subno)
			break;

	if (cp->node.succ) {
		/* already cached */

		if (vtp_size(&cp->page) == size) {
			// move to front.
			add_head(ca->hash + h, unlink_node(ca->hash + h, &cp->node));
		} else {
			cache_page *new_cp;

			if (!(new_cp = malloc(sizeof(*cp) - sizeof(cp->page) + size)))
				return 0;
			unlink_node(ca->hash + h, &cp->node);
			free(cp);
			cp = new_cp;
			add_head(ca->hash + h, &cp->node);
		}
	} else {
		if (!(cp = malloc(sizeof(*cp) - sizeof(cp->page) + size)))
			return 0;
		if (vtp->subno >= ca->hi_subno[vtp->pgno])
			ca->hi_subno[vtp->pgno] = vtp->subno + 1;
		ca->npages++;
		add_head(ca->hash + h, &cp->node);
	}

	memcpy(&cp->page, vtp, size);

	return &cp->page;
}



/////////////////////////////////
// this is for browsing the cache
/////////////////////////////////

/*
    Same as cache_get but doesn't make the found entry new
*/

static struct vt_page *
cache_lookup(struct cache *ca, int pgno, int subno)
{
	cache_page *cp;
	int h = hash(pgno);

	for_all_nodes (cp, ca->hash + h, node)
		if (cp->page.pgno == pgno)
			if (subno == ANY_SUB || cp->page.subno == subno)
				return &cp->page;
	return NULL;
}

int
vbi_cache_foreach(struct vbi *vbi, int pgno, int subno,
		      int dir, foreach_callback *func, void *data)
{
	struct cache *ca = vbi->cache;
    struct vt_page *vtp;
    int wrapped = 0;
    int r;

    if (ca->npages == 0)
	return 0;

    if ((vtp = cache_lookup(ca, pgno, subno)))
	subno = vtp->subno;
    else if (subno == ANY_SUB)
	subno = 0;

    for (;;) {
	if ((vtp = cache_lookup(ca, pgno, subno)))
	    if ((r = func(data, vtp, wrapped)))
		return r;

	subno += dir;

	while (subno < 0 || subno >= ca->hi_subno[pgno]) {
	    pgno += dir;
	    if (pgno < 0x100) {
		pgno = 0x8FF;
		wrapped = 1;
	    }
	    if (pgno > 0x8FF) {
		pgno = 0x100;
		wrapped = 1;
	    }
	    subno = dir < 0 ? ca->hi_subno[pgno] - 1 : 0;
	}
    }
}

/* preliminary */
int
vbi_cache_hi_subno(struct vbi *vbi, int pgno)
{
	return vbi->cache->hi_subno[pgno];
}

static struct cache_ops cops =
{
};


struct cache *
vbi_cache_init(struct vbi *vbi)
{
    struct cache *ca;
//    struct vt_page *vtp;
    int i;

    if (!(ca = malloc(sizeof(*ca))))
	goto fail1;

    for (i = 0; i < HASH_SIZE; ++i)
	init_list(ca->hash + i);

    memset(ca->hi_subno, 0, sizeof(ca->hi_subno));
    ca->erc = 1;
    ca->npages = 0;
    ca->op = &cops;

    return ca;

//fail2:
    free(ca);
fail1:
    return 0;
}






