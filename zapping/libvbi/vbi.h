#ifndef VBI_H
#define VBI_H

#include "vt.h"
#include "cc.h"

#include "dllist.h"
#include "cache.h"
//#include "lang.h"

#include "../common/types.h"
#include "libvbi.h"

// #define PLL_ADJUST	4


struct vbi
{
    struct cache *cache;
    struct dl_head clients[1];

	int			quit; // stoopid

	int			event_mask;
	vbi_network		network;

	struct teletext		vt;
	struct caption		cc;

    // sliced data source
    void *fifo;
};

struct vbi *vbi_open(char *vbi_dev_name, struct cache *ca, int fine_tune);
void vbi_close(struct vbi *vbi);
int vbi_add_handler(struct vbi *vbi, int event_mask, void *handler, void *data);
void vbi_del_handler(struct vbi *vbi, void *handler, void *data);


extern void *	vbi_mainloop(void *p);




void	out_of_sync(struct vbi *vbi);
void vbi_send(struct vbi *vbi, int type, int pgno, int subno, int i1, int i2, int i3, void *p1);

#endif
