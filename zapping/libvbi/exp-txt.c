#include <stdio.h>
#include <string.h>
#include "os.h"
#include "export.h"

static int txt_open(struct export *e);
static int txt_option(struct export *e, int opt, char *arg);
static int txt_output(struct export *e, char *name, struct fmt_page *pg);

static char *txt_opts[] =	// module options
{
    "color",			// generate ansi color codes (and attributes)
    "gfx-chr=<char>",		// substitute <char> for gfx-symbols
    "fg=<0-7|none>",		// assume term has <x> as foreground color
    "bg=<0-7|none>",		// assume term has <x> as background color
    0
};

struct txt_data			// private data in struct export
{
    u8 color;
    u8 gfx_chr;
    u8 def_fg;
    u8 def_bg;
    struct fmt_char curr[1];
    FILE *fp;
};

struct export_module export_txt[1] =	// exported module definition
{
  {
    "ascii",			// id
    "txt",			// extension
    txt_opts,			// options
    sizeof(struct txt_data),	// data size
    txt_open,			// open
    0,				// close
    txt_option,			// option
    txt_output,			// output
  }
};

struct export_module export_ansi[1] =	// exported module definition
{
  {
    "ansi",			// id
    "txt",			// extension
    txt_opts,			// options
    sizeof(struct txt_data),	// data size
    txt_open,			// open
    0,				// close
    txt_option,			// option
    txt_output,			// output
  }
};

///////////////////////////////////////////////////////

#define D  ((struct txt_data *)e->data)


#ifdef BSD
char *
stpcpy(char *dst, const char *src)
{
    while (*dst = *src++)
	dst++;
    return dst;
}
#endif


static int
txt_open(struct export *e)
{
    D->gfx_chr = '#';
    D->def_fg = -1;
    D->def_bg = -1;
    if (e->mod == export_ansi)
	D->color = 1;
    return 0;
}

static int
txt_option(struct export *e, int opt, char *arg)
{
    switch (opt)
    {
	case 1: // color
	    D->color = 1;
	    break;
	case 2: // gfx-chr=
	    D->gfx_chr = *arg ?: ' ';
	    break;
	case 3: // fg=
	    D->def_fg = *arg - '0';
	    break;
	case 4: // bg=
	    D->def_bg = *arg - '0';
	    break;
    }
    return 0;
}

static void
put_attr(struct export *e, struct fmt_char *new)
{
}


static int
txt_output(struct export *e, char *name, struct fmt_page *pg)
{
    struct fmt_char def_c[1];
    struct fmt_char l[W+2];
    #define L (l+1)
    int x, y;

	export_error("cannot create file");
	return -1;
}
