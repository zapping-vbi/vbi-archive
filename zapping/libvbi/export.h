#ifndef EXPORT_H
#define EXPORT_H

#include "vt.h"
#include "misc.h"
#include "lang.h"
#include "../common/types.h"

#include "format.h"

struct fmt_page
{
	struct vt_page *vtp;
	/* XXX volatile */

	attr_char		data[H][W];

	int			reveal;

	attr_rgba		screen_colour;
	attr_opacity		screen_opacity;

	attr_rgba *		colour_map;

	unsigned char *		drcs_clut;		/* 64 entries */
	unsigned char *		drcs[32];		/* 16 * 48 * 12 * 10 nibbles, LSN first */

	font_descriptor	*	font[2];

	/* private */

	vt_pagenum		nav_link[6];
	char			nav_index[W];

	/* private temporary */

	magazine *		magazine;
	vt_extension *		ext;

	attr_opacity		page_opacity[2];
	attr_opacity		boxed_opacity[2];

	unsigned int		double_height_lower;

	/* XXX need a page update flag,
	 * a) drcs or objects unresolved, redraw when available (consider long
         *    refresh cycles, eg. displayed page is a subpage), we want to
	 *    display asap and don't know if references can be resolved at all 
	 * b) referenced page changed, unlikely but possible
	 * c) displayed page changed
	 * NB source can be any page and subpage.
	 */
};

extern int		vbi_format_page(struct fmt_page *pg, struct vt_page *vtp, int display_rows);



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

void vbi_draw_page(struct fmt_page *pg, void *data, int conceal);
void vbi_get_rendered_size(int *w, int *h);



static inline unsigned int
dec2bcd(unsigned int dec)
{
	return (dec % 10) + ((dec / 10) % 10) * 16 + (dec / 100) * 256;
}

static inline unsigned int
bcd2dec(unsigned int bcd)
{
	return (bcd & 15) + ((bcd >> 4) & 15) * 10 + (bcd >> 8) * 100;
}

static inline unsigned int
add_bcd(unsigned int a, unsigned int b)
{
	unsigned int t;

	a += 0x06666666;
	t  = a + b;
	b ^= a ^ t;
	b  = (~b & 0x11111110) >> 3;
	b |= b * 2;

	return (t - b) & 0xFFF;
}		     

#endif /* EXPORT_H */
