#include <libgen.h>

#ifndef XML2
#define children childs
#endif

#ifndef TAB
#define TAB "  "
#endif

static char *buf = NULL;
static void (*write_abstract)(FILE *fp, const char *docname,
			      const char *abstract) = NULL;

#define init_parser() _init_parser(__FILE__)

static void _init_parser(const char *file)
{
  struct stat st;
  FILE *parser_code;

  if (!(parser_code = fopen(file, "rt")))
    {
      fprintf(stderr, "%s not found\n", file);
      exit(1);
    }

  if (fstat(fileno(parser_code), &st))
    {
      fprintf(stderr, "fstat failed in %s: %s [%d]\n", file,
	      strerror(errno), errno);
      exit(1);
    }

  if (!(buf = malloc(st.st_size)))
    {
      perror("malloc");
      exit(1);
    }

  if (!fread(buf, st.st_size, 1, parser_code))
    {
      perror("fread");
      exit(1);
    }

  fclose(parser_code);
}

#define print(X) _print(X, fp)

static void
_print(const char *name, FILE *fp)
{
  char *p, b2[strlen(name)+10];

  sprintf(b2, "½%s\n", name);

  p = strstr(buf, b2);

  if (!p)
    {
      fprintf(stderr, "section %s not found\n", name);
      exit(1);
    }

  p+=strlen(b2);

  for (;*p && *p!='½';p++)
    fprintf(fp, "%c", *p);
}

#define print_nonbreak(X) _print_nonbreak(X, fp)

static void
_print_nonbreak(const char *string, FILE *fp) __attribute__ ((unused));

static void
_print_nonbreak(const char *string, FILE *fp)
{
  int i;

  for (i=0; i<strlen(string); i++)
    {
      if (string[i] != ' ')
	fprintf(fp, "%c", string[i]);
      else
	fprintf(fp, "&nbsp;");
    }
}

/* returns 1 if there's nothing interesting the the text */
static int
is_junk(const char *text)
{
  int i;

  if (!text)
    return 1;

  for (i=0; i<strlen(text);i++)
    if (text[i] != ' ' &&
	text[i] != '\n' &&
	text[i] != '\r')
      return 0;

  return 1;
}

/* any unrecognised code is parsed by this */
static void
parse_html(xmlNodePtr node, xmlDocPtr doc, FILE *fp)
{
  xmlNodePtr ptr = node->children;
  struct _xmlAttr *attr=node->properties;
  xmlChar *label;

  fprintf(fp, "<%s", node->name);
  while (attr)
    {
#ifdef XML2
      label = xmlNodeListGetString(doc, attr->children, 1);
#else
      label = xmlNodeListGetString(doc, attr->val, 1);
#endif
      if (label)
	{
	  fprintf(fp, " %s=\"%s\"", attr->name, label);
	  xmlFree(label);
	}
      attr = attr->next;
    }
  fprintf(fp, ">");

  while (ptr)
    {
      switch (ptr->type)
	{
	case XML_ELEMENT_NODE:
	  parse_html(ptr, doc, fp);
	  break;
	case XML_TEXT_NODE:
	  if (!is_junk(ptr->content))
	    fprintf(fp, ptr->content);
	  break;
	default:
	  break;
	}
      ptr = ptr->next;
    }

  if (strcasecmp(node->name, "img") &&
      strcasecmp(node->name, "br"))
    fprintf(fp, "</%s>", node->name);
}

static void
parse_contents(xmlDocPtr doc, FILE *fp, const char *filename);

static void
parse_include(xmlNodePtr node, xmlDocPtr doc, FILE *fp)
{
  static int include_count = 0;
  xmlChar *file = xmlNodeListGetString(doc, node->children, 1);
  xmlChar *abstract;
  xmlDocPtr included;
  char *filename;
  int i;

  if (!file)
    return;

  included = xmlParseFile(file);

  if (!included)
    {
      fprintf(stderr, "Couldn't parse %s, skipped\n", file);
      return;
    }

  if (!xmlDocGetRootElement(included) ||
      xmlDocGetRootElement(included)->type != XML_ELEMENT_NODE)
    {
      fprintf(stderr, "Invalid XML doc %s, skipped\n", file);
      xmlFreeDoc(included);
      return;
    }

  if (++include_count > 10)
    {
      fprintf(stderr, "Error!: too many nested includes\n");
      exit(1);
    }

  for (i = 0; i<include_count; i++)
    fprintf(stderr, TAB);

  fprintf(stderr, "+ %s\n", file);

  filename = strdup(basename(file));

  if (strstr(filename, ".xml"))
    *strstr(filename, ".xml") = 0;

  if (write_abstract &&
      (abstract = xmlGetProp(xmlDocGetRootElement(included), "abstract")))
    {
      write_abstract(fp, filename, abstract);
      xmlFree(abstract);
    }

  parse_contents(included, fp, filename);

  free(filename);

  include_count--;

  xmlFreeDoc(included);
}

/* parse_header should be provided by the theme parser */
static void
parse_header(xmlNodePtr node, xmlDocPtr doc, FILE *fp);

static void
parse_contents(xmlDocPtr doc, FILE *fp, const char *filename)
{
  xmlNodePtr node = xmlDocGetRootElement(doc)->children;

  while (node)
    {
      switch (node->type)
	{
	case XML_ELEMENT_NODE:
	  if (!strcasecmp(node->name, "header"))
	    parse_header(node, doc, fp);
	  else if (!strcasecmp(node->name, "include"))
	    parse_include(node, doc, fp);
	  else
	    parse_html(node, doc, fp);
	  break;
	case XML_COMMENT_NODE:
	  if (node->content)
	    {
	      fprintf(fp, "<!--");
	      fprintf(fp, node->content);
	      fprintf(fp, "-->");
	    }
	  break;
	case XML_TEXT_NODE:
	  if (!is_junk(node->content))
	    fprintf(fp, node->content);
	  break;
	case XML_PI_NODE:
	  if (node->content)
	    fprintf(fp, "<?%s %s?>", node->name, node->content);
	  break;
	default:
	  fprintf(stderr, "Unkown node type %d [%s]\n", node->type,
		  node->name);
	  break;
	}
      node = node->next;
    }
}

static void
print_icon_header(FILE *fp)
{
  fprintf(fp,
	  "<link REL=\"icon\" HREF=\"/bookmark.ico\" TYPE=\"image/png\">\n");
}
