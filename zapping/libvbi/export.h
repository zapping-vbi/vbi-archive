#ifndef EXPORT_H
#define EXPORT_H

#include "vt.h"
#include "lang.h"
#include "../common/types.h"
#include "format.h"
// #include "vbi.h"

/* not public */
extern int		vbi_format_page(struct vbi *vbi, struct fmt_page *pg,
					struct vt_page *vtp, int display_rows, int navigation);



struct export
{
    struct export_module *mod;	// module type
    char *fmt_str;		// saved option string (splitted)
    char *err_str;		// NULL if none
    // global options
    int reveal;			// reveal hidden chars
    // local data for module's use.  initialized to 0.
    struct { int dummy; } data[0];
};

struct export_module
{
    char *fmt_name;		// the format type name (ASCII/HTML/PNG/...)
    char *extension;		// the default file name extension
    char **options;		// module options
    int local_size;
    int (*open)(struct export *fmt);
    void (*close)(struct export *fmt);
    int (*option)(struct export *fmt, int opt, char *arg);
    int (*output)(struct export *fmt, char *name, struct fmt_page *pg);
};


extern struct export_module *modules[];	// list of modules (for help msgs)
void export_error(struct export *e, char *str, ...);	// set error
char *export_errstr(struct export *e);		// return last error
char *export_mkname(struct export *e, char *fmt, int pgno, int subno, char *usr);

struct export *export_open(char *fmt);
void export_close(struct export *e);
int export(struct export *e, struct fmt_page *pg, char *user_str);



void vbi_draw_page_region(struct fmt_page *pg, void *data, int
			  reveal, int scol, int srow, int width, int
			  height, int rowstride, int flash_on);
#define vbi_draw_page(X, Y, Z) \
	vbi_draw_page_region(X, Y, Z, 0, 0, 40, 25, -1, 1)
void vbi_get_rendered_size(int *w, int *h);



#endif /* EXPORT_H */
