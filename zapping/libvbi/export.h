#ifndef EXPORT_H
#define EXPORT_H

#include "vt.h"
#include "misc.h"
#include "../common/types.h"


typedef enum {
	TRANSPARENT_SPACE, TRANSPARENT, SEMI_TRANSPARENT, OPAQUE
} opacity;

typedef enum {
	BLACK, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE
} colours;

/*
	N	DW  OT	    DH	    DS  OT
			    DH2	    DS2 OB
 */
typedef enum {
	NORMAL,	DOUBLE_WIDTH, DOUBLE_HEIGHT, DOUBLE_SIZE,
	OVER_TOP, OVER_BOTTOM, DOUBLE_HEIGHT2, DOUBLE_SIZE2
} glyph_size;

typedef struct fmt_char
{
    u16 ch;  // alevt
    u8 attr; // alevt
	unsigned	underline	: 1;
	unsigned	flash		: 1;
	unsigned	size		: 3;
	unsigned	opacity		: 2;
	unsigned	foreground	: 5;
	unsigned	background	: 5;
	unsigned	glyph		: 16;
} attr_char;

#define EA_GRAPHIC	16	// graphic symbol
#define EA_SEPARATED	32	// use separated graphic symbol

#define E_DEF_FG	7
#define E_DEF_BG	0
#define E_DEF_ATTR	0

struct fmt_page
{
    struct vt_page *vtp;
    struct fmt_char data[H][W];
    unsigned int colour_map[32];
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
void
fmt_page(int reveal, struct fmt_page *pg, struct vt_page *vtp);

/*
  renders the formatted page into mem (1byte per pixel, paletted) and
  stores in width and height the dimensions in pixels.
  Returns a newly allocated buffer holding the image.
*/
unsigned char *
mem_output(struct fmt_page *pg, int *width, int *height);
#endif
