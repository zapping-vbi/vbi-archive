/* gcc -ounicode unicode.c ure.c -DTEST -lunicode */
/* libunicode: ftp://ftp.gnome.org/pub/GNOME/unstable/sources */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <glib.h>

#include "ucs-2.h"

// #define TEST 1

static int do_ucs2_swap = -1; /* INTERNAL not supported, going the
				 hard way */
/* charset passed to iconv, use native endianness (not supported by
   glibc 2.1.2, but libiconv does) */
static char *ucs2_label = "UCS-2-INTERNAL";
static ucs2_t *cb=NULL; /* conversion buffer */
static size_t cbs=0; /* size of the conversion buffer */

#define slw(b) (((b&0xff)<<8)+((b&0xff00)>>8))

size_t ucs2_strlen(const void *string)
{
  ucs2_t *p = (ucs2_t *)string;
  size_t i=0;

  if (!string)
    return 0;

  for (i=0; *p; i++) p++;

  return i;
}

/* XXX replace this all by glib */

void *
convert (const void *string, int bytes,
	 const char *input, const char *output)
{
  if (!string) return NULL;

  return g_convert (string, bytes,
		    input, output, NULL, NULL, NULL);
}

static void
ucs2_endianness_workaround(void)
{
  /* first try the explicit UCS-2 mode */
  ucs2_t *converted =
    convert("b", 1, "ISO-8859-1", "UCS-2-INTERNAL");

  /* UCS-2-INTERNAL not supported, try UCS-2 */
  if (!converted)
    {
      ucs2_label = "UCS-2";
      converted = convert("b", 1, "ISO-8859-1", "UCS-2");
    }

  if (!converted)
    return; /* UCS-2 not supported, iconv broken */

  do_ucs2_swap = 0;

  if (*converted != 'b') /* check endianness */
    {
      if (slw(*converted) == 'b')
	do_ucs2_swap = 1;
      else
	fprintf(stderr, "Warning:: iconv UCS-2 implementation broken\n");
    }

  g_free(converted);
}

static void*
code2ucs2 (const void *string, const char *code)
{
  ucs2_t *converted;

  if (do_ucs2_swap < 0)
    if (!startup_ucs2())
      return NULL;

  converted = convert(string, strlen(string), code, ucs2_label);

  if (do_ucs2_swap>0)
    swab(converted, converted, ucs2_strlen(converted)*2);
  
  return converted;
}

static void*
ucs22code (const void *string, const char *code)
{
  size_t len = ucs2_strlen(string)*2;

  if (do_ucs2_swap < 0)
    if (!startup_ucs2())
      return NULL;

  if (!len)
    return NULL;

  if (do_ucs2_swap>0)
    {
      if (len > cbs)
	{
	  if (!(cb = realloc(cb, len)))
	    {
	      cbs = 0;
	      goto fallback;
	    }
	  cbs = len;
	}
      if (cb)
	{
	  swab(string, cb, len);
	  return convert(cb, len, ucs2_label, code);
	}
    }

 fallback:
  return convert(string, len, ucs2_label, code);
}

void*
local2ucs2 (const void *string)
{
  const char *local = get_locale_charset();

  if (!local)
    local = "ISO-8859-1";

  return code2ucs2 (string, local);
}

void*
ucs22local (const void *string)
{
  const char *local = get_locale_charset();

  if (!local)
    local = "ISO-8859-1";

  return ucs22code (string, local);
}

void*
latin2ucs2 (const void *string)
{
  return code2ucs2 (string, "ISO-8859-1");
}

void*
ucs22latin (const void *string)
{
  return ucs22code (string, "ISO-8859-1");
}

char*
local2utf8 (const char *string)
{
  const char *local = get_locale_charset();

  if (!local)
    local = "ISO-8859-1";

  return convert(string, strlen(string),
		 local, "UTF-8");
}

void *
utf82ucs2 (const char *string)
{
  return code2ucs2(string, "UTF-8");
}

#if TEST
int main(int argc, char *argv[])
{
  ucs2_t *in =
    latin2ucs2("If you have suggestions, etc. please mail "
	       "\"nobody@nobody.net\"\n"
	       "This program has been brought to you by"
	       " \"http://www.echelon.gov/show.cgi?everything=1\".");
  char *out;
  ure_buffer_t ub = ure_buffer_create();
  ure_dfa_t ud;
  unicode_char_t c=0;
  char *url = "https?://([:alnum:]|[-~./?%_=+])+"; // URL regexp
  char *email = "([:alnum:]|[-~.])+@([:alnum:]|[-~.])+"; // Email regexp
  ucs2_t *pattern = latin2ucs2(url);
  unsigned long ms, me;

  if (!startup_ucs2())
    {
      fprintf(stderr, "UCS-2 couldn't be started, exitting...\n");
      return 1;
    }

  fprintf(stderr, "compiling regex...\n");
  ud = ure_compile(pattern, ucs2_strlen(pattern), 0, ub);
  if (!ud)
    fprintf(stderr, "Compile failed!\n");

  if (ud)
    {
      ucs2_t *p;
      fprintf(stderr, "searching...\n");
      if (ure_exec(ud, 0, in, ucs2_strlen(in), &ms, &me))
	{
	  fprintf(stderr, "match: <");
	  for (p=in+ms; ms < me; ms++, p++)
	    fprintf(stderr, "%c", (char)*p);
	  fprintf(stderr, ">\n");
	}
      ure_write_dfa(ud, stdout);
      ure_dfa_free(ud);
    }

  out = ucs22latin(in);

  free(in);
  free(out);

  ure_buffer_free(ub);

  shutdown_ucs2();

  return 0;
}
#endif

int
startup_ucs2(void)
{
  ucs2_endianness_workaround();

  if (do_ucs2_swap < 0)
    return 0;

  return 1;
}

void
shutdown_ucs2(void)
{
  if (cb)
    free(cb);

  cb = NULL;
  cbs = 0;
  do_ucs2_swap = -1;
}

static const char* locale_charset = NULL;

const char* get_locale_charset (void)
{
  extern int g_get_charset(const char **);

  if (locale_charset)
    return locale_charset;

  g_get_charset (&locale_charset);

  return locale_charset;
}
