/*
 *  Zapzilla - Export modules interface
 *
 *  Copyright (C) 2001 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998,1999 Edgar Toernig (froese@gmx.de)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id: export.h,v 1.29 2001-08-20 00:53:23 mschimek Exp $ */

#ifndef EXPORT_H
#define EXPORT_H

#include "libvbi.h"
#include "../common/types.h"
#include "../common/errstr.h"

#ifdef ENABLE_NLS
#    include <libintl.h>
#    define _(String) gettext (String)
#    ifdef gettext_noop
#        define N_(String) gettext_noop (String)
#    else
#        define N_(String) (String)
#    endif
#else
/* Stubs that do something close enough.  */
#    define textdomain(String) (String)
#    define gettext(String) (String)
#    define dgettext(Domain,Message) (Message)
#    define dcgettext(Domain,Message,Type) (Message)
#    define bindtextdomain(Domain,Directory) (Domain)
#    define _(String) (String)
#    define N_(String) (String)
#endif

typedef struct _vbi_export_module_priv vbi_export_module_priv;

struct vbi_export {
	vbi_export_module_priv *mod;
	vbi_network		network;

    int	reveal;		// reveal hidden chars
    char *fmt_str;		// saved option string (splitted)

	int			data[0];
};

struct _vbi_export_module_priv {
	vbi_export_module_priv *next;
	vbi_export_module	pub;

	char *			extension;
	vbi_export_option *	options;
	int			local_size;

	bool			(* open)(vbi_export *);
	void			(* close)(vbi_export *);
	bool			(* output)(vbi_export *, FILE *, char *, struct fmt_page *);
	vbi_export_option *	(* query_option)(vbi_export *, int opt);
	bool			(* set_option)(vbi_export *, int opt, char *str_arg, int num_arg);
};

/* Helper functions */

#define VBI_AUTOREG_EXPORT_MODULE(name)

#if 0 // doesn't work --??
extern void		vbi_register_export_module(vbi_export_module_priv *);

#define VBI_AUTOREG_EXPORT_MODULE(name)					\
static void vbi_autoreg_##name(void) __attribute__ ((constructor));	\
static void vbi_autoreg_##name(void) {					\
	vbi_register_export_module(&name);				\
}
#endif

extern void		vbi_export_write_error(vbi_export *, char *);

#endif /* EXPORT_H */
