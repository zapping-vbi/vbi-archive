#ifndef VBI_H
#define VBI_H

#include "vt.h"
#include "dllist.h"
#include "cache.h"
#include "lang.h"

#define PLL_ADJUST	4

struct raw_page
{
    struct vt_page page[1];
    struct enhance enh[1];
};

struct vbi
{
    int fd;
    struct cache *cache;
    struct dl_head clients[1];
    int bufsize;		// 32*2k or 38*2k (depending on bttv version)
    u32 seq;
    struct raw_page rpage[8];	// one for each magazin
    struct raw_page *ppage;	// points to page of previous pkt0
    /*
    struct vt_page page[8];	// one for each magazin
    struct vt_page *ppage;	// points to page of previous pkt0
    */
    int pll_fixed;		// 0 = auto, 1..2*PLL_ADJUST+1 = fixed
    int pll_adj;
    int pll_dir;
    int pll_cnt;
    int pll_err, pll_lerr;
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

#endif
