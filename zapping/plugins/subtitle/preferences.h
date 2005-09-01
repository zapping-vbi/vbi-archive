/*
 *  Zapping TV viewer
 *
 *  Copyright (C) 2000-2002 Iñaki García Etxebarria
 *  Copyright (C) 2000-2005 Michael H. Schimek
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

/* $Id: preferences.h,v 1.1 2005-09-01 01:40:53 mschimek Exp $ */

#ifndef SUBTITLE_PREFERENCES_H
#define SUBTITLE_PREFERENCES_H

#include <gtk/gtk.h>
#include <gconf/gconf-changeset.h>

G_BEGIN_DECLS

#define TYPE_SUBTITLE_PREFS (subtitle_prefs_get_type ())
#define SUBTITLE_PREFS(obj)						\
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_SUBTITLE_PREFS, SubtitlePrefs))
#define SUBTITLE_PREFS_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_SUBTITLE_PREFS, SubtitlePrefsClass))
#define IS_SUBTITLE_PREFS(obj)						\
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_SUBTITLE_PREFS))
#define IS_SUBTITLE_PREFS_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_SUBTITLE_PREFS))
#define SUBTITLE_PREFS_GET_CLASS(obj)					\
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_SUBTITLE_PREFS, SubtitlePrefsClass))

typedef struct _SubtitlePrefs SubtitlePrefs;
typedef struct _SubtitlePrefsClass SubtitlePrefsClass;

struct _SubtitlePrefs
{
  GtkTable		table;

  GConfChangeSet *	change_set;
};

struct _SubtitlePrefsClass
{
  GtkTableClass		parent_class;
};

extern GConfEnumStringPair subtitle_charset_enum [];
extern GConfEnumStringPair subtitle_level_enum [];
extern GConfEnumStringPair subtitle_interp_enum [];

extern void
subtitle_prefs_cancel		(SubtitlePrefs *	prefs);
extern void
subtitle_prefs_apply		(SubtitlePrefs *	prefs);
extern GType
subtitle_prefs_get_type		(void) G_GNUC_CONST;
extern GtkWidget *
subtitle_prefs_new		(void);

G_END_DECLS

#endif /* SUBTITLE_PREFERENCES_H */
