#ifndef VBI_H
#define VBI_H

#include "vt.h"
#include "dllist.h"
#include "cache.h"
#include "lang.h"

/* #include "../common/fifo.h"
  libvbi.h exports this globally :-(
 */

#define PLL_ADJUST	4

struct raw_page
{
    struct vt_page page[1];
    struct enhance enh[1];
};

#define BUFS 4



struct vbi
{
    int fd;
    struct cache *cache;
    struct dl_head clients[1];
    // raw buffer management
    int bufsize;		// nr of bytes sent by this device
    int nbufs;
    unsigned char *bufs[BUFS];
    int bpl;			// bytes per line
    u32 seq;
    // page assembly
    struct raw_page rpage[8];	// one for each magazin
    struct raw_page *ppage;	// points to page of previous pkt0
    // phase correction
    int pll_fixed;		// 0 = auto, 1..2*PLL_ADJUST+1 = fixed
    int pll_adj;
    int pll_dir;
    int pll_cnt;
    int pll_err, pll_lerr;
    // v4l2 decoder data
    int bpb;			// bytes per bit * 2^16
    int bp8bl, bp8bh;		// bytes per 8-bit low/high
    int soc, eoc;		// start/end of clock run-in
    // sliced data source
    void *fifo;
};

struct vbi_client
{
    struct dl_node node[1];
    void (*handler)(void *data, struct vt_event *ev);
    void *data;
};

struct vbi *vbi_open(char *vbi_dev_name, struct cache *ca, int fine_tune,
								int big_buf);
void vbi_close(struct vbi *vbi);
void vbi_reset(struct vbi *vbi);
int vbi_add_handler(struct vbi *vbi, void *handler, void *data);
void vbi_del_handler(struct vbi *vbi, void *handler, void *data);
struct vt_page *vbi_query_page(struct vbi *vbi, int pgno, int subno);
void vbi_pll_reset(struct vbi *vbi, int fine_tune);

int v4l2_vbi_setup_dev(struct vbi *vbi);
int v4l_vbi_setup_dev(struct vbi *vbi);

void out_of_sync(struct vbi *vbi);
int vbi_line(struct vbi *vbi, u8 *p);

#endif
