#ifndef CACHE_H
#define CACHE_H

#include "vt.h"
#include "../common/list.h"

#define HASH_SIZE	113

struct cache
{
    list hash[HASH_SIZE];
    int erc;			// error reduction circuit on
    int npages;
    unsigned short hi_subno[0x9ff + 1];	// 0:pg not in cache, 1-3f80:highest subno + 1
    struct cache_ops *op;
};

struct cache_ops
{
};

typedef int foreach_callback(void *, struct vt_page *, int); 

extern struct cache *   vbi_cache_init(struct vbi *);
extern void		vbi_cache_destroy(struct vbi *);
extern struct vt_page * vbi_cache_put(struct vbi *, struct vt_page *vtp);
extern struct vt_page * vbi_cache_get(struct vbi *, int pgno, int subno, int subno_mask);
extern int              vbi_cache_foreach(struct vbi *, int pgno, int subno,
					  int dir, foreach_callback *func, void *data);
extern void             vbi_cache_flush(struct vbi *);

#endif




























































