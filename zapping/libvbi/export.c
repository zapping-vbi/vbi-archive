#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "vt.h"
#include "misc.h"
#include "export.h"
#include "vbi.h"
#include "hamm.h"

#include "lang.h"

/* to be exported */

typedef enum {
	LEVEL_1,
	LEVEL_1p5,
	LEVEL_2p5,
	LEVEL_3p5
} implementation_level;

/* -> struct vbi */
static int max_level = LEVEL_3p5;



#include "../common/types.h"
#include "../common/math.h"

extern struct export_module export_txt[1];
extern struct export_module export_ansi[1];
extern struct export_module export_html[1];
extern struct export_module export_png[1];
extern struct export_module export_ppm[1];

struct export_module *modules[] =
{
    export_txt,
    export_ansi,
    export_html,
    export_ppm,
#ifdef WITH_PNG
    export_png,
#endif
    0
};

static char *glbl_opts[] =
{
    "reveal",		// show hidden text
    "hide",		// don't show hidden text (default)
    0
};

static char errbuf[64];

void
export_error(char *str, ...)
{
    va_list args;

    va_start(args, str);
    vsnprintf(errbuf, sizeof(errbuf)-1, str, args);
}

char *
export_errstr(void)
{
    return errbuf;
}


static int
find_opt(char **opts, char *opt, char *arg)
{
    int err = 0;
    char buf[256];
    char **oo, *o, *a;

    if ((oo = opts))
	while ((o = *oo++))
	{
	    if ((a = strchr(o, '=')))
	    {
		a = buf + (a - o);
		o = strcpy(buf, o);
		*a++ = 0;
	    }
	    if (strcasecmp(o, opt) == 0)
	    {
		if ((a != 0) == (arg != 0))
		    return oo - opts;
		err = -1;
	    }
	}
    return err;
}


struct export *
export_open(char *fmt)
{
    struct export_module **eem, *em;
    struct export *e;
    char *opt, *optend, *optarg;
    int opti;

    if ((fmt = strdup(fmt)))
    {
	if ((opt = strchr(fmt, ',')))
	    *opt++ = 0;
	for (eem = modules; (em = *eem); eem++)
	    if (strcasecmp(em->fmt_name, fmt) == 0)
		break;
	if (em)
	{
	    if ((e = malloc(sizeof(*e) + em->local_size)))
	    {
		e->mod = em;
		e->fmt_str = fmt;
		e->reveal = 0;
		memset(e + 1, 0, em->local_size);
		if (not em->open || em->open(e) == 0)
		{
		    for (; opt; opt = optend)
		    {
			if ((optend = strchr(opt, ',')))
			    *optend++ = 0;
			if (not *opt)
			    continue;
			if ((optarg = strchr(opt, '=')))
			    *optarg++ = 0;
			if ((opti = find_opt(glbl_opts, opt, optarg)) > 0)
			{
			    if (opti == 1) // reveal
				e->reveal = 1;
			    else if (opti == 2) // hide
				e->reveal = 0;
			}
			else if (opti == 0 &&
				(opti = find_opt(em->options, opt, optarg)) > 0)
			{
			    if (em->option(e, opti, optarg))
				break;
			}
			else
			{
			    if (opti == 0)
				export_error("%s: unknown option", opt);
			    else if (optarg)
				export_error("%s: takes no arg", opt);
			    else
				export_error("%s: missing arg", opt);
			    break;
			}
		    }
		    if (opt == 0)
			return e;

		    if (em->close)
			em->close(e);
		}
		free(e);
	    }
	    else
		export_error("out of memory");
	}
	else
	    export_error("unknown format: %s", fmt);
	free(fmt);
    }
    else
	export_error("out of memory");
    return 0;
}


void
export_close(struct export *e)
{
    if (e->mod->close)
	e->mod->close(e);
    free(e->fmt_str);
    free(e);
}


static char *
hexnum(char *buf, unsigned int num)
{
    char *p = buf + 5;

    num &= 0xffff;
    *--p = 0;
    do
    {
	*--p = "0123456789abcdef"[num % 16];
	num /= 16;
    } while (num);
    return p;
}

static char *
adjust(char *p, char *str, char fill, int width)
{
    int l = width - strlen(str);

    while (l-- > 0)
	*p++ = fill;
    while ((*p = *str++))
	p++;
    return p;
}

char *
export_mkname(struct export *e, char *fmt, struct vt_page *vtp, char *usr)
{
    char bbuf[1024];
    char *p = bbuf;

    while ((*p = *fmt++))
	if (*p++ == '%')
	{
	    char buf[32], buf2[32];
	    int width = 0;

	    p--;
	    while (*fmt >= '0' && *fmt <= '9')
		width = width*10 + *fmt++ - '0';

	    switch (*fmt++)
	    {
		case '%':
		    p = adjust(p, "%", '%', width);
		    break;
		case 'e':	// extension
		    p = adjust(p, e->mod->extension, '.', width);
		    break;
		case 'p':	// pageno[.subno]
		    if (vtp->subno)
			p = adjust(p,strcat(strcat(hexnum(buf, vtp->pgno),
				"."), hexnum(buf2, vtp->subno)), ' ', width);
		    else
			p = adjust(p, hexnum(buf, vtp->pgno), ' ', width);
		    break;
		case 'S':	// subno
		    p = adjust(p, hexnum(buf, vtp->subno), '0', width);
		    break;
		case 'P':	// pgno
		    p = adjust(p, hexnum(buf, vtp->pgno), '0', width);
		    break;
		case 's':	// user strin
		    p = adjust(p, usr, ' ', width);
		    break;
		//TODO: add date, channel name, ...
	    }
	}
    p = strdup(bbuf);
    if (not p)
	export_error("out of memory");
    return p;
}


#define printable(c) ((((c) & 0x7F) < 0x20 || ((c) & 0x7F) > 0x7E) ? '.' : ((c) & 0x7F))

#undef printv
#define printv printf
// #define printv(templ, ...)




static int
vbi_resolve_flof(int x, struct vt_page *vtp, int *page, int *subpage)
{
	int code= 7, i, c;

/* fixme */
return FALSE;	
	
	if ((!vtp) || (!page) || (!subpage))
		return FALSE;
	
	if (!(vtp->data.lop.flof))
		return FALSE;
#if 0 //obsolete
	for (i=0; (i <= x) && (i<40); i++)
		if ((c = vtp->data[24][i]) < 8) /* color code */
			code = c; /* Store it for later on */
#endif
	if (code >= 8) /* not found ... weird */
		return FALSE;
	
	code = " \0\1\2\3 \3 "[code]; /* color->link conversion table */
	
	if ((code > 6) || ((vtp->data.lop.link[code].pgno & 0xff) == 0xff))
		return FALSE;
	
	*page = vtp->data.lop.link[code].pgno;
	*subpage = vtp->data.lop.link[code].subno; /* 0x3f7f handled? */
	
	return TRUE;
}

#define notdigit(x) (!isdigit(x))

static int
vbi_check_subpage(const char *p, int x, int *n1, int *n2)
{
    p += x;

    if (x >= 0 && x < 42-5)
	if (notdigit(p[1]) || notdigit(p[0]))
	    if (isdigit(p[2]))
		if (p[3] == '/' || p[3] == ':')
		    if (isdigit(p[4]))
			if (notdigit(p[5]) || notdigit(p[6]))
			{
			    *n1 = p[2] % 16;
			    if (isdigit(p[1]))
				*n1 += p[1] % 16 * 16;
			    *n2 = p[4] % 16;
			    if (isdigit(p[5]))
				*n2 = *n2 * 16 + p[5] % 16;
			    if ((*n2 > 0x99) || (*n1 > 0x99) ||
				(*n1 > *n2))
			      return FALSE;
			    return TRUE;
			}
    return FALSE;
}

static int
vbi_check_page(const char *p, int x, int *pgno, int *subno)
{
    p += x;

    if (x >= 0 && x < 42-4)
      if (notdigit(p[0]) && notdigit(p[4]))
	if (isdigit(p[1]))
	  if (isdigit(p[2]))
	    if (isdigit(p[3]))
	      {
		*pgno = p[1] % 16 * 256 + p[2] % 16 * 16 + p[3] % 16;
		*subno = ANY_SUB;
		if (*pgno >= 0x100 && *pgno <= 0x899)
		  return TRUE;
	      }
    return FALSE;
}

/*
  Text navigation.
  Given the page, the x and y, tries to find a page number in that
  position. If succeeds, returns TRUE
*/
static int
vbi_resolve_page(int x, int y, struct vt_page *vtp, int *page,
		 int *subpage, struct fmt_page *pg)
{
	int i, n1, n2;
	char buffer[42]; /* The line and two spaces on the sides */

	if ((y > 24) || (y <= 0) || (x < 0) || (x > 39) || (!vtp)
	    || (!page) || (!subpage) || (!pg))
		return FALSE;

// {mhs}
// XXX new .ch:  123  123  112233  112233
//                    123          112233

	if (y == 24)
		return vbi_resolve_flof(x, vtp, page, subpage);

	buffer[0] = buffer[41] = ' ';

	for (i=1; i<41; i++)
		buffer[i] = pg->data[y][i-1].glyph & 0x3FF; // XXX not pure ASCII

	for (i = -2; i < 1; i++)
		if (vbi_check_page(buffer, x+i, page, subpage))
			return TRUE;

	/* try to resolve subpage */
	for (i = -4; i < 1; i++)
		if (vbi_check_subpage(buffer, x+i, &n1, &n2))
		{
			if (vtp->subno != n1)
				return FALSE; /* mismatch */
			n1 = dec2hex(hex2dec(n1)+1);
			if (n1 > n2)
				n1 = 1;
			*page = vtp->pgno;
			*subpage = n1;
			return TRUE;
		}
	
	return FALSE;
}


/*
    << navigation.c
    >> format.c
*/


static void
screen_colour(struct fmt_page *pg, int colour)
{ 
	pg->screen_colour = colour;

	if (colour == TRANSPARENT_BLACK
	    || (pg->vtp->flags & (C5_NEWSFLASH | C6_SUBTITLE)))
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
		if (!convert_pop(vtp, function)) {
			printv("... no pop page or hamming error\n");
			return 0;
		}
	} else if (vtp->function == PAGE_FUNCTION_POP)
		vtp->function = function;
	else if (vtp->function != function) {
		printv("... source page wrong function %d, expected %d\n",
			vtp->function, function);
		return 0;
	}

	vtp->function = function;

	pointer = vtp->data.pop.pointer[packet * 24 + i * 2 + ((address >> 4) & 1)];

	printv("... triplet pointer %d\n", pointer);

	if (pointer > 506) {
		printv("... triplet pointer out of bounds (%d)\n", pointer);
		return 0;
	}

	if (0) {
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

#define PROTECT_HEADER 0 /* n columns left hand (8) */

static bool
enhance(struct fmt_page *pg, object_type type, vt_triplet *p, int inv_row, int inv_column)
{
	attr_char ac, mac, *acp, buf[40];
	int xattr, xattr_buf[40];
	int active_column, active_row;
	int offset_column, offset_row;
	int min_column, max_column, i;
	int foreground, background, gl;
	font_descriptor *font;
	glyph_size size;
	opacity opacity;
	int drcs_s1[2];
	u8 *rawp;

	active_column = 0;
	active_row = 0;

	offset_column = 0;
	offset_row = 0;

	min_column = (inv_row == 0) ? PROTECT_HEADER : 0;
	max_column = (type == OBJ_TYPE_ADAPTIVE) ? -1 : 39;

	rawp = pg->vtp->data.lop.raw[(pg->double_height_lower & (1 << inv_row)) ? inv_row - 1 : inv_row];
	memcpy(buf, pg->data[inv_row], 40 * sizeof(attr_char));

	xattr = 0;
	memset(xattr_buf, 0, sizeof(xattr_buf));

	foreground	= WHITE;
	background	= BLACK;
	size		= NORMAL;
	opacity		= pg->page_opacity[inv_row + active_row > 0];

	drcs_s1[0] = 0; /* global */
	drcs_s1[1] = 0; /* normal */

	font = pg->font[0];

	for (;; p++) {
		if (p->mode == 0xFF) {
			printv("enh no triplet, not received (yet)?\n");
			goto finish;
		}

		if (p->address >= 40) {
			/*
			 *  Row address triplets
			 */
			int s = p->data >> 5;
			int row = (p->address - 40) ? : 24;

			switch (p->mode) {
			case 0x00:		/* full screen colour */
				if (s == 0 && type <= OBJ_TYPE_ACTIVE
				    && max_level >= LEVEL_2p5)
					screen_colour(pg, p->data & 0x1F);

				break;

			case 0x07:		/* address display row 0 */
				if (p->address != 0x3F)
					break; /* reserved, no position */

				row = 0;

				/* fall through */

			case 0x01:		/* full row colour */
				active_column = 0;

				if ((i = inv_row + row) < 25) {
					if (s == 0)
						pg->row_colour[i] = p->data & 0x1F;
					else if (s == 3)
						for (; i < 25; i++)
							pg->row_colour[i] = p->data & 0x1F;
				}

				goto set_active;

			/* case 0x02: reserved */
			/* case 0x03: reserved */

			case 0x04:		/* set active position */
				if (max_level >= LEVEL_2p5) {
					if (p->data >= 40)
						break; /* reserved */

					active_column = p->data;
				}

			set_active:
				printv("enh set_active row %d col %d\n", row, active_column);

				if (row == active_row)
					break;

				active_row += inv_row;

				if (active_row <= 24) {
					if (max_column >= min_column)
						memcpy(&pg->data[active_row][min_column],
						       &buf[min_column],
						       (max_column - min_column + 1) * sizeof(attr_char));

					if (type == OBJ_TYPE_NONE || type == OBJ_TYPE_ACTIVE)
						font = pg->font[0];
				}

				active_row = row;
				row += inv_row;

				min_column = (row == 0) ? PROTECT_HEADER : 0;

				if (row <= 24) {
					rawp = pg->vtp->data.lop.raw[(pg->double_height_lower & (1 << row)) ? row - 1 : row];
					memcpy(buf, pg->data[row], 40 * sizeof(attr_char));

					if (type == OBJ_TYPE_ADAPTIVE)
						max_column = -1;
				}

				break;

			/* case 0x05: reserved */
			/* case 0x06: reserved */
			/* case 0x08 ... 0x0F: PDC data */

			case 0x10:		/* origin modifier */
				if (max_level < LEVEL_2p5)
					break;

				if (p->data >= 72)
					break; /* invalid */

				offset_column = p->data;
				offset_row = p->address - 40;

				printv("enh origin modifier col %+d row %+d\n", offset_column, offset_row);

				break;

			case 0x11 ... 0x13:	/* object invocation */
			{
				int source = (p->address >> 3) & 3;
				object_type new_type = p->mode & 3;
				vt_triplet *trip; 
				int row, column;

				if (max_level < LEVEL_2p5)
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

					if (!(pg->vtp->enh_lines & 1)) {
						printv("... no packet %d\n", designation);
						break;
					}

					trip = pg->vtp->data.lop.triplet + designation * 13 + triplet;
				}
				else /* global / public */
				{
					magazine *mag = pg->magazine;
					page_function function;
					int pgno;

					if (source == 3) {
						function = PAGE_FUNCTION_GPOP;
						pgno = pg->vtp->data.lop.link[24].pgno;
						i = 0;

						if (NO_PAGE(pgno)) {
							if (max_level < LEVEL_3p5
							    || NO_PAGE(pgno = mag->pop_link[8].pgno))
								pgno = mag->pop_link[0].pgno;
						} else
							printv("... X/27/4 GPOP overrides MOT\n");
					} else {
						function = PAGE_FUNCTION_POP;
						pgno = pg->vtp->data.lop.link[25].pgno;

						if (NO_PAGE(pgno)) {
							if ((i = mag->pop_lut[pg->vtp->pgno & 0xFF]) == 0) {
								printv("... MOT pop_lut empty\n");
								break; /* has no link (yet) */
							}

							if (max_level < LEVEL_3p5
							    || NO_PAGE(pgno = mag->pop_link[i + 8].pgno))
								pgno = mag->pop_link[i + 0].pgno;
						} else
							printv("... X/27/4 POP overrides MOT\n");
					}

					if (NO_PAGE(pgno)) {
						int j;

						printv("... dead MOT link %d: ", i);
						for (j = 0; j < 8; j++)
							printv("%04x ", mag->pop_link[j].pgno);
						printv("\n");
						break; /* has no link (yet) */
					}

					printv("... %s obj\n", (source == 3) ? "global" : "public");

					trip = resolve_obj_address(pg->vtp->vbi, new_type, pgno,
						(p->address << 7) + p->data, function);

					if (!trip)
						break;
				}

				row = inv_row + active_row;
				column = inv_column + active_column;

				if (row <= 24 && max_column >= min_column)
					memcpy(&pg->data[row][min_column], &buf[min_column],
					       (max_column - min_column + 1) * sizeof(attr_char));

				enhance(pg, new_type, trip,
					row + offset_row, column + offset_column);

				if (row <= 24)
					memcpy(buf, pg->data[row], 40 * sizeof(attr_char));

				printv("... object done\n");

				offset_row = 0;
				offset_column = 0;

				break;
			}

			/* case 0x14: reserved */

			case 0x15 ... 0x17:	/* object definition */
				printv("enh obj definition 0x%02x 0x%02x\n", p->mode, p->data);
				printv("enh terminated\n");
				goto finish;

			case 0x18:		/* drcs mode */
				printv("enh DRCS mode 0x%02x\n", p->data);
				drcs_s1[p->data >> 6] = p->data & 15;
				break;

			/* case 0x19 ... 0x1E: reserved */
			
			case 0x1F:		/* termination marker */
				printv("enh terminated\n");
				goto finish;
			}
		} else {
			/*
			 *  Column address triplets
			 */
			int s = p->data >> 5;		

			switch (p->mode) {
			case 0x00:		/* foreground colour */
				active_column = p->address;

				if (s == 0 && max_level >= LEVEL_2p5) {
					foreground = p->data & 0x1F;

					if (type != OBJ_TYPE_PASSIVE) {
						i = inv_column + active_column;

						if (i > max_column)
							max_column = i;

						for (; i < 40; i++) {
							int raw = parity(rawp[i]) & 0x78;

							buf[i].foreground = foreground;

							/* spacing alpha foreground, set-after */
							/* spacing mosaic foreground, set-after */
							if (type != OBJ_TYPE_ADAPTIVE /* 13.4 */
							    && (raw == 0x00 || raw == 0x10))
								break;
						}
					}

					printv("enh col %d foreground %d\n", active_column, foreground);
				}

				break;

			case 0x01:		/* G1 block mosaic character */
				if (max_level >= LEVEL_2p5) {
					active_column = p->address;

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
				if (max_level < LEVEL_2p5)
					break;

				/* fall through */

			case 0x02:		/* G3 smooth mosaic or line drawing character */
				active_column = p->address;

				if (p->data >= 0x20) {
					gl = GL_SMOOTH_MOSAIC_G3 + p->data;
					goto store;
				}

				break;

			case 0x03:		/* background colour */
				active_column = p->address;

				if (s == 0 && max_level >= LEVEL_2p5) {
					background = p->data & 0x1F;

					if (type != OBJ_TYPE_PASSIVE) {
						i = inv_column + active_column;

						if (i > max_column)
							max_column = i;

						if (i < 40) /* override spacing attribute at active position */
							buf[i++].background = background;

						for (; i < 40; i++) {
							int raw = parity(rawp[i]);

							/* spacing black background, set-at */
							/* spacing new background, set-at */
							if (type != OBJ_TYPE_ADAPTIVE /* 13.4 */
							    && (raw == 0x1C || raw == 0x1D))
								break;

							buf[i].background = background;
						}
					}

					printv("enh col %d background %d\n", active_column, background);
				}

				break;

			/* case 0x04: reserved */
			/* case 0x05: reserved */
			/* case 0x06: pdc data */

			case 0x07:		/* additional flash functions */	
				active_column = p->address;

				if (s == 0 && max_level >= LEVEL_2p5) {
					/* TODO */
					printv("enh col %d flash 0x%02x\n", active_column, p->data);
				}

				break;

			case 0x08:		/* modified G0 and G2 character set designation */
				active_column = p->address;

				if (max_level >= LEVEL_2p5) {
					printv("enh col %d modify character set %d\n", active_column, p->data);

					if (VALID_CHARACTER_SET(p->data))
						font = font_descriptors + p->data;
					else
						font = pg->font[0];
				}

				break;

			case 0x09:		/* G0 character */			
				active_column = p->address;

				if (max_level >= LEVEL_2p5 && p->data >= 0x20) {
					gl = glyph_lookup(font->G0, NO_SUBSET, p->data);
					goto store;
				}

				break;

			/* case 0x0A: reserved */

			case 0x0C:		/* display attributes */
			{
				int row = inv_row + active_row;
				int touch;

				if (max_level < LEVEL_2p5)
					break;

				active_column = p->address;

				printv("enh col %d display attr 0x%02x\n", active_column, p->data);

				size = ((p->data & 0x40) ? DOUBLE_WIDTH : 0)
					+ ((p->data & 1) ? DOUBLE_HEIGHT : 0);
				opacity = (p->data & 2) ? pg->boxed_opacity[row > 0] : pg->page_opacity[row > 0];
				xattr = (xattr & ~0xFF) | p->data;
				touch = 7;

				if (type != OBJ_TYPE_PASSIVE && row < 25) {
					i = inv_column + active_column;

					if (i > max_column)
						max_column = i;

					for (; touch && i < 40; i++) {
						int raw = parity(rawp[i]);

						if (type == OBJ_TYPE_ADAPTIVE)
							raw = -1;
						else if (i > (inv_column + active_column))
							/* active takes priority over set-at */
							switch (raw) {
							case 0x0C:		/* normal size */
								touch &= ~1;
								break;

							case 0x18:		/* conceal */
								touch &= ~4;
								break;
							}

						if (touch & 1)
							buf[i].size = size;
						if (touch & 2)
							buf[i].opacity = opacity;
						xattr_buf[i] = xattr;

						switch (raw) {
						case 0x0A:		/* end box */
						case 0x0B:		/* start box */
							if (i < 39 && parity(rawp[i]) == raw)
								touch &= ~2;
							break;

						case 0x0D:		/* double height */
						case 0x0E:		/* double width */
						case 0x0F:		/* double size */
							touch &= ~1;
							break;
						}
					}
				}

				break;
			}

			/*
				d6	 double width
				d5	underline / separate
				d4	invert colour
				d3	reserved
				d2	conceal
				d1	 boxing / window
				d0	 double height

				set-at, 1=yes
				The action persists to the end of a display row but may be cancelled by
				the transmission of a further triplet of this type with the relevant bit set to '0', or, in
				most cases, by an appropriate spacing attribute on the Level 1 page.
				ADAPTIVE: no spacing attr.
				PASSIVE: no end of row.
			*/

			case 0x0D:		/* drcs character invocation */
			{
				magazine *mag = pg->magazine;
				int normal = p->data >> 6;
				int offset = p->data & 0x3F;
				struct vt_page *drcs_vtp;
				page_function function;
				int pgno, page;

				if (max_level < LEVEL_2p5)
					break;

				active_column = p->address;

				if (offset >= 48)
					break; /* invalid */

				page = normal * 16 + drcs_s1[normal];

				printv("enh col %d DRCS %d/0x%02x\n", active_column, page, p->data);

				if (!pg->drcs[page]) {
					if (!normal) {
						function = PAGE_FUNCTION_GDRCS;
						pgno = pg->vtp->data.lop.link[26].pgno;
						i = 0;

						if (NO_PAGE(pgno)) {
							if (max_level < LEVEL_3p5
							    || NO_PAGE(pgno = mag->drcs_link[8]))
								pgno = mag->drcs_link[0];
						} else
							printv("... X/27/4 GDRCS overrides MOT\n");
					} else {
						function = PAGE_FUNCTION_DRCS;
						pgno = pg->vtp->data.lop.link[25].pgno;

						if (NO_PAGE(pgno)) {
							if ((i = mag->drcs_lut[pg->vtp->pgno & 0xFF]) == 0) {
								printv("... MOT drcs_lut empty\n");
								break; /* has no link (yet) */
							}

							if (max_level < LEVEL_3p5
							    || NO_PAGE(pgno = mag->drcs_link[i + 8]))
								pgno = mag->drcs_link[i + 0];
						} else
							printv("... X/27/4 DRCS overrides MOT\n");
					}

					if (NO_PAGE(pgno)) {
						printv("... dead MOT link %d\n", i);
						break; /* has no link (yet) */
					}

					printv("... %s drcs from page %03x/%04x\n",
						normal ? "normal" : "global", pgno, drcs_s1[normal]);

					drcs_vtp = pg->vtp->vbi->cache->op->get(pg->vtp->vbi->cache,
						pgno, drcs_s1[normal], 0x000F);

					if (!drcs_vtp) {
						printv("... page not cached\n");
						break;
					}
// XXX; GDRCS -> DRCS (MIP)
					convert_drcs(drcs_vtp, function);

					pg->drcs[page] = drcs_vtp->data.drcs.bits[0];
				}

				gl = GL_DRCS + page * 256 + offset;
				goto store;
			}

			case 0x0E:		/* font style */
				if (max_level < LEVEL_3p5)
					break;

				active_column = p->address;

				/* TODO */

				printv("enh col %d font style 0x%02x\n", active_column, p->data);

				break;

			case 0x0F:		/* G2 character */
				active_column = p->address;

				if (p->data >= 0x20) {
					gl = glyph_lookup(font->G2, NO_SUBSET, p->data);
					goto store;
				}

				break;

			case 0x10 ... 0x1F:	/* characters including diacritical marks */
				active_column = p->address;

				if (p->data >= 0x20) {
					gl = glyph_lookup(font->G0, NO_SUBSET, p->data);
					gl = compose_glyph(gl, p->mode - 16);

			store:
					printv("enh row %d col %d print 0x%02x/0x%02x -> 0x%04x %c\n",
						active_row, active_column, p->mode, p->data,
						gl, glyph2latin(gl));

					if ((i = inv_column + active_column) >= 40)
						break;

					buf[i].glyph = gl;

					if (type == OBJ_TYPE_PASSIVE) {
						buf[i].foreground = foreground;
						buf[i].background = background;
						buf[i].size = size;
					} else
						if (active_column > max_column)
							max_column = active_column;
				}

				break;
			}
		}
	}

finish:
	active_row += inv_row;

	if (active_row <= 24)
		if (max_column >= min_column)
			memcpy(&pg->data[active_row][min_column], &buf[min_column],
			       (max_column - min_column + 1) * sizeof(attr_char));

	acp = pg->data[0];

	for (active_row = 0; active_row < 24; active_row++) {
		for (active_column = 0; active_column < 40; acp++, active_column++) {
//			printv("%d ", acp->size);

			switch (acp->size) {
			case NORMAL:
				if (active_row < 23
				    && (acp[40].size == DOUBLE_HEIGHT2 || acp[40].size == DOUBLE_SIZE2)) {
					acp[40].glyph = GL_SPACE;
					acp[40].size = NORMAL;
				}

				if (active_column < 39
				    && (acp[1].size == OVER_TOP || acp[1].size == OVER_BOTTOM)) {
					acp[1].glyph = GL_SPACE;
					acp[1].size = NORMAL;
				}

				break;

			case DOUBLE_HEIGHT:
				ac = acp[0];
				ac.size = DOUBLE_HEIGHT2;
				acp[40] = ac;
				break;

			case DOUBLE_SIZE:
				ac = acp[0];
				ac.size = DOUBLE_SIZE2;
				acp[40] = ac;
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

	return TRUE;
}

static inline bool
default_object_invocation(struct fmt_page *pg)
{
	magazine *mag = pg->magazine;
	pop_link *pop;
	int i, order;

	if (!(i = mag->pop_lut[pg->vtp->pgno & 0xFF]))
		return FALSE; /* has no link (yet) */

	pop = mag->pop_link + i + 8;

	if (max_level < LEVEL_3p5 || NO_PAGE(pop->pgno)) {
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

		trip = resolve_obj_address(pg->vtp->vbi, type, pop->pgno,
			pop->default_obj[i ^ order].address, PAGE_FUNCTION_POP);

		if (!trip)
			return FALSE;

		enhance(pg, type, trip, 0, 0);
	}

	return TRUE;
}



void
fmt_page(int reveal,
	struct fmt_page *pg, struct vt_page *vtp,
	int display_rows)
{
	char buf[16];
	magazine *mag;
	vt_extension *ext;
	int column, row, i;

	printv("\nFormatting page %03x/%04x\n", vtp->pgno, vtp->subno);

	pg->vtp = vtp;

	pg->magazine =
	mag = (max_level <= LEVEL_1p5) ? vtp->vbi->magazine
		: vtp->vbi->magazine + (vtp->pgno >> 8);

	if (!(ext = vtp->data.lop.extension))
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

	screen_colour(pg, ext->def_screen_colour);

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



	sprintf(buf, "\2%x.%02x\7", vtp->pgno, vtp->subno & 0xff);


	if (display_rows > 1 && vtp->function == PAGE_FUNCTION_LOP)
		display_rows = vtp->data.lop.flof ? 25 : 24;
	else
		display_rows = 1;

	i = 0;
	reveal = !!reveal;
	pg->double_height_lower = 0;

	for (row = 0; row < display_rows; row++) {
		font_descriptor *font;
		int mosaic_glyphs;
		int held_mosaic_glyph;
		bool hold, mosaic;
		bool double_height, wide_char;
		attr_char ac;

		held_mosaic_glyph = GL_CONTIGUOUS_BLOCK_MOSAIC_G1 + 0; /* blank */

		memset(&ac, 0, sizeof(ac));

		ac.foreground	= ext->foreground_clut + WHITE;
		ac.background	= ext->background_clut + BLACK;
		mosaic_glyphs	= GL_CONTIGUOUS_BLOCK_MOSAIC_G1;
		ac.opacity	= pg->page_opacity[row > 0];
		font		= pg->font[0];
		reveal	       |= 2;
		hold		= FALSE;
		mosaic		= FALSE;

		double_height	= FALSE;
		wide_char	= FALSE;

		for (column = 0; column < W; ++column) {
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
				reveal &= 1;
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
					ac.glyph = reveal ? held_mosaic_glyph : GL_SPACE;
				} else
					ac.glyph = reveal ? glyph_lookup(font->G0, font->subset, raw) : GL_SPACE;

			if (!wide_char) {
				pg->data[row][column] = ac;

				wide_char = /*!!*/(ac.size & DOUBLE_WIDTH);

				if (wide_char && column < 39) {
					attr_char t = ac;

					t.size = OVER_TOP;
					pg->data[row][column + 1] = t;
				}
			} else
				wide_char = FALSE;

			/* set-after spacing attributes */

			switch (raw) {
			case 0x00 ... 0x07:	/* alpha + foreground colour */
				ac.foreground = ext->foreground_clut + (raw & 7);
				mosaic_glyphs = GL_CONTIGUOUS_BLOCK_MOSAIC_G1;
				reveal |= 2;
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
				reveal |= 2;
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
			for (column = 0; column < W; column++) {
				ac = pg->data[row][column];

				switch (ac.size) {
				case DOUBLE_HEIGHT:
					ac.size = DOUBLE_HEIGHT2;
					pg->data[row + 1][column] = ac;
					break;
		
				case DOUBLE_SIZE:
					ac.size = DOUBLE_SIZE2;
					pg->data[row + 1][column] = ac;
					ac.size = OVER_BOTTOM;
					pg->data[row + 1][++column] = ac;
					break;

				default: /* NORMAL, DOUBLE_WIDTH, OVER_TOP */
					ac.size = NORMAL;
					ac.glyph = GL_SPACE;
					pg->data[row + 1][column] = ac;
					break;
				}
			}

			i += 40;
			row++;

			pg->double_height_lower |= 1 << row;
		}
	}

	if (row < 25) {
		attr_char ac;

		memset(&ac, 0, sizeof(ac));

		ac.foreground	= ext->foreground_clut + WHITE;
		ac.background	= ext->background_clut + BLACK;
		ac.opacity	= pg->page_opacity[1];
		ac.glyph	= GL_SPACE;

		for (i = row * W; i < 25 * W; i++)
			pg->data[0][i] = ac;
	}

	/* Local enhancement data and objects */

	if (max_level >= LEVEL_1p5) {
		struct fmt_page page;
		bool success;

		memcpy(&page, pg, sizeof(struct fmt_page));

		for (i = 0; i < 25; i++)
			pg->row_colour[i] = ext->def_row_colour;

		if (!(vtp->flags & (C5_NEWSFLASH | C6_SUBTITLE))) {
			pg->boxed_opacity[0] = TRANSPARENT_SPACE;
			pg->boxed_opacity[1] = TRANSPARENT_SPACE;
		}

		if (vtp->enh_lines & 1) {
			printv("enhancement packets %08x\n", vtp->enh_lines);
			success = enhance(pg, LOCAL_ENHANCEMENT_DATA, vtp->data.lop.triplet, 0, 0);
		} else
			success = default_object_invocation(pg);

		if (success) {
			// XXX row colour, TRANSPARENT_BLACK
		} else
			memcpy(pg, &page, sizeof(struct fmt_page));
	}

{
	int page;

	page = vtp->pgno & 15;
	if (page > 9) page = 1000;
	page |= ((vtp->pgno >> 4) & 15) * 10;
	if (page > 99) page = 1000;
	page |= ((vtp->pgno >> 8) & 15) * 100;

	if (page < 899)
	    printf("PAGE BTT: %d\n", vtp->vbi->btt[page]);
}

#if TEST
	for (row = 1; row < 24; row++)
		for (column = 0; column < 40; column++) {
			int page = ((vtp->pgno >> 4) & 15) * 10 + (vtp->pgno & 15);

			if (page <= 15) {
				if (row <= 23 && column <= 31) {
					pg->data[row][column].foreground = WHITE;
					pg->data[row][column].background = BLACK;
					pg->data[row][column].size = NORMAL;
					pg->data[row][column].glyph =
						compose_glyph((row - 1) * 32 + column, (page & 15));
				}
			} else if (page == 16) {
				if (row <= 14 && column <= 12) {
					pg->data[row][column].foreground = WHITE;
					pg->data[row][column].background = BLACK;
					pg->data[row][column].size = NORMAL;
					pg->data[row][column].glyph =
						national_subst[row - 1][column];
				}
			}
		}
#endif

#if 0 /* ascii test */
	/*
	 *  NB: No mosaics, no drcs, Latin only (-> glyph2unicode),
	 *      double height/width chars replicate.
	 */
	for (row = 0; row < 25; row++) {
		for (column = 0; column < W; column++)
			putchar(glyph2latin(pg->data[row][column].glyph));
		putchar('\n');
	}
#endif

	for (row = 0; row < display_rows; row++)
		for (column = 0; column < W; column++) {
			int page, subpage;

			if (!vbi_resolve_page(column, row, vtp, &page,
					      &subpage, pg))
				page = subpage = 0;
			pg->data[row][column].link_page = page;
			pg->data[row][column].link_subpage = subpage;
		}
}






int
export(struct export *e, struct vt_page *vtp, char *name)
{
    struct fmt_page pg[1];

    fmt_page(e->reveal, pg, vtp, 24);
    return e->mod->output(e, name, pg);
}
