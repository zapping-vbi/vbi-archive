/**
 * Simple XML->HTML converter for the web page in http://zapping.sf.net
 * Modern theme.
 * (C) 2001 Iñaki García Etxebarria
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <tree.h>
#include <parser.h>
#include <xmlmemory.h>

#define theme "simple"
#include "parser_common.h"

enum {
  NAME,
  XML,
  HTML
};

static char *pages[][3] = 
{
  {"index", "index_" theme ".xml", "../index_" theme ".php"},
  {"screenshots", "screenshots.xml", "../screenshots_" theme ".php"}
};

#define num_pages (sizeof(pages)/sizeof(*pages))

static void
parse_header(xmlNodePtr node, xmlDocPtr doc, FILE *fp)
{
  xmlChar *label = xmlNodeListGetString(doc, node->children, 1);

  if (!label)
    return;

  fprintf(fp, "<center><h3>%s</h3></center>", label);
}

static int abstract_count = 0;

static void
my_write_abstract(FILE *fp, const char *docname, const char *abstract)
{
  if (abstract_count++)
    fprintf(fp, "<a name=\"%s\"></a><center><h2>%s</h2></center>\n",
	    docname, abstract);
}

static void
parse(xmlDocPtr doc, FILE *fp, int index)
{
  xmlChar *title, *abstract;
  xmlNodePtr node = xmlDocGetRootElement(doc);
  time_t now = time(NULL);
  const char *xml = pages[index][XML];
  char buf[256];

  abstract_count = 0;

  title = xmlGetProp(node, "title");
  abstract = xmlGetProp(node, "abstract");

  print("doctype");

  fprintf(fp, TAB "<!--\n");
  fprintf(fp, TAB "Generated from %s on %s", xml, ctime(&now));
  fprintf(fp, TAB "-->\n");

  print("header1");

  if (title)
    fprintf(fp, title);
  else
    fprintf(fp, pages[index][NAME]);

  print("header2");

  sprintf(buf, "TOC_%d", index);
  print(buf);

  if (abstract)
    fprintf(fp, "<h1>%s</h1>\n", abstract);

  parse_contents(doc, fp, pages[index][NAME]);

  sprintf(buf, "THE_REST_%d", index);
  print(buf);

  print("THE_REST");

  if (title)
    xmlFree(title);

  if (abstract)
    xmlFree(abstract);
}

int main(int argc, char *argv[])
{
  int i;
  FILE *fp;
  xmlDocPtr doc;
  
  init_parser();
  write_abstract = my_write_abstract;

  for (i=0; i<num_pages; i++)
    {
      if (!(doc = xmlParseFile(pages[i][XML])))
	{
	  fprintf(stderr, "Cannot parse %s, skipping\n", pages[i][XML]);
	  continue;
	}
      if (!xmlDocGetRootElement(doc) ||
	  xmlDocGetRootElement(doc)->type != XML_ELEMENT_NODE)
	{
	  fprintf(stderr, "No valid root element found in %s, skipping\n",
		  pages[i][XML]);
	  xmlFreeDoc(doc);
	  continue;
	}

      if (!(fp = fopen(pages[i][HTML], "w+t")))
	{
	  perror("fopen");
	  exit(1);
	}

      fprintf(stderr, "%s -> %s\n", pages[i][XML], pages[i][HTML]);

      parse(doc, fp, i);

      xmlFreeDoc(doc);
      fclose(fp);
    }

  return 0;
}

#if 0
½doctype
<!doctype html public "-//W3C//DTD HTML 4.01 Transitional//EN"
	"http://www.w3.org/TR/html4/loose.dtd">
<html>
½header1
<head>
    <meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
    <title>½
½header2
</title>
  </head>
<body bgcolor="#FFFFFF">
<center>
[ <a href="http://cvs.sourceforge.net/cgi-bin/viewcvs.cgi/~checkout~/zapping/zapping/ChangeLog?content-type=text/plain">ChangeLog</a> |
<a href="http://sourceforge.net/projects/zapping">Sourceforge
project page</a> |
½TOC_0
<a href="screenshots.php">Screenshots</a> |
<a href="#contact">Contributing</a> ]
<p>
<img SRC="images_simple/logo.jpeg" ALT="[Zapping Logo]" height=168 width=436>
</p>
</center>
½TOC_1
<a href="index.php">Index</a> |
<a href="index.php#contact">Contributing</a> ]
</center>
½THE_REST_0
<p>I would like to thank <a href="http://sourceforge.net"><img
SRC="http://sourceforge.net/sflogo.php?group_id=2599&amp;type=1"
BORDER=0 height=31 width=88 ALT="Sourceforge Logo"></a>
for hosting this site and supporting Open Software
Concept. <b>Thanks</b>.<p>
½THE_REST_1
½THE_REST
<center>
<a href="http://validator.w3.org/check/referer"><img border="0"
src="valid-html401.gif" alt="Valid HTML 4.01" height=31 width="88"></a>
<a href="rescd.html"><img src="images_simple/gtcd.png" border="0"
ALT="ResCD"></a>
<form action="<?php echo $PHP_SELF ?>" method="POST">
</center>
<div align="right">
<select name="sel_theme" title="Select your theme here">
<option value="simple" <?php if ($theme=="simple") echo "selected" ?>>Simple</option>
<option value="carsten" <?php if ($theme=="carsten") echo "selected" ?>>Carsten</option>
<option value="modern" <?php if ($theme=="modern") echo "selected" ?>>Modern</option>
</select><input type="submit" border="0" name="Submit" value="Select Theme">
</div>
</form>
</body>
</html>
½
#endif
