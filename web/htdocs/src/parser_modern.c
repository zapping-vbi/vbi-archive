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

#define theme "modern"
#include "parser_common.h"

enum {
  NAME,
  XML,
  HTML
};

static char *pages[][3] = 
{
  {"index", "index_" theme ".xml", "../index_" theme ".php"},
  {"screenshots", "screenshots.xml", "../screenshots_" theme ".php"},
  {"download", "download.xml", "../download_" theme ".php"},
  {"contact", "contact_" theme ".xml", "../contact_" theme ".php"},
  {"links", "links_" theme ".xml", "../links_" theme ".php"},
  {"changelog", "changelog.xml", "../changelog_" theme ".php"}
};

#define num_pages (sizeof(pages)/sizeof(*pages))

static void
parse_header(xmlNodePtr node, xmlDocPtr doc, FILE *fp)
{
  xmlChar *label = xmlNodeListGetString(doc, node->children, 1);

  if (!label)
    return;

  fprintf(fp, "<h2>%s</h2>", label);
}

static int abstract_count=0;

static void
my_write_abstract(FILE *fp, const char *docname, const char *abstract)
{
  if (abstract_count++)
    fprintf(fp, "<a name=\"%s\"></a>\n", docname);
}

static void
parse(xmlDocPtr doc, FILE *fp, int index)
{
  xmlChar *title, *abstract;
  xmlNodePtr node = xmlDocGetRootElement(doc);
  time_t now = time(NULL);
  const char *xml = pages[index][XML];
  int i;

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

  print("abstract1");

  if (abstract)
    print_nonbreak(abstract);
  else if (title)
    print_nonbreak(title);
  else
    print_nonbreak(pages[index][NAME]);
  
  print("abstract2");

  print("body1");

  for (i = 0; i<num_pages; i++)
    if (i != index)
      {
	print("link1");
	fprintf(fp, "\"%s.php\">%c", pages[i][NAME],
		toupper(pages[i][NAME][0]));
	print_nonbreak(&pages[i][NAME][1]);
	print("link2");
      }

  print("navbar_stuff");

  parse_contents(doc, fp, pages[index][NAME]);

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

  write_abstract = my_write_abstract;
  init_parser();

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
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html>
½header1
  <head>
    <title>½
½header2
</title>
  <style type="text/css"><!--
    select.theme {
	background-color:#fff;
	border:1px #666666 solid;
	font-size: 10px;
	color: #666666;
	font-family: arial,sans-serif;
    }
    input.theme {
	font-size: 10px;
	font-family: arial,sans-serif;
	margin-top: 1px;
	margin-bottom: 1px;
    }
    a {
	text-decoration: none;
	color: yellow;
    }
    a:hover {
	text-decoration: underline;
	color: white;
    }
    body {
	margin: 0;
	background-color: #464646;
    }
    h1 {
	color: white;
    }
    h2 {
	color: yellow;
	background-color: #666666;
    }
    h6 {
	font-family: western, fantasy;
	color: #888888;
    }
    tbody {
	color: white;
    }
    td.contents {
	border-style: ridge;
	padding: 2;
    }
    .table {
	border-style: ridge;
	padding: 2;
    }
  --></style>
  </head>

½abstract1
  <body link="yellow" vlink="yellow" text="yellow" bgcolor="#464646">
    <table border="0" width="100%" cellspacing="0" cellpadding="0" align="center">
      <tr>
        <td width="100%">
          <table border="0" width="100%" cellspacing="0" cellpadding="0">
            <tr>
              <td width="390"><img border="0" src="images_modern/top-left.gif" width="390" height="108" alt=""></td>
              <td align="left"><h1>½
½abstract2
</h1></td>
            </tr>
          </table>
        </td>
      </tr>

½body1
      <tr>
        <td width="100%">
          <table border="0" width="100%" cellspacing="0" cellpadding="0">
            <tr>
              <td valign="top">
½link1
                <!-- navbar link -->
                <table border="0" cellspacing="0" cellpadding="0" width="100%">
                  <tr>
                    <td colspan="3"><img src="images_modern/button1_nw.gif" width="120" height="8" alt=""><td>
                  </tr>
                  <tr>
                    <td><img src="images_modern/button1_w.gif" width="71" height="30" alt=""></td>
                    <td bgcolor="#646464" align="right" width="100%">
                    <a href=½
½link2
</a></td>
                    <td><img src="images_modern/button1_e.gif" width="14" height="30" alt=""></td>
                  </tr>
                  <tr>
                    <td colspan="3"><img src="images_modern/button1_sw.gif" width="120" height="12" alt=""></td>
                  </tr>
                </table>

½navbar_stuff
                <!-- navbar stuff -->
                <table border="0" cellspacing="0" cellpadding="0" width="120">
                  <tr>
                    <td align="center" bgcolor="#5b5b5b">
                      <p><a href="http://www.guistuff.com"><img border="0" src="images_modern/gui.gif" width="105" height="78" alt="guistuff"></a></p>
                      <p><a href="http://validator.w3.org/check/referer"><img border="0" src="valid-html401.gif" alt="Valid HTML 4.01!" height="31" width="88"></a></p>
		      <form action="<?php echo $PHP_SELF ?>" method="POST">
		      <select name="sel_theme" class="theme" title="Select your theme here">
		      <option value="simple" <?php if ($theme=="simple") echo "selected" ?>>Simple</option>
		      <option value="carsten" <?php if ($theme=="carsten") echo "selected" ?>>Carsten</option>
		      <option value="modern" <?php if ($theme=="modern") echo "selected" ?>>Modern</option>
		      </select>
		      <input type="submit" name="Submit" value="Change" class="theme">
		      </form>
                    </td>
                  </tr>
                </table>
              </td>
              <td class="contents" width="100%" valign="top">½
½THE_REST
              </td>
            </tr>
          </table>
        </td>
      </tr>
    </table>
  </body>
</html>
½
#endif
