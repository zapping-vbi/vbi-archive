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
#include "hamm.h"
#include "lang.h"
#include "export.h"

void reset_magazines(struct vbi *vbi);
#define printable(c) ((((c) & 0x7F) < 0x20 || ((c) & 0x7F) > 0x7E) ? '.' : ((c) & 0x7F))

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
	static void dump_drcs_download(u8 *p, u8 *drcs_mode);

	/* curiosity hack */
/*	if (rvtp->page->function == PAGE_FUNCTION_GDRCS
	    || rvtp->page->function == PAGE_FUNCTION_DRCS)
		dump_drcs_download(rvtp->page->raw, rvtp->drcs_mode);
*/
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




/*
 *  Packet 28/29
 */

/*
 *  Table 30: Colour Map (0xBGR)
 */
static u16
default_colour_map[32] = {
    0x000, 0x00F, 0x0F0, 0x0FF, 0xF00, 0xF0F, 0xFF0, 0xFFF,
    0x000, 0x007, 0x070, 0x077, 0x700, 0x707, 0x770, 0x777,
    0x50F, 0x07F, 0x7F0, 0xBFF, 0xAC0, 0x005, 0x256, 0x77C,
    0x333, 0x77F, 0x7F7, 0x7FF, 0xF77, 0xF7F, 0xFF7, 0xDDD
};

void
vbi_set_default_region(struct vbi *vbi, int default_region)
{
	int i;
	struct vt_extension *ext;

	for (i = 0; i < 8; i++) {
		ext = vbi->magazine_extension + i;

		ext->char_set[0] =
		ext->char_set[1] =
			default_region;
	}
}

void
reset_magazines(struct vbi *vbi)
{
	struct vt_extension *ext;
	int i, j;

	vbi->initial_page.pgno = 0x100;
	vbi->initial_page.subno = 0x3F7F; /* any */

	for (i = 0; i < 8; i++) {
		ext = vbi->magazine_extension + i;

		ext->designations		= 0;

		ext->char_set[0]		= 16;		/* Latin G0, G2, English subset */
		ext->char_set[0]		= 16;		/* Latin G0, English subset */
								/* Region Western Europe and Turkey */

		ext->def_screen_colour		= BLACK;	/* A.5 */
		ext->def_row_colour		= BLACK;	/* A.5 */
		ext->black_bg_substitution	= FALSE;
		ext->foreground_clut		= 0;
		ext->background_clut		= 0;

		ext->left_side_panel		= FALSE;
		ext->right_side_panel		= FALSE;
		ext->left_panel_columns		= 0;		/* sum 16 */

		for (j = 0; j < 8; j++)
			ext->dclut4[0][j] = j & 3;

		for (j = 0; j < 32; j++)
			ext->dclut16[0][j] = j & 15;

		memcpy(ext->colour_map, default_colour_map,
			sizeof(ext->colour_map));
	}
}

static void
dump_extension(struct vt_extension *ext)
{
	int i;

	printf("designations %08x\n", ext->designations);
	printf("char set primary %d secondary %d\n",
		ext->char_set[0], ext->char_set[1]);
	printf("default screen col %d row col %d\n",
		ext->def_screen_colour, ext->def_row_colour);
	printf("bbg subst %d colour table remapping %d, %d\n",
		ext->black_bg_substitution, ext->foreground_clut, ext->background_clut);
	printf("panel left %d right %d left columns %d\n",
		ext->left_side_panel, ext->right_side_panel, ext->left_panel_columns);
	printf("colour map (bgr):\n");
	for (i = 0; i <= 31; i++) {
		printf("%03x, ", ext->colour_map[i]);
		if ((i % 8) == 7) printf("\n");
	}
	printf("dclut4 global: ");
	for (i = 0; i <= 3; i++)
		printf("%2d ", ext->dclut4[0][i]);
	printf("\ndclut4 normal: ");
	for (i = 0; i <= 3; i++)
		printf("%2d ", ext->dclut4[1][i]);
	printf("\ndclut16 global: ");
	for (i = 0; i <= 15; i++)
		printf("%2d ", ext->dclut16[0][i]);
	printf("\ndclut16 normal: ");
	for (i = 0; i <= 15; i++)
		printf("%2d ", ext->dclut16[1][i]);
	printf("\n\n");
}

/*
 *  DRCS (hehe!)
 */

static void
dump_drc(u8 *p)
{
	int i, j;

	for (i = 0; i < 10; i++) {
		for (j = 0; j < 6; j++) {
			putchar("0123456789ABCDEF"[p[j] >> 4]);
			putchar("0123456789ABCDEF"[p[j] & 15]);
		}
		
		putchar('\n');
		p += 6;
	}
}

static void
dump_drcs_download(u8 *p, u8 *drcs_mode)
{
	int i, j, k, l, c;
	u8 drc[60], *d;

	for (i = 0; i < 48; i++) {
		int add_planes = 0;

		switch (drcs_mode[i]) {
		case DRCS_MODE_12_10_4:
			add_planes += 2;

		case DRCS_MODE_12_10_2:
			add_planes += 1;

		case DRCS_MODE_12_10_1:
			memset(drc, 0, sizeof(drc));

			i += add_planes;

			for (j = 0; j <= add_planes; p += 20, j++) {
				chk_parity(p, 20);

				for (d = drc, k = 0; k < 20; d += 3, k++) {
					d[0] |= (((p[k] & 0x20) >> 1) | ((p[k] & 0x10) >> 4)) << j;
					d[1] |= (((p[k] & 0x08) << 1) | ((p[k] & 0x04) >> 2)) << j;
					d[2] |= (((p[k] & 0x02) << 3) | ((p[k] & 0x01) >> 0)) << j;
				}
			}

			dump_drc(drc);

			break;

		case DRCS_MODE_6_5_4:
			memset(drc, 0, sizeof(drc));
			chk_parity(p, 20);

			for (j = 0; j < 4; j++)
				for (d = drc, k = 0; k < 5; d += 10, k++)
					for (l = 0; l < 6; l++)
						d[l] |= ((p[k] >> (5 - l)) & 1) << j;

			for (j = 0; j < 60; j += 12)
				for (k = 0; k < 6; k++)
					drc[j + k + 6] = drc[j + k] |= drc[j + k] << 4;

			p += 20;

			dump_drc(drc);

			break;

		default:
			p += 20;

			break;
		}
	}
}					    

static int
hamm8_page_number(vt_pagenum *pn, u8 *raw, int magazine)
{
	int b1, b2, b3, m;
	int err = 0;

	b1 = hamm16(raw + 0, &err);
	b2 = hamm16(raw + 2, &err);
	b3 = hamm16(raw + 4, &err);
	if (err & 0xf000)
		return 0;

	m = ((b3 >> 5) & 6) + (b2 >> 7);

	pn->pgno = ((magazine ^ m) ? : 8) * 256 + b1;
	pn->subno = (b3 * 256 + b2) & 0x3f7f;

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

	    memcpy(cvtp->raw[0]+0, p, 40);
	    cvtp->errors = (err >> 8) + chk_parity(p + 8, 32);;
	    cvtp->pgno = mag8 * 256 + b1;
	    cvtp->subno = (b2 + b3 * 256) & 0x3f7f;
	    cvtp->lang = "\0\4\2\6\1\5\3\7"[b4 >> 5] + (latin1 ? 0 : 8);

	    cvtp->national = bit_reverse[b4] & 7;

	    cvtp->flags = b4 & 0x1f;
	    cvtp->flags |= b3 & 0xc0;
	    cvtp->flags |= (b2 & 0x80) >> 2;
	    cvtp->lines = 1;
	    cvtp->flof = 0;
	    vbi->ppage = rvtp;


	    conv2latin(p + 8, 32, cvtp->lang);
	    vbi_send(vbi, EV_HEADER, cvtp->pgno, cvtp->subno, cvtp->flags, p);

	    if (b1 == 0xff)
		return 0;

	    cvtp->flags |= PG_ACTIVE;
	    cvtp->function = PAGE_FUNCTION_UNKNOWN;
	    cvtp->coding = PAGE_CODING_UNKNOWN;
	    init_enhance(rvtp->enh);
	    memcpy(cvtp->data[0]+0, p, 40);
	    memset(cvtp->data[0]+40, ' ', sizeof(cvtp->data)-40);
	    memset(cvtp->raw[0]+40, ' ', sizeof(cvtp->raw)-40);
	    rvtp->extension.designations = 0;
	    cvtp->num_triplets = 0;
	    cvtp->vbi = vbi;

		for (i = 0; i < 24; i++) {
			cvtp->link[i].pgno = 0x0FF;
			cvtp->link[i].subno = 0x3F7F;
			cvtp->enh_link[i >> 1].pgno = 0x0FF;
			cvtp->enh_link[i >> 1].subno = 0xFFFF;
		}

	    return 0;
	}

	case 1 ... 24:
	{
		memcpy(cvtp->raw[pkt], p, 40);

	     err = chk_parity(p, 40);

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
		int designation;
		unsigned int t;
		vt_triplet triplet;

	    if (~cvtp->flags & PG_ACTIVE)
		return 0;

		designation = hamm8(p, &err);
		if (err & 0xf000)
			return 4;

		if (cvtp->num_triplets >= 16 * 13
		    || cvtp->num_triplets != designation * 13
		    || (cvtp->num_triplets > 0
			&& cvtp->triplets[cvtp->num_triplets - 1].stop)) { /* XXX correct? */
			cvtp->num_triplets = -1;
			return 0;
		}

//	fprintf(stderr, "packet %d/%d/%d page %x/%x\n", mag8, pkt, designation, cvtp->pgno, cvtp->subno);

		for (p++, i = 0; i < 13; p += 3, i++) {
			t = hamm24(p, &err);

			triplet.address = t & 0x3F;
			triplet.mode = (t >> 6) & 0x1F;
			triplet.data = t >> 11;
			triplet.stop = !((~t) & 0x7FF);

//    if ((triplet.mode >= 0x18 || triplet.mode <= 0x08) && !triplet.stop)
//	printf("triplet %02x %02x %02x %d\n",
//		triplet.address, triplet.mode, triplet.data, triplet.stop);

			cvtp->triplets[cvtp->num_triplets++] = triplet;
		}

		return 0;
	}

	case 27:
	{
		int designation, control, crc;

	    if (~cvtp->flags & PG_ACTIVE)
		return 0; // -1 flushes all pages.  we may never resync again :(

		designation = hamm8(p, &err);
		if (err & 0xf000)
			return 4;

printf("X/27/%d\n", designation);
		switch (designation) {
		case 0:
			control = hamm8(p + 37, &err);
			if (err & 0xf000)
				return 4;
printf("X/27/%d %02x\n", designation, control);

			crc = p[38] + p[39] * 256;

			if ((control & 7) != 7)
				return 0; /* local COP */

			cvtp->flof = control >> 3; /* display row 24 */

			/* fall through */
		case 1:
		case 2:
		case 3:
			for (p++, i = 0; i <= 5; p += 6, i++) {
				if (!hamm8_page_number(cvtp->link + designation * 6 + i, p, mag))
					return 1;

// printf("X/27/%d link[%d] page %03x/%03x\n", designation, i,
//	cvtp->link[designation * 6 + i].pgno, cvtp->link[designation * 6 + i].subno);
			}

			break;

		case 4:
		case 5:
			for (p++, i = 0; i <= 5; p += 6, i++) {
				int t1, t2;

				t1 = hamm24(p + 0, &err);
				t2 = hamm24(p + 3, &err);
				if (err & 0xf000)
					return 4;

				cvtp->enh_link[designation * 6 - 24 + i].type = t1 & 3;
				cvtp->enh_link[designation * 6 - 24 + i].pgno =
					((((t1 >> 12) & 0x7) ^ mag) ? : 8) * 256
					+ ((t1 >> 11) & 0x0F0) + ((t1 >> 7) & 0x00F);
				cvtp->enh_link[designation * 6 - 24 + i].subno =
					(t2 >> 3) & 0xFFFF;
printf("X/27/%d enh_link[%d] type %d page %03x subno %04x\n", designation, i,
	cvtp->enh_link[designation * 6 - 24 + i].type,
	cvtp->enh_link[designation * 6 - 24 + i].pgno,
	cvtp->enh_link[designation * 6 - 24 + i].subno);
			}

			break;
		}

		return 0;
	}

	case 28:
	case 29:
	{
		int designation, function, coding;
		int triplets[13], *triplet = triplets, buf = 0, left = 0;
		struct vt_extension *x;

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

		designation = hamm8(p, &err);
		if (err & 0xf000)
			return 4;

//	printf("packet %d/%d/%d\n", mag8, pkt, designation);

		for (p++, i = 0; i < 13; p += 3, i++)
			triplet[i] = hamm24(p, &err);

		switch (designation) {
		case 0: /* X/28/0, M/29/0 Level 2.5 */
		case 4: /* X/28/4, M/29/4 Level 3.5 */
			if (err & 0xf000)
				return 4;

			function = bits(4);
			coding = bits(3);

			if (pkt == 28) {
				if (cvtp->function != PAGE_FUNCTION_UNKNOWN
				    && cvtp->function != function)
					return 0; /* XXX discard rpage? */

				cvtp->function = function;
				cvtp->coding = coding;
			}

			if (function != PAGE_FUNCTION_LOP
			    || coding != PAGE_CODING_PARITY)
				return 0;

			/* XXX X/28/0 Format 2, distinguish how? */

			x = vbi->magazine_extension + mag8 - 1;

			if (pkt == 28) {
				if (!rvtp->extension.designations) {
					memcpy(&rvtp->extension, x, sizeof(rvtp->extension));
					rvtp->extension.designations <<= 16;
				}

				x = &rvtp->extension;
			}

			if (designation == 4 && (x->designations & (1 << 0)))
				bits(14 + 2 + 1 + 4);
			else {
				x->char_set[0] = bits(7);
				x->char_set[1] = bits(7);

				x->left_side_panel = bits(1);
				x->right_side_panel = bits(1);

				bits(1); /* panel status: level 2.5/3.5 */

				x->left_panel_columns = bit_reverse[bits(4)] >> 4;

				if (x->left_side_panel | x->right_side_panel)
					x->left_panel_columns =
						x->left_panel_columns ? : 16;
			}

			if (designation == 4)
				for (i = 0; i <= 15; i++)
					x->colour_map[i] = bits(12);
			else
				for (i = 16; i <= 31; i++)
					x->colour_map[i] = bits(12);

			if (designation == 4 && (x->designations & (1 << 0)))
				bits(10 + 1 + 3);
			else {
				x->def_screen_colour = bits(5);
				x->def_row_colour = bits(5);

				x->black_bg_substitution = bits(1);

				i = bits(3); /* colour table remapping */

				x->foreground_clut = "\00\00\00\10\10\20\20\20"[i];
				x->background_clut = "\00\10\20\10\20\10\20\30"[i];
			}

			x->designations |= 1 << designation;

			if (pkt == 29) {
				if (0 && designation == 4)
					x->designations &= ~(1 << 0);

				/*
				    XXX update
				    inherited_mag_desig = page->extension.designations >> 16;
				    new_mag_desig = 1 << designation;
				    page_desig = page->extension.designations;
				     if (((inherited_mag_desig | page_desig) & new_mag_desig) == 0)
				    shortcut: AND of (inherited_mag_desig | page_desig) of all pages
				     with extensions, no updates required in round 2++
				    other option, all M/29/x should have been received within the
				    maximum repetition interval of 20 s.
				 */
			}

//	dump_extension(x);

			return 0;

		case 1: /* X/28/1, M/29/1 Level 3.5 DRCS CLUT */

			x = vbi->magazine_extension + mag8 - 1;

			if (pkt == 28) {
				if (!rvtp->extension.designations) {
					memcpy(&rvtp->extension, x, sizeof(rvtp->extension));
					rvtp->extension.designations = 0;
				}

				x = &rvtp->extension;
			}

			triplet++;

			for (i = 0; i < 8; i++)
				x->dclut4[0][i] = bit_reverse[bits(5)] >> 3;

			for (i = 0; i < 32; i++)
				x->dclut16[0][i] = bit_reverse[bits(5)] >> 3;

			x->designations |= 1 << 1;

//	dump_extension(x);

			return 0;

		case 3: /* X/28/3 Level 2.5, 3.5 DRCS download page */
			if (pkt == 29)
				break; /* M/29/3 undefined */

			if (err & 0xf000)
				return 4;

			function = bits(4);
			coding = bits(3);

			if (cvtp->function != PAGE_FUNCTION_UNKNOWN
			    && cvtp->function != function)
				return 0; /* XXX discard rpage? */

			cvtp->function = function;
			cvtp->coding = coding;

			if (function != PAGE_FUNCTION_GDRCS
			    || function != PAGE_FUNCTION_DRCS
			    || coding != PAGE_CODING_PARITY)
				return 0;

			bits(11);

			for (i = 0; i < 48; i++) {
				rvtp->drcs_mode[i] = bits(4);
//	printf("%1x ", rvtp->drcs_mode[i]);
			}
//	printf("\n");
			return 0;
		}

		return 0;
	}

	case 30:
	{
		int designation;

		if (mag8 != 8)
			return 0;

		designation = hamm8(p, &err);
		if (err & 0xf000)
			return 4;

		if (designation > 4)
			return 0;

		if (!hamm8_page_number(&vbi->initial_page, p + 1, 0))
			return 1;

		if ((vbi->initial_page.pgno & 0xFF) == 0xFF) {
			vbi->initial_page.pgno = 0x100;
			vbi->initial_page.subno = 0x3F7F; /* any */
		}

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
    reset_magazines(vbi);

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
    reset_magazines(vbi);
    vbi_send(vbi, EV_RESET, 0, 0, 0, 0);
}
