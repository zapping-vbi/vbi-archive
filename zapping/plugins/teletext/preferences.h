/*
 *  Zapping TV viewer
 *
 *  Copyright (C) 2000, 2001, 2002 Iñaki García Etxebarria
 *  Copyright (C) 2000, 2001, 2002, 2003, 2004 Michael H. Schimek
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

/* $Id: preferences.h,v 1.1 2004-11-03 06:46:33 mschimek Exp $ */

#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <gtk/gtk.h>
#include <gconf/gconf-changeset.h>

G_BEGIN_DECLS

#define TYPE_TELETEXT_PREFS (teletext_prefs_get_type ())
#define TELETEXT_PREFS(obj)						\
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_TELETEXT_PREFS, TeletextPrefs))
#define TELETEXT_PREFS_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_TELETEXT_PREFS, TeletextPrefsClass))
#define IS_TELETEXT_PREFS(obj)						\
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_TELETEXT_PREFS))
#define IS_TELETEXT_PREFS_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_TELETEXT_PREFS))
#define TELETEXT_PREFS_GET_CLASS(obj)					\
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_TELETEXT_PREFS, TeletextPrefsClass))

typedef struct _TeletextPrefs TeletextPrefs;
typedef struct _TeletextPrefsClass TeletextPrefsClass;

struct _TeletextPrefs
{
  GtkTable		table;

  GtkAdjustment *	cache_size;
  GtkAdjustment *	cache_networks;

  GConfChangeSet *	change_set;
};

struct _TeletextPrefsClass
{
  GtkTableClass		parent_class;
};

extern GConfEnumStringPair teletext_charset_enum [];
extern GConfEnumStringPair teletext_level_enum [];
extern GConfEnumStringPair teletext_interp_enum [];

extern void
teletext_prefs_cancel		(TeletextPrefs *	prefs);
extern void
teletext_prefs_apply		(TeletextPrefs *	prefs);
extern GType
teletext_prefs_get_type		(void) G_GNUC_CONST;
extern GtkWidget *
teletext_prefs_new		(void);

G_END_DECLS

#endif /* PREFERENCES_H */
