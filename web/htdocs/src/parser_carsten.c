/**
 * Simple XML->HTML converter for the web page in http://zapping.sf.net
 * Carsten theme.
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

static const char *theme = "carsten";
#include "parser_common.h"

enum {
  NAME,
  XML,
  HTML
};

static char *source_pages[] =
{
  "index", "screenshots", "download", "news", "bugs", "links",
  "contact", "changelog", "resources"
};

#define num_pages (sizeof(source_pages)/sizeof(*source_pages))

static char pages[num_pages][3][256];

static const char *logo_description[num_pages] =
{
  "Zapping - Logo",
  "Screenshots 4 you",
  "Download",
  "News about Zapping",
  "Found a bug? Report it here",
  "Links",
  "Contact the authors",
  "Changes in Zapping",
  "Some useful things"
};

static void
parse_header(xmlNodePtr node, xmlDocPtr doc, FILE *fp)
{
  xmlChar *label = xmlNodeListGetString(doc, node->children, 1);

  if (!label)
    return;

  fprintf(fp, "<center><h2>%s</h2></center>", label);
}

static void
parse(xmlDocPtr doc, FILE *fp, int index)
{
  xmlChar *title;
  xmlNodePtr node = xmlDocGetRootElement(doc);
  time_t now = time(NULL);
  const char *xml = pages[index][XML];
  char buf[256];
  int i;

  title = xmlGetProp(node, "title");

  print("doctype");

  if (title)
    fprintf(fp,"<head>\n<title>\nZapping -- Linux TV Viewer -- %s\n</title>\n",
	    title);
  else
    fprintf(fp,"<head>\n<title>\nZapping -- Linux TV Viewer -- %s\n</title>\n",
	    pages[index][NAME]);

  fprintf(fp, "<!--\n");
  fprintf(fp, "\tGenerated from %s on %s", xml, ctime(&now));
  fprintf(fp, "-->\n");

  print("header");

  print("TV_SET1");

  if (index)
    fprintf(fp, "<a href=\"index.php\">");
  fprintf(fp, "<img src=\"images_carsten/zapping-tv/channel%02d.%s\" "
	  "width=\"127\" height=\"107\" border=\"0\" "
	  "alt=\"Zapping a TV Viewer for Linux\">", index+1,
	  (index==4) ? "gif" : "jpg");
  if (index)
    fprintf(fp, "</a>");

  print("TV_SET2");

  fprintf(fp, "<img src=\"images_carsten/zapping-tv/ch%02d.jpg\" "
	  "width=\"46\" height=\"35\" border=\"0\" align=left "
	  "hspace=\"0\" vspace=\"0\" alt=\"\">\n", index+1);

  print("TV_SET3");
  print("LOGO1");

  fprintf(fp, "<img src=\"images_carsten/logo/logo-%s.png\" "
	  "width=\"218\" height=\"44\" border=\"0\" alt=\"%s\">",
	  pages[index][NAME], logo_description[index]);

  print("LOGO2");

  print("MENU_OPEN");

  for (i=1; i<num_pages; i++)
    {
      sprintf(buf, "MENU%d%s", i, (i==index)?"_LIGHT":"");
      print(buf);
    }

  print("MENU_CLOSE");
  
  print("TOC1");

  parse_contents(doc, fp, pages[index][NAME]);

  print("THE_REST");

  if (title)
    xmlFree(title);
}

int main(int argc, char *argv[])
{
  int i;
  FILE *fp;
  xmlDocPtr doc;
  
  init_parser();

  for (i = 0; i<num_pages; i++)
    {
      sprintf(pages[i][NAME], source_pages[i]);
      sprintf(pages[i][XML], "%s.xml", source_pages[i]);
      sprintf(pages[i][HTML], "../%s_%s.php", source_pages[i], theme);
    }

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
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
<html>
½header
<meta name="description" content="Zapping is a TV Viewer Software for Linux/Unix. Zapping is Open Source and has the ability to use plugins, it has also nice extra build in features.">
<meta name="language" content="en-us">
<meta name="audience" content="all">
<meta name="ROBOTS" content="INDEX,FOLLOW">
<meta name="page-topic" content="software">
<meta name="publisher" content="Carsten Menke">
<meta name="author" content="Iñaki García Etxebarria (Inaki Garcia Etxebarria)">
<meta name="keywords" content="Zapping,OpenSource,TV,Software,for,Linux,Free,TV,Viewer,Software,program,proggie,apps,application,gnu">
<meta name="revisit-after" content="30">
<meta name="copyright" content="Iñaki García Etxebarria & Carsten Menke">
<meta name="generator" content="http://url-submission.de">

<style type="text/css"><!--
td.leftborder {
  background-image: url("images_carsten/bar-left.png");
  background-color: blue;
  background-repeat: repeat-y;
}
td.rightborder {
  background-image: url("images_carsten/bar-right.png");
  background-color: blue;
  background-repeat: repeat-y;
}
select.theme {
    background-color:#fff;
    border:1px #666666 solid;
    font-size: 10px;
    color: #666666;
    font-family: arial,sans-serif;
 }
input.theme {
    margin-top: 1px;
    margin-bottom: 1px;
    font-size: 10px;
    font-family: arial,sans-serif;
 }
--></style>

</head>
½TV_SET1
<body bgcolor="#000000" link="yellow" vlink="yellow" text="#FFFFFF">

<table width=800 border=0 cellpadding=0 cellspacing=0>
<tr>
<td width=158 height=158 valign="top">

<!-- ########### Zapping TV SET TABLE #####################-->
<table border="0"  cellpadding="0" cellspacing="0" width="158">
<colgroup>
        <col width=16>
        <col width=127>
        <col width=15>
</colgroup>
<tr>
<td width="16" height="16"><img src="images_carsten/zapping-tv/top-edge-left.jpg" width="16" height="16" border="0" alt=""></td>
<td width="127" height="16"><img src="images_carsten/zapping-tv/top-bar.jpg" width="127" height="16" border="0" alt=""></td>
<td width="15" height="16"><img src="images_carsten/zapping-tv/top-edge-right.jpg" width="15" height="16"  border="0"  alt=""></td>
</tr>
<tr>
<td width="16" height="107"><img src="images_carsten/zapping-tv/left-bar.jpg" width="16" height="107" border="0" alt=""></td>
<td width="127" height="107">½
½TV_SET2
</td>
<td width="15" height="107"><img src="images_carsten/zapping-tv/right-bar.jpg" width="15" height="107" border="0" alt=""></td></tr>
<tr>
<td width="16" height="35"><img src="images_carsten/zapping-tv/bottom-edge-left.jpg" width="16" height="35" border="0" alt=""></td>
<td width="127" height="35">
½TV_SET3
<img src="images_carsten/zapping-tv/bottom-right.jpg" width="81" height="35" border="0"  hspace="0" vspace="0" alt=""></td>
<td width="15" height="35"><img src="images_carsten/zapping-tv/bottom-edge-right.jpg" width="15" height="35" border="0" alt=""></td>
</tr>
</table>
<!-- ############## END of Zapping TV Set TABLE ##################-->
<td width=642 >
<img src="images_carsten/spacebar.png" width="160" height="1" align="left" alt="">
½LOGO1
<!-- ###################### BEGIN LOGO TABLE ###################-->
<table border="0" cellpadding="0" cellspacing="0" width="260">
<tr>
<td width="36" height="28"><img src="images_carsten/logo/logo-left-top.png" width="36" height="28" border="0" alt=""></td>
<td width="218" height="28"><img src="images_carsten/logo/logo-top.png" width="218" height="28" border="0" alt=""></td>
<td width="36" height="28"><img src="images_carsten/logo/logo-right-top.png" width="36" height="28" border="0" alt=""></td>
</tr>
<tr>
<td width="36" height="44"><img src="images_carsten/logo/logo-left-middle.png" width="36" height="44" border="0" alt=""></td>
<td width="218" height="44">½
½LOGO2
</td>
<td width="36" height="44"><img src="images_carsten/logo/logo-right-middle.png" width="36" height="44" border="0" alt=""></td>
</tr>
<tr>
<td width="36" height="28"><img src="images_carsten/logo/logo-left-bottom.png" width="36" height="28" border="0" alt=""></td>
<td width="218" height="28"><img src="images_carsten/logo/logo-bottom.png" width="218" height="28" border="0" alt=""></td>
<td width="36" height="28"><img src="images_carsten/logo/logo-right-bottom.png" width="36" height="28" border="0" alt=""></td>
</tr>
</table>

<!-- ################### END LOGO TABLE ##################-->
</td>
</tr>
</table>
<br>
<table border="0" cellspacing="0" cellpadding="0" width="100%">
<colgroup>
        <col width=130>
        <col width=60>
        <col width=10>
        <col width=590>
        <col width=10>
</colgroup>
<!-- ##################### END PREFORMATTING ################################## -->
½MENU_OPEN
<tr>
<th width="130" valign="top"  rowspan="3">
½MENU1
<a href="screenshots.php"><img src="images_carsten/menu1.png" width="130" height="39" border="0" align="top" alt="Find screenshots of the famous Linux TV software Zapping here"></a>
½MENU2
<a href="download.php"><img src="images_carsten/menu2.png" width="130" height="34" border="0" align="top" alt="Download Zapping, the TV Viewer for Linux"></a>
½MENU3
<a href="news.php"><img src="images_carsten/menu3.png" width="130" height="34" border="0" align="top" alt="What's new at Zapping, your Linux TV Viewer"></a>
½MENU4
<a href="bugs.php"><img src="images_carsten/menu4.png" width="130" height="34" border="0" align="top" alt="Report Bugs, you've found in Zapping the Linux TV Program"></a>
½MENU5
<a href="links.php"><img src="images_carsten/menu5.png" width="130" height="34" border="0" align="top" alt="Links, to other interesting Linux sites"></a>
½MENU6
<a href="contact.php"><img src="images_carsten/menu6.png" width="130" height="34" border="0" align="top" alt="Contact the programmer of Linux TV Viewer Zapping"></a>
½MENU7
<a href="changelog.php"><img src="images_carsten/menu7.png" width="130" height="34" border="0" align="top" alt="Changelog for the TV Software Zapping, so what has changed"></a>
½MENU8
<a href="resources.php"><img src="images_carsten/menu8.png" width="130" height="42" border="0" align="top" alt="Resources you need to know, Linux Geek"></a>
½MENU1_LIGHT
<img src="images_carsten/menu1-light.png" width="130" height="39" border="0" align="top" alt="Find screenshots of the famous Linux TV software Zapping here">
½MENU2_LIGHT
<img src="images_carsten/menu2-light.png" width="130" height="34" border="0" align="top" alt="Download Zapping, the TV Viewer for Linux">
½MENU3_LIGHT
<img src="images_carsten/menu3-light.png" width="130" height="34" border="0" align="top" alt="What's new at Zapping, your Linux TV Viewer">
½MENU4_LIGHT
<img src="images_carsten/menu4-light.png" width="130" height="34" border="0" align="top" alt="Report Bugs, you've found in Zapping the Linux TV Program">
½MENU5_LIGHT
<img src="images_carsten/menu5-light.png" width="130" height="34" border="0" align="top" alt="Links, to other interesting Linux sites">
½MENU6_LIGHT
<img src="images_carsten/menu6-light.png" width="130" height="34" border="0" align="top" alt="Contact the programmer of Linux TV Viewer Zapping">
½MENU7_LIGHT
<img src="images_carsten/menu7-light.png" width="130" height="34" border="0" align="top" alt="Changelog for the TV Software Zapping, so what has changed">
½MENU8_LIGHT
<img src="images_carsten/menu8-light.png" width="130" height="42" border="0" align="top" alt="Resources you need to know, Linux Geek">
½MENU_CLOSE
<form action="<?php echo $PHP_SELF ?>" method="POST">
<select name="sel_theme" class="theme" title="Select your theme here">
<option value="simple" <?php if ($theme=="simple") echo "selected" ?>>Simple</option>
<option value="carsten" <?php if ($theme=="carsten") echo "selected" ?>>Carsten</option>
<option value="modern" <?php if ($theme=="modern") echo "selected" ?>>Modern</option>
</select>
<input type="submit" name="Submit" value="Change" class="theme">
</form>
</th>
<th  width="60" height="10"><img src="images_carsten/spacebar.png" width="1" height="1" alt=""></th>
<th width="10" height="10"><img src="images_carsten/edge-left-top.png" width="10" height="10" alt=""></th>
<th width="100%" height="10"><img src="images_carsten/bar-top.png" width="100%" height="10" alt=""></th>
<th width="10" height="10"><img src="images_carsten/edge-right-top.png" width="10" height="10" alt=""></th>
</tr>
<!-- ################# END ROW 1 ###################################### -->
<tr>
<td></td>
<td width="10" height="100%" class="leftborder"><img src="images_carsten/bar-left.png" width="10"
height="480" alt=""></td>
½TOC1

<!-- ###### Begin Text Table of Contents ############################### -->
<td width="100%" height="100%" bgcolor="#000099" valign="top">½
½THE_REST
</td>
<!-- ################# End Text Table of Contents ############################### -->

<td width="10" height="100%" class="rightborder"><img src="images_carsten/bar-right.png" width="10" height="480" alt=""></td>
</tr>
<!-- ################### END ROW 2 #################################### -->
<tr>
<td  width="60" height="10"><img src="images_carsten/spacebar.png" width="1" height="1" alt=""></td>
<td width="10" height="10"><img src="images_carsten/edge-left-bottom.png" width="10" height="10" alt=""></td>
<td width="100%" height="10"><img src="images_carsten/bar-bottom.png" width="100%" height="10" alt=""></td>
<td width="10" height="10"><img src="images_carsten/edge-right-bottom.png" width="10" height="10" alt=""></td>
</tr>
<!-- ###################### END ROW 3 ################################# -->
</table>

</BODY>
</html>
½
#endif
