/* gcc -ounicode unicode.c ure.c -lunicode */
/* libunicode: ftp://ftp.gnome.org/pub/GNOME/unstable/sources */
#include <iconv.h>
#include <unicode.h>
#include <stdio.h>
#include <ctype.h>
#include "ure.h"

#define slw(b) (((b&0xff)<<8)+((b&0xff00)>>8))

static char*
utf8_strstrcase (const char *haystack, const char *needle)
{
	unicode_char_t *nuni;
	unicode_char_t unival;
	int nlen;
	const unsigned char *o, *p;

	if (haystack == NULL) return NULL;
	if (needle == NULL) return NULL;
	if (strlen (needle) == 0) return (char*)haystack;
	if (strlen (haystack) == 0) return NULL;

	nuni = alloca (sizeof (unicode_char_t) * strlen (needle));

	nlen = 0;
	for (p = unicode_get_utf8 (needle, &unival); p && unival; p = unicode_get_utf8 (p, &unival)) {
		nuni[nlen++] = unicode_tolower (unival);
	}
	/* NULL means there was illegal utf-8 sequence */
	if (!p) return NULL;

	o = haystack;
	for (p = unicode_get_utf8 (o, &unival); p && unival; p = unicode_get_utf8 (p, &unival)) {
		int sc;
		sc = unicode_tolower (unival);
		/* We have valid stripped char */
		if (sc == nuni[0]) {
			const char *q = p;
			int npos = 1;
			while (npos < nlen) {
				q = unicode_get_utf8 (q, &unival);
				if (!q || !unival) return NULL;
				sc = unicode_tolower (unival);
				if (sc != nuni[npos]) break;
				npos++;
			}
			if (npos == nlen) {
				return (char*)p;
			}
		}
		o = p;
	}

	return NULL;
}

static void ucs2_print(const void *string)
{
  unsigned short *p = (unsigned short*)string;
  unsigned char c, d;
  unsigned short s;

  if (!string)
    return;

  for (c=' '; (s = *p); p++)
    {
      c = s;
      d = s>>8;
      fprintf(stderr, "%c%c", isprint(c) ? c : '_',
	      isprint(d) ? d : '_');
    }
}

static size_t ucs2_strlen(const void *string)
{
  unsigned short *p = (unsigned short*)string;
  size_t i=0;

  if (!string)
    return 0;

  for (i=0; *p; i++) p++;

  return i;
}

static void*
convert (const void *string, int bytes,
	 const char *input, const char *output)
{
  // TODO : iconv error checking
  iconv_t ic;
  char *new, *ob;
  const char * ib;
  size_t ibl, obl;
  
  if (!string) return NULL;
  
  ic = iconv_open (output, input);
  if (ic == (iconv_t) -1) return NULL;
  
  ib = string;
  ibl = bytes;
  new = ob = (char*) malloc (sizeof(char) * (ibl * 6 + 2));
  obl = ibl * 6 + 2;
  
  //  while (ibl > 0)
    iconv (ic, &ib, &ibl, &ob, &obl);
  
  *((unsigned short*)ob) = 0;
  
  iconv_close(ic);
  
  return new;
}

static void*
utf82latin (const void *string)
{
  return convert(string, strlen(string), "UTF-8", "ISO-8859-1");
}

static void*
latin2utf8 (const void *string)
{
  return convert(string, strlen(string), "ISO-8859-1", "UTF-8");
}

static void*
utf82ucs2 (const void *string)
{
  return convert(string, strlen(string), "UTF-8", "UCS-2");
}

static void*
ucs22utf8 (const void *string)
{
  return convert(string, ucs2_strlen(string)*2, "UCS-2", "UTF-8");
}

static void*
latin2ucs2 (const void *string)
{
  return convert(string, strlen(string), "ISO-8859-1", "UCS-2");
}

static void*
ucs22latin (const void *string)
{
  return convert(string, ucs2_strlen(string)*2, "UCS-2", "ISO-8859-1");
}

ucs4_t
#ifdef __STDC__
_ure_tolower(ucs4_t c)
#else
_ure_tolower(c)
ucs4_t c;
#endif
{
  return unicode_tolower(c);
}

/* libunicode -> ure FIXME: no fscking idea of half of these */
static unsigned long libunicode2ure[] =
{
  _URE_CNTRL,
  _URE_PUA,
  _URE_PUA,
  _URE_PUA,
  _URE_PUA,
  _URE_LOWER | _URE_NONSPACING,
  _URE_MODIFIER,
  _URE_OTHERLETTER,
  _URE_TITLE,
  _URE_UPPER | _URE_NONSPACING,
  _URE_COMBINING,
  _URE_PUA,
  _URE_NONSPACING,
  _URE_NUMDIGIT,
  _URE_NUMOTHER,
  _URE_NUMOTHER,
  _URE_OTHERPUNCT,
  _URE_DASHPUNCT,
  _URE_CLOSEPUNCT,
  _URE_OTHERPUNCT,
  _URE_OTHERPUNCT,
  _URE_OTHERPUNCT,
  _URE_OPENPUNCT,
  _URE_CURRENCYSYM,
  _URE_OTHERSYM,
  _URE_MATHSYM,
  _URE_OTHERSYM,
  _URE_LINESEP,
  _URE_PARASEP,
  _URE_SPACESEP
};

int
#ifdef __STDC__
_ure_matches_properties(unsigned long props, ucs4_t c)
#else
_ure_matches_properties(props, c)
unsigned long props;
ucs4_t c;
#endif
{
  unsigned long up = 0;
  unsigned long type = unicode_type(c);
  unsigned long result;

  if ((type <= UNICODE_SPACE_SEPARATOR) && (type >= 0))
    result = libunicode2ure[type];
  else
    result = 0;

  return (result & props);
}

int main(int argc, char *argv[])
{
  ucs2_t *in = latin2ucs2("Iñaki García Etxeeeebababarria");
  char *out;
  ure_buffer_t ub = ure_buffer_create();
  ure_dfa_t ud;
  unicode_char_t c=0;
  int index = 2;
  ucs2_t *pattern = latin2ucs2("tx[aeiou]*(ba)*rria");
  unsigned long ms, me;

  unicode_init();

  me = ucs2_strlen(in);
  for (ms=0; ms<me; ms++)
    in[ms] = slw(in[ms]);
  me = ucs2_strlen(pattern);
  for (ms=0; ms<me; ms++)
    pattern[ms] = slw(pattern[ms]);

  fprintf(stderr, "compiling regex (%c)...\n", *in == 'I' ? 'Y' : 'N');
  ud = ure_compile(pattern, ucs2_strlen(pattern), 1, ub);
  if (!ud)
    fprintf(stderr, "Compile failed!\n");

  c = in[index];
  fprintf(stderr, "char type: %d\n", unicode_type(c));
  c = unicode_toupper(c);
  in[index] = c;

  if (ud)
    {
      fprintf(stderr, "searching...\n");
      fprintf(stderr, "ure_exec: %d\n",
	      ure_exec(ud, URE_DOT_MATCHES_SEPARATORS,
		       in, ucs2_strlen(in), &ms, &me));
      ure_dfa_free(ud);
    }

  out = ucs22latin(in);
  fprintf(stderr, "%s\n", out);

  free(in);
  free(out);

  ure_buffer_free(ub);

  return 0;
}
