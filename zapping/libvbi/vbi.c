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

	if (rvtp->page->pgno % 256 != page)
	{
	    rvtp->page->flags &= ~PG_ACTIVE;
//	    enhance(rvtp->enh, rvtp->page);

if (0)
if ((rvtp->page->pgno & 15) > 9 || (rvtp->page->pgno & 0xFF) > 0x99) {
int k, l;
for (k = 0; k < 25; k++) {
for (l = 0; l < 40; l++)
    printf("%02x ", rvtp->page->data.unknown.raw[k][l]);
for (l = 0; l < 40; l++)
    printf("%c", printable(rvtp->page->data.unknown.raw[k][l]));
printf("\n");
}
}


	    if (rvtp->extension.designations != 0) {
		rvtp->page->extension = rvtp->extension; /* XXX temporary */
	    }

	    if (vbi->cache)
		cvtp = vbi->cache->op->put(vbi->cache, rvtp->page);
	    if (cvtp && rvtp->extension.designations != 0) {
		cvtp->data.unknown.extension = &cvtp->extension;
	    }
	    vbi_send(vbi, EV_PAGE, 0, 0, 0, cvtp ?: rvtp->page);
	}
    }
}




/*
 *  Packet 28/29
 */

/*
 *  ETS 300 706 Table 30: Colour Map
 */
static const rgba
default_colour_map[32] = {
	0xFF000000, 0xFF0000FF, 0xFF00FF00, 0xFF00FFFF,	0xFFFF0000, 0xFFFF00FF, 0xFFFFFF00, 0xFFFFFFFF,
	0xFF000000, 0xFF000077, 0xFF007700, 0xFF007777, 0xFF770000, 0xFF770077, 0xFF777700, 0xFF777777,
	0xFF5500FF, 0xFF0077FF, 0xFF77FF00, 0xFFBBFFFF, 0xFFAACC00, 0xFF000055, 0xFF225566, 0xFF7777CC,
	0xFF333333, 0xFF7777FF, 0xFF77FF77, 0xFF77FFFF, 0xFFFF7777, 0xFFFF77FF, 0xFFFFFF77, 0xFFDDDDDD
};

static const unsigned char
n2nn[16] = {
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
};

void
vbi_set_default_region(struct vbi *vbi, int default_region)
{
	int i;
	vt_extension *ext;

	for (i = 0; i < 9; i++) {
		ext = &vbi->magazine[i].extension;

		ext->char_set[0] =
		ext->char_set[1] =
			default_region;
	}
}

void
reset_magazines(struct vbi *vbi)
{
	magazine *mag;
	vt_extension *ext;
	int i, j;

	vbi->initial_page.pgno = 0x100;
	vbi->initial_page.subno = ANY_SUB;

	memset(vbi->magazine, 0, sizeof(vbi->magazine));

	for (i = 0; i < 9; i++) {
		mag = vbi->magazine + i;

		for (j = 0; j < 16; j++) {
			mag->pop_link[j].pgno = 0x0FF;		/* unused */
			mag->drcs_link[j] = 0x0FF;		/* unused */
		}

		ext = &mag->extension;

		ext->char_set[0]		= 16;		/* Latin G0, G2, English subset */
		ext->char_set[0]		= 16;		/* Latin G0, English subset */
								/* Region Western Europe and Turkey */

		ext->def_screen_colour		= BLACK;	/* A.5 */
		ext->def_row_colour		= BLACK;	/* A.5 */
		ext->foreground_clut		= 0;
		ext->background_clut		= 0;

		for (j = 0; j < 8; j++)
			ext->drcs_clut[j + 2] = j & 3;

		for (j = 0; j < 32; j++)
			ext->drcs_clut[j + 10] = j & 15;

		memcpy(ext->colour_map, default_colour_map,
			sizeof(ext->colour_map));
	}
}

static void
dump_extension(vt_extension *ext)
{
	int i;

	printf("designations %08x\n", ext->designations);
	printf("char set primary %d secondary %d\n",
		ext->char_set[0], ext->char_set[1]);
	printf("default screen col %d row col %d\n",
		ext->def_screen_colour, ext->def_row_colour);
	printf("bbg subst %d colour table remapping %d, %d\n",
		ext->fallback.black_bg_substitution, ext->foreground_clut, ext->background_clut);
	printf("panel left %d right %d left columns %d\n",
		ext->fallback.left_side_panel, ext->fallback.right_side_panel,
		ext->fallback.left_panel_columns);
	printf("colour map (bgr):\n");
	for (i = 0; i <= 31; i++) {
		printf("%08x, ", ext->colour_map[i]);
		if ((i % 8) == 7) printf("\n");
	}
	printf("dclut4 global: ");
	for (i = 0; i <= 3; i++)
		printf("%2d ", ext->drcs_clut[i + 2]);
	printf("\ndclut4 normal: ");
	for (i = 0; i <= 3; i++)
		printf("%2d ", ext->drcs_clut[i + 6]);

	printf("\ndclut16 global: ");
	for (i = 0; i <= 15; i++)
		printf("%2d ", ext->drcs_clut[i + 10]);
	printf("\ndclut16 normal: ");
	for (i = 0; i <= 15; i++)
		printf("%2d ", ext->drcs_clut[i + 26]);
	printf("\n\n");
}


static void
dump_drc(u8 *p)
{
	int i, j;

	for (i = 0; i < 10; i++) {
		for (j = 0; j < 6; j++) {
			putchar("0123456789ABCDEF"[p[j] & 15]);
			putchar("0123456789ABCDEF"[p[j] >> 4]);
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

void
drcs_conv(struct vt_page *vtp)
{
	int i, j, k, l, c;
	u8 *drc = vtp->drcs_bits[0], *d;
	u8 *drcs_mode = vtp->drcs_mode;

	u8 *p = vtp->data.unknown.raw[1];

	for (i = 0; i < 48; i++) {
		int add_planes = 0;

//		switch (drcs_mode[i]) {
		switch (DRCS_MODE_12_10_1) {
		case DRCS_MODE_12_10_4:
			add_planes += 2;

		case DRCS_MODE_12_10_2:
			add_planes += 1;

		case DRCS_MODE_12_10_1:
			memset(drc, 0, 60);

			i += add_planes;

			for (j = 0; j <= add_planes; p += 20, j++) {
				for (d = drc, k = 0; k < 20; d += 3, k++) {
					int t = p[k] & 0x7F; // XXX parity

					d[0] |= (((t & 0x20) >> 5) | ((t & 0x10) >> 0)) << j;
					d[1] |= (((t & 0x08) >> 3) | ((t & 0x04) << 2)) << j;
					d[2] |= (((t & 0x02) >> 1) | ((t & 0x01) << 4)) << j;
				}
			}
//printf("DRCS CONV %d\n", i);
//	dump_drc(drc);
			drc += 60;
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








static inline int
parse_mot(magazine *mag, int packet, u8 *p)
{
	int err = 0, i, j;

	switch (packet) {
	case 1 ... 8:
	{
		int index = (packet - 1) << 5;
		char n0, n1;

		for (i = 0; i < 20; index++, i++) {
			if (i == 10)
				index += 6;

			err = 0;
			n0 = hamm8(p++, &err);
			n1 = hamm8(p++, &err);

			if (err & 0xf000)
				continue;

			mag->pop_lut[index] = n0 & 7;
			mag->drcs_lut[index] = n1 & 7;
		}

		return 0;
	}

	case 9 ... 14:
	{
		int index = (packet - 9) * 3;
		char n0, n1;

		for (i = 0; i < 20; index++, i++) {
			if (i == 6 || i == 12) {
				if (index == 0x100)
					break;
				else
					index += 10;
			}

			err = 0;
			n0 = hamm8(p++, &err);
			n1 = hamm8(p++, &err);

			if (err & 0xf000)
				continue;

			mag->pop_lut[index] = n0 & 7;
			mag->drcs_lut[index] = n1 & 7;
		}

		return 0;
	}

	case 15 ... 18: /* not used */
		return 0;

	case 22 ... 23:	/* level 3.5 pops */
		packet--;

	case 19 ... 20: /* level 2.5 pops */
	{
		pop_link *pop = mag->pop_link + (packet - 19) * 4;
		char n[10];

		for (i = 0; i < 4; pop++, i++) {
			for (err = j = 0; j < 10; j++)
				n[j] = hamm8(p++, &err);
//printf("MOT m%p %d %d err %d\n", mag, packet, i, err);

			if (err & 0xf000) /* unused bytes poss. not hammed (N3) */
				continue;

			pop->pgno = (((n[0] & 7) ? : 8) << 8) + (n[1] << 4) + n[2];
//printf("MOT %d %d pgno %d\n", packet, i, pop->pgno);

			/* n[3] number of subpages ignored */

			if (n[4] & 1)
				memset(&pop->fallback, 0, sizeof(pop->fallback));
			else {
				int x = (n[4] >> 1) & 3;

				pop->fallback.black_bg_substitution = n[4] >> 3;
				pop->fallback.left_side_panel = x & 1;
				pop->fallback.right_side_panel = x >> 1;
				pop->fallback.left_panel_columns = "\00\20\20\10"[x];
			}

			pop->default_obj[0].type = n[5] & 3;
			pop->default_obj[0].address = (n[7] << 4) + n[6];
			pop->default_obj[1].type = n[5] >> 2;
			pop->default_obj[1].address = (n[9] << 4) + n[8];
		}

		return 0;
	}

	case 21:	/* level 2.5 drcs */
	case 24:	/* level 3.5 drcs */
	{
		int index = (packet == 21) ? 0 : 8;
		char n[4];

		for (i = 0; i < 8; index++, i++) {
			for (err = j = 0; j < 4; j++)
				n[j] = hamm8(p++, &err);

			if (err & 0xf000)
				continue;

			mag->drcs_link[index] = (((n[0] & 7) ? : 8) << 8) + (n[1] << 4) + n[2];

			/* n[3] number of subpages ignored */
		}

		return 0;
	}
	}

	return 0;
}

static bool
parse_pop(struct vt_page *vtp, int packet, u8 *p)
{
	int designation, triplet[13];
	vt_triplet *trip;
	int i, err = 0;

//printf("POP %x/%d/%d\n", vtp->pgno, vtp->subno, packet);

	if ((designation = hamm8a[p[0]]) < 0) {
//printf("designation *\n");
		return FALSE;
	}

// XXX granularity
	for (p++, i = 0; i < 13; p += 3, i++)
		triplet[i] = hamm24(p, &err);
	if (err & 0xf000) {
//printf("triplets *\n");
		return FALSE;
}

	if (packet == 26)
		packet += designation;

	switch (packet) {
	case 1 ... 2:
		if (!(designation & 1)) {
//printf("fixed *\n");
			return FALSE; /* fixed usage */
}

	case 3 ... 4:
		if (designation & 1) {
			int index = (packet - 1) * 26;

			for (index += 2, i = 1; i < 13; index += 2, i++) {
				vtp->data.pop.pointer[index + 0] = triplet[i] & 0x1FF;
				vtp->data.pop.pointer[index + 1] = triplet[i] >> 9;
			}

			return TRUE;
		}

		/* fall through */

	case 5 ... 42:
		trip = vtp->data.pop.triplet + (packet - 3) * 13;

		for (i = 0; i < 13; trip++, i++) {
			trip->address = triplet[i] & 0x3F;
			trip->mode = (triplet[i] >> 6) & 0x1F;
			trip->data = triplet[i] >> 11;
			trip->stop = !((~triplet[i]) & 0x7FF);
// printf("TR %d ad %d %02x mo %02x da %d %02x\n",
//    trip - vtp->data.pop.triplet, trip->address, trip->address, trip->mode, trip->data, trip->data);
		}

		return TRUE;
	}

//printf("packet *\n");
	return FALSE;
}

bool
convert_pop(struct vt_page *vtp, page_function function)
{
	struct vt_page page;
	int i;

	if (vtp->function != PAGE_FUNCTION_UNKNOWN)
		return FALSE;

	page.pgno = 0;
	page.subno = 0;

	memset(page.data.pop.pointer, 0xFF, sizeof(page.data.pop.pointer));
	memset(page.data.pop.triplet, 0xFF, sizeof(page.data.pop.triplet));

	for (i = 1; i <= 25; i++)
		if (vtp->lop_lines & (1 << i))
			if (!parse_pop(&page, i, vtp->data.unknown.raw[i]))
				; // return FALSE;

	memcpy(&page.data.pop.triplet[23 * 13],
		vtp->data.unknown.triplet,
		16 * 13 * sizeof(vt_triplet));

	vtp->function = function;

	memcpy(&vtp->data, &page.data, sizeof(page.data));
if(0)
for (i = 0; i < 24 * 4; i++)
    printf("pointer %d: %d\n", i, vtp->data.pop.pointer[i]);

	return TRUE;
}



// process one videotext packet

/* XXX!!! p read-only */

static int
vt_packet(struct vbi *vbi, u8 *p)
{
    struct vt_page *cvtp;
    struct raw_page *rvtp;
    int hdr, mag, mag8, pkt, i, j;
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
	    struct vt_page *vtp;
	    u8 raw[40];

	    b1 = hamm16(p, &err);	// page number
	    b2 = hamm16(p+2, &err);	// subpage number + flags
	    b3 = hamm16(p+4, &err);	// subpage number + flags
	    b4 = hamm16(p+6, &err);	// language code + more flags

// printf("out %x %x\n", vbi->ppage->page->pgno, rvtp->page->pgno);
	    if (vbi->ppage->page->flags & PG_MAGSERIAL)
		vbi_send_page(vbi, vbi->ppage, b1);
	    vbi_send_page(vbi, rvtp, b1);

	    if (err & 0xf000)
		return 4;

	    memcpy(raw, p, 40);
	    cvtp->errors = (err >> 8) + chk_parity(p + 8, 32);;

	    cvtp->pgno = mag8 * 256 + b1;
	    cvtp->subno = (b2 + b3 * 256) & 0x3f7f;
// printf("incoming %x\n", cvtp->pgno);

if(0)
if ((b1 & 15) > 9 || b1 > 0x99)
    printf("** data page %03x/%04x\n", cvtp->pgno, cvtp->subno);

	    cvtp->national = bit_reverse[b4] & 7;

	    cvtp->flags = b4 & 0x1f;
	    cvtp->flags |= b3 & 0xc0;
	    cvtp->flags |= (b2 & 0x80) >> 2;
	    cvtp->data.unknown.flof = 0;
	    vbi->ppage = rvtp;

/* GONE
	    conv2latin(p + 8, 32, cvtp->lang);
	    vbi_send(vbi, EV_HEADER, cvtp->pgno, cvtp->subno, cvtp->flags, p);
*/
	    if (b1 == 0xff)
		return 0;
	    cvtp->flags |= PG_ACTIVE;


		if (!(cvtp->flags & C4_ERASE_PAGE)
		    && (vtp = vbi->cache->op->get(vbi->cache, cvtp->pgno, cvtp->subno, 0xFFFF))) {
			memcpy(&cvtp->data, &vtp->data, sizeof(cvtp->data));
			/* XXX write cache directly | erc?*/
			/* XXX data page update */

			cvtp->function = vtp->function;

			if (cvtp->function == PAGE_FUNCTION_UNKNOWN
			    || cvtp->function == PAGE_FUNCTION_LOP)
				memcpy(cvtp->data.unknown.raw[0], raw, 40);

			cvtp->lop_lines = vtp->lop_lines;
			cvtp->enh_lines = vtp->enh_lines;
		} else {
			switch (b1) {
			case 0xFE:
				cvtp->function = PAGE_FUNCTION_MOT;
				break;

			default:
				if (b1 <= 0x99 && (b1 & 15) <= 9)
					cvtp->function = PAGE_FUNCTION_LOP;
				else
					cvtp->function = PAGE_FUNCTION_UNKNOWN;
				break;
			}

			memcpy(cvtp->data.unknown.raw[0] + 0, raw, 40);
			memset(cvtp->data.unknown.raw[0] + 40, ' ', sizeof(cvtp->data.unknown.raw) - 40);

			for (i = 0; i < 24; i++) {
				cvtp->data.unknown.link[i].pgno = 0x0FF;
				cvtp->data.unknown.link[i].subno = 0x3F7F;
			}

			for (i = 0; i < 12; i++) {
				cvtp->data.unknown.link[i + 24].pgno = 0x0FF;
				cvtp->data.unknown.link[i + 24].subno = 0xFFFF;
			}

			memset(cvtp->data.unknown.triplet, 0xFF, sizeof(cvtp->data.unknown.triplet));

			cvtp->lop_lines = 1;
			cvtp->enh_lines = 0;
		}

	    rvtp->extension.designations = 0;
	    rvtp->num_triplets = 0;
	    cvtp->vbi = vbi;

//if (cvtp->pgno == 0x1EE) {
//    printf("H2 %p 1EE func %d\n", cvtp, cvtp->function);
//}
	    return 0;
	}

	case 1 ... 25:
	    if (~cvtp->flags & PG_ACTIVE)
		return 0;

		switch (cvtp->function) {
		case PAGE_FUNCTION_MOT:
// printf("MOT %x %d\n", cvtp->pgno, pkt);
			if (parse_mot(vbi->magazine + mag8, pkt, p))
				return 0;
			break;

		case PAGE_FUNCTION_POP:
		case PAGE_FUNCTION_GPOP:
			if (parse_pop(cvtp, pkt, p))
				return 0;
			break;

		default:
			memcpy(cvtp->data.unknown.raw[pkt], p, 40);

	    err = chk_parity(p, 40);
	    cvtp->errors += err;
/* GONE
	    conv2latin(p, 40, cvtp->lang);
	    memcpy(cvtp->data[pkt], p, 40);
*/
			break;
		}

		cvtp->lop_lines |= 1 << pkt;

		return 0;

	case 26:
	{
		int designation;
		vt_triplet triplet;

	    if (~cvtp->flags & PG_ACTIVE)
		return 0;

		switch (cvtp->function) {
		case PAGE_FUNCTION_POP:
		case PAGE_FUNCTION_GPOP:
			if (parse_pop(cvtp, pkt, p))
				return 0;
			return 0; // XXX

		default:
			break;
		}

		if ((designation = hamm8a[p[0]]) < 0)
			return 4;

		if (rvtp->num_triplets >= 16 * 13 || rvtp->num_triplets != designation * 13) {
			rvtp->num_triplets = -1;
			return 0;
		}
if (0)
if((cvtp->pgno & 0xFF0) == 0x100)
fprintf(stderr, "%d %p packet %d/%d/%d page %x/%x\n",
    cvtp->function, cvtp,
    mag8, pkt, designation, cvtp->pgno, cvtp->subno);

		for (p++, i = 0; i < 13; p += 3, i++) {
			int t = hamm24(p, &err);

			triplet.address = t & 0x3F;
			triplet.mode = (t >> 6) & 0x1F;
			triplet.data = t >> 11;
			triplet.stop = !((~t) & 0x7FF);

//    if ((triplet.mode >= 0x18 || triplet.mode <= 0x08) && !triplet.stop)
//if((cvtp->pgno & 0xFF0) == 0x100)
//	printf("%d: triplet %02x %02x %02x %d\n", designation,
//		triplet.address, triplet.mode, triplet.data, triplet.stop);

			cvtp->data.unknown.triplet[rvtp->num_triplets++] = triplet;
		}

		cvtp->enh_lines |= 1 << designation;

		return 0;
	}

	case 27:
	{
		int designation, control, crc;

	    if (~cvtp->flags & PG_ACTIVE)
		return 0; // -1 flushes all pages.  we may never resync again :(

		if ((designation = hamm8a[p[0]]) < 0)
			return 4;

// printf("X/27/%d\n", designation);

		switch (designation) {
		case 0:
			if ((control = hamm8a[p[37]]) < 0)
				return 4;

// printf("%x/%x X/27/%d %02x\n", cvtp->pgno, cvtp->subno, designation, control);

			crc = p[38] + p[39] * 256;
//	printf("CRC: %04x\n", crc);

			/* ETR 287 subclause 10.6 */
			if ((control & 7) == 0)
				return 0;

			cvtp->data.unknown.flof = control >> 3; /* display row 24 */

			/* fall through */

		case 1:
		case 2:
		case 3:
			for (p++, i = 0; i <= 5; p += 6, i++) {
				if (!hamm8_page_number(cvtp->data.unknown.link
				    + designation * 6 + i, p, mag))
				; //	return 1;

// printf("X/27/%d link[%d] page %03x/%03x\n", designation, i,
//	cvtp->data.unknown.link[designation * 6 + i].pgno, cvtp->data.unknown.link[designation * 6 + i].subno);
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

				cvtp->data.unknown.link[designation * 6 + i].type = t1 & 3;
				cvtp->data.unknown.link[designation * 6 + i].pgno =
					((((t1 >> 12) & 0x7) ^ mag) ? : 8) * 256
					+ ((t1 >> 11) & 0x0F0) + ((t1 >> 7) & 0x00F);
				cvtp->data.unknown.link[designation * 6 + i].subno =
					(t2 >> 3) & 0xFFFF;
if(0)
 printf("X/27/%d link[%d] type %d page %03x subno %04x\n", designation, i,
	cvtp->data.unknown.link[designation * 6 + i].type,
	cvtp->data.unknown.link[designation * 6 + i].pgno,
	cvtp->data.unknown.link[designation * 6 + i].subno);
			}

			break;
		}

		return 0;
	}

	case 28:
	case 29:
	{
		int designation, function;
		int triplets[13], *triplet = triplets, buf = 0, left = 0;
		vt_extension *ext;

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
			bits(3); /* page coding ignored */

			/*
			 *  ZDF and BR3 transmit GPOP 1EE/.. with 1/28/0 function
			 *  0 = PAGE_FUNCTION_LOP, should be PAGE_FUNCTION_GPOP.
			 *  Makes no sense to me.
			 */
			if (0 && pkt == 28) {
				if (cvtp->function != PAGE_FUNCTION_UNKNOWN
				    && cvtp->function != function)
					return 0; /* XXX discard rpage? */

				cvtp->function = function;
			}

			if (function != PAGE_FUNCTION_LOP)
				return 0;

			/* XXX X/28/0 Format 2, distinguish how? */

			ext = &vbi->magazine[mag8].extension;

			if (pkt == 28) {
				if (!rvtp->extension.designations) {
					memcpy(&rvtp->extension, ext, sizeof(rvtp->extension));
					rvtp->extension.designations <<= 16;
				}

				ext = &rvtp->extension;
			}

			if (designation == 4 && (ext->designations & (1 << 0)))
				bits(14 + 2 + 1 + 4);
			else {
				ext->char_set[0] = bits(7);
				ext->char_set[1] = bits(7);

				ext->fallback.left_side_panel = bits(1);
				ext->fallback.right_side_panel = bits(1);

				bits(1); /* panel status: level 2.5/3.5 */

				ext->fallback.left_panel_columns = bit_reverse[bits(4)] >> 4;

				if (ext->fallback.left_side_panel
				    | ext->fallback.right_side_panel)
					ext->fallback.left_panel_columns =
						ext->fallback.left_panel_columns ? : 16;
			}

			j = (designation == 4) ? 16 : 32;

			for (i = j - 16; i < j; i++) {
				rgba col = bits(12);

				if (i == 8) /* transparent */
					continue;

				col |= (col & 0xF00) << 8;
				col &= 0xF00FF;
				col |= (col & 0xF0) << 4;
				col &= 0xF0F0F;

				ext->colour_map[i] = col | (col << 4) | 0xFF000000UL;
			}

			if (designation == 4 && (ext->designations & (1 << 0)))
				bits(10 + 1 + 3);
			else {
				ext->def_screen_colour = bits(5);
				ext->def_row_colour = bits(5);

				ext->fallback.black_bg_substitution = bits(1);

				i = bits(3); /* colour table remapping */

				ext->foreground_clut = "\00\00\00\10\10\20\20\20"[i];
				ext->background_clut = "\00\10\20\10\20\10\20\30"[i];
			}

			ext->designations |= 1 << designation;

			if (pkt == 29) {
				if (0 && designation == 4)
					ext->designations &= ~(1 << 0);

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

// if (cvtp->pgno==0x1EE) {
// 	dump_extension(ext);
// exit(EXIT_SUCCESS);
// }
			return 0;

		case 1: /* X/28/1, M/29/1 Level 3.5 DRCS CLUT */

			ext = &vbi->magazine[mag8].extension;

			if (pkt == 28) {
				if (!rvtp->extension.designations) {
					memcpy(&rvtp->extension, ext, sizeof(rvtp->extension));
					rvtp->extension.designations = 0;
				}

				ext = &rvtp->extension;
				/* XXX TODO */
			}

			triplet++;

			for (i = 0; i < 8; i++)
				ext->drcs_clut[i + 2] = bit_reverse[bits(5)] >> 3;

			for (i = 0; i < 32; i++)
				ext->drcs_clut[i + 10] = bit_reverse[bits(5)] >> 3;

			ext->designations |= 1 << 1;

//	dump_extension(ext);

			return 0;

		case 3: /* X/28/3 Level 2.5, 3.5 DRCS download page */
			if (pkt == 29)
				break; /* M/29/3 undefined */

			if (err & 0xf000)
				return 4;

			function = bits(4);
			bits(3); /* page coding ignored */

			if (cvtp->function != PAGE_FUNCTION_UNKNOWN
			    && cvtp->function != function)
				return 0; /* XXX discard rpage? */

			cvtp->function = function;

			if (function != PAGE_FUNCTION_GDRCS
			    || function != PAGE_FUNCTION_DRCS)
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

/* face it - GONE
	    err += chk_parity(p+20, 20);
	    conv2latin(p+20, 20, 0);
	    vbi_send(vbi, EV_XPACKET, mag8, pkt, err, p);
*/
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
    struct vbi *vbi;
    extern void open_vbi(void);
    
    

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
	vtp = vbi->cache->op->get(vbi->cache, pgno, subno, 0xFFFF);
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
