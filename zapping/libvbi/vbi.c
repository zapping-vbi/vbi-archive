#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "os.h"
#include "vt.h"
#include "misc.h"
#include "vbi.h"
#include "hamm.h"
#include "lang.h"
#include "export.h"
#include "tables.h"

static void reset_magazines(struct vbi *vbi);
#define printable(c) ((((c) & 0x7F) < 0x20 || ((c) & 0x7F) > 0x7E) ? '.' : ((c) & 0x7F))
static bool convert_drcs(struct vt_page *vtp, unsigned char *raw);

void
out_of_sync(struct vbi *vbi)
{
    int i;

    // discard all in progress pages

	for (i = 0; i < 8; i++)
		vbi->raw_page[i].page->function = PAGE_FUNCTION_DISCARD;
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
vbi_send_page(struct vbi *vbi, struct raw_page *rvtp)
{
    struct vt_page *cvtp = 0;

    if (rvtp->page->function != PAGE_FUNCTION_DISCARD)
    {
//	if (rvtp->page->pgno % 256 != page)
	{

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
	    if (rvtp->page->function == PAGE_FUNCTION_DRCS
		|| rvtp->page->function == PAGE_FUNCTION_GDRCS) {
		    if (!convert_drcs(rvtp->page, rvtp->page->data.drcs.raw[1]))
			return;
	    } else if (rvtp->page->function == PAGE_FUNCTION_UNKNOWN
		|| rvtp->page->function == PAGE_FUNCTION_LOP)
		if (rvtp->extension.designations != 0) {
		    rvtp->page->extension = rvtp->extension; /* XXX temporary */
		}

	    if (vbi->cache)
		cvtp = vbi->cache->op->put(vbi->cache, rvtp->page);

	    if (cvtp->function == PAGE_FUNCTION_UNKNOWN
		|| cvtp->function == PAGE_FUNCTION_LOP) {
    		if (cvtp && rvtp->extension.designations != 0)
		    cvtp->data.unknown.extension = &cvtp->extension;
		else
		    cvtp->data.unknown.extension = NULL;
	    }

	    vbi_send(vbi, EV_PAGE, 0, 0, 0, cvtp ?: rvtp->page);

	}
    }
}

static inline void
dump_pagenum(vt_pagenum page)
{
	printf("T%x %3x/%04x\n", page.type, page.pgno, page.subno);
}

static void
dump_extension(vt_extension *ext)
{
	int i;

	printf("Extension:\ndesignations %08x\n", ext->designations);
	printf("char set primary %d secondary %d\n", ext->char_set[0], ext->char_set[1]);
	printf("default screen col %d row col %d\n", ext->def_screen_colour, ext->def_row_colour);
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
dump_drcs(struct vt_page *vtp)
{
	int i, j, k;
	unsigned char *p = vtp->data.drcs.bits[0];

	printf("\nDRCS page %03x/%04x\n", vtp->pgno, vtp->subno);

	for (i = 0; i < 48; i++) {
		printf("DRC #%d mode %02x\n", i, vtp->data.drcs.mode[i]);

		for (j = 0; j < 10; p += 6, j++) {
			for (k = 0; k < 6; k++)
				printf("%x%x", p[k] & 15, p[k] >> 4);
			putchar('\n');
		}
	}
}

static void
dump_page_info(struct vbi *vbi)
{
	int i, j;

	for (i = 0; i < 0x800; i += 8) {
		printf("%03x: ", i + 0x100);

		for (j = 0; j < 8; j++)
			printf("%02x:%02x:%04x ",
				vbi->page_info[i + j].btt & 0xFF,
				vbi->page_info[i + j].mip & 0xFF,
				vbi->page_info[i + j].sub_pages & 0xFFFF);

		putchar('\n');
	}

	putchar('\n');
}

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

void
vbi_set_default_region(struct vbi *vbi, int default_region)
{
	int i;

	for (i = 0; i < 9; i++) {
		vt_extension *ext = &vbi->magazine[i].extension;

		ext->char_set[0] =
		ext->char_set[1] =
			default_region;
	}
}

static void
reset_magazines(struct vbi *vbi)
{
	magazine *mag;
	vt_extension *ext;
	int i, j;

	vbi->initial_page.pgno = 0x100;
	vbi->initial_page.subno = ANY_SUB;

	vbi->top = FALSE;

	memset(vbi->page_info, 0xFF, sizeof(vbi->page_info));

	/* Magazine defaults */

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

static inline bool
hamm8_page_number(vt_pagenum *p, u8 *raw, int magazine)
{
	int b1, b2, b3, err, m;

	err = b1 = hamm16a(raw + 0);
	err |= b2 = hamm16a(raw + 2);
	err |= b3 = hamm16a(raw + 4);

	if (err < 0)
		return FALSE;

	m = ((b3 >> 5) & 6) + (b2 >> 7);

	p->pgno = ((magazine ^ m) ? : 8) * 256 + b1;
	p->subno = (b3 * 256 + b2) & 0x3f7f;

	return TRUE;
}

static inline bool
parse_mot(magazine *mag, u8 *raw, int packet)
{
	int err, i, j;

	switch (packet) {
	case 1 ... 8:
	{
		int index = (packet - 1) << 5;
		char n0, n1;

		for (i = 0; i < 20; index++, i++) {
			if (i == 10)
				index += 6;

			err = n0 = hamm8a[*raw++];
			err |= n1 = hamm8a[*raw++];

			if (err < 0)
				continue;

			mag->pop_lut[index] = n0 & 7;
			mag->drcs_lut[index] = n1 & 7;
		}

		return TRUE;
	}

	case 9 ... 14:
	{
		int index = (packet - 9) * 0x30 + 10;
		char n0, n1;

		for (i = 0; i < 20; index++, i++) {
			if (i == 6 || i == 12) {
				if (index == 0x100)
					break;
				else
					index += 10;
			}

			err = n0 = hamm8a[*raw++];
			err |= n1 = hamm8a[*raw++];

			if (err < 0)
				continue;

			mag->pop_lut[index] = n0 & 7;
			mag->drcs_lut[index] = n1 & 7;
		}

		return TRUE;
	}

	case 15 ... 18: /* not used */
		return TRUE;

	case 22 ... 23:	/* level 3.5 pops */
		packet--;

	case 19 ... 20: /* level 2.5 pops */
	{
		pop_link *pop = mag->pop_link + (packet - 19) * 4;
		char n[10];

		for (i = 0; i < 4; raw += 10, pop++, i++) {
			for (err = j = 0; j < 10; j++)
				err |= n[j] = hamm8a[raw[j]];

			if (err < 0) /* unused bytes poss. not hammed (^ N3) */
				continue;

			pop->pgno = (((n[0] & 7) ? : 8) << 8) + (n[1] << 4) + n[2];

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

		return TRUE;
	}

	case 21:	/* level 2.5 drcs */
	case 24:	/* level 3.5 drcs */
	    {
		int index = (packet == 21) ? 0 : 8;
		char n[4];

		for (i = 0; i < 8; raw += 4, index++, i++) {
			for (err = j = 0; j < 4; j++)
				err |= n[j] = hamm8a[raw[j]];

			if (err < 0)
				continue;

			mag->drcs_link[index] = (((n[0] & 7) ? : 8) << 8) + (n[1] << 4) + n[2];

			/* n[3] number of subpages ignored */
		}

		return TRUE;
	    }
	}

	return TRUE;
}

static bool
parse_pop(struct vt_page *vtp, u8 *raw, int packet)
{
	int designation, triplet[13];
	vt_triplet *trip;
	int i, err = 0;

	if ((designation = hamm8a[raw[0]]) < 0) {
//		printf("POP %3x/%04x/%d designation hamming error\n",
//			vtp->pgno, vtp->subno, packet);
		return FALSE;
	}

// XXX granularity
	for (raw++, i = 0; i < 13; raw += 3, i++)
		triplet[i] = hamm24(raw, &err);
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

static unsigned int expand[64];

static void init_expand(void) __attribute__ ((constructor));

static void
init_expand(void)
{
	int i, j, n;

	for (i = 0; i < 64; i++) {
		for (n = j = 0; j < 6; j++)
			if (i & (0x20 >> j))
				n |= 1 << (j * 4);
		expand[i] = n;
	}
}

static bool
convert_drcs(struct vt_page *vtp, unsigned char *raw)
{
	unsigned char *p, *d;
	int i, j, q;

	p = raw;

	for (i = 1; i < 25; p += 40, i++)
		if (vtp->lop_lines & (1 << i)) {
			for (j = 0; j < 40; j++)
				if (parity(p[j]) < 0x40)
					p[j] = 0x40; // XXX reception error
		} else
			memset(p, 0x40, 40); // XXX missing?

	p = raw;
	d = vtp->data.drcs.bits[0];

	for (i = 0; i < 48; i++) {
		switch (vtp->data.drcs.mode[i]) {
		case DRCS_MODE_12_10_1:
			for (j = 0; j < 20; d += 3, j++) {
				d[0] = q = expand[p[j] & 0x3F];
				d[1] = q >> 8;
				d[2] = q >> 16;
			}
			p += 20;
			break;

		case DRCS_MODE_12_10_2:
			for (j = 0; j < 20; d += 3, j++) {
				q = expand[p[j +  0] & 0x3F]
				  + expand[p[j + 20] & 0x3F] * 2;
				d[0] = q;
				d[1] = q >> 8;
				d[2] = q >> 16;
			}
			p += 40;
			d += 60;
			i += 1;
			break;

		case DRCS_MODE_12_10_4:
			for (j = 0; j < 20; d += 3, j++) {
				q = expand[p[j +  0] & 0x3F]
				  + expand[p[j + 20] & 0x3F] * 2
				  + expand[p[j + 40] & 0x3F] * 4
				  + expand[p[j + 60] & 0x3F] * 8;
				d[0] = q;
				d[1] = q >> 8;
				d[2] = q >> 16;
			}
			p += 80;
			d += 180;
			i += 3;
			break;

		case DRCS_MODE_6_5_4:
			for (j = 0; j < 20; p += 4, d += 6, j++) {
				q = expand[p[0] & 0x3F]
				  + expand[p[1] & 0x3F] * 2
				  + expand[p[2] & 0x3F] * 4
				  + expand[p[3] & 0x3F] * 8;
				d[0] = (q & 15) * 0x11;
				d[1] = ((q >> 4) & 15) * 0x11;
				d[2] = ((q >> 8) & 15) * 0x11;
				d[3] = ((q >> 12) & 15) * 0x11;
				d[4] = ((q >> 16) & 15) * 0x11;
				d[5] = (q >> 20) * 0x11;
			}
			break;

		default:
			p += 20;
			d += 60;
			break;
		}
	}

	if (1)
		dump_drcs(vtp);

	return TRUE;
}

static inline bool
parse_mip(struct vbi *vbi, int mag8, u8 *raw, int packet)
{
	switch (packet) {
	case 1 ... 8:
	{
		int n, i, index = mag8 * 256 + (packet - 1) * 32;

		for (i = 0; i < 20; raw += 2, index++, i++) {
			if (i == 10)
				index += 6;

			if ((n = hamm16a(raw)) < 0)
				continue;

			vbi->page_info[index - 0x100].mip = n;
		}

		return TRUE;
	}

	case 9 ... 14:
	{
		int n, i, index = mag8 * 256 + (packet - 9) * 0x30 + 10;

		for (i = 0; i < 20; raw += 2, index++, i++) {
			if (i == 6 || i == 12) {
				if (index == 0x100)
					break;
				else
					index += 10;
			}

			if ((n = hamm16a(raw)) < 0)
				continue;

			vbi->page_info[index - 0x100].mip = n;
		}

		return TRUE;
	}

	/* XXX subpages */

	}

	return TRUE;
}

/*
 *  Table Of Pages navigation
 */

static const int dec2bcd[20] = {
	0x000, 0x040, 0x080, 0x120, 0x160, 0x200, 0x240, 0x280, 0x320, 0x360,
	0x400, 0x440, 0x480, 0x520, 0x560, 0x600, 0x640, 0x680, 0x720, 0x760
};

static bool
top_page_number(vt_pagenum *p, u8 *raw)
{
	char n[8];
	int pgno, err, i;

	for (err = i = 0; i < 8; i++)
		err |= n[i] = hamm8a[raw[i]];

	pgno = n[0] * 256 + n[1] * 16 + n[2];

	if (err < 0 || pgno > 0x8FF)
		return FALSE;

	p->pgno = pgno;
	p->subno = ((n[3] << 12) | (n[4] << 8) | (n[5] << 4) | n[6]) & 0x3f7f; // ?
	p->type = n[7]; // ?

	return TRUE;
}

static inline bool
parse_btt(struct vbi *vbi, u8 *raw, int packet)
{
	switch (packet) {
	case 1 ... 20:
	{
		int i, j, index = dec2bcd[packet - 1];
		char n;

		for (i = 0; i < 4; i++) {
			for (j = 0; j < 10; index++, j++)
				if ((n = hamm8a[*raw++]) >= 0)
					vbi->page_info[index].btt = n;

			index += ((index & 0xFF) == 0x9A) ? 0x66 : 0x06;
		}

		break;
	}

	case 21 ... 23:
	    {
		vt_pagenum *p = vbi->btt_link + (packet - 21) * 5;
		int i;

		vbi->top = TRUE;

		for (i = 0; i < 5; raw += 8, p++, i++) {
			if (!top_page_number(p, raw))
				continue;

			if (0) {
				printf("BTT #%d: ", (packet - 21) * 5);
				dump_pagenum(*p);
			}

			switch (p->type) {
			case 1: // MPT?
			case 2: // AIT?
				vbi->page_info[p->pgno - 0x100].mip = MIP_TOP_PAGE;
				break;
			}
		}

		break;
	    }
	}

	if (0 && packet == 1)
		dump_page_info(vbi);

	return TRUE;
}

static bool
parse_ait(struct vt_page *vtp, u8 *raw, int packet)
{
	int i, n;
	ait_entry *ait;

	if (packet < 1 || packet > 23)
		return TRUE;

	ait = vtp->data.ait + (packet - 1) * 2;

	if (top_page_number(&ait[0].page, raw + 0)) {
		for (i = 0; i < 12; i++)
			if ((n = parity(raw[i + 8])) >= 0)
				ait[0].text[i] = n;
	}

	if (top_page_number(&ait[1].page, raw + 20)) {
		for (i = 0; i < 12; i++)
			if ((n = parity(raw[i + 28])) >= 0)
				ait[1].text[i] = n;
	}

	return TRUE;
}

static inline bool
parse_mpt(struct vbi *vbi, u8 *raw, int packet)
{
	int i, j, index;
	char n;

	switch (packet) {
	case 1 ... 20:
		index = dec2bcd[packet - 1];

		for (i = 0; i < 4; i++) {
			for (j = 0; j < 10; index++, j++)
				if ((n = hamm8a[*raw++]) >= 0) {
					if (n > 0)
						vbi->page_info[index].sub_pages = n;
				}

			index += ((index & 0xFF) == 0x9A) ? 0x66 : 0x06;
		}
	}

	return TRUE;
}

bool
convert_page(struct vbi *vbi, struct vt_page *vtp, page_function new_function)
{
	struct vt_page page;
	int i;

	if (vtp->function != PAGE_FUNCTION_UNKNOWN)
		return FALSE;

	page.pgno = vtp->pgno;
	page.subno = vtp->subno;

	switch (new_function) {
	case PAGE_FUNCTION_LOP:
		vtp->function = new_function;
		return TRUE;

	case PAGE_FUNCTION_GPOP:
	case PAGE_FUNCTION_POP:
		memset(page.data.pop.pointer, 0xFF, sizeof(page.data.pop.pointer));
		memset(page.data.pop.triplet, 0xFF, sizeof(page.data.pop.triplet));

		for (i = 1; i <= 25; i++)
			if (vtp->lop_lines & (1 << i))
				if (!parse_pop(&page, vtp->data.unknown.raw[i], i))
					return FALSE;

			memcpy(&page.data.pop.triplet[23 * 13],	vtp->data.unknown.triplet,
				16 * 13 * sizeof(vt_triplet));
		break;

	case PAGE_FUNCTION_GDRCS:
	case PAGE_FUNCTION_DRCS:
		memcpy(page.data.drcs.raw, vtp->data.unknown.raw, sizeof(page.data.drcs.raw));
		memset(page.data.drcs.bits, 0, sizeof(page.data.drcs.bits));
		memset(page.data.drcs.mode, 0, sizeof(page.data.drcs.mode));
		page.lop_lines = vtp->lop_lines;

		if (!convert_drcs(&page, vtp->data.unknown.raw[1]))
			return FALSE;

		break;

	case PAGE_FUNCTION_AIT:
		memset(page.data.ait, 0, sizeof(page.data.ait));

		for (i = 1; i <= 23; i++)
			if (vtp->lop_lines & (1 << i))
				if (!parse_ait(&page, vtp->data.unknown.raw[i], i))
					return FALSE;
		break;

	case PAGE_FUNCTION_MPT:
		for (i = 1; i <= 20; i++)
			if (vtp->lop_lines & (1 << i))
				if (!parse_mpt(vbi, vtp->data.unknown.raw[i], i))
					return FALSE;
		break;

	case PAGE_FUNCTION_MPT_EX:
	default:
		return FALSE;
	}

	vtp->function = new_function;

	memcpy(&vtp->data, &page.data, sizeof(page.data));

	return TRUE;
}

/*
 *  Station identification for automatic channel programming
 *  and multi-channel cache; Programme status display.
 *
 *  XXX interface to be defined
 */

typedef enum {
	CNI_NONE,
	CNI_VPS,	/* VPS format */
	CNI_8301,	/* Teletext packet 8/30 format 1 */
	CNI_8302,	/* Teletext packet 8/30 format 2 */
	CNI_X26		/* Teletext packet X/26 local enhancement */
} cni_type;

typedef struct {
	cni_type	type;
	int		code;
} cni;

static bool
station_lookup(cni *cni, char **country, char **short_name, char **long_name)
{
	int j, code = cni->code;

	if (!code)
		return FALSE;

	switch (cni->type) {
	case CNI_8301:
		for (j = 0; PDC_CNI[j].short_name; j++)
			if (PDC_CNI[j].cni1 == code) {
				*country = country_names_en[PDC_CNI[j].country];
				*short_name = PDC_CNI[j].short_name;
				*long_name = PDC_CNI[j].long_name;
				return TRUE;
			}
		break;

	case CNI_8302:
		for (j = 0; PDC_CNI[j].short_name; j++)
			if (PDC_CNI[j].cni2 == code) {
				*country = country_names_en[PDC_CNI[j].country];
				*short_name = PDC_CNI[j].short_name;
				*long_name = PDC_CNI[j].long_name;
				return TRUE;
			}

		code &= 0x0FFF;

		/* fall through */

	case CNI_VPS:
		/* if (code == 0x0DC3) in decoder
			code = mark ? 0x0DC2 : 0x0DC1; */

		for (j = 0; VPS_CNI[j].short_name; j++)
			if (VPS_CNI[j].cni == code) {
				*country = country_names_en[VPS_CNI[j].country];
				*short_name = VPS_CNI[j].short_name;
				*long_name = VPS_CNI[j].long_name;
				return TRUE;
			}
		break;

	case CNI_X26:
		for (j = 0; PDC_CNI[j].short_name; j++)
			if (PDC_CNI[j].cni3 == code) {
				*country = country_names_en[PDC_CNI[j].country];
				*short_name = PDC_CNI[j].short_name;
				*long_name = PDC_CNI[j].long_name;
				return TRUE;
			}

		/* try code | 0x0080 & 0x0FFF -> VPS ? */

		break;

	default:
	}

	return FALSE;
}

/* Test only */

static const char *month_names[] = {
	"0?", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug",
	"Sep", "Oct", "Nov", "Dec", "13?", "14?", "15?"
};

static const char *pcs_names[] = {
	"unknown", "mono", "stereo", "bilingual"
};

#define PIL(day, mon, hour, min) \
	(((day) << 15) + ((mon) << 11) + ((hour) << 6) + ((min) << 0))

static void
dump_pil(int pil)
{
	int day, mon, hour, min;

	day = pil >> 15;
	mon = (pil >> 11) & 0xF;
	hour = (pil >> 6) & 0x1F;
	min = pil & 0x3F;

	if (pil == PIL(0, 15, 31, 63))
		printf("... PDC: Timer-control (no PDC)\n");
	else if (pil == PIL(0, 15, 30, 63))
		printf("... PDC: Recording inhibit/terminate\n");
	else if (pil == PIL(0, 15, 29, 63))
		printf("... PDC: Interruption\n");
	else if (pil == PIL(0, 15, 28, 63))
		printf("... PDC: Continue\n");
	else if (pil == PIL(31, 15, 31, 63))
		printf("... PDC: No time\n");
	else
		printf("... PDC: %05x, %2d %s %02d:%02d\n",
			pil, day, month_names[mon], hour, min);
}

static void
dump_pty(int pty)
{
	if (pty == 0xFF)
		printf("... prog. type: %02x unused", pty);
	else
		printf("... prog. type: %02x class %s", pty, program_class[pty >> 4]);

	if (pty < 0x80) {
		if (program_type[pty >> 4][pty & 0xF])
			printf(", type %s", program_type[pty >> 4][pty & 0xF]);
		else
			printf(", type undefined");
	}

	putchar('\n');
}

static void
parse_x26_pdc(int address, int mode, int data)
{
	static int day, month, lto, caf, duration;
	static int hour[2], min[2], mi = 0;
	cni c;

	if (mode != 6 && address < 40)
		return;

	switch (mode) {
	case 6:
		if (address >= 40)
			return;
		min[mi] = data & 0x7F; // BCD
		break;

	case 8:
		c.type = CNI_X26;
		c.code = address * 256 + data;

		if (0) { /* country and network identifier */
			char *country, *short_name, *long_name;

			if (station_lookup(&c, &country, &short_name, &long_name))
				printf("X/26 country: %s\n... station: %s\n", country, long_name);
			else
				printf("X/26 unknown CNI %04x\n", c.code);
		}

		return;

	case 9:
		month = address & 15;
		day = (data >> 4) * 10 + (data & 15);
		break;

	case 10:
		hour[0] = data & 0x3F; // BCD
		caf = !!(data & 0x40);
		mi = 0;
		break;

	case 11:
		hour[1] = data & 0x3F; // BCD
		duration = !!(data & 0x40);
		mi = 1;
		break;

	case 12:
		lto = (data & 0x40) ? ((~0x7F) | data) : data;
		break;

	case 13:
		if (0) {
			printf("X/26 pty series: %d\n", address == 0x30);
			dump_pty(data | 0x80);
		}
		break;

	default:
		return;
	}

	/*
	 *  It's life, but not as we know it...
	 */
	if (0 && mode == 6)
		printf("X/26 %2d/%2d/%2d; lto=%d, caf=%d, end/dur=%d; %d %s %02x:%02x %02x:%02x\n",
			address, mode, data,
			lto, caf, duration,
			day, month_names[month],
			hour[0], min[0], hour[1], min[1]);
}

static bool
parse_bsd(u8 *raw, int packet, int designation)
{
	int err, i;

	switch (packet) {
	case 26:
		/* TODO, iff */
		break;

	case 30:
		if (designation >= 4)
			break;

		if (designation <= 1) {
			cni c;

//			printf("\nPacket 8/30/%d:\n", designation);

			c.type = CNI_8301;
			c.code = bit_reverse[raw[7]] * 256 + bit_reverse[raw[8]];

			if (0) { /* country and network identifier */
				char *country, *short_name, *long_name;

				if (station_lookup(&c, &country, &short_name, &long_name))
					printf("... country: %s\n... station: %s\n", country, long_name);
				else
					printf("... unknown CNI %04x\n", c.code);
			}

			if (0) { /* local time */
				int lto, mjd, utc_h, utc_m, utc_s;
				struct tm tm;
				time_t ti;

				lto = (raw[9] & 0x7F) >> 1;

				mjd = + ((raw[10] & 15) - 1) * 10000
				      + ((raw[11] >> 4) - 1) * 1000
				      + ((raw[11] & 15) - 1) * 100
				      + ((raw[12] >> 4) - 1) * 10
				      + ((raw[12] & 15) - 1);

			    	utc_h = ((raw[13] >> 4) - 1) * 10 + ((raw[13] & 15) - 1);
				utc_m = ((raw[14] >> 4) - 1) * 10 + ((raw[14] & 15) - 1);
				utc_s = ((raw[15] >> 4) - 1) * 10 + ((raw[15] & 15) - 1);

				ti = (mjd - 40587) * 86400 + 43200;
				localtime_r(&ti, &tm);

				printf("... local time: MJD %d %02d %s %04d, UTC %02d:%02d:%02d %c%02d%02d\n",
					mjd, tm.tm_mday, month_names[tm.tm_mon + 1], tm.tm_year + 1900,
					utc_h, utc_m, utc_s, (raw[9] & 0x80) ? '-' : '+', lto >> 1, (lto & 1) * 30);
			}
		} else /* if (designation <= 3) */ {
			int t, n[7], pil, pty, pcs;
			cni c;

//			printf("\nPacket 8/30/%d:\n", designation);

			for (err = i = 0; i < 7; i++) {
				err |= t = hamm16a(raw + i * 2 + 6);
				n[i] = bit_reverse[t];
			}

			if (err < 0)
				return FALSE;

			c.type = CNI_8302;
			c.code = + ((n[4] & 0x03) << 10)
				 + ((n[5] & 0xC0) << 2)
				 + (n[2] & 0xC0)
				 + (n[5] & 0x3F)
				 + ((n[1] & 0x0F) << 12);

			if (c.code == 0x0DC3)
				c.code = (n[2] & 0x10) ? 0x0DC2 : 0x0DC1;

			pcs = n[1] >> 6;
			pil = ((n[2] & 0x3F) << 14) + (n[3] << 6) + (n[4] >> 2);
			pty = n[6];

			if (0) { /* country and network identifier */
				char *country, *short_name, *long_name;

				if (station_lookup(&c, &country, &short_name, &long_name))
					printf("... country: %s\n... station: %s\n", country, long_name);
				else
					printf("... unknown CNI %04x\n", c.code);
			}

			if (0) { /* PDC data */
				int lci, luf, prf, mi;

				lci = (n[0] >> 2) & 3;
				luf = !!(n[0] & 2);
				prf = n[0] & 1;
				mi = !!(n[1] & 0x20);

				printf("... label channel %d: update %d,"
				       " prepare to record %d, mode %d\n",
					lci, luf, prf, mi);
				dump_pil(pil);
			}

			if (0) {
				printf("... analog audio: %s\n", pcs_names[pcs]);
				dump_pty(pty);
			}
		}

		/*
		 *  "transmission status message, e.g. the programme title",
		 *  "default G0 set". Render like subtitles a la TV or lang.c
		 *  translation to latin etc?
		 */
		if (0) { 
			printf("... status: \"");

			for (i = 20; i < 40; i++) {
				int c = parity(raw[i]);

				c = (c < 0) ? '?' : printable(c);
				putchar(c);
			}

			printf("\"\n");
		}

		return TRUE;
	}

	return TRUE;
}













// process one videotext packet

/* XXX!!! p read-only */


static bool
vt_packet(struct vbi *vbi, u8 *p)
{
	struct vt_page *cvtp;
	struct raw_page *rvtp;
	magazine *mag;
	int mag0, mag8;
	int packet, designation;
	int i, j, err = 0;

	if ((packet = hamm16a(p)) < 0)
		return FALSE;

	mag0 = packet & 7;
	mag8 = mag0 ? : 8;
	mag = vbi->magazine + mag8;
	packet >>= 3;

	rvtp = vbi->raw_page + mag0;
	cvtp = rvtp->page;

	p += 2;

	switch (packet) {
	case 0:
	{
		int page, subpage, flags;
		struct raw_page *curr;
		struct vt_page *vtp;

		if ((page = hamm16a(p)) < 0) {
			out_of_sync(vbi);
			return FALSE;
		}

		if ((curr = vbi->current)) {
			vtp = curr->page;

			if (!(vtp->flags & C11_MAGAZINE_SERIAL)) {
				curr = rvtp;
				vtp = curr->page;
			}

			if (((vtp->pgno ^ page) & 0xFF)
			    && vtp->function != PAGE_FUNCTION_DISCARD) {
				vbi_send_page(vbi, curr);

				/* objective: format the page only once */
				if (0 /* rolling header requested */
				    && vtp->function == PAGE_FUNCTION_LOP
				    && !(vtp->flags & C9_INTERRUPTED)
				    && (vtp->pgno <= 0x199 || (vtp->flags & C11_MAGAZINE_SERIAL)))
				{
					if (0) {
						for (i = 0; i < 40; i++)
							putchar(printable(vtp->data.unknown.raw[0][i]));
						putchar('\r');
						fflush(stdout);
					}
				}

				vtp->function = PAGE_FUNCTION_DISCARD;
			}
		}

		vbi->current = rvtp;
		cvtp->pgno = mag8 * 256 + page;

		subpage = hamm16a(p + 2) + hamm16a(p + 4) * 256;
		flags = hamm16a(p + 6);

		if (page == 0xFF || (subpage | flags) < 0) {
			cvtp->function = PAGE_FUNCTION_DISCARD;
			return FALSE;
		}

		cvtp->subno = subpage & 0x3F7F;
		cvtp->national = bit_reverse[flags] & 7;
		cvtp->flags = (flags << 16) + subpage;

		if (0 && ((page & 15) > 9 || page > 0x99))
			printf("data page %03x/%04x\n", cvtp->pgno, cvtp->subno);

		if (!(cvtp->flags & C4_ERASE_PAGE)
		    && (vtp = vbi->cache->op->get(vbi->cache, cvtp->pgno, cvtp->subno, 0xFFFF)))
		{
			memcpy(&cvtp->data, &vtp->data, sizeof(cvtp->data));
			/* XXX write cache directly | erc?*/
			/* XXX data page update */

			cvtp->function = vtp->function;

			switch (cvtp->function) {
			case PAGE_FUNCTION_UNKNOWN:
			case PAGE_FUNCTION_LOP:
				memcpy(cvtp->data.unknown.raw[0], p, 40);

			default:
			}

			cvtp->lop_lines = vtp->lop_lines;
			cvtp->enh_lines = vtp->enh_lines;
		} else {
			cvtp->flags |= C4_ERASE_PAGE;

			if (0)
				printf("rebuilding %3x/%04x from scratch\n", cvtp->pgno, cvtp->subno);

			if (cvtp->pgno == 0x1F0) {
				cvtp->function = PAGE_FUNCTION_BTT;
				vbi->page_info[0x1F0 - 0x100].mip = MIP_TOP_PAGE;
			} else if (page == 0xFD) {
				cvtp->function = PAGE_FUNCTION_MIP;
				vbi->page_info[(mag8 * 256) + 0xFD - 0x100].mip = MIP_SYSTEM_PAGE;
			} else if (page == 0xFE) {
				cvtp->function = PAGE_FUNCTION_MOT;
				vbi->page_info[(mag8 * 256) + 0xFE - 0x100].mip = MIP_SYSTEM_PAGE;
			} else {
				cvtp->function = PAGE_FUNCTION_UNKNOWN;

				memcpy(cvtp->data.unknown.raw[0] + 0, p, 40);
				memset(cvtp->data.unknown.raw[0] + 40, 0x20, sizeof(cvtp->data.unknown.raw) - 40);
				memset(cvtp->data.unknown.link, 0xFF, sizeof(cvtp->data.unknown.link));
				memset(cvtp->data.unknown.triplet, 0xFF, sizeof(cvtp->data.unknown.triplet));

				cvtp->data.unknown.flof = FALSE;
			}

			cvtp->lop_lines = 1;
			cvtp->enh_lines = 0;
		}

		if (cvtp->function == PAGE_FUNCTION_UNKNOWN) {
			page_function function = PAGE_FUNCTION_UNKNOWN;

			switch (vbi->page_info[cvtp->pgno - 0x100].mip) {
			case 0x01 ... 0x51:
			case 0x70 ... 0x7F:
			case 0x81 ... 0xD1:
			case 0xF4 ... 0xF7:
				function = PAGE_FUNCTION_LOP;
				break;

			case MIP_SYSTEM_PAGE:	/* no MOT or MIP?? */
				/* remains function = PAGE_FUNCTION_UNKNOWN; */
				break;

			case MIP_TOP_PAGE:
				for (i = 0; i < 8; i++)
					if (cvtp->pgno == vbi->btt_link[i].pgno)
						break;
				if (i < 8) {
					switch (vbi->btt_link[i].type) {
					case 1:
						function = PAGE_FUNCTION_MPT;
						break;

					case 2:
						function = PAGE_FUNCTION_AIT;
						break;

					default:
						if (0)
							printf("page is TOP, link %d, unknown type %d\n",
								i, vbi->btt_link[i].type);
					}
				} else if (0)
					printf("page claims to be TOP, link not found\n");

				break;

			case 0xE5:
			case 0xE8 ... 0xEB:
				function = PAGE_FUNCTION_DRCS;
				break;

			case 0xE6:
			case 0xEC ... 0xEF:
				function = PAGE_FUNCTION_POP;
				break;

			case 0x52 ... 0x6F:	/* reserved */
			case 0xE3:		/* EPG/NexTView transport layer */
			case 0xFD:		/* ACI page */
			case 0x80:		/* closed service */
			case 0xD2 ... 0xDF:	/* reserved */
			case 0xE0 ... 0xE2:	/* data broadcasting */
			case 0xE4:		/* data broadcasting */
			case 0xF0 ... 0xF3:	/* broadcaster system page */
			case 0xFA ... 0xFC:	/* reserved */
				function = PAGE_FUNCTION_DISCARD;
				break;

			default:
				if (page <= 0x99 && (page & 15) <= 9)
					function = PAGE_FUNCTION_LOP;
				/* else remains
					function = PAGE_FUNCTION_UNKNOWN; */
			}

			if (function != PAGE_FUNCTION_UNKNOWN) {
				convert_page(vbi, cvtp, function);
			}
		}

		rvtp->extension.designations = 0;
		rvtp->num_triplets = 0;
	cvtp->vbi = vbi; // temporary

		return TRUE;
	}

	case 1 ... 25:
		switch (cvtp->function) {
		case PAGE_FUNCTION_DISCARD:
			return TRUE;

		case PAGE_FUNCTION_MOT:
			if (!parse_mot(vbi->magazine + mag8, p, packet))
				return FALSE;
			break;

		case PAGE_FUNCTION_MIP:
			if (!parse_mip(vbi, mag8, p, packet))
				return FALSE;
			break;

		case PAGE_FUNCTION_GPOP:
		case PAGE_FUNCTION_POP:
			if (!parse_pop(cvtp, p, packet))
				return FALSE;
			break;

		case PAGE_FUNCTION_GDRCS:
		case PAGE_FUNCTION_DRCS:
			memcpy(cvtp->data.drcs.raw[packet], p, 40);
			break;

		case PAGE_FUNCTION_BTT:
			if (!parse_btt(vbi, p, packet))
				return FALSE;
			break;

		case PAGE_FUNCTION_AIT:
			if (!(parse_ait(cvtp, p, packet)))
				return FALSE;
			break;

		case PAGE_FUNCTION_MPT:
			if (!(parse_mpt(vbi, p, packet)))
				return FALSE;
			break;

		default:
			memcpy(cvtp->data.unknown.raw[packet], p, 40);
			break;
		}

		cvtp->lop_lines |= 1 << packet;

		return TRUE;

	case 26:
	{
		int designation;
		vt_triplet triplet;

		switch (cvtp->function) {
		case PAGE_FUNCTION_DISCARD:
			return TRUE;

		case PAGE_FUNCTION_GPOP:
		case PAGE_FUNCTION_POP:
			return parse_pop(cvtp, p, packet);

		case PAGE_FUNCTION_GDRCS:
		case PAGE_FUNCTION_DRCS:
		case PAGE_FUNCTION_BTT:
		case PAGE_FUNCTION_AIT:
		case PAGE_FUNCTION_MPT:
			/* X/26 ? */
			return TRUE;

		default:
		}

		if ((designation = hamm8a[p[0]]) < 0)
			return FALSE;

		if (rvtp->num_triplets >= 16 * 13
		    || rvtp->num_triplets != designation * 13) {
			rvtp->num_triplets = -1;
			return FALSE;
		}

		for (p++, i = 0; i < 13; p += 3, i++) {
			int t = hamm24(p, &err);

			triplet.address = t & 0x3F;
			triplet.mode = (t >> 6) & 0x1F;
			triplet.data = t >> 11;
			triplet.stop = !((~t) & 0x7FF);

// XXX if !err
			cvtp->data.unknown.triplet[rvtp->num_triplets++] = triplet;

			if (0) /* ATTN: mind PDC transmission order */
				if (triplet.mode >= 6 && triplet.mode <= 13)
					parse_x26_pdc(triplet.address, triplet.mode, triplet.data);
		}

		cvtp->enh_lines |= 1 << designation;

		return TRUE;
	}

	case 27:
	{
		int designation, control, crc;

		if (cvtp->function == PAGE_FUNCTION_DISCARD)
			return TRUE;

		if ((designation = hamm8a[p[0]]) < 0)
			return 4;

printf("X/27/%d page %x\n", designation, cvtp->pgno);

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
				    + designation * 6 + i, p, mag0))
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
					((((t1 >> 12) & 0x7) ^ mag0) ? : 8) * 256
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
		if (cvtp->function == PAGE_FUNCTION_DISCARD)
			return TRUE;

		/* fall through */

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

//	printf("packet %d/%d/%d page %3x\n", mag8, packet, designation, cvtp->pgno);

		for (p++, i = 0; i < 13; p += 3, i++)
			triplet[i] = hamm24(p, &err);

		switch (designation) {
		case 0: /* X/28/0, M/29/0 Level 2.5 */
		case 4: /* X/28/4, M/29/4 Level 3.5 */
			if (err & 0xf000)
				return 4;

			function = bits(4);
			bits(3); /* page coding ignored */

//			printf("... function %d\n", function);

			/*
			 *  ZDF and BR3 transmit GPOP 1EE/.. with 1/28/0 function
			 *  0 = PAGE_FUNCTION_LOP, should be PAGE_FUNCTION_GPOP.
			 *  Makes no sense to me. Update: also encountered pages
			 *  mFE and mFF with function = 0. Strange. 
			 */
			if (function != PAGE_FUNCTION_LOP && packet == 28) {
				if (cvtp->function != PAGE_FUNCTION_UNKNOWN
				    && cvtp->function != function)
					return 0; /* XXX discard rpage? */

// XXX rethink				cvtp->function = function;
			}

			if (function != PAGE_FUNCTION_LOP)
				return 0;

			/* XXX X/28/0 Format 2, distinguish how? */

			ext = &vbi->magazine[mag8].extension;

			if (packet == 28) {
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

			if (packet == 29) {
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

			if (packet == 28) {
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
			if (packet == 29)
				break; /* M/29/3 undefined */

			if (err & 0xf000)
				return 4;

			function = bits(4);
			bits(3); /* page coding ignored */

//		printf("... function %d\n", function);

			if (function != PAGE_FUNCTION_GDRCS
			    || function != PAGE_FUNCTION_DRCS)
				return 0;

			if (cvtp->function == PAGE_FUNCTION_UNKNOWN) {
				memmove(cvtp->data.drcs.raw, cvtp->data.unknown.raw, sizeof(cvtp->data.drcs.raw));
				memset(cvtp->data.drcs.bits, 0, sizeof(cvtp->data.drcs.bits));

				bits(11);

				for (i = 0; i < 48; i++)
					cvtp->data.drcs.mode[i] = bits(4);

				cvtp->function = function;
			} else if (cvtp->function == function) {
				bits(11);

				for (i = 0; i < 48; i++)
					cvtp->data.drcs.mode[i] = bits(4);
			} // else discard?

			return 0;
		}

		return 0;
	}

	case 30:
		if (mag8 != 8)
			break;

		if ((designation = hamm8a[*p]) < 0)
			return FALSE;

		if (designation > 4)
			break;

		if (!hamm8_page_number(&vbi->initial_page, p + 1, 0))
			return FALSE;

		if ((vbi->initial_page.pgno & 0xFF) == 0xFF) {
			vbi->initial_page.pgno = 0x100;
			vbi->initial_page.subno = ANY_SUB;
		}

		return parse_bsd(p, packet, designation);

	default:
		break;
	}

	return TRUE;
}

/* Quick Hack(tm) to read from a sample stream */

//static char *sample_file = "libvbi/samples/t2-br";
static char *sample_file = NULL; // disabled
static FILE *sample_fd;

void
sample_beta(struct vbi *vbi)
{
	unsigned char wst[42];
	char buf[256];
//	double dt;
	int index, line;
	int items;
	int i;

	if (feof(sample_fd)) {
		rewind(sample_fd);
		printf("Rewind sample stream\n");
	}

	{
		fgets(buf, 255, sample_fd);

		/* usually 0.04 (1/25) */
//		dt = strtod(buf, NULL);

		items = fgetc(sample_fd);

//		printf("%8.6f %d:\n", dt, items);

		for (i = 0; i < items; i++) {
			index = fgetc(sample_fd);
			line = fgetc(sample_fd);
			line += 256 * fgetc(sample_fd);

			if (index != 0) {
				printf("Oops! Confusion in vbi.c/sample_beta()\n");
				// index: 0 == Teletext
				exit(EXIT_FAILURE);
			}

			fread(wst, 1, 42, sample_fd);

			vt_packet(vbi, wst);
		}
	}
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

    if (sample_fd) {
	    sample_beta(vbi);
	    return;
    }

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
    
    

    if (not(vbi = calloc(1, sizeof(*vbi)))) // must clear for reset_magazines
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
    out_of_sync(vbi);
    reset_magazines(vbi);

    if (sample_file)
	if (!(sample_fd = fopen(sample_file, "r")))
	    printf("Cannot open %s: %s\n", sample_file, strerror(errno));

    return vbi;

fail2:
    free(vbi);
fail1:
    return 0;
}

void
vbi_close(struct vbi *vbi)
{
    reset_magazines(vbi);

    if (vbi->cache)
	vbi->cache->op->close(vbi->cache);

    close_vbi_v4lx(vbi->fifo);

    if (sample_fd)
	fclose(sample_fd);
    sample_fd = NULL;

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
