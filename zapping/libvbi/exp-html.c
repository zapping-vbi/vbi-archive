/* Copyright 1999 by Paul Ortyl <ortylp@from.pl> */

#include <stdio.h>
#include <string.h>
#include "lang.h"
#include "export.h"

static int html_open(struct export *e);
static int html_option(struct export *e, int opt, char *arg);
static int html_output(struct export *e, char *name, struct fmt_page *pg);

static char *html_opts[] =	// module options
{
  "gfx-chr=<char>",             // substitute <char> for gfx-symbols
  "bare",                     // no headers  
   0                      
};

struct html_data			// private data in struct export
{
  u8 gfx_chr;
  u8 bare;
};

struct export_module export_html[1] =	// exported module definition
{
  {
    "html",			// id
    "html",			// extension
    html_opts,			// options
    sizeof(struct html_data),	// size
    html_open,			// open
    0,				// close
    html_option,		// option
    html_output			// output
  }
};

#define D  ((struct html_data *)e->data)

static int
html_open(struct export *e)
{
    D->gfx_chr = '#';
    D->bare = 0;
    //e->reveal=1;	// the default should be the same for all formats.
    return 0;
}

static int
html_option(struct export *e, int opt, char *arg)
{
  
  switch (opt)
    {
    case 1: // gfx-chr=
      D->gfx_chr = *arg ?: ' ';
      break;
    case 2: // bare (no headers)
      D->bare=1;
      break;
    }
  return 0;
}

///////////////////////////////////////////////////////

#define HTML_BLACK   "#000000"
#define HTML_RED     "#FF0000"
#define HTML_GREEN   "#00FF00"
#define HTML_YELLOW  "#FFFF00"
#define HTML_BLUE    "#0000FF"
#define HTML_MAGENTA "#FF00FF"
#define HTML_CYAN    "#00FFFF"
#define HTML_WHITE   "#FFFFFF"

#undef UNREADABLE_HTML //no '\n'
#define STRIPPED_HTML   //only necessary fields in header

static int html_output(struct export *e, char *name, struct fmt_page *pg)
{
  return 0;
}
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////	
