#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include "os.h"
#include "vt.h"
#include "misc.h"
#include "vbi.h"
#include "fdset.h"
#include "hamm.h"
#include "lang.h"

void
out_of_sync(struct vbi *vbi)
{
    int i;

    // discard all in progress pages
    for (i = 0; i < 8; ++i)
	vbi->rpage[i].page->flags &= ~PG_ACTIVE;
}

// send an event to all clients

static void
vbi_send(struct vbi *vbi, int type, int i1, int i2, int i3, void *p1)
{
    struct vt_event ev[1];
    struct vbi_client *cl, *cln;

    ev->resource = vbi;
    ev->type = type;
    ev->i1 = i1;
    ev->i2 = i2;
    ev->i3 = i3;
    ev->p1 = p1;

    for (cl = $ vbi->clients->first; (cln = $ cl->node->next); cl = cln)
	cl->handler(cl->data, ev);
}

static void
vbi_send_page(struct vbi *vbi, struct raw_page *rvtp, int page)
{
    struct vt_page *cvtp = 0;

    if (rvtp->page->flags & PG_ACTIVE)
    {
	if (rvtp->page->pgno % 256 != page)
	{
	    rvtp->page->flags &= ~PG_ACTIVE;
	    enhance(rvtp->enh, rvtp->page);
	    if (vbi->cache)
		cvtp = vbi->cache->op->put(vbi->cache, rvtp->page);
	    vbi_send(vbi, EV_PAGE, 0, 0, 0, cvtp ?: rvtp->page);
	}
    }
}

// fine tune pll
// this routines tries to adjust the sampling point of the decoder.
// it collects parity and hamming errors and moves the sampling point
// a 10th of a bitlength left or right.

#define PLL_SAMPLES	4	// number of err vals to collect
#define PLL_ERROR	4	// if this err val is crossed, readjust
//#define PLL_ADJUST	4	// max/min adjust (10th of bitlength)

/* OBSOLETE */
static void
pll_add(struct vbi *vbi, int n, int err)
{
    if (vbi->pll_fixed)
	return;

    if (err > PLL_ERROR*2/3)	// limit burst errors
	err = PLL_ERROR*2/3;

    vbi->pll_err += err;
    vbi->pll_cnt += n;
    if (vbi->pll_cnt < PLL_SAMPLES)
	return;

    if (vbi->pll_err > PLL_ERROR)
    {
	if (vbi->pll_err > vbi->pll_lerr)
	    vbi->pll_dir = -vbi->pll_dir;
	vbi->pll_lerr = vbi->pll_err;

	vbi->pll_adj += vbi->pll_dir;
	if (vbi->pll_adj < -PLL_ADJUST || vbi->pll_adj > PLL_ADJUST)
	{
	    vbi->pll_adj = 0;
	    vbi->pll_dir = -1;
	    vbi->pll_lerr = 0;
	}
    }
    vbi->pll_cnt = 0;
    vbi->pll_err = 0;
}

/* OBSOLETE */
void
vbi_pll_reset(struct vbi *vbi, int fine_tune)
{
    vbi->pll_fixed = fine_tune >= -PLL_ADJUST && fine_tune <= PLL_ADJUST;

    vbi->pll_err = 0;
    vbi->pll_lerr = 0;
    vbi->pll_cnt = 0;
    vbi->pll_dir = -1;
    vbi->pll_adj = 0;
    if (vbi->pll_fixed)
	vbi->pll_adj = fine_tune;
}



/*
 *  Packet 28/29
 */

/*
 *  Table 30: Colour Map (0xBGR)
 */
static u16
default_colour_map[32] = {
    0x000, 0x00F, 0x0F0, 0x0FF,
    0xF00, 0xF0F, 0xFF0, 0xFFF,
    0x000, 0x007, 0x070, 0x077,
    0x700, 0x707, 0x770, 0x777,
    0x50F, 0x07F, 0x7F0, 0xBFF,
    0xAC0, 0x005, 0x256, 0x77C,
    0x333, 0x77F, 0x7F7, 0x7FF,
    0xF77, 0xF7F, 0xFF7, 0xDDD
};

/* channel -> magazine -> page */

struct page_extension {

	char		primary_char_set;
	char		secondary_char_set;

	char		def_screen_colour;	/* border */
	char		def_row_colour;

	char		foreground_clut;	/* 0, 8, 16, 24 */
	char		background_clut;

	u16		colour_map[32];
};

static int
parse_extension(u8 *p)
{
	int triplets[13], *triplet = triplets, buf = 0, left = 0;
	int i, err = 0;

	static int
	bits(int count)
	{
		int r, n;

		r = buf;
		if ((n = count - left) > 0) {
			r |= (buf = *triplet++) << left;
			left = 18;
		} else
			n = count;
		buf >>= n;
		left -= n;

		return r & ((1UL << count) - 1);
	}

	for (i = 0; i < 13; p += 3, i++)
		triplet[i] = hamm24(p, &err);

	if (err & 0xf000)
		return 0;

	printf("page function %d\n", bits(4));
	printf("page coding %d\n", bits(3));
	printf("c/s designation %d\n", bits(7));
	printf("c/s second %d\n", bits(7));
	printf("left panel %d\n", bits(1));
	printf("right panel %d\n", bits(1));
	printf("panel status %d\n", bits(1));
	printf("side panel columns %d\n", bits(4));
	for (i = 0; i <= 15; i++)
		printf("clut entry %d %03x (bgr)\n", i, bits(12));
	printf("def screen col %d\n", bits(5));
	printf("def row col %d\n", bits(5));
	printf("bbg subst %d\n", bits(1));
	printf("colour table remapping %d\n", bits(3));
	printf("\n");

	return 1;
}











// process one videotext packet

static int
vt_packet(struct vbi *vbi, u8 *p)
{
    struct vt_page *cvtp;
    struct raw_page *rvtp;
    int hdr, mag, mag8, pkt, i;
    int err = 0;

    hdr = hamm16(p, &err);
    if (err & 0xf000)
	return -4;

    mag = hdr & 7;
    mag8 = mag?: 8;
    pkt = (hdr >> 3) & 0x1f;
    p += 2;

    rvtp = vbi->rpage + mag;
    cvtp = rvtp->page;

    switch (pkt)
    {
	case 0:
	{
	    int b1, b2, b3, b4;

	    b1 = hamm16(p, &err);	// page number
	    b2 = hamm16(p+2, &err);	// subpage number + flags
	    b3 = hamm16(p+4, &err);	// subpage number + flags
	    b4 = hamm16(p+6, &err);	// language code + more flags

	    if (vbi->ppage->page->flags & PG_MAGSERIAL)
		vbi_send_page(vbi, vbi->ppage, b1);
	    vbi_send_page(vbi, rvtp, b1);

	    if (err & 0xf000)
		return 4;

	    cvtp->errors = (err >> 8) + chk_parity(p + 8, 32);;
	    cvtp->pgno = mag8 * 256 + b1;
	    cvtp->subno = (b2 + b3 * 256) & 0x3f7f;
	    cvtp->lang = "\0\4\2\6\1\5\3\7"[b4 >> 5] + (latin1 ? 0 : 8);
	    cvtp->flags = b4 & 0x1f;
	    cvtp->flags |= b3 & 0xc0;
	    cvtp->flags |= (b2 & 0x80) >> 2;
	    cvtp->lines = 1;
	    cvtp->flof = 0;
	    vbi->ppage = rvtp;

	    pll_add(vbi, 1, cvtp->errors);

	    conv2latin(p + 8, 32, cvtp->lang);
	    vbi_send(vbi, EV_HEADER, cvtp->pgno, cvtp->subno, cvtp->flags, p);

	    if (b1 == 0xff)
		return 0;

	    cvtp->flags |= PG_ACTIVE;
	    init_enhance(rvtp->enh);
	    memcpy(cvtp->data[0]+0, p, 40);
	    memset(cvtp->data[0]+40, ' ', sizeof(cvtp->data)-40);
	    return 0;
	}

	case 1 ... 24:
	{
	    pll_add(vbi, 1, err = chk_parity(p, 40));

	    if (~cvtp->flags & PG_ACTIVE)
		return 0;

	    cvtp->errors += err;
	    cvtp->lines |= 1 << pkt;
	    conv2latin(p, 40, cvtp->lang);
	    memcpy(cvtp->data[pkt], p, 40);
	    return 0;
	}

	/* X/25: keyword search ...? */

	case 26:
	{
	    int d, t[13];

	    if (~cvtp->flags & PG_ACTIVE)
		return 0;

	    d = hamm8(p, &err);
	    if (err & 0xf000)
		return 4;

	    for (i = 0; i < 13; ++i)
		t[i] = hamm24(p + 1 + 3*i, &err);
	    if (err & 0xf000)
		return 4;

//	    printf("enhance on %x/%x\n", cvtp->pgno, cvtp->subno);
	    add_enhance(rvtp->enh, d, t);
	    return 0;
	}

	case 27:
	{
	    // FLOF data (FastText)
	    int b1,b2,b3,x;
	    
	    if (~cvtp->flags & PG_ACTIVE)
		return 0; // -1 flushes all pages.  we may never resync again :(

	    b1 = hamm8(p, &err);
	    b2 = hamm8(p + 37, &err);
	    if (err & 0xf000)
		return 4;
	    if (b1 != 0 || not(b2 & 8))
		return 0;

	    for (i = 0; i < 6; ++i)
	    {
		err = 0;
		b1 = hamm16(p+1+6*i, &err);
		b2 = hamm16(p+3+6*i, &err);
		b3 = hamm16(p+5+6*i, &err);
		if (err & 0xf000)
		    return 1;
		x = (b2 >> 7) | ((b3 >> 5) & 0x06);
		cvtp->link[i].pgno = ((mag ^ x) ?: 8) * 256 + b1;
		cvtp->link[i].subno = (b2 + b3 * 256) & 0x3f7f;
	    }
	    cvtp->flof = 1;
	    return 0;
	}

	case 28:
	{
	    int d;

	    d = hamm8(p, &err);
	    if (err & 0xf000)
		return 4;

//	    printf("%d/28/%d\n", mag8, d);
//	    parse_extension(p + 1);

	    return 0;
	}

	case 29:
	{
	    int d;

	    d = hamm8(p, &err);
	    if (err & 0xf000)
		return 4;

//	    printf("%d/29/%d\n", mag8, d);
//	    parse_extension(p + 1);

	    return 0;
	}

	case 30:
	{
	    if (mag8 != 8)
		return 0;

	    p[0] = hamm8(p, &err);		// designation code
	    p[1] = hamm16(p+1, &err);		// initial page
	    p[3] = hamm16(p+3, &err);		// initial subpage + mag
	    p[5] = hamm16(p+5, &err);		// initial subpage + mag
	    if (err & 0xf000)
		return 4;

	    err += chk_parity(p+20, 20);
	    conv2latin(p+20, 20, 0);

	    vbi_send(vbi, EV_XPACKET, mag8, pkt, err, p);
	    return 0;
	}

	default:
	    // unused at the moment...
	    //vbi_send(vbi, EV_XPACKET, mag8, pkt, err, p);
	    return 0;
    }
    return 0;
}

#include "../common/fifo.h"
#include "v4lx.h"
#include "sliced.h"

#define SLICED_TELETEXT_B \
	(SLICED_TELETEXT_B_L10_625 | SLICED_TELETEXT_B_L25_625)

/*
    Preliminary bottom half of the Teletext thread,
    called by zvbi.c/zvbi_thread(). Note fifo is
    unbuffered yet.
 */
void
vbi_teletext(struct vbi *vbi, buffer *b)
{
    vbi_sliced *s;
    int items;

    /* call out_of_sync if timestamp delta > 1.5 * frame period */

    s = (vbi_sliced *) b->data;
    items = b->used / sizeof(vbi_sliced);

    while (items) {
	if (s->id & SLICED_TELETEXT_B)
	    vt_packet(vbi, s->data);

	items--;
	s++;
    }
}



// process one raw vbi line
/* OBSOLETE */
int
vbi_line(struct vbi *vbi, u8 *p)
{
    u8 data[43], min, max;
    int dt[256], hi[6], lo[6];
    int i, n, sync, thr;
    int bpb = vbi->bpb;

    /* remove DC. edge-detector */
    for (i = vbi->soc; i < vbi->eoc; ++i)
	dt[i] = p[i+bpb/FAC] - p[i];	// amplifies the edges best.

    /* set barrier */
    for (i = vbi->eoc; i < vbi->eoc+16; i += 2)
	dt[i] = 100, dt[i+1] = -100;

    /* find 6 rising and falling edges */
    for (i = vbi->soc, n = 0; n < 6; ++n)
    {
	while (dt[i] < 32)
	    i++;
	hi[n] = i;
	while (dt[i] > -32)
	    i++;
	lo[n] = i;
    }
    if (i >= vbi->eoc)
	return -1;	// not enough periods found

    i = hi[5] - hi[1];	// length of 4 periods (8 bits)
    if (i < vbi->bp8bl || i > vbi->bp8bh)
	return -2;	// bad frequency
    /* AGC and sync-reference */
    min = 255, max = 0, sync = 0;
    for (i = hi[4]; i < hi[5]; ++i)
	if (p[i] > max)
	    max = p[i], sync = i;
    for (i = lo[4]; i < lo[5]; ++i)
	if (p[i] < min)
	    min = p[i];
    thr = (min + max) / 2;

    p += sync;

    /* search start-byte 11100100 */
    for (i = 4*bpb + vbi->pll_adj*bpb/10; i < 16*bpb; i += bpb)
	if (p[i/FAC] > thr && p[(i+bpb)/FAC] > thr) // two ones is enough...
	{
	    /* got it... */
	    memset(data, 0, sizeof(data));

	    for (n = 0; n < 43*8; ++n, i += bpb)
		if (p[i/FAC] > thr)
		    data[n/8] |= 1 << (n%8);

	    if (data[0] != 0x27)	// really 11100100? (rev order!)
		return -3;

	    if ((i = vt_packet(vbi, data+1)))
	      {
		if (i < 0)
		  pll_add(vbi, 2, -i);
		else
		  pll_add(vbi, 1, i);
	      }
	    return 0;
	}
    return -4;
}

int
vbi_add_handler(struct vbi *vbi, void *handler, void *data)
{
    struct vbi_client *cl;

    if (not(cl = malloc(sizeof(*cl))))
	return -1;
    cl->handler = handler;
    cl->data = data;
    dl_insert_last(vbi->clients, cl->node);
    return 0;
}

void
vbi_del_handler(struct vbi *vbi, void *handler, void *data)
{
    struct vbi_client *cl;

    for (cl = $ vbi->clients->first; cl->node->next; cl = $ cl->node->next)
	if (cl->handler == handler && cl->data == data)
	{
	    dl_remove(cl->node);
	    break;
	}
    return;
}

struct vbi *
vbi_open(char *vbi_name, struct cache *ca, int fine_tune, int big_buf)
{
    static int inited = 0;
    struct vbi *vbi;
    extern void open_vbi(void);
    
    
    if (not inited)
	lang_init();
    inited = 1;

    if (not(vbi = malloc(sizeof(*vbi))))
    {
	error("out of memory");
	goto fail1;
    }

    vbi->fifo = open_vbi_v4lx(vbi_name);

    if (!vbi->fifo)
    {
	ioerror(vbi_name);
	goto fail2;
    }

    vbi->cache = ca;

    dl_init(vbi->clients);
    vbi->seq = 0;
    out_of_sync(vbi);
    vbi->ppage = vbi->rpage;

    vbi_pll_reset(vbi, fine_tune);
    // now done by sliced device
    ///* now done by v4l2 and v4l modules */
    ////    fdset_add_fd(fds, vbi->fd, vbi_handler, vbi);
    return vbi;

fail2:
    free(vbi);
fail1:
    return 0;
}

void
vbi_close(struct vbi *vbi)
{
//    fdset_del_fd(fds, vbi->fd);
    if (vbi->cache)
	vbi->cache->op->close(vbi->cache);

    close_vbi_v4lx(vbi->fifo);
//    close(vbi->fd);

    free(vbi);
}


struct vt_page *
vbi_query_page(struct vbi *vbi, int pgno, int subno)
{
    struct vt_page *vtp = 0;

    if (vbi->cache)
	vtp = vbi->cache->op->get(vbi->cache, pgno, subno);
    if (vtp == 0)
    {
	// EV_PAGE will come later...
	return 0;
    }

    vbi_send(vbi, EV_PAGE, 1, 0, 0, vtp);
    return vtp;
}

void
vbi_reset(struct vbi *vbi)
{
    if (vbi->cache)
	vbi->cache->op->reset(vbi->cache);
    vbi_send(vbi, EV_RESET, 0, 0, 0, 0);
}
