#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "libvbi.h"
#include "vt.h"
#include "export.h"
#include "vbi.h"
#include "hamm.h"
#include "lang.h"
#include "../common/types.h"
#include "../common/math.h"


#define DEBUG 0

#if DEBUG
#define printable(c) ((((c) & 0x7F) < 0x20 || ((c) & 0x7F) > 0x7E) ? '.' : ((c) & 0x7F))
#define printv printf
#else
#define printv(templ, args...)
#endif

/*
    XXX t2-br p. 490 
    XXX 3sat p. 888
 */

#define ROWS			25
#define COLUMNS			40
#define LAST_ROW		((ROWS - 1) * COLUMNS)

/*
 *  FLOF navigation
 */

static const attr_colours
flof_link_col[4] = { RED, GREEN, YELLOW, CYAN };

static inline void
flof_navigation_bar(struct fmt_page *pg, struct vt_page *vtp)
{
	attr_char ac;
	int n, i, k, ii;

	memset(&ac, 0, sizeof(ac));

	ac.foreground	= WHITE;
	ac.background	= BLACK;
	ac.opacity	= pg->page_opacity[1];
	ac.glyph	= GL_SPACE;

	for (i = 0; i < COLUMNS; i++)
		pg->text[LAST_ROW + i] = ac;

	ac.link = TRUE;

	for (i = 0; i < 4; i++) {
		ii = i * 10 + 3;

		for (k = 0; k < 3; k++) {
			n = ((vtp->data.lop.link[i].pgno >> ((2 - k) * 4)) & 15) + '0';

			if (n > '9')
				n += 'A' - '9';

			ac.glyph = n;
			ac.foreground = flof_link_col[i];
			pg->text[LAST_ROW + ii + k] = ac;
			pg->nav_index[ii + k] = i;
		}

		pg->nav_link[i].pgno = vtp->data.lop.link[i].pgno;
		pg->nav_link[i].subno = vtp->data.lop.link[i].subno;
	}
}

static inline void
flof_links(struct fmt_page *pg, struct vt_page *vtp)
{
	attr_char *acp = pg->text + LAST_ROW;
	int i, j, k, col = -1, start = 0;

	for (i = 0; i < COLUMNS + 1; i++) {
		if (i == COLUMNS || (acp[i].foreground & 7) != col) {
			for (k = 0; k < 4; k++)
				if (flof_link_col[k] == col)
					break;

			if (k < 4 && !NO_PAGE(vtp->data.lop.link[k].pgno)) {
				/* Leading and trailing spaces not sensitive */

				for (j = i - 1; j >= start && acp[j].glyph == GL_SPACE; j--);

				for (; j >= start; j--) {
					acp[j].link = TRUE;
					pg->nav_index[j] = k;
				}

		    		pg->nav_link[k].pgno = vtp->data.lop.link[k].pgno;
		    		pg->nav_link[k].subno = vtp->data.lop.link[k].subno;
			}

			if (i >= COLUMNS)
				break;

			col = acp[i].foreground & 7;
			start = i;
		}

		if (start == i && acp[i].glyph == GL_SPACE)
			start++;
	}
}

/*
 *  TOP navigation
 */

static bool
top_label(struct vbi *vbi, struct fmt_page *pg, font_descriptor *font,
	  int index, int pgno, int foreground, int ff)
{
	int column = index * 13 + 1;
	struct vt_page *vtp;
	attr_char *acp;
	ait_entry *ait;
	int i, j;

	acp = &pg->text[LAST_ROW + column];

	for (i = 0; i < 8; i++)
		if (vbi->vt.btt_link[i].type == 2) {
			vtp = vbi->cache->op->get(vbi->cache,
				vbi->vt.btt_link[i].pgno, vbi->vt.btt_link[i].subno, 0x3f7f);

			if (!vtp) {
				printv("top ait page %x not cached\n", vbi->btt_link[i].pgno);
				continue;
			} else if (vtp->function != PAGE_FUNCTION_AIT) {
				printv("no ait page %x\n", vtp->pgno);
				continue;
			}

			for (ait = vtp->data.ait, j = 0; j < 46; ait++, j++)
				if (ait->page.pgno == pgno) {
					pg->nav_link[index].pgno = pgno;
					pg->nav_link[index].subno = ANY_SUB;

					for (i = 11; i >= 0; i--)
						if (ait->text[i] > 0x20)
							break;

					if (ff && (i <= (11 - ff))) {
						acp += (11 - ff - i) >> 1;
						column += (11 - ff - i) >> 1;

						acp[i + 1].link = TRUE;
						pg->nav_index[column + i + 1] = index;

						acp[i + 2].glyph = 0x003E;
						acp[i + 2].foreground = foreground;
						acp[i + 2].link = TRUE;
						pg->nav_index[column + i + 2] = index;

						if (ff > 1) {
							acp[i + 3].glyph = 0x003E;
							acp[i + 3].foreground = foreground;
							acp[i + 3].link = TRUE;
							pg->nav_index[column + i + 3] = index;
						}
					} else {
						acp += (11 - i) >> 1;
						column += (11 - i) >> 1;
					}

					for (; i >= 0; i--) {
						acp[i].glyph = glyph_lookup(font->G0, font->subset,
							(ait->text[i] < 0x20) ? 0x20 : ait->text[i]);
						acp[i].foreground = foreground;
						acp[i].link = TRUE;
						pg->nav_index[column + i] = index;
					}

					return TRUE;
				}
		}

	return FALSE;
}

static inline void
top_navigation_bar(struct vbi *vbi, struct fmt_page *pg,
	struct vt_page *vtp)
{
	attr_char ac;
	int i, got;

	printv("PAGE BTT: %d\n", vbi->page_info[vtp->pgno - 0x100].btt);

	memset(&ac, 0, sizeof(ac));

	ac.foreground	= 32 + WHITE;
	ac.background	= 32 + BLACK;
	ac.opacity	= pg->page_opacity[1];
	ac.glyph	= GL_SPACE;

	for (i = 0; i < COLUMNS; i++)
		pg->text[LAST_ROW + i] = ac;

	if (pg->page_opacity[1] != OPAQUE)
		return;

	if (0 && DEBUG)
		for (i = 0x100; i < 0x8FF; i++) {
			printv("%x ", vbi->page_info[i - 0x100].btt & 15);
			if ((i & 0x3F) == 0x3F) putchar('\n');
		}

//	top_label(vbi, pg, pg->font[0], 0, vtp->pgno, RED, FALSE);

	switch (vbi->vt.page_info[vtp->pgno - 0x100].btt) {
	case 1: // subtitles?
	default:
		break;

	case 4 ... 5:
	case 6 ... 7:
	case 8:
	case 10:
		for (i = vtp->pgno; i != vtp->pgno + 1; i = (i == 0) ? 0x89a : i - 1)
			if (vbi->vt.page_info[i - 0x100].btt >= 4 && vbi->vt.page_info[i - 0x100].btt <= 7) {
				top_label(vbi, pg, pg->font[0], 0, i, 32 + WHITE, 0);
				break;
			}

		for (i = vtp->pgno + 1, got = FALSE; i != vtp->pgno; i = (i == 0x899) ? 0x100 : i + 1)
			switch (vbi->vt.page_info[i - 0x100].btt) {
			case 4 ... 5:
				top_label(vbi, pg, pg->font[0], 2, i, 32 + YELLOW, 2);
				return;

			case 6 ... 7:
				if (!got) {
					top_label(vbi, pg, pg->font[0], 1, i, 32 + GREEN, 1);
					got = TRUE;
				}

				break;
			}
	}
}

static ait_entry *
next_ait(struct vbi *vbi, int pgno, int subno)
{
	struct vt_page *vtp;
	ait_entry *ait, *mait = NULL;
	int mpgno = 0xFFF, msubno = 0xFFFF;
	int i, j;

	for (i = 0; i < 8; i++) {
		if (vbi->vt.btt_link[i].type == 2) {
			vtp = vbi->cache->op->get(vbi->cache,
				vbi->vt.btt_link[i].pgno, vbi->vt.btt_link[i].subno, 0x3f7f);

			if (!vtp) {
				printv("top ait page %x not cached\n", vbi->btt_link[i].pgno);
				continue;
			} else if (vtp->function != PAGE_FUNCTION_AIT) {
				printv("no ait page %x\n", vtp->pgno);
				continue;
			}

			for (ait = vtp->data.ait, j = 0; j < 46; ait++, j++) {
				if (!ait->page.pgno)
					break;

				if (ait->page.pgno < pgno
				    || (ait->page.pgno == pgno && ait->page.subno <= subno))
					continue;

				if (ait->page.pgno > mpgno
				    || (ait->page.pgno == mpgno && ait->page.subno > msubno))
					continue;

				mait = ait;
				mpgno = ait->page.pgno;
				msubno = ait->page.subno;
			}
		}
	}

	return mait;
}

static int
top_index(struct vbi *vbi, struct fmt_page *pg, int subno)
{
	attr_char ac, *acp;
	ait_entry *ait;
	int i, j, k, n, lines;
	int xpgno, xsubno;

	memset(&ac, 0, sizeof(ac));

	ac.foreground	= 32 + BLACK;
	ac.background	= 32 + BLUE;
	ac.glyph	= GL_SPACE;

	for (i = 0; i < COLUMNS * ROWS; i++)
		pg->text[i] = ac;

	ac.size = DOUBLE_SIZE;

	for (i = 0; i < 5; i++) {
		// this asks for i18n and some2glyph :-P
		ac.glyph = "INDEX"[i];
		pg->text[1 * COLUMNS + 2 + i * 2] = ac;
	}

	ac.size = NORMAL;

	acp = &pg->text[4 * COLUMNS];
	lines = 17;
	xpgno = 0;
	xsubno = 0;

	while ((ait = next_ait(vbi, xpgno, xsubno))) {
		xpgno = ait->page.pgno;
		xsubno = ait->page.subno;

		if (subno > 0) {
			if (lines-- == 0) {
				subno--;
				lines = 17;
			}

			continue;
		} else if (lines-- <= 0)
			continue;

		for (i = 11; i >= 0; i--)
			if (ait->text[i] > 0x20)
				break;

		switch (vbi->vt.page_info[ait->page.pgno - 0x100].btt) {
		case 6 ... 7:
			k = 3;
			break;

		default:
    			k = 1;
		}

		for (j = 0; j <= i; j++) { // XXX font
			acp[k + j].glyph = glyph_lookup(pg->font[0]->G0,
				pg->font[0]->subset,
				(ait->text[j] < 0x20) ? 0x20 : ait->text[j]);
		}

		for (k += i + 2; k <= 34; k++)
			acp[k].glyph = '.';

		for (j = 0; j < 3; j++) {
			n = ((ait->page.pgno >> ((2 - j) * 4)) & 15) + '0';

			if (n > '9')
				n += 'A' - '9';

			acp[j + 35].glyph = n;
 		}

		acp += COLUMNS;
	}

	return 1;
}

/*
 *  Zapzilla navigation
 */

static int
keyword(vbi_link *ld, unsigned char *p, int column,
	int pgno, int subno, int *back)
{
	unsigned char *s = p + column;
	int i, j, k, l;

	ld->type = VBI_LINK_NONE;
	ld->pgno = 0;
	ld->subno = ANY_SUB;
	ld->text[0] = 0;
	*back = 0;

	if (isdigit(*s)) {
		for (i = 0; isdigit(s[i]); i++)
			ld->pgno = ld->pgno * 16 + (s[i] & 15);

		if (isdigit(s[-1]) || i > 3)
			return i;

		if (i == 3) {
			if (ld->pgno >= 0x100 && ld->pgno <= 0x899)
				ld->type = VBI_LINK_PAGE;

			return i;
		}

		if (s[i] != '/' && s[i] != ':')
			return i;

		s += i += 1;

		for (ld->subno = j = 0; isdigit(s[j]); j++)
			ld->subno = ld->subno * 16 + (s[j] & 15);

		if (j > 1 || subno != ld->pgno || ld->subno > 0x99)
			return i + j;

		if (ld->pgno == ld->subno)
			ld->subno = 0x01;
		else
			ld->subno = add_bcd(ld->pgno, 0x01);

		ld->type = VBI_LINK_SUBPAGE;
		ld->pgno = pgno;

		return i + j;
	} else if (!strncasecmp(s, "https://", i = 8)) {
		ld->type = VBI_LINK_HTTP;
	} else if (!strncasecmp(s, "http://", i = 7)) {
		ld->type = VBI_LINK_HTTP;
	} else if (!strncasecmp(s, "www.", i = 4)) {
		ld->type = VBI_LINK_HTTP;
		strcpy(ld->text, "http://");
	} else if (!strncasecmp(s, "ftp://", i = 6)) {
		ld->type = VBI_LINK_FTP;
	/*
	 *  A few German networks invented this format since Latin1/German
	 *  replaced the at sign with a paragraph sign and they apparently
	 *  can't afford a level 1.5 generator.
	 */
	} else if (*s == '@' || *s == 167) {
		ld->type = VBI_LINK_EMAIL;
		strcpy(ld->text, "mailto:");
		*s = '@';
		i = 1;
	} else
		return 1;

	for (j = k = l = 0;;) {
		// RFC 1738
		while (isalnum(s[i + j]) || strchr("%&/=?+-~:;@", s[i + j])) {
			j++;
			l++;
		}

		if (s[i + j] == '.') {
			if (l < 1)
				return i;		
			l = 0;
			j++;
			k++;
		} else
			break;
	}

	if (k < 1 || l < 1) {
		ld->type = VBI_LINK_NONE;
		return i;
	}

	k = 0;

	if (ld->type == VBI_LINK_EMAIL) {
		for (; isalnum(s[k - 1]) || strchr("-~._", s[k - 1]); k--);

		if (k == 0) {
			ld->type = VBI_LINK_NONE;
			return i;
		}

		*back = k;
	}

	strncat(ld->text, s + k, i + j - k);

	return i + j;
}

static inline void
zap_links(struct fmt_page *pg, int row)
{
	unsigned char buffer[43]; /* One row, two spaces on the sides and NUL */
	vbi_link ld;
	attr_char *acp;
	bool link[43];
	int i, j, n, b;

	acp = &pg->text[row * COLUMNS];

	for (i = j = 0; i < COLUMNS; i++) {
		if (acp[i].size == OVER_TOP || acp[i].size == OVER_BOTTOM)
			continue;
		buffer[j + 1] = glyph2latin(acp[i].glyph);
		j++;
	}

	buffer[0] = ' '; 
	buffer[j + 1] = ' ';
	buffer[j + 2] = 0;

	for (i = 0; i < COLUMNS; i += n) { 
		n = keyword(&ld, buffer, i + 1,
			pg->pgno, pg->subno, &b);

		for (j = b; j < n; j++)
			link[i + j] = (ld.type != VBI_LINK_NONE);
	}

	for (i = j = 0; i < COLUMNS; i++) {
		acp[i].link = link[j];

		if (acp[i].size == OVER_TOP || acp[i].size == OVER_BOTTOM)
			continue;
		j++;
	}
}

void
vbi_resolve_link(struct fmt_page *pg, int column, int row, vbi_link *ld)
{
	unsigned char buffer[43];
	attr_char *acp;
	int i, j, b;

	assert(column >= 0 && column < COLUMNS);

	acp = &pg->text[row * COLUMNS];

	if (row == (ROWS - 1) && acp[column].link) {
		i = pg->nav_index[column];

		ld->type = VBI_LINK_PAGE;
		ld->pgno = pg->nav_link[i].pgno;
		ld->subno = pg->nav_link[i].subno;

		return;
	}

	if (row < 1 || row > 23) {
		ld->type = VBI_LINK_NONE;
		return;
	}

	for (i = j = b = 0; i < COLUMNS; i++) {
		if (acp[i].size == OVER_TOP || acp[i].size == OVER_BOTTOM)
			continue;
		if (i < column && !acp[i].link)
			j = b = -1;

		buffer[j + 1] = glyph2latin(acp[i].glyph);

		if ((buffer[j + 1] == '@' ||
		     buffer[j + 1] == 167) && b <= 0)
			b = j;
		j++;
	}

	buffer[0] = ' ';
	buffer[j + 1] = ' ';
	buffer[j + 2] = 0;

	keyword(ld, buffer, 1, pg->pgno, pg->subno, &i);

	if (ld->type == VBI_LINK_NONE)
		keyword(ld, buffer, b + 1,
			pg->pgno, pg->subno, &i);
}

void
vbi_resolve_home(struct fmt_page *pg, vbi_link *ld)
{
	ld->type = VBI_LINK_PAGE;
	ld->pgno = pg->nav_link[5].pgno;
	ld->subno = pg->nav_link[5].subno;
}

/*
 *  Try find a page title for the bookmarks.
 *  TRUE if success, buf min 41 chars.
 *
 *  TODO: FLOF, TTX character set, buf character set (currently Latin-1 assumed).
 */
int
vbi_page_title(struct vbi *vbi, int pgno, int subno, char *buf)
{
	font_descriptor *font;
	struct vt_page *vtp;
	ait_entry *ait;
	int i, j;

	if (vbi->vt.top) {
		for (i = 0; i < 8; i++)
			if (vbi->vt.btt_link[i].type == 2) {
				vtp = vbi->cache->op->get(vbi->cache,
					vbi->vt.btt_link[i].pgno, vbi->vt.btt_link[i].subno, 0x3f7f);

				if (!vtp) {
					printv("p/t top ait page %x not cached\n", vbi->btt_link[i].pgno);
					continue;
				} else if (vtp->function != PAGE_FUNCTION_AIT) {
					printv("p/t no ait page %x\n", vtp->pgno);
					continue;
				}

				for (ait = vtp->data.ait, j = 0; j < 46; ait++, j++)
					if (ait->page.pgno == pgno) {
						font = font_descriptors + 0; // XXX

						for (i = 11; i >= 0; i--)
							if (ait->text[i] > 0x20)
								break;

						buf[i + 1] = 0;

						for (; i >= 0; i--)
							buf[i] = glyph2latin(glyph_lookup(
								font->G0, font->subset,
								(ait->text[i] < 0x20) ?
									0x20 : ait->text[i]));
						return TRUE;
					}
			}
	} else {
		/* find a FLOF link and the corresponding label */
	}

	return FALSE;
}

/* ------------------------------------------------------------------ */

static void
screen_colour(struct fmt_page *pg, struct vt_page *vtp, int colour)
{ 
	pg->screen_colour = colour;

	if (colour == TRANSPARENT_BLACK
	    || (vtp->flags & (C5_NEWSFLASH | C6_SUBTITLE)))
		pg->screen_opacity = TRANSPARENT_SPACE;
	else
		pg->screen_opacity = OPAQUE;
}

static vt_triplet *
resolve_obj_address(struct vbi *vbi, object_type type,
	int pgno, object_address address, page_function function)
{
	int s1, packet, pointer;
	struct vt_page *vtp;
	vt_triplet *trip;
	int i;

	s1 = address & 15;
	packet = ((address >> 7) & 3);
	i = ((address >> 5) & 3) * 3 + type;

	printv("obj invocation, source page %03x/%04x, "
		"pointer packet %d triplet %d\n", pgno, s1, packet + 1, i);

	vtp = vbi->cache->op->get(vbi->cache, pgno, s1, 0x000F);

	if (!vtp) {
		printv("... page not cached\n");
		return 0;
	}

	if (vtp->function == PAGE_FUNCTION_UNKNOWN) {
		if (!(vtp = vbi_convert_page(vbi, vtp, TRUE, function))) {
			printv("... no g/pop page or hamming error\n");
			return 0;
		}
	} else if (vtp->function == PAGE_FUNCTION_POP)
		vtp->function = function;
	else if (vtp->function != function) {
		printv("... source page wrong function %d, expected %d\n",
			vtp->function, function);
		return 0;
	}

	pointer = vtp->data.pop.pointer[packet * 24 + i * 2 + ((address >> 4) & 1)];

	printv("... triplet pointer %d\n", pointer);

	if (pointer > 506) {
		printv("... triplet pointer out of bounds (%d)\n", pointer);
		return 0;
	}

	if (DEBUG) {
		packet = (pointer / 13) + 3;

		if (packet <= 25)
			printv("... object start in packet %d, triplet %d (pointer %d)\n",
				packet, pointer % 13, pointer);
		else
			printv("... object start in packet 26/%d, triplet %d (pointer %d)\n",
				packet - 26, pointer % 13, pointer);	
	}

	trip = vtp->data.pop.triplet + pointer;

	printv("... obj def: ad 0x%02x mo 0x%04x dat %d=0x%x\n",
		trip->address, trip->mode, trip->data, trip->data);

	address ^= trip->address << 7;
	address ^= trip->data;

	if (trip->mode != (type + 0x14) || (address & 0x1FF)) {
		printv("... no object definition\n");
		return 0;
	}

	return trip + 1;
}

/* XXX todo: panels */

static bool
enhance(struct vbi *vbi, magazine *mag,	extension *ext,
	struct fmt_page *pg, struct vt_page *vtp,
	object_type type, vt_triplet *p,
	int inv_row, int inv_column)
{
	attr_char ac, mac, *acp;
	int active_column, active_row;
	int offset_column, offset_row;
	int row_colour, next_row_colour;
	font_descriptor *font;
	int invert;
	int drcs_s1[2];

	static void
	flush(int column)
	{
		int row = inv_row + active_row;
		int i;

		if (row >= ROWS)
			return;

		if (type == OBJ_TYPE_PASSIVE && !mac.glyph) {
			active_column = column;
			return;
		}

		printv("flush [%08x%c,%d%c,%d%c,%d%c,%d%c] %d ... %d\n",
			ac.glyph, mac.glyph ? '*' : ' ',
			ac.foreground, mac.foreground ? '*' : ' ',
			ac.background, mac.background ? '*' : ' ',
			ac.size, mac.size ? '*' : ' ',
			ac.flash, mac.flash ? '*' : ' ',
			active_column, column - 1);

		for (i = inv_column + active_column; i < inv_column + column;) {
			attr_char c;

			if (i > 39)
				break;

			c = acp[i];

			if (mac.underline) {
				int u = ac.underline;

				if (!mac.glyph)
					ac.glyph = c.glyph;

				if (gl_isg1(ac.glyph)) {
					if (u)
						ac.glyph |= 0x20; /* separated */
					else
						ac.glyph &= ~0x20; /* contiguous */
					mac.glyph = ~0;
					u = 0;
				}

				c.underline = u;
			}
			if (mac.foreground)
				c.foreground = (ac.foreground == TRANSPARENT_BLACK) ?
					row_colour : ac.foreground;
			if (mac.background)
				c.background = (ac.background == TRANSPARENT_BLACK) ?
					row_colour : ac.background;
			if (invert) {
				int t = c.foreground;

				c.foreground = c.background;
				c.background = t;
			}
			if (mac.opacity)
				c.opacity = ac.opacity;
			if (mac.flash)
				c.flash = ac.flash;
			if (mac.conceal)
				c.conceal = ac.conceal;
			if (mac.glyph) {
				c.glyph = ac.glyph;
				mac.glyph = 0;

				c.size = ac.size;
			}

			acp[i] = c;

			if (type == OBJ_TYPE_PASSIVE)
				break;

			i++;

			if (type != OBJ_TYPE_PASSIVE
			    && type != OBJ_TYPE_ADAPTIVE) {
				int raw;

				raw = (row == 0 && i < 9) ?
					0x20 : parity(vtp->data.lop.raw[row][i - 1]);

				/* set-after spacing attributes cancelling non-spacing */

				switch (raw) {
				case 0x00 ... 0x07:	/* alpha + foreground colour */
				case 0x10 ... 0x17:	/* mosaic + foreground colour */
					printv("... fg term %d %02x\n", i, raw);
					mac.foreground = 0;
					mac.conceal = 0;
					break;

				case 0x08:		/* flash */
					mac.flash = 0;
					break;

				case 0x0A:		/* end box */
				case 0x0B:		/* start box */
					if (i < COLUMNS && parity(vtp->data.lop.raw[row][i]) == raw) {
						printv("... boxed term %d %02x\n", i, raw);
						mac.opacity = 0;
					}

					break;

				case 0x0D:		/* double height */
				case 0x0E:		/* double width */
				case 0x0F:		/* double size */
					printv("... size term %d %02x\n", i, raw);
					mac.size = 0;
					break;
				}

				if (i > 39)
					break;

				raw = (row == 0 && i < 8) ?
					0x20 : parity(vtp->data.lop.raw[row][i]);

				/* set-at spacing attributes cancelling non-spacing */

				switch (raw) {
				case 0x09:		/* steady */
					mac.flash = 0;
					break;

				case 0x0C:		/* normal size */
					printv("... size term %d %02x\n", i, raw);
					mac.size = 0;
					break;

				case 0x18:		/* conceal */
					mac.conceal = 0;
					break;

					/*
					 *  Non-spacing underlined/separated display attribute
					 *  cannot be cancelled by a subsequent spacing attribute.
					 */

				case 0x1C:		/* black background */
				case 0x1D:		/* new background */
					printv("... bg term %d %02x\n", i, raw);
					mac.background = 0;
					break;
				}
			}
		}

		active_column = column;
	}

	static void
	flush_row(void)
	{
		if (type == OBJ_TYPE_PASSIVE || type == OBJ_TYPE_ADAPTIVE)
			flush(active_column + 1);
		else
			flush(COLUMNS);

		if (type != OBJ_TYPE_PASSIVE)
			memset(&mac, 0, sizeof(mac));
	}

	active_column = 0;
	active_row = 0;

	acp = &pg->text[(inv_row + 0) * COLUMNS];

	offset_column = 0;
	offset_row = 0;

	row_colour =
	next_row_colour = ext->def_row_colour;

	drcs_s1[0] = 0; /* global */
	drcs_s1[1] = 0; /* normal */

	memset(&ac, 0, sizeof(ac));
	memset(&mac, 0, sizeof(mac));

	invert = 0;

	if (type == OBJ_TYPE_PASSIVE) {
		ac.foreground = WHITE;
		ac.background = BLACK;
		ac.opacity = pg->page_opacity[1];

		mac.foreground = ~0;
		mac.background = ~0;
		mac.opacity = ~0;
		mac.size = ~0;
		mac.underline = ~0;
		mac.conceal = ~0;
		mac.flash = ~0;
	}

	font = pg->font[0];

	for (;; p++) {
		if (p->address >= COLUMNS) {
			/*
			 *  Row address triplets
			 */
			int s = p->data >> 5;
			int row = (p->address - COLUMNS) ? : (ROWS - 1);
			int column = 0;

			switch (p->mode) {
			case 0x00:		/* full screen colour */
				if (vbi->vt.max_level >= VBI_LEVEL_2p5
				    && s == 0 && type <= OBJ_TYPE_ACTIVE)
					screen_colour(pg, vtp, p->data & 0x1F);

				break;

			case 0x07:		/* address display row 0 */
				if (p->address != 0x3F)
					break; /* reserved, no position */

				row = 0;

				/* fall through */

			case 0x01:		/* full row colour */
				row_colour = next_row_colour;

				if (s == 0) {
					row_colour = p->data & 0x1F;
					next_row_colour = ext->def_row_colour;
				} else if (s == 3) {
					row_colour =
					next_row_colour = p->data & 0x1F;
				}

				goto set_active;

			case 0x02:		/* reserved */
			case 0x03:		/* reserved */
				break;

			case 0x04:		/* set active position */
				if (vbi->vt.max_level >= VBI_LEVEL_2p5) {
					if (p->data >= COLUMNS)
						break; /* reserved */

					column = p->data;
				}

				if (row > active_row)
					row_colour = next_row_colour;

			set_active:
				printv("enh set_active row %d col %d\n", row, column);

				if (row > active_row)
					flush_row();

				active_row = row;
				active_column = column;

				acp = &pg->text[(inv_row + active_row) * COLUMNS];

				break;

			case 0x05:		/* reserved */
			case 0x06:		/* reserved */
			case 0x08 ... 0x0F:	/* PDC data */
				break;

			case 0x10:		/* origin modifier */
				if (vbi->vt.max_level < VBI_LEVEL_2p5)
					break;

				if (p->data >= 72)
					break; /* invalid */

				offset_column = p->data;
				offset_row = p->address - COLUMNS;

				printv("enh origin modifier col %+d row %+d\n",
					offset_column, offset_row);

				break;

			case 0x11 ... 0x13:	/* object invocation */
			{
				int source = (p->address >> 3) & 3;
				object_type new_type = p->mode & 3;
				vt_triplet *trip;

				if (vbi->vt.max_level < VBI_LEVEL_2p5)
					break;

				printv("enh obj invocation source %d type %d\n", source, new_type);

				if (new_type <= type) { /* 13.2++ */
					printv("... priority violation\n");
					break;
				}

				if (source == 0) /* illegal */
					break;
				else if (source == 1) { /* local */
					int designation = (p->data >> 4) + ((p->address & 1) << 4);
					int triplet = p->data & 15;

					if (type != LOCAL_ENHANCEMENT_DATA || triplet > 12)
						break; /* invalid */

					printv("... local obj %d/%d\n", designation, triplet);

					if (!(vtp->enh_lines & 1)) {
						printv("... no packet %d\n", designation);
						return FALSE;
					}

					trip = vtp->data.enh_lop.enh + designation * 13 + triplet;
				}
				else /* global / public */
				{
					page_function function;
					int pgno, i;

					if (source == 3) {
						function = PAGE_FUNCTION_GPOP;
						pgno = vtp->data.lop.link[24].pgno;
						i = 0;

						if (NO_PAGE(pgno)) {
							if (vbi->vt.max_level < VBI_LEVEL_3p5
							    || NO_PAGE(pgno = mag->pop_link[8].pgno))
								pgno = mag->pop_link[0].pgno;
						} else
							printv("... X/27/4 GPOP overrides MOT\n");
					} else {
						function = PAGE_FUNCTION_POP;
						pgno = vtp->data.lop.link[25].pgno;

						if (NO_PAGE(pgno)) {
							if ((i = mag->pop_lut[vtp->pgno & 0xFF]) == 0) {
								printv("... MOT pop_lut empty\n");
								return FALSE; /* has no link (yet) */
							}

							if (vbi->vt.max_level < VBI_LEVEL_3p5
							    || NO_PAGE(pgno = mag->pop_link[i + 8].pgno))
								pgno = mag->pop_link[i + 0].pgno;
						} else
							printv("... X/27/4 POP overrides MOT\n");
					}

					if (NO_PAGE(pgno)) {
						printv("... dead MOT link %d\n", i);
						return FALSE; /* has no link (yet) */
					}

					printv("... %s obj\n", (source == 3) ? "global" : "public");

					trip = resolve_obj_address(vbi, new_type, pgno,
						(p->address << 7) + p->data, function);

					if (!trip)
						return FALSE;
				}

				row = inv_row + active_row;
				column = inv_column + active_column;

				enhance(vbi, mag, ext, pg, vtp, new_type, trip,
					row + offset_row, column + offset_column);

				printv("... object done\n");

				offset_row = 0;
				offset_column = 0;

				break;
			}

			case 0x14:		/* reserved */
				break;

			case 0x15 ... 0x17:	/* object definition */
				flush_row();
				printv("enh obj definition 0x%02x 0x%02x\n", p->mode, p->data);
				printv("enh terminated\n");
				goto swedish;

			case 0x18:		/* drcs mode */
				printv("enh DRCS mode 0x%02x\n", p->data);
				drcs_s1[p->data >> 6] = p->data & 15;
				break;

			case 0x19 ... 0x1E:	/* reserved */

			case 0x1F:		/* termination marker */
			default:
				flush_row();
				printv("enh terminated %02x\n", p->mode);
				goto swedish;
			}
		} else {
			/*
			 *  Column address triplets
			 */
			int s = p->data >> 5;
			int column = p->address;
			int gl;

			switch (p->mode) {
			case 0x00:		/* foreground colour */
				if (vbi->vt.max_level >= VBI_LEVEL_2p5 && s == 0) {
					if (column > active_column)
						flush(column);

					ac.foreground = p->data & 0x1F;
					mac.foreground = ~0;

					printv("enh col %d foreground %d\n", active_column, ac.foreground);
				}

				break;

			case 0x01:		/* G1 block mosaic character */
				if (vbi->vt.max_level >= VBI_LEVEL_2p5) {
					if (column > active_column)
						flush(column);

					if (p->data & 0x20) {
						gl = GL_CONTIGUOUS_BLOCK_MOSAIC_G1 + p->data;
						goto store;
					} else if (p->data >= 0x40) {
						gl = glyph_lookup(font->G0, NO_SUBSET, p->data);
						goto store;
					}
				}

				break;

			case 0x0B:		/* G3 smooth mosaic or line drawing character */
				if (vbi->vt.max_level < VBI_LEVEL_2p5)
					break;

				/* fall through */

			case 0x02:		/* G3 smooth mosaic or line drawing character */
				if (p->data >= 0x20) {
					if (column > active_column)
						flush(column);

					gl = GL_SMOOTH_MOSAIC_G3 + p->data;
					goto store;
				}

				break;

			case 0x03:		/* background colour */
				if (vbi->vt.max_level >= VBI_LEVEL_2p5 && s == 0) {
					if (column > active_column)
						flush(column);

					ac.background = p->data & 0x1F;
					mac.background = ~0;

					printv("enh col %d background %d\n", active_column, ac.background);
				}

				break;

			case 0x04:		/* reserved */
			case 0x05:		/* reserved */
			case 0x06:		/* PDC data */

			case 0x07:		/* additional flash functions */
				if (vbi->vt.max_level >= VBI_LEVEL_2p5) {
					if (column > active_column)
						flush(column);

					/*
					 *  Only one flash function (if any) implemented:
					 *  1 - Normal flash to background colour
					 *  0 - Slow rate (1 Hz)
					 */
					ac.flash = !!(p->data & 3);
					mac.flash = ~0;

					printv("enh col %d flash 0x%02x\n", active_column, p->data);
				}

				break;

			case 0x08:		/* modified G0 and G2 character set designation */
				if (vbi->vt.max_level >= VBI_LEVEL_2p5) {
					if (column > active_column)
						flush(column);

					if (VALID_CHARACTER_SET(p->data))
						font = font_descriptors + p->data;
					else
						font = pg->font[0];

					printv("enh col %d modify character set %d\n", active_column, p->data);
				}

				break;

			case 0x09:		/* G0 character */
				if (vbi->vt.max_level >= VBI_LEVEL_2p5 && p->data >= 0x20) {
					if (column > active_column)
						flush(column);

					gl = glyph_lookup(font->G0, NO_SUBSET, p->data);
					goto store;
				}

				break;

			case 0x0A:		/* reserved */
				break;

			case 0x0C:		/* display attributes */
				if (vbi->vt.max_level < VBI_LEVEL_2p5)
					break;

				if (column > active_column)
					flush(column);

				ac.size = ((p->data & 0x40) ? DOUBLE_WIDTH : 0)
					+ ((p->data & 1) ? DOUBLE_HEIGHT : 0);
				mac.size = ~0;

				if (p->data & 2) {
					if (vtp->flags & (C5_NEWSFLASH | C6_SUBTITLE))
						ac.opacity = SEMI_TRANSPARENT;
					else
						ac.opacity = TRANSPARENT_SPACE;
				} else
					ac.opacity = pg->page_opacity[1];
				mac.opacity = ~0;

				ac.conceal = !!(p->data & 4);
				mac.conceal = ~0;

				/* (p->data & 8) reserved */

				invert = p->data & 0x10;

				ac.underline = !!(p->data & 0x20);
				mac.underline = ~0;

				printv("enh col %d display attr 0x%02x\n", active_column, p->data);

				break;

			case 0x0D:		/* drcs character invocation */
			{
				int normal = p->data >> 6;
				int offset = p->data & 0x3F;
				struct vt_page *dvtp;
				page_function function;
				int pgno, page, i;

				if (vbi->vt.max_level < VBI_LEVEL_2p5)
					break;

				if (offset >= 48)
					break; /* invalid */

				if (column > active_column)
					flush(column);

				page = normal * 16 + drcs_s1[normal];

				printv("enh col %d DRCS %d/0x%02x\n", active_column, page, p->data);

				/* if (!pg->drcs[page]) */ {
					if (!normal) {
						function = PAGE_FUNCTION_GDRCS;
						pgno = vtp->data.lop.link[26].pgno;
						i = 0;

						if (NO_PAGE(pgno)) {
							if (vbi->vt.max_level < VBI_LEVEL_3p5
							    || NO_PAGE(pgno = mag->drcs_link[8]))
								pgno = mag->drcs_link[0];
						} else
							printv("... X/27/4 GDRCS overrides MOT\n");
					} else {
						function = PAGE_FUNCTION_DRCS;
						pgno = vtp->data.lop.link[25].pgno;

						if (NO_PAGE(pgno)) {
							if ((i = mag->drcs_lut[vtp->pgno & 0xFF]) == 0) {
								printv("... MOT drcs_lut empty\n");
								return FALSE; /* has no link (yet) */
							}

							if (vbi->vt.max_level < VBI_LEVEL_3p5
							    || NO_PAGE(pgno = mag->drcs_link[i + 8]))
								pgno = mag->drcs_link[i + 0];
						} else
							printv("... X/27/4 DRCS overrides MOT\n");
					}

					if (NO_PAGE(pgno)) {
						printv("... dead MOT link %d\n", i);
						return FALSE; /* has no link (yet) */
					}

					printv("... %s drcs from page %03x/%04x\n",
						normal ? "normal" : "global", pgno, drcs_s1[normal]);

					dvtp = vbi->cache->op->get(vbi->cache,
						pgno, drcs_s1[normal], 0x000F);

					if (!dvtp) {
						printv("... page not cached\n");
						return FALSE;
					}

					if (dvtp->function == PAGE_FUNCTION_UNKNOWN) {
						if (!(dvtp = vbi_convert_page(vbi, dvtp, TRUE, function))) {
							printv("... no g/drcs page or hamming error\n");
							return FALSE;
						}
					} else if (dvtp->function == PAGE_FUNCTION_DRCS) {
						dvtp->function = function;
					} else if (dvtp->function != function) {
						printv("... source page wrong function %d, expected %d\n",
							dvtp->function, function);
						return FALSE;
					}

					if (dvtp->data.drcs.invalid & (1ULL << offset)) {
						printv("... invalid drcs, prob. tx error\n");
						return FALSE;
					}

					pg->drcs[page] = dvtp->data.drcs.bits[0];
				}

				gl = GL_DRCS + page * 256 + offset;
				goto store;
			}

			case 0x0E:		/* font style */
			{
				int italic, bold, proportional;
				int col, row, count;
				attr_char *acp;

				if (vbi->vt.max_level < VBI_LEVEL_3p5)
					break;

				row = inv_row + active_row;
				count = (p->data >> 4) + 1;
				acp = &pg->text[row * COLUMNS];

				proportional = (p->data >> 0) & 1;
				bold = (p->data >> 1) & 1;
				italic = (p->data >> 2) & 1;

				while (row < ROWS && count > 0) {
					for (col = inv_column + column; col < COLUMNS; col++) {
						acp[col].italic = italic;
		    				acp[col].bold = bold;
						acp[col].proportional = proportional;
					}

					acp += COLUMNS;
					row++;
					count--;
				}

				printv("enh col %d font style 0x%02x\n", active_column, p->data);

				break;
			}

			case 0x0F:		/* G2 character */
				if (p->data >= 0x20) {
					if (column > active_column)
						flush(column);

					gl = glyph_lookup(font->G2, NO_SUBSET, p->data);
					goto store;
				}

				break;

			case 0x10 ... 0x1F:	/* characters including diacritical marks */
				if (p->data >= 0x20) {
					if (column > active_column)
						flush(column);

					gl = glyph_lookup(font->G0, NO_SUBSET, p->data);
					gl = compose_glyph(gl, p->mode - 16);

			store:
					printv("enh row %d col %d print 0x%02x/0x%02x -> 0x%04x %c\n",
						active_row, active_column, p->mode, p->data,
						gl, glyph2latin(gl));

					ac.glyph = gl;
					mac.glyph = ~0;
				}

				break;
			}
		}
	}

swedish:

#if 0
	acp = pg->data[0];

	for (active_row = 0; active_row < ROWS; active_row++) {
		printv("%2d: ", active_row);

		for (active_column = 0; active_column < COLUMNS; acp++, active_column++) {
			printv("%04x ", acp->glyph);
		}

		printv("\n");
	}
#endif

	return TRUE;
}

static void
post_enhance(struct fmt_page *pg)
{
	attr_char ac, *acp;
	int column, row;

	acp = pg->text;

	for (row = 0; row < ROWS - 1; row++) {
		for (column = 0; column < COLUMNS; acp++, column++) {
//			printv("%d ", acp->size);

			if (acp->opacity == TRANSPARENT_SPACE
			    || (acp->foreground == TRANSPARENT_BLACK
				&& acp->background == TRANSPARENT_BLACK)) {
				acp->opacity = TRANSPARENT_SPACE;
				acp->glyph = GL_SPACE;
			} else if (acp->background == TRANSPARENT_BLACK)
				acp->opacity = SEMI_TRANSPARENT;
			/* transparent foreground not implemented */

			switch (acp->size) {
			case NORMAL:
				if (row < 23
				    && (acp[COLUMNS].size == DOUBLE_HEIGHT2 || acp[COLUMNS].size == DOUBLE_SIZE2)) {
					acp[COLUMNS].glyph = GL_SPACE;
					acp[COLUMNS].size = NORMAL;
				}

				if (column < 39
				    && (acp[1].size == OVER_TOP || acp[1].size == OVER_BOTTOM)) {
					acp[1].glyph = GL_SPACE;
					acp[1].size = NORMAL;
				}

				break;

			case DOUBLE_HEIGHT:
				ac = acp[0];
				ac.size = DOUBLE_HEIGHT2;
				acp[COLUMNS] = ac;
				break;

			case DOUBLE_SIZE:
				ac = acp[0];
				ac.size = DOUBLE_SIZE2;
				acp[COLUMNS] = ac;
				ac.size = OVER_BOTTOM;
				acp[41] = ac;
				/* fall through */

			case DOUBLE_WIDTH:
				ac = acp[0];
				ac.size = OVER_TOP;
				acp[1] = ac;
				break;

			default:
				break;
			}
		}

//		printv("\n");
	}
}

static inline bool
default_object_invocation(struct vbi *vbi, magazine *mag,
	extension *ext, struct fmt_page *pg, struct vt_page *vtp)
{
	pop_link *pop;
	int i, order;

	if (!(i = mag->pop_lut[vtp->pgno & 0xFF]))
		return FALSE; /* has no link (yet) */

	pop = mag->pop_link + i + 8;

	if (vbi->vt.max_level < VBI_LEVEL_3p5 || NO_PAGE(pop->pgno)) {
		pop = mag->pop_link + i;

		if (NO_PAGE(pop->pgno)) {
			printv("default object has dead MOT pop link %d\n", i);
			return FALSE;
		}
	}

	order = pop->default_obj[0].type > pop->default_obj[1].type;

	for (i = 0; i < 2; i++) {
		object_type type = pop->default_obj[i ^ order].type;
		vt_triplet *trip;

		if (type == OBJ_TYPE_NONE)
			continue;

		printv("default object #%d invocation, type %d\n", i ^ order, type);

		trip = resolve_obj_address(vbi, type, pop->pgno,
			pop->default_obj[i ^ order].address, PAGE_FUNCTION_POP);

		if (!trip)
			return FALSE;

		enhance(vbi, mag, ext, pg, vtp, type, trip, 0, 0);
	}

	return TRUE;
}

int
vbi_format_page(struct vbi *vbi,
	struct fmt_page *pg, struct vt_page *vtp,
	int display_rows, int navigation)
{
	char buf[16];
	magazine *mag;
	extension *ext;
	int column, row, i;

	if (vtp->function != PAGE_FUNCTION_LOP)
		return 0;

	printv("\nFormatting page %03x/%04x\n", vtp->pgno, vtp->subno);

	pg->pgno = vtp->pgno;
	pg->subno = vtp->subno;

	pg->rows = ROWS;
	pg->columns = COLUMNS;

	mag = (vbi->vt.max_level <= VBI_LEVEL_1p5) ?
		vbi->vt.magazine : vbi->vt.magazine + (vtp->pgno >> 8);

	if (vtp->data.lop.ext)
		ext = &vtp->data.ext_lop.ext;
	else
		ext = &mag->extension;

	/* Character set designation */

	pg->font[0] = font_descriptors + 0;
	pg->font[1] = font_descriptors + 0;

	for (i = 0; i < 2; i++) {
		int char_set = ext->char_set[i];

		if (VALID_CHARACTER_SET(char_set))
			pg->font[i] = font_descriptors + char_set;

		char_set = (char_set & ~7) + vtp->national;

		if (VALID_CHARACTER_SET(char_set))
			pg->font[i] = font_descriptors + char_set;
	}

	/* Colours */

	screen_colour(pg, vtp, ext->def_screen_colour);

	pg->colour_map = ext->colour_map;
	pg->drcs_clut = ext->drcs_clut;

	/* Opacity */

	pg->page_opacity[1] =
		(vtp->flags & (C5_NEWSFLASH | C6_SUBTITLE | C10_INHIBIT_DISPLAY)) ?
			TRANSPARENT_SPACE : OPAQUE;
	pg->boxed_opacity[1] =
		(vtp->flags & C10_INHIBIT_DISPLAY) ? TRANSPARENT_SPACE : SEMI_TRANSPARENT;

	if (vtp->flags & C7_SUPPRESS_HEADER) {
		pg->page_opacity[0] = TRANSPARENT_SPACE;
		pg->boxed_opacity[0] = TRANSPARENT_SPACE;
	} else {
		pg->page_opacity[0] = pg->page_opacity[1];
		pg->boxed_opacity[0] = pg->boxed_opacity[1];
	}

	/* DRCS */

	memset(pg->drcs, 0, sizeof(pg->drcs));

	/* Current page number in header */

	sprintf(buf, "\2%x.%02x\7", vtp->pgno, vtp->subno & 0xff);

	i = 0;
	pg->double_height_lower = 0;

	for (row = 0; row < display_rows; row++) {
		font_descriptor *font;
		int mosaic_glyphs;
		int held_mosaic_glyph;
		bool hold, mosaic;
		bool double_height, wide_char;
		attr_char ac, *acp = &pg->text[row * COLUMNS];

		held_mosaic_glyph = GL_CONTIGUOUS_BLOCK_MOSAIC_G1 + 0; /* blank */

		memset(&ac, 0, sizeof(ac));

		ac.foreground	= ext->foreground_clut + WHITE;
		ac.background	= ext->background_clut + BLACK;
		mosaic_glyphs	= GL_CONTIGUOUS_BLOCK_MOSAIC_G1;
		ac.opacity	= pg->page_opacity[row > 0];
		font		= pg->font[0];
		hold		= FALSE;
		mosaic		= FALSE;

		double_height	= FALSE;
		wide_char	= FALSE;

		for (column = 0; column < COLUMNS; ++column) {
			int raw;

			if (row == 0 && column < 8) {
				raw = buf[column];
				i++;
			} else if ((raw = parity(vtp->data.lop.raw[0][i++])) < 0)
				raw = ' ';

			/* set-at spacing attributes */

			switch (raw) {
			case 0x09:		/* steady */
				ac.flash = FALSE;
				break;

			case 0x0C:		/* normal size */
				ac.size = NORMAL;
				break;

			case 0x18:		/* conceal */
				ac.conceal = TRUE;
				break;

			case 0x19:		/* contiguous mosaics */
				mosaic_glyphs = GL_CONTIGUOUS_BLOCK_MOSAIC_G1;
				break;

			case 0x1A:		/* separated mosaics */
				mosaic_glyphs = GL_SEPARATED_BLOCK_MOSAIC_G1;
				break;

			case 0x1C:		/* black background */
				ac.background = ext->background_clut + BLACK;
				break;

			case 0x1D:		/* new background */
				ac.background = ext->background_clut + (ac.foreground & 7);
				break;

			case 0x1E:		/* hold mosaic */
				hold = TRUE;
				break;
			}

			if (raw <= 0x1F)
				ac.glyph = (hold & mosaic) ? held_mosaic_glyph : GL_SPACE;
			else
				if (mosaic && (raw & 0x20)) {
					held_mosaic_glyph = mosaic_glyphs + raw;
					ac.glyph = held_mosaic_glyph;
				} else
					ac.glyph = glyph_lookup(font->G0, font->subset, raw);

			if (!wide_char) {
				acp[column] = ac;

				wide_char = /*!!*/(ac.size & DOUBLE_WIDTH);

				if (wide_char && column < 39) {
					acp[column + 1] = ac;
					acp[column + 1].size = OVER_TOP;
				}
			} else
				wide_char = FALSE;

			/* set-after spacing attributes */

			switch (raw) {
			case 0x00 ... 0x07:	/* alpha + foreground colour */
				ac.foreground = ext->foreground_clut + (raw & 7);
				ac.conceal = FALSE;
				mosaic = FALSE;
				break;

			case 0x08:		/* flash */
				ac.flash = TRUE;
				break;

			case 0x0A:		/* end box */
				if (column < 39 && parity(vtp->data.lop.raw[0][i]) == 0x0a)
					ac.opacity = pg->page_opacity[row > 0];
				break;

			case 0x0B:		/* start box */
				if (column < 39 && parity(vtp->data.lop.raw[0][i]) == 0x0b)
					ac.opacity = pg->boxed_opacity[row > 0];
				break;

			case 0x0D:		/* double height */
				if (row <= 0 || row >= 23)
					break;
				ac.size = DOUBLE_HEIGHT;
				double_height = TRUE;
				break;

			case 0x0E:		/* double width */
				printv("spacing col %d row %d double width\n", column, row);
				if (column < 39)
					ac.size = DOUBLE_WIDTH;
				break;

			case 0x0F:		/* double size */
				printv("spacing col %d row %d double size\n", column, row);
				if (column >= 39 || row <= 0 || row >= 23)
					break;
				ac.size = DOUBLE_SIZE;
				double_height = TRUE;

				break;

			case 0x10 ... 0x17:	/* mosaic + foreground colour */
				ac.foreground = ext->foreground_clut + (raw & 7);
				ac.conceal = FALSE;
				mosaic = TRUE;
				break;

			case 0x1F:		/* release mosaic */
				hold = FALSE;
				break;

			case 0x1B:		/* ESC */
				font = (font == pg->font[0]) ? pg->font[1] : pg->font[0];
				break;
			}
		}

		if (double_height) {
			for (column = 0; column < COLUMNS; column++) {
				ac = acp[column];

				switch (ac.size) {
				case DOUBLE_HEIGHT:
					ac.size = DOUBLE_HEIGHT2;
					acp[COLUMNS + column] = ac;
					break;
		
				case DOUBLE_SIZE:
					ac.size = DOUBLE_SIZE2;
					acp[COLUMNS + column] = ac;
					ac.size = OVER_BOTTOM;
					acp[COLUMNS + (++column)] = ac;
					break;

				default: /* NORMAL, DOUBLE_WIDTH, OVER_TOP */
					ac.size = NORMAL;
					ac.glyph = GL_SPACE;
					acp[COLUMNS + column] = ac;
					break;
				}
			}

			i += COLUMNS;
			row++;

			pg->double_height_lower |= 1 << row;
		}
	}

	if (row < ROWS) {
		attr_char ac;

		memset(&ac, 0, sizeof(ac));

		ac.foreground	= ext->foreground_clut + WHITE;
		ac.background	= ext->background_clut + BLACK;
		ac.opacity	= pg->page_opacity[1];
		ac.glyph	= GL_SPACE;

		for (i = row * COLUMNS; i < ROWS * COLUMNS; i++)
			pg->text[i] = ac;
	}

	/* Local enhancement data and objects */

	if (vbi->vt.max_level >= VBI_LEVEL_1p5) {
		struct fmt_page page;
		bool success;

		memcpy(&page, pg, sizeof(struct fmt_page));

		if (!(vtp->flags & (C5_NEWSFLASH | C6_SUBTITLE))) {
			pg->boxed_opacity[0] = TRANSPARENT_SPACE;
			pg->boxed_opacity[1] = TRANSPARENT_SPACE;
		}

		if (vtp->enh_lines & 1) {
			printv("enhancement packets %08x\n", vtp->enh_lines);
			success = enhance(vbi, mag, ext, pg, vtp, LOCAL_ENHANCEMENT_DATA,
				vtp->data.enh_lop.enh, 0, 0);
		} else
			success = default_object_invocation(vbi, mag, ext, pg, vtp);

		if (success)
			post_enhance(pg);
		else
			memcpy(pg, &page, sizeof(struct fmt_page));
	}

	/* Navigation */

	if (navigation) {
		pg->nav_link[5].pgno = vbi->vt.initial_page.pgno;
		pg->nav_link[5].subno = vbi->vt.initial_page.subno;

		for (row = 1; row < MIN(ROWS - 1, display_rows); row++)
			zap_links(pg, row);

		if (display_rows >= ROWS) {
			if (vtp->data.lop.flof) {
				if (vtp->data.lop.link[5].pgno >= 0x100
				    && vtp->data.lop.link[5].pgno <= 0x899
				    && (vtp->data.lop.link[5].pgno & 0xFF) != 0xFF) {
					pg->nav_link[5].pgno = vtp->data.lop.link[5].pgno;
					pg->nav_link[5].subno = vtp->data.lop.link[5].subno;
				}

				if (vtp->lop_lines & (1 << 24))
					flof_links(pg, vtp);
				else
					flof_navigation_bar(pg, vtp);
			} else if (vbi->vt.top)
				top_navigation_bar(vbi, pg, vtp);
		}
	}

	return 1;
}

int
vbi_fetch_page(struct vbi *vbi, struct fmt_page *pg,
	int pgno, int subno, int display_rows, int navigation)
{
	struct vt_page *vtp;
	int row;

	switch (pgno) {
	case 0x900:
		if (!vbi->vt.top || !top_index(vbi, pg, subno))
			return 0;

		post_enhance(pg);

		for (row = 1; row < ROWS; row++)
			zap_links(pg, row);

		return 1;

	default:
		vtp = vbi->cache->op->get(vbi->cache, pgno, subno, 0xFFFF);

		if (!vtp)
			return 0;

		return vbi_format_page(vbi, pg, vtp, display_rows, navigation);
	}
}
