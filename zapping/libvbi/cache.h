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
	//    void (*close)(struct cache *ca);
    //    struct vt_page *(*get)(struct cache *ca, int pgno, int subno, int subno_mask);
    //    struct vt_page *(*put)(struct cache *ca, struct vt_page *vtp);
	//    void (*reset)(struct cache *ca);
    struct vt_page *(*foreach_pg)(struct cache *ca, int pgno, int subno,
					    int dir, int (*func)(), void *data);
    int (*foreach_pg2)(struct cache *ca, int pgno, int subno,
		       int dir, int (*func)(), void *data);

};



extern struct cache *   vbi_cache_init(struct vbi *);
extern void		vbi_cache_destroy(struct vbi *);
extern struct vt_page * vbi_cache_put(struct vbi *, struct vt_page *vtp);
extern struct vt_page * vbi_cache_get(struct vbi *, int pgno, int subno, int subno_mask);

#endif





























































