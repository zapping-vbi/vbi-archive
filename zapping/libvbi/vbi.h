#ifndef VBI_H
#define VBI_H

#include "vt.h"
#include "dllist.h"
#include "cache.h"
//#include "lang.h"

#include "../common/types.h"

// #define PLL_ADJUST	4


struct raw_page
{
    struct vt_page page[1];
//	vt_extension		extension;
	u8			drcs_mode[48];
	int			num_triplets;
	int			ait_page;
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
    // magazine defaults

	vt_pagenum		initial_page;
	magazine		magazine[9];	/* 1 ... 8; #0 unmodified level 1 ... 1.5 default */

	struct {
		signed char		btt;
		unsigned char		mip;
		unsigned short		sub_pages;
	}			page_info[0x800];

	vt_pagenum		btt_link[15];
	bool			top;		/* use top navigation, flof overrides */

    // page assembly
	struct raw_page		raw_page[8];
	struct raw_page		*current;

    // sliced data source
    void *fifo;
};

struct vbi_client
{
    struct dl_node node[1];
    void (*handler)(void *data, struct vt_event *ev);
    void *data;
};

struct vbi *vbi_open(char *vbi_dev_name, struct cache *ca, int fine_tune);
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
void vbi_set_default_region(struct vbi *vbi, int default_region);

extern struct vt_page *convert_page(struct vbi *vbi, struct vt_page *vtp, bool cached, page_function new_function);

#endif
