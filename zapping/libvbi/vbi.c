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


#define BPL	2048
#define FAC	(1<<16)
#define STEP	335062	// (int)(35.468950/6.9375*FAC+.5)	// PAL!

#define BASE_VIDIOCPRIVATE	192
#define BTTV_VERSION		_IOR('v' , BASE_VIDIOCPRIVATE+6, int)
#define BTTV_VBISIZE		_IOR('v' , BASE_VIDIOCPRIVATE+8, int)


static u8 rawbuf[BPL*19*2];	// one global buffer for raw vbi data
			// (putting this on the stack may trash the mm-system)

int vbi_big_buf = -1;
int vbi_fine_tune = 0;	// delay decoding n/10 bit length



static void
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

    for (cl = $ vbi->clients->first; cln = $ cl->node->next; cl = cln)
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

// process one videotext packet

static int
vt_line(struct vbi *vbi, u8 *p)
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

	    //printf("enhance on %x/%x\n", cvtp->pgno, cvtp->subno);
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



// process one raw vbi line

static int
vbi_line(struct vbi *vbi, u8 *p)
{
    u8 data[43], min, max;
    int dt[256], hi[6], lo[6];
    int i, n, sync, thr;

    /* remove DC. edge-detector */
    for (i = 40; i < 240; ++i)
	dt[i] = p[i+STEP/FAC] - p[i];	// amplifies the edges best.

    /* set barrier */
    for (i = 240; i < 256; i += 2)
	dt[i] = 100, dt[i+1] = -100;

    /* find 6 rising and falling edges */
    for (i = 40, n = 0; n < 6; ++n)
    {
	while (dt[i] < 32)
	    i++;
	hi[n] = i;
	while (dt[i] > -32)
	    i++;
	lo[n] = i;
    }
    if (i >= 240)
	return -1;	// not enough periods found

    i = hi[5] - hi[1];	// length of 4 periods (8 bits), normally 40.9
    if (i < 39 || i > 42)
	return -1;	// bad frequency

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
    for (i = 4*STEP + vbi->pll_adj*STEP/10; i < 16*STEP; i += STEP)
	if (p[i/FAC] > thr && p[(i+STEP)/FAC] > thr) // two ones is enough...
	{
	    /* got it... */
	    memset(data, 0, sizeof(data));

	    for (n = 0; n < 43*8; ++n, i += STEP)
		if (p[i/FAC] > thr)
		    data[n/8] |= 1 << (n%8);

	    if (data[0] != 0x27)	// really 11100100? (rev order!)
		return -1;

	    if (i = vt_line(vbi, data+1))
		if (i < 0)
		    pll_add(vbi, 2, -i);
		else
		    pll_add(vbi, 1, i);
	    return 0;
	}
    return -1;
}



// called when new vbi data is waiting

void
vbi_handler(struct vbi *vbi, int fd)
{
    int n, i;
    u32 seq;

    n = read(vbi->fd, rawbuf, vbi->bufsize);

    if (dl_empty(vbi->clients))
	return;

    if (n != vbi->bufsize)
	return;

    seq = *(u32 *)&rawbuf[n - 4];
    if (vbi->seq+1 != seq)
    {
	out_of_sync(vbi);
	if (seq < 3 && vbi->seq >= 3)
	    vbi_reset(vbi);
    }
    vbi->seq = seq;

    if (seq > 1)	// the first may contain data from prev channel
	for (i = 0; i+BPL <= n; i += BPL)
	    vbi_line(vbi, rawbuf + i);
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
    
    if (not inited)
	lang_init();
    inited = 1;

    if (not(vbi = malloc(sizeof(*vbi))))
    {
	error("out of memory");
	goto fail1;
    }

    if ((vbi->fd = open(vbi_name, O_RDONLY)) == -1)
    {
	ioerror(vbi_name);
	goto fail2;
    }

    if ((vbi->bufsize = ioctl(vbi->fd, BTTV_VBISIZE)) == -1)
    {
	// big_buf == -1 default, 0 small, 1 large buffer
#ifdef DEFAULT_SMALL_BUF
	vbi->bufsize = big_buf==1 ? 19*2*2048 : 16*2*2048; // small is def
#else
	vbi->bufsize = big_buf!=0 ? 19*2*2048 : 16*2*2048; // large is def
#endif
    }
    else if (vbi->bufsize < BPL || vbi->bufsize > (int)sizeof(rawbuf)
						    || vbi->bufsize % BPL != 0)
    {
	error("illegal size of bttv driver's buffer (%d bytes)", vbi->bufsize);
	goto fail3;
    }
    else if (big_buf != -1)
	error("-oldbttv/-newbttv option ignored.");

    vbi->cache = ca;

    dl_init(vbi->clients);
    vbi->seq = 0;
    out_of_sync(vbi);
    vbi->ppage = vbi->rpage;

    vbi_pll_reset(vbi, fine_tune);
    fdset_add_fd(fds, vbi->fd, vbi_handler, vbi);
    return vbi;

fail3:
    close(vbi->fd);
fail2:
    free(vbi);
fail1:
    return 0;
}



void
vbi_close(struct vbi *vbi)
{
    fdset_del_fd(fds, vbi->fd);
    if (vbi->cache)
	vbi->cache->op->close(vbi->cache);
    close(vbi->fd);
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
