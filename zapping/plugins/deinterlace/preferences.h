/*
 *  Zapping TV viewer
 *
 *  Copyright (C) 2004 Michael H. Schimek
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

/* $Id: preferences.h,v 1.2 2007-08-30 14:14:26 mschimek Exp $ */

#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <gtk/gtk.h>
#include <gconf/gconf-changeset.h>

G_BEGIN_DECLS

#define TYPE_DEINTERLACE_PREFS (deinterlace_prefs_get_type ())
#define DEINTERLACE_PREFS(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj),	\
  TYPE_DEINTERLACE_PREFS, DeinterlacePrefs))
#define DEINTERLACE_PREFS_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TYPE_DEINTERLACE_PREFS, DeinterlacePrefsClass))
#define IS_DEINTERLACE_PREFS(obj)					\
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_DEINTERLACE_PREFS))
#define IS_DEINTERLACE_PREFS_CLASS(klass)				\
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_DEINTERLACE_PREFS))
#define DEINTERLACE_PREFS_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TYPE_DEINTERLACE_PREFS, DeinterlacePrefsClass))

typedef struct _DeinterlacePrefs DeinterlacePrefs;
typedef struct _DeinterlacePrefsClass DeinterlacePrefsClass;

struct _DeinterlacePrefs
{
  GtkTable		table;

  GtkWidget *		option_table;
  GConfChangeSet *	change_set;
};

struct _DeinterlacePrefsClass
{
  GtkTableClass		parent_class;
};

extern GConfEnumStringPair resolution_enum [];

extern void
deinterlace_prefs_cancel	(DeinterlacePrefs *	prefs);
extern GType
deinterlace_prefs_get_type	(void) G_GNUC_CONST;
extern GtkWidget *
deinterlace_prefs_new		(void);

G_END_DECLS

#endif /* PREFERENCES_H */

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
