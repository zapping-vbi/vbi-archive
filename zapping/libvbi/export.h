#ifndef EXPORT_H
#define EXPORT_H

#include "vt.h"
#include "misc.h"
#include "lang.h"
#include "../common/types.h"

typedef enum {
	BLACK, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE,
} colours;

typedef enum {
	TRANSPARENT_SPACE, TRANSPARENT, SEMI_TRANSPARENT, OPAQUE
} opacity;

/*
	N	DW  OT	    DH	    DS  OT
			    DH2	    DS2 OB
 */
typedef enum {
	NORMAL,	DOUBLE_WIDTH, DOUBLE_HEIGHT, DOUBLE_SIZE,
	OVER_TOP, OVER_BOTTOM, DOUBLE_HEIGHT2, DOUBLE_SIZE2
} glyph_size;

typedef struct fmt_char {
	unsigned	underline	: 1;
	unsigned	bold		: 1;
	unsigned	italic		: 1;
	unsigned	flash		: 1;
	unsigned	size		: 8;
	unsigned	opacity		: 8;
	unsigned	foreground	: 8;	// 5
	unsigned	background	: 8;	// 5
	unsigned	glyph		: 32;
	unsigned	link_page	: 16;
	unsigned	link_subpage	: 8;
} attr_char;

struct fmt_page
{
	struct vt_page *vtp;
	struct fmt_char data[H][W];

	rgba			screen_colour;
	opacity			screen_opacity;

	rgba *			colour_map;

	unsigned char *		drcs_clut;		/* 64 entries */
	unsigned char *		drcs[32];		/* 16 * 48 * 12 * 10 nibbles, LSN first */

	/* private */

	magazine *		magazine;
	font_descriptor	*	font[2];
	opacity			page_opacity[2];
	opacity			boxed_opacity[2];

	unsigned int		double_height_lower;
	unsigned char		row_colour[25];

	/* XXX need a page update flag,
	 * a) drcs or objects unresolved, redraw when available (consider long
         *    refresh cycles, eg. displayed page is a subpage), we want to
	 *    display asap and don't know if references can be resolved at all 
	 * b) referenced page changed, unlikely but possible
	 * c) displayed page changed
	 * NB source can be any page and subpage.
	 */
};

struct export
{
    struct export_module *mod;	// module type
    char *fmt_str;		// saved option string (splitted)
    // global options
    int reveal;			// reveal hidden chars
    // local data for module's use.  initialized to 0.
    struct { int dummy; } data[0];
};

struct export_module
{
    char *fmt_name;		// the format type name (ASCII/HTML/PNG/...)
    char *extension;		// the default file name extension
    char **options;		// module options
    int local_size;
    int (*open)(struct export *fmt);
    void (*close)(struct export *fmt);
    int (*option)(struct export *fmt, int opt, char *arg);
    int (*output)(struct export *fmt, char *name, struct fmt_page *pg);
};


extern struct export_module *modules[];	// list of modules (for help msgs)
void export_error(char *str, ...);	// set error
char *export_errstr(void);		// return last error
char *export_mkname(struct export *e, char *fmt, struct vt_page *vtp, char *usr);

struct export *export_open(char *fmt);
void export_close(struct export *e);
int export(struct export *e, struct vt_page *vtp, char *user_str);

/* formats the vtp page into a more easily usable format */
//void
//fmt_page(int reveal, struct fmt_page *pg, struct vt_page *vtp, );

/*
  renders the formatted page into mem (1byte per pixel, paletted) and
  stores in width and height the dimensions in pixels.
  Returns a newly allocated buffer holding the image.
    {mhs} output is 0xAABBGGRR now
*/
unsigned int *
mem_output(struct fmt_page *pg, int *width, int *height);

#define dec2hex(dec) \
  (((int)(dec)%10) + ((((int)(dec)/10)%10)<<4) + ((((int)(dec)/100)%10)*256))
#define hex2dec(hex) \
  (((int)(hex)&0xf) + (((int)(hex)>>4)&0xf)*10 + (((int)(hex)>>8)&0xf)*100)
#endif
