/* Template plugin for Zapping
 * Copyright (C) 2000-2001 Iñaki García Etxebarria
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "config.h"

#define ZCONF_DOMAIN "/zapping/ttxview/"
#include "zconf.h"

#include "src/plugin_common.h"
#include "src/globals.h"
#include "src/properties.h"

#include "main.h"
#include "view.h"
#include "subtitle.h"


#ifdef HAVE_LIBZVBI

/* Libzvbi returns static strings which must be localized using
   the zvbi domain. Stupid. Fix planned for next version. */
#ifdef ENABLE_NLS
#  include <libintl.h>
#  define V_(String) dgettext("zvbi", String)
#else
#  define V_(String) (String)
#endif

#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkx.h>
#include <ctype.h>

#include "interface.h"
#include "frequencies.h"
#include "zvbi.h"
#include "v4linterface.h"
#include "zmisc.h"
#include "zmodel.h"
#include "common/fifo.h"
#include "osd.h"
#include "remote.h"
#include "properties-handler.h"

#include "plugins/teletext/main.h"

#include "toolbar.h"
#include "bookmark.h"
#include "export.h"
#include "color.h"
#include "search.h"
#include "view.h"
#include "window.h"

extern tveng_tuned_channel *	global_channel_list;
extern gint			cur_tuned_channel;

GtkActionGroup *	teletext_action_group;

bookmark_list		bookmarks;
BookmarkEditor *	bookmarks_dialog;

static GtkWidget *	color_dialog;
ZModel *		color_zmodel;

ZModel *		ttxview_zmodel;	/* created / destroyed a view */

GList *			teletext_windows;
GList *			teletext_views;

static __inline__ int
vbi_bcd2dec2			(int			n)
{
  return vbi_bcd2dec ((unsigned int) n);
}

static __inline__ int
vbi_dec2bcd2			(int			n)
{
  return vbi_dec2bcd ((unsigned int) n);
}

static __inline__ int
vbi_add_bcd2			(int			a,
				 int			b)
{
  return vbi_add_bcd ((unsigned int) a, (unsigned int) b);
}



GtkWidget *
ttxview_popup			(GtkWidget *		widget,
				 GdkEventButton *	event)
{
  TeletextView *view;

  if ((view = teletext_view_from_widget (widget)))
    {
      vbi_link ld;

      teletext_view_vbi_link_from_pointer_position
	(view, &ld, (int) event->x, (int) event->y);

      return teletext_view_popup_menu_new (view, &ld, FALSE);
    }
  else
    {
      return NULL;
    }
}


static void
on_add_bookmark_activate	(GtkWidget *		menu_item _unused_,
				 TeletextView *		data)
{
  tveng_tuned_channel *channel;
  vbi_pgno pgno;
  vbi_subno subno;
  gchar *description;
  vbi_decoder *vbi;
  gchar title[48];

  channel = tveng_tuned_channel_nth (global_channel_list,
				     (unsigned int) cur_tuned_channel);

  if (data->page >= 0x100)
    pgno = data->page;
  else
    pgno = data->fmt_page->pgno;

  subno = data->monitored_subpage;

  description = NULL;

  vbi = zvbi_get_object ();

  if (vbi && vbi_page_title (vbi, pgno, subno, title))
    {
      /* FIXME the title encoding should be UTF-8, but isn't
	 really defined. g_convert returns NULL on failure. */
      description = g_convert (title, NUL_TERMINATED,
			       "UTF-8", "ISO-8859-1",
			       NULL, NULL, NULL);
    }

  bookmark_list_add (&bookmarks,
		     channel ? channel->name : NULL,
		     pgno, subno, description);

  g_free (description);

  zmodel_changed (bookmarks.zmodel);

  if (data->appbar)
    {
      gchar *buffer;

      if (subno && subno != VBI_ANY_SUBNO)
	buffer = g_strdup_printf (_("Added page %x.%02x to bookmarks"),
				  pgno, subno);
      else
	buffer = g_strdup_printf (_("Added page %x to bookmarks"), pgno);

      gnome_appbar_set_status (GNOME_APPBAR (data->appbar), buffer);

      g_free (buffer);
    }
}

static void
set_transient_for		(GtkWindow *		window,
				 TeletextView *		view)
{
  GtkWidget *parent;

  parent = GTK_WIDGET (view);

  for (;;)
    {
      if (!parent)
	return;

      if (GTK_IS_WINDOW (parent))
	break;
      
      parent = parent->parent;
    }

  gtk_window_set_transient_for (window,	GTK_WINDOW (parent));
}

static void
on_edit_bookmarks_activate	(GtkWidget *		menu_item _unused_,
				 TeletextView *		data)
{
  GtkWidget *widget;

  if (bookmarks_dialog)
    {
      gtk_window_present (GTK_WINDOW (bookmarks_dialog));
    }
  else if ((widget = bookmark_editor_new (&bookmarks)))
    {
      set_transient_for (GTK_WINDOW (widget), data);
      gtk_widget_show_all (widget);
    }
}

static void
on_bookmark_menu_item_activate	(GtkWidget *		menu_item,
				 TeletextView *		data)
{
  bookmark *b;
  GList *glist;

  b = g_object_get_data (G_OBJECT (menu_item), "bookmark");

  for (glist = bookmarks.bookmarks; glist; glist = glist->next)
    if (glist->data == b)
      break;

  if (!glist)
    return;

  if (zapping->info
      && global_channel_list
      && b->channel)
    {
      tveng_tuned_channel *channel;

      channel = tveng_tuned_channel_by_name (global_channel_list,
					     b->channel);
      if (channel)
	z_switch_channel (channel, zapping->info);
    }

  teletext_view_load_page (data, b->pgno, b->subno, NULL);
}

static GnomeUIInfo
bookmarks_uiinfo [] = {
  {
    GNOME_APP_UI_ITEM, N_("_Add Bookmark"), NULL,
    G_CALLBACK (on_add_bookmark_activate), NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GTK_STOCK_ADD,
    GDK_D, (GdkModifierType) GDK_CONTROL_MASK, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Edit Bookmarks..."), NULL,
    G_CALLBACK (on_edit_bookmarks_activate), NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    GDK_B, (GdkModifierType) GDK_CONTROL_MASK, NULL
  },
  GNOMEUIINFO_END
};

GtkWidget *
bookmarks_menu_new		(TeletextView *		view)
{
  GtkMenuShell *menu;
  GtkWidget *widget;
  GList *glist;

  menu = GTK_MENU_SHELL (gtk_menu_new ());

  bookmarks_uiinfo[0].user_data = view;
  bookmarks_uiinfo[1].user_data = view;

  gnome_app_fill_menu (menu,
		       bookmarks_uiinfo,
		       /* accel */ NULL,
		       /* mnemo */ TRUE,
		       /* position */ 0);

  if (!bookmarks.bookmarks)
    return GTK_WIDGET (menu);

  widget = gtk_separator_menu_item_new ();
  gtk_widget_show (widget);
  gtk_menu_shell_append (menu, widget);

  for (glist = bookmarks.bookmarks; glist; glist = glist->next)
    {
      bookmark *b;
      GtkWidget *menu_item;
      const gchar *tmpl;
      gchar *buffer;
      gchar *channel;

      b = (bookmark * ) glist->data;

      if (b->subno != VBI_ANY_SUBNO)
	tmpl = "%s%s%x.%x";
      else
	tmpl = "%s%s%x";

      channel = b->channel;
      if (channel && !*channel)
	channel = NULL;

      buffer = g_strdup_printf (tmpl,
				channel ? channel : "",
				channel ? " " : "",
				b->pgno,
				b->subno);

      if (b->description && *b->description)
	{
	  menu_item = z_gtk_pixmap_menu_item_new (b->description,
						  GTK_STOCK_JUMP_TO);
	  z_tooltip_set (menu_item, buffer);
	}
      else
	{
	  menu_item = z_gtk_pixmap_menu_item_new (buffer, GTK_STOCK_JUMP_TO);
	}

      gtk_widget_show (menu_item);

      g_object_set_data (G_OBJECT (menu_item), "bookmark", b);
      g_signal_connect (G_OBJECT (menu_item), "activate",
			G_CALLBACK (on_bookmark_menu_item_activate), view);

      gtk_menu_shell_append (menu, menu_item);

      g_free (buffer);
    }

  return GTK_WIDGET (menu);
}

GtkWidget *
ttxview_bookmarks_menu_new	(GtkWidget *		widget)
{
  TeletextView *view;

  if ((view = teletext_view_from_widget (widget)))
    return bookmarks_menu_new (view);

  return NULL;
}

/*
 *  Miscellaneous menu stuff
 */

/*
static void
ttxview_resize			(ttxview_data *		data,
				 guint			width,
				 guint			height,
				 gboolean		aspect)
{
  gint main_width;
  gint main_height;
  gint darea_width;
  gint darea_height;

  gdk_window_get_geometry (GTK_WIDGET (data->app)->window, NULL, NULL,
			   &main_width, &main_height, NULL);

  gdk_window_get_geometry (data->darea->window, NULL, NULL,
			   &darea_width, &darea_height, NULL);

  if (aspect)
    {
      height = darea_width * height / width;
    }

  width += main_width - darea_width;
  height += main_height - darea_height;

  gdk_window_resize (data->darea->window, width, height);
}
*/

/*****************************************************************************/

static PyObject *
py_ttx_open_new			(PyObject *		self _unused_,
				 PyObject *		args)
{
  TeletextView *view;
  int page;
  int subpage;
  vbi_pgno pgno;
  vbi_subno subno;
  guint width;
  guint height;
  GtkWidget *dolly;

  view = teletext_view_from_widget (python_command_widget ());

  if (view
      && view->fmt_page
      && view->fmt_page->pgno)
    {
      page = vbi_bcd2dec2 (view->fmt_page->pgno);
      subpage = vbi_bcd2dec2 (view->monitored_subpage & 0xFF);
    }
  else
    {
      page = 100;
      subpage = -1;
    }

  if (!ParseTuple (args, "|ii", &page, &subpage))
    g_error ("zapping.ttx_open_new(|ii)");

  if (page >= 100 && page <= 899)
    pgno = vbi_dec2bcd2 (page);
  else
    py_return_false;

  if (subpage < 0)
    subno = VBI_ANY_SUBNO;
  else if ((guint) subpage <= 99)
    subno = vbi_dec2bcd2 (subpage);
  else
    py_return_false;

  if (!zvbi_get_object ())
    py_return_false;

  width = 300;
  height = 200;

  if (view)
    gdk_window_get_geometry (GTK_WIDGET (view)->window,
			     /* x */ NULL,
			     /* y */ NULL,
			     &width,
			     &height,
			     /* depth */ NULL);

  dolly = teletext_window_new ();
  view = teletext_view_from_widget (dolly);

  teletext_view_load_page (view, pgno, subno, NULL);

  gtk_widget_realize (dolly);

  z_update_gui ();

  gdk_window_resize (dolly->window, (int) width, (int) height);

  gtk_widget_show (dolly);

  py_return_true;
}






#endif /* HAVE_LIBZVBI */







/* This is the description of the plugin, change as appropiate */
static const gchar str_canonical_name[] = "teletext";
static const gchar str_descriptive_name[] = N_("Teletext plugin");
static const gchar str_description[] = ("");
static const gchar str_short_description[] = ("");
static const gchar str_author[] = "";
/* The format of the version string must be
   %d[[.%d[.%d]][other_things]], where the things between [] aren't
   needed, and if not present, 0 will be assumed */
static const gchar str_version[] = "1.0";


/*
  Declaration of the static symbols of the plugin. Refer to the docs
  to know what does each of these functions do
*/
gint plugin_get_protocol ( void )
{
  /* You don't need to modify this function */
  return PLUGIN_PROTOCOL;
}

static gboolean
plugin_running			(void _unused_)
{
  return FALSE;
}

static
void plugin_get_info ( const gchar ** canonical_name,
		       const gchar ** descriptive_name,
		       const gchar ** description,
		       const gchar ** short_description,
		       const gchar ** author,
		       const gchar ** version )
{
  /* Usually, this one doesn't need modification either */
  if (canonical_name)
    *canonical_name = _(str_canonical_name);
  if (descriptive_name)
    *descriptive_name = _(str_descriptive_name);
  if (description)
    *description = _(str_description);
  if (short_description)
    *short_description = _(str_short_description);
  if (author)
    *author = _(str_author);
  if (version)
    *version = _(str_version);
}







static void
attach_label			(GtkTable *		table,
				 guint			row,
				 const gchar *		text)
{
  GtkWidget *widget;

  widget = gtk_label_new_with_mnemonic (text);
  gtk_widget_show (widget);
  gtk_table_attach (table, widget, 0, 1, row, row + 1,
                    (GtkAttachOptions)(GTK_FILL),
                    (GtkAttachOptions)(0),
		    /* xpadding */ 0,
		    /* ypadding */ 0);
  gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_CENTER);
  gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);
}

static void
attach_combo_box		(GtkTable *		table,
				 guint			row,
				 const gchar **		option_menu,
				 const gchar *		gconf_key,
				 const GConfEnumStringPair *lookup_table,
				 const gchar *		tooltip)
{
  GtkWidget *widget;

  widget = z_gconf_combo_box_new (option_menu, gconf_key, lookup_table);
  gtk_widget_show (widget);

  if (tooltip)
    z_tooltip_set (widget, tooltip);

  gtk_table_attach (table, widget, 1, 2, row, row + 1,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0), 0, 0);
}

static GConfEnumStringPair
charset_enum [] = {
  { 0, "western_and_central_europe" },
  { 8, "eastern_europe" },
  { 16, "western_europe_and_turkey" },
  { 24, "central_and_southeast_europe" },
  { 32, "cyrillic" },
  { 48, "greek_and_turkish" },
  { 64, "arabic" },
  { 80, "hebrew_and_arabic" },
  { 0, NULL }
};

static GConfEnumStringPair
level_enum [] = {
  { VBI_WST_LEVEL_1, "1.0" },
  { VBI_WST_LEVEL_1p5, "1.5" },
  { VBI_WST_LEVEL_2p5, "2.5" },
  { VBI_WST_LEVEL_3p5, "3.5" },
  { 0, NULL }
};

static GConfEnumStringPair
interp_enum [] = {
  { GDK_INTERP_NEAREST, "nearest" },
  { GDK_INTERP_TILES,	"tiles" },
  { GDK_INTERP_BILINEAR, "bilinear" },	
  { GDK_INTERP_HYPER,	"hyper" },
  { 0, NULL }
};

static GtkWidget *
create_preferences		(void)
{
  static const gchar *charset_menu [] = {
    N_("Western and Central Europe"),
    N_("Eastern Europe"),
    N_("Western Europe and Turkey"),
    N_("Central and Southeast Europe"),
    N_("Cyrillic"),
    N_("Greek and Turkish"),
    N_("Arabic"),
    N_("Hebrew and Arabic"),
    NULL,
  };
  static const gchar *level_menu [] = {
    N_("Level 1.0"),
    N_("Level 1.5 (additional national characters)"),
    N_("Level 2.5 (more colours, font styles and graphics)"),
    N_("Level 3.5 (proportional spacing, multicolour graphics)"),
    NULL,
  };
  static const gchar *interp_menu [] = {
    N_("Nearest (fast, looks ugly when scaled)"),
    N_("Tiles (slightly slower, somewhat better)"),
    N_("Bilinear (good quality but slow)"),
    N_("Hyper (best quality but CPU intensive)"),
    NULL,
  };
  GtkTable *table;
  GtkWidget *widget;
  GConfChangeSet *cs;

  widget = gtk_table_new (4, 2, /* homogeneous */ FALSE);
  table = GTK_TABLE (widget);
  gtk_container_set_border_width (GTK_CONTAINER (table), 3);
  gtk_table_set_row_spacings (table, 3);
  gtk_table_set_col_spacings (table, 12);

  attach_label (table, 1, _("Default _region:"));
  attach_label (table, 2, _("_Teletext implementation:"));
  attach_label (table, 3, _("_Interpolation type:"));

  attach_combo_box (table, 1, charset_menu,
		    "/apps/zapping/plugins/teletext/default_charset",
		    charset_enum,
		    _("Some stations fail to transmit a complete language "
		      "identifier, so the Teletext viewer may not display "
		      "the correct font or national characters. You can "
		      "select your geographical region here as an "
		      "additional hint."));
  attach_combo_box (table, 2, level_menu,
		    "/apps/zapping/plugins/teletext/level", level_enum,
		    NULL);
  attach_combo_box (table, 3, interp_menu,
		    "/apps/zapping/plugins/teletext/view/interp_type",
		    interp_enum,
		    _("Quality/speed trade-off when scaling and "
		      "anti-aliasing the page."));

  cs = gconf_client_change_set_from_current
    (gconf_client, /* err */ NULL,
     "/apps/zapping/plugins/teletext/default_charset",
     "/apps/zapping/plugins/teletext/level",
     "/apps/zapping/plugins/teletext/view/interp_type",
     NULL);

  g_object_set_data_full (G_OBJECT (widget),
			  "teletext-change-set", cs,
			  (GCallback) gconf_change_set_unref);

  return widget;
}

static void
cancel_preferences		(GtkWidget *		page)
{
  GConfChangeSet *cs;

  cs = (GConfChangeSet *) g_object_get_data (G_OBJECT (page),
					     "teletext-change-set");

  /* Revert to old values, error ignored. */
  gconf_client_commit_change_set (gconf_client, cs,
				  /* remove_committed */ FALSE,
				  /* err */ NULL);
}

static void
properties_add			(GtkDialog *		dialog)
{
  static SidebarEntry se = {
    .label		= N_("Teletext"),
    .icon_name		= "teletext48.png",
    .create		= create_preferences,
    .cancel		= cancel_preferences,
    .help_link_id	= "zapping-settings-vbi",
  };
  static const SidebarGroup sg = {
    N_("Plugins"), &se, 1
  };

  standard_properties_add (dialog, &sg, 1, /* glade */ NULL);
}

static void
default_charset_notify		(GConfClient *		client _unused_,
				 guint			cnxn_id _unused_,
				 GConfEntry *		entry _unused_,
				 gpointer		unused _unused_)
{
  if (entry->value && zvbi_get_object ())
    {
      const gchar *s;
      gint enum_value;

      s = gconf_value_get_string (entry->value);

      if (s && gconf_string_to_enum (charset_enum, s, &enum_value))
	vbi_teletext_set_default_region (zvbi_get_object(), enum_value);
    }
}

static void
level_notify			(GConfClient *		client _unused_,
				 guint			cnxn_id _unused_,
				 GConfEntry *		entry _unused_,
				 gpointer		unused _unused_)
{
  if (entry->value && zvbi_get_object ())
    {
      const gchar *s;
      gint enum_value;

      s = gconf_value_get_string (entry->value);

      if (s && gconf_string_to_enum (level_enum, s, &enum_value))
	vbi_teletext_set_level (zvbi_get_object(), enum_value);
    }
}

static void
plugin_close			(void)
{
  while (teletext_windows)
    gtk_widget_destroy (GTK_WIDGET (teletext_windows->data));

  if (bookmarks_dialog)
    {
      gtk_widget_destroy (GTK_WIDGET (bookmarks_dialog));
      bookmarks_dialog = NULL;
    }

  bookmark_list_save (&bookmarks);
  bookmark_list_destroy (&bookmarks);

  if (color_dialog)
    {
      gtk_widget_destroy (color_dialog);
      color_dialog = NULL;
    }

  g_object_unref (G_OBJECT (color_zmodel));
  color_zmodel = NULL;

  g_object_unref (G_OBJECT (ttxview_zmodel));
  ttxview_zmodel = NULL;
}

static void
colors_action			(GtkAction *		action _unused_,
				 gpointer		user_data _unused_)
{
  if (color_dialog)
    {
      gtk_window_present (GTK_WINDOW (color_dialog));
    }
  else if ((color_dialog = color_dialog_new ()))
    {
      gtk_widget_show_all (color_dialog);
    }
}

static void
preferences_action		(GtkAction *		action _unused_,
				 gpointer		user_data _unused_)
{
  python_command_printf (NULL,
			 "zapping.properties('%s','%s')",
			 _("VBI Options"), _("General"));
}

static GtkActionEntry
actions [] = {
  { "Preferences", NULL, N_("_Preferences"), NULL,
    NULL, G_CALLBACK (preferences_action) },
  { "Colors", NULL, N_("_Colors"), NULL,
    NULL, G_CALLBACK (colors_action) },
};

static gboolean
plugin_init			(PluginBridge		bridge _unused_,
				 tveng_device_info *	info _unused_)
{
  static const property_handler ph = {
    .add = properties_add,
  };

  /* Preliminary. */
  _ttxview_popup = ttxview_popup;
  _ttxview_bookmarks_menu_new = ttxview_bookmarks_menu_new;
  _ttxview_hotlist_menu_insert = ttxview_hotlist_menu_insert;

  append_property_handler (&ph);

  teletext_action_group = gtk_action_group_new ("TeletextActions");
#ifdef ENABLE_NLS
  gtk_action_group_set_translation_domain (teletext_action_group,
					   GETTEXT_PACKAGE);
#endif					   
  gtk_action_group_add_actions (teletext_action_group,
				actions, G_N_ELEMENTS (actions), NULL);

  ttxview_zmodel = ZMODEL (zmodel_new ());
  g_object_set_data (G_OBJECT (ttxview_zmodel), "count", GINT_TO_POINTER (0));

  color_zmodel = ZMODEL (zmodel_new ());

  bookmark_list_init (&bookmarks);
  bookmark_list_load (&bookmarks);

  /* Error ignored */
  gconf_client_notify_add (gconf_client,
			   "/apps/zapping/plugins/teletext/default_charset",
			   default_charset_notify, NULL,
			   /* destroy */ NULL, /* err */ NULL);
  /* Error ignored */
  gconf_client_notify_add (gconf_client,
			   "/apps/zapping/plugins/teletext/level",
			   level_notify, NULL,
			   /* destroy */ NULL, /* err */ NULL);

  zcc_bool (TRUE, "Show toolbar", "view/toolbar");
  zcc_bool (TRUE, "Show statusbar", "view/statusbar");

  zcc_char (g_get_home_dir(), "Export directory", "exportdir");

  zcc_bool (FALSE, "Search uses regular expression", "ure_regexp");
  zcc_bool (FALSE, "Search case insensitive", "ure_casefold");

  zconf_create_int (128, "Brightness", "/zapping/options/text/brightness");
  zconf_create_int (64, "Contrast", "/zapping/options/text/contrast");

  cmd_register ("ttx_open_new", py_ttx_open_new, METH_VARARGS,
		("Open new Teletext window"), "zapping.ttx_open_new()");

  return TRUE;
}






static gboolean
plugin_start			(void)
{
  return FALSE;
}

static void
plugin_stop			(void)
{
}

static void
plugin_load_config		(gchar *		root_key _unused_)
{
}

static void
plugin_save_config		(gchar *		root_key _unused_)
{
}

/*
  Read only access to the bundle. You can do X or GTK calls from here,
  but you shouldn't modify the members of the struct.
*/
static
void plugin_read_frame ( capture_frame * frame  _unused_)
{
}


static
gboolean plugin_get_public_info (gint index _unused_,
				 gpointer * ptr _unused_,
				 gchar ** symbol _unused_,
				 gchar ** description _unused_,
				 gchar ** type _unused_,
				 gint * hash _unused_)
{
  return FALSE; /* No symbols. */
}

static
void plugin_add_gui (GnomeApp * app _unused_)
{
  /*
    Define this function only if you are going to do something to the
    main Zapping window.
  */
}

static
void plugin_remove_gui (GnomeApp * app _unused_)
{
  /*
    Define this function if you have defined previously plugin_add_gui
   */
}

static
struct plugin_misc_info * plugin_get_misc_info (void)
{
  static struct plugin_misc_info returned_struct =
  {
    sizeof(struct plugin_misc_info), /* size of this struct */
    6, /* plugin priority, this is just an example */
    0 /* Category */
  };

  /*
    Tell that the template plugin should be run with a somewhat high
    priority (just to put an example)
  */
  return (&returned_struct);
}

/* Return FALSE if we aren't able to access a symbol, you should only
   need to edit the pointer table, not the code */
gboolean plugin_get_symbol(gchar * name, gint hash, gpointer * ptr)
{
  /* Usually this table is the only thing you will need to change */
  struct plugin_exported_symbol table_of_symbols[] =
  {
    SYMBOL(plugin_get_info, 0x1234),
    SYMBOL(plugin_init, 0x1234),
    SYMBOL(plugin_close, 0x1234),
    SYMBOL(plugin_start, 0x1234),
    SYMBOL(plugin_stop, 0x1234),
    SYMBOL(plugin_load_config, 0x1234),
    SYMBOL(plugin_save_config, 0x1234),
    SYMBOL(plugin_running, 0x1234),
    SYMBOL(plugin_read_frame, 0x1234),
    SYMBOL(plugin_get_public_info, 0x1234),
    SYMBOL(plugin_add_gui, 0x1234),
    SYMBOL(plugin_remove_gui, 0x1234),
    SYMBOL(plugin_get_misc_info, 0x1234),
    SYMBOL2(teletext, view_new),
    SYMBOL2(teletext, view_from_widget),
    SYMBOL2(teletext, view_on_key_press),
    SYMBOL2(teletext, toolbar_new),
  };
  gint num_exported_symbols =
    sizeof(table_of_symbols)/sizeof(struct plugin_exported_symbol);
  gint i;

  /* Try to find the given symbol in the table of exported symbols
   of the plugin */
  for (i=0; i<num_exported_symbols; i++)
    if (!strcmp(table_of_symbols[i].symbol, name))
      {
	if (table_of_symbols[i].hash != hash)
	  {
	    if (ptr)
	      *ptr = GINT_TO_POINTER(0x3); /* hash collision code */
	    /* Warn */
	    g_warning(_("Check error: \"%s\" in plugin %s "
		       "has hash 0x%x vs. 0x%x"), name,
		      str_canonical_name, 
		      table_of_symbols[i].hash,
		      hash);
	    return FALSE;
	  }
	if (ptr)
	  *ptr = table_of_symbols[i].ptr;
	return TRUE; /* Success */
      }

  if (ptr)
    *ptr = GINT_TO_POINTER(0x2); /* Symbol not found in the plugin */
  return FALSE;
}
