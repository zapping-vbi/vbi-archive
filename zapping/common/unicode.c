/* gcc -ounicode unicode.c ure.c -DTEST -lunicode */
/* libunicode: ftp://ftp.gnome.org/pub/GNOME/unstable/sources */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <iconv.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
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

void *
convert (const void *string, int bytes,
	 const char *input, const char *output)
{
  iconv_t ic;
  char *new, *ib, *ob;
  size_t ibl, obl;

  if (!string) return NULL;
  
  ic = iconv_open (output, input);
  if (ic == (iconv_t) -1) return NULL;
  
  ib = (char *) string;
  ibl = bytes;
  new = ob = (char*) calloc (1, sizeof(char) * (ibl * 6 + 2));
  obl = ibl * 6 + 2;

  iconv (ic, (void *) &ib, &ibl, (void *) &ob, &obl);
  
  *((unsigned short*)ob) = 0;
  
  iconv_close(ic);
  
  return new;
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

  free(converted);
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
  unicode_init();
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

/* This comes from libiconv 1.5.1, written by Bruno Haible */

static const char* locale_charset = NULL;
#define streq(s1,s2) (!strcmp(s1,s2))

const char* get_locale_charset (void)
{
  // When you call setlocale(LC_CTYPE,""), is examines the environment
  // variables:
  // 1. environment variable LC_ALL - an override for all LC_* variables,
  // 2. environment variable LC_CTYPE,
  // 3. environment variable LANG - a default for all LC_* variables.
  const char * locale;

  if (locale_charset)
    return locale_charset;

  locale = getenv("LC_ALL");
  if (!locale || !*locale) {
    locale = getenv("LC_CTYPE");
    if (!locale || !*locale)
      locale = getenv("LANG");
  }
  if (locale && *locale) {
    // The most general syntax of a locale (not all optional parts
    // recognized by all systems) is
    // language[_territory][.codeset][@modifier][+special][,[sponsor][_revision]]
    // To retrieve the codeset, search the first dot. Stop searching when
    // a '@' or '+' or ',' is encountered.
    char* buf = (char*) malloc(strlen(locale)+1);
    const char* codeset = NULL;
    {
      const char* cp = locale;
      for (; *cp != '\0' && *cp != '@' && *cp != '+' && *cp != ','; cp++) {
	if (*cp == '.') {
	  codeset = ++cp;
	  for (; *cp != '\0' && *cp != '@' && *cp != '+' && *cp != ','; cp++);
	  if (*cp != '\0') {
	    size_t n = cp - codeset;
	    memcpy(buf,codeset,n);
	    buf[n] = '\0';
	    codeset = buf;
	  }
	  break;
	}
      }
    }
    if (codeset) {
      // Canonicalize the charset given after the dot.
      if (   streq(codeset,"ISO8859-1")
	     || streq(codeset,"ISO_8859-1")
	     || streq(codeset,"iso88591")
	     || streq(codeset,"88591")
	     || streq(codeset,"88591.en")
	     || streq(codeset,"8859")
	     || streq(codeset,"8859.in")
	     || streq(codeset,"ascii")
             )
	locale_charset = "ISO-8859-1";
      else
	if (   streq(codeset,"ISO8859-2")
	       || streq(codeset,"ISO_8859-2")
	       || streq(codeset,"iso88592")
	       )
	  locale_charset = "ISO-8859-2";
	else
          if (   streq(codeset,"ISO8859-5")
		 || streq(codeset,"ISO_8859-5")
		 || streq(codeset,"iso88595")
		 )
            locale_charset = "ISO-8859-5";
          else
	    if (   streq(codeset,"ISO8859-6")
		   || streq(codeset,"ISO_8859-6")
		   || streq(codeset,"iso88596")
		   )
	      locale_charset = "ISO-8859-6";
	    else
	      if (   streq(codeset,"ISO8859-7")
		     || streq(codeset,"ISO_8859-7")
		     || streq(codeset,"iso88597")
		     )
		locale_charset = "ISO-8859-7";
	      else
		if (   streq(codeset,"ISO8859-8")
		       || streq(codeset,"iso88598")
		       )
		  locale_charset = "ISO-8859-8";
		else
		  if (   streq(codeset,"ISO8859-9")
			 || streq(codeset,"ISO_8859-9")
			 || streq(codeset,"iso88599")
			 )
		    locale_charset = "ISO-8859-9";
		  else
		    if (streq(codeset, "ISO8859-15")
			|| streq(codeset, "ISO_8859-15")
			|| streq(codeset, "iso885915"))
		      locale_charset = "ISO-8859-15";
		    else
		      if (streq(codeset,"KOI8-R"))
			locale_charset = "KOI8-R";
		      else
			if (streq(codeset,"KOI8-U"))
			  locale_charset = "KOI8-U";
			else
			  if (   streq(codeset,"eucJP")
				 || streq(codeset,"ujis")
				 || streq(codeset,"AJEC")
				 )
			    locale_charset = "eucJP";
			  else
			    if (   streq(codeset,"JIS7")
				   || streq(codeset,"jis7")
				   || streq(codeset,"JIS")
				   || streq(codeset,"ISO-2022-JP")
				   )
			      locale_charset = "ISO-2022-JP"; /* was: "JIS7"; */
			    else
			      if (   streq(codeset,"SJIS")
				     || streq(codeset,"mscode")
				     || streq(codeset,"932")
				     )
				locale_charset = "SJIS";
			      else
				if (   streq(codeset,"eucKR")
				       || streq(codeset,"949")
				       )
				  locale_charset = "eucKR";
				else
				  if (streq(codeset,"eucCN"))
				    locale_charset = "eucCN";
				  else
				    if (streq(codeset,"eucTW"))
				      locale_charset = "eucTW";
				    else
				      if (streq(codeset,"TACTIS"))
					locale_charset = "TIS-620"; /* was: "TACTIS"; */
				      else
					if (streq(codeset,"EUC") || streq(codeset,"euc")) {
					  if (locale[0]=='j' && locale[1]=='a')
					    locale_charset = "eucJP";
					  else if (locale[0]=='k' && locale[1]=='o')
					    locale_charset = "eucKR";
					  else if (locale[0]=='z' && locale[1]=='h' && locale[2]=='_') {
					    if (locale[3]=='C' && locale[4]=='N')
					      locale_charset = "eucCN";
					    else if (locale[3]=='T' && locale[4]=='W')
					      locale_charset = "eucTW";
					  }
					}
					else
					  // The following are CLISP extensions.
					  if (   streq(codeset,"UTF-8")
						 || streq(codeset,"utf8")
						 )
					    locale_charset = "UTF-8";
					  else /* hope that libxml
						  understands this */
					    locale_charset = strdup(codeset);
    } else {
      // No dot found. Choose a default, based on locale.
      if (   streq(locale,"iso_8859_1")
	     || streq(locale,"ISO8859-1")
	     || streq(locale,"ISO-8859-1")
             )
	locale_charset = "ISO-8859-1";
      else {
	// Choose a default, based on the language only.
	const char* underscore = strchr(locale,'_');
	const char* lang;
	if (underscore) {
	  size_t n = underscore - locale;
	  memcpy(buf,locale,n);
	  buf[n] = '\0';
	  lang = buf;
	} else {
	  lang = locale;
	}
	if (   streq(lang,"af") || streq(lang,"afrikaans")
	       || streq(lang,"ca") || streq(lang,"catalan")
	       || streq(lang,"da") || streq(lang,"danish") || streq(lang,"dansk")
	       || streq(lang,"de") || streq(lang,"german") || streq(lang,"deutsch")
	       || streq(lang,"en") || streq(lang,"english")
	       || streq(lang,"es") || streq(lang,"spanish")
#ifndef ASCII_CHS
	       || streq(lang,"espa\361ol") || streq(lang,"espa\303\261ol") // español
#endif
	       || streq(lang,"eu") || streq(lang,"basque")
	       || streq(lang,"fi") || streq(lang,"finnish")
	       || streq(lang,"fo") || streq(lang,"faroese") || streq(lang,"faeroese")
	       || streq(lang,"fr") || streq(lang,"french")
#ifndef ASCII_CHS
	       || streq(lang,"fran\347ais") || streq(lang,"fran\303\247ais") // français
#endif
	       || streq(lang,"ga") || streq(lang,"irish")
	       || streq(lang,"gd") || streq(lang,"scottish")
	       || streq(lang,"gl") || streq(lang,"galician")
	       || streq(lang,"is") || streq(lang,"icelandic")
	       || streq(lang,"it") || streq(lang,"italian")
	       || streq(lang,"nl") || streq(lang,"dutch")
	       || streq(lang,"no") || streq(lang,"norwegian")
	       || streq(lang,"pt") || streq(lang,"portuguese")
	       || streq(lang,"sv") || streq(lang,"swedish")
	       )
	  locale_charset = "ISO-8859-1";
	else
	  if (   streq(lang,"cs") || streq(lang,"czech")
		 || streq(lang,"cz")
		 || streq(lang,"hr") || streq(lang,"croatian")
		 || streq(lang,"hu") || streq(lang,"hungarian")
		 || streq(lang,"pl") || streq(lang,"polish")
		 || streq(lang,"ro") || streq(lang,"romanian") || streq(lang,"rumanian")
		 || streq(lang,"sh") /* || streq(lang,"serbocroatian") ?? */
		 || streq(lang,"sk") || streq(lang,"slovak")
		 || streq(lang,"sl") || streq(lang,"slovene") || streq(lang,"slovenian")
		 || streq(lang,"sq") || streq(lang,"albanian")
		 )
	    locale_charset = "ISO-8859-2";
	  else
	    if (   streq(lang,"eo") || streq(lang,"esperanto")
		   || streq(lang,"mt") || streq(lang,"maltese")
		   )
	      locale_charset = "ISO-8859-3";
	    else
	      if (   streq(lang,"be") || streq(lang,"byelorussian")
		     || streq(lang,"bg") || streq(lang,"bulgarian")
		     || streq(lang,"mk") || streq(lang,"macedonian")
		     || streq(lang,"sp")
		     || streq(lang,"sr") || streq(lang,"serbian")
		     )
		locale_charset = "ISO-8859-5";
	      else
		if (streq(lang,"ar") || streq(lang,"arabic")
		    )
		  locale_charset = "ISO-8859-6";
		else
		  if (streq(lang,"el") || streq(lang,"greek")
		      )
		    locale_charset = "ISO-8859-7";
		  else
		    if (streq(lang,"iw") || streq(lang,"he") || streq(lang,"hebrew")
			)
		      locale_charset = "ISO-8859-8";
		    else
		      if (streq(lang,"tr") || streq(lang,"turkish")
			  )
			locale_charset = "ISO-8859-9";
		      else
			if (   streq(lang,"et") || streq(lang,"estonian")
			       || streq(lang,"lt") || streq(lang,"lithuanian")
			       || streq(lang,"lv") || streq(lang,"latvian")
			       )
			  locale_charset = "ISO-8859-10";
			else
			  if (streq(lang,"ru") || streq(lang,"russian")
			      )
			    locale_charset = "KOI8-R";
			  else
			    if (streq(lang,"uk") || streq(lang,"ukrainian")
				)
			      locale_charset = "KOI8-U";
			    else
			      if (   streq(lang,"ja")
				     || streq(lang,"Jp")
				     || streq(lang,"japan")
				     || streq(lang,"Japanese-EUC")
				     )
				locale_charset = "eucJP";
			      else
				if (0)
				  locale_charset = "ISO-2022-JP"; /* was: "JIS7"; */
				else
				  if (streq(lang,"japanese")
				      )
				    locale_charset = "SJIS";
				  else
				    if (streq(lang,"ko") || streq(lang,"korean")
					)
				      locale_charset = "eucKR";
				    else
				      if (streq(lang,"chinese-s")
					  )
					locale_charset = "eucCN";
				      else
					if (streq(lang,"chinese-t")
					    )
					  locale_charset = "eucTW";
					else
					  if (streq(lang,"th")
					      )
					    locale_charset =
					      "TIS-620"; /* was:
							    "TACTIS";
							 */
					else
					  if (streq(lang, "zh"))
					    locale_charset = "GB2313";
					  else {
					  }
      }
    }
    free(buf);
  }
  if (!locale_charset)
    fprintf(stderr, "Couldn't guess the charset for the current locale,\n"
	    "please set LC_ALL, LC_CTYPE or LANG correctly.\n"
	    "If it's correctly set, please send a bug report to the "
	    "mailing list: zapping-misc@lists.sourceforge.net\n");

  return (locale_charset ? locale_charset : "ISO-8859-1");
}
