#ifndef EXPORT_H
#define EXPORT_H

#include "vt.h"
#include "lang.h"
#include "../common/types.h"
#include "format.h"
#include "libvbi.h"

/* not public */
extern int		vbi_format_page(struct vbi *vbi, struct fmt_page *pg,
					struct vt_page *vtp, int display_rows, int navigation);

typedef enum {
	VBI_EXPORT_BOOL = 1,	/* TRUE (1) or FALSE (0), def.num */
	VBI_EXPORT_INT,		/* Integer min - max, def.num */
	VBI_EXPORT_MENU,	/* Index of menu[], min - max, def.num */
	VBI_EXPORT_STRING,	/* String, def.str */
} vbi_export_option_type;

typedef struct {
	vbi_export_option_type	type;
	char *			keyword;
	char *			label;		/* i18n */
	union {
		char *			str;	/* i18n */
		int			num;
	}			def;
	int			min, max;
	char **			menu;		/* max - min + 1 entries, i18n */
	char *			tooltip;	/* or NULL, i18n */
} vbi_export_option;









struct export
{
    struct export_module *mod;	// module type
    char *fmt_str;		// saved option string (splitted)
    char *err_str;		// NULL if none
    vbi_network			network;
    // global options
    int reveal;			// reveal hidden chars
    // local data for module's use.  initialized to 0.
    struct { int dummy; } data[0];
};

struct export_module
{
    char *fmt_name;		// the format type name (ASCII/HTML/PNG/...)
    char *extension;		// the default file name extension
	vbi_export_option *	options;
    int local_size;
    int (*open)(struct export *fmt);
    void (*close)(struct export *fmt);
    int (*option)(struct export *fmt, int opt, char *str_arg, int num_arg);
    int (*output)(struct export *fmt, char *name, struct fmt_page *pg);
};


extern struct export_module *modules[];	// list of modules (for help msgs)
void export_error(struct export *e, char *str, ...);	// set error
char *export_errstr(struct export *e);		// return last error
char *export_mkname(struct export *e, char *fmt, int pgno, int subno, char *usr);

struct export *export_open(char *fmt, vbi_network *);
void export_close(struct export *e);
int export(struct export *e, struct fmt_page *pg, char *user_str);


extern int		vbi_export_set_option(struct export *exp, int index, ...);


void vbi_get_rendered_size(int *w, int *h);


#endif /* EXPORT_H */
