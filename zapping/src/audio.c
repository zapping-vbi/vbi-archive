/* Zapping (TV viewer for the Gnome Desktop)
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include <common/fifo.h> // current_time
#include <math.h>
#include <unistd.h>

#include "audio.h"
#define ZCONF_DOMAIN "/zapping/options/audio/"
#include "zconf.h"
#include "properties.h"
#include "interface.h"
#include "zmisc.h"

extern audio_backend_info esd_backend;
#if USE_OSS
extern audio_backend_info oss_backend;
#endif
#if HAVE_ARTS
extern audio_backend_info arts_backend;
#endif

static audio_backend_info *backends [] =
{
  &esd_backend,
#if USE_OSS
  &oss_backend,
#endif
#if HAVE_ARTS
  &arts_backend
#endif
};

#define num_backends (sizeof(backends)/sizeof(backends[0]))

typedef struct {
  gint		owner; /* backend owning this handle */
  gpointer	handle; /* for the backend */
} mhandle;

/* Generic stuff */
gpointer
open_audio_device (gboolean stereo, gint rate, enum audio_format
		   format)
{
  gint cur_backend = zcg_int(NULL, "backend");
  gpointer handle = backends[cur_backend]->open(stereo, rate, format);
  mhandle *mhandle;

  if (!handle)
    {
      ShowBox(_("Cannot open the configured audio device.\n"
		"You might want to setup another kind of audio\n"
		"device in the Properties dialog."),
	      GNOME_MESSAGE_BOX_WARNING);
      return NULL;
    }

  mhandle = g_malloc0(sizeof(*mhandle));
  mhandle->handle = handle;
  mhandle->owner = cur_backend;

  return mhandle;
}

void
close_audio_device (gpointer handle)
{
  mhandle *mhandle = handle;

  backends[mhandle->owner]->close(mhandle->handle);

  g_free(mhandle);
}

void
read_audio_data (gpointer handle, gpointer dest, gint num_bytes,
		 double *timestamp)
{
  mhandle *mhandle = handle;

  backends[mhandle->owner]->read(mhandle->handle, dest, num_bytes,
				timestamp);
}

static void
on_backend_activate	(GtkObject		*menuitem,
			 gpointer		pindex)
{
  gint index = GPOINTER_TO_INT(pindex);
  GtkObject * audio_backends =
    GTK_OBJECT(gtk_object_get_user_data(menuitem));
  gint cur_sel =
    GPOINTER_TO_INT(gtk_object_get_data(audio_backends, "cur_sel"));
  GtkWidget **boxes =
    (GtkWidget**)gtk_object_get_data(audio_backends, "boxes");

  if (cur_sel == index)
    return;

  gtk_object_set_data(audio_backends, "cur_sel", pindex);
  gtk_widget_hide(boxes[cur_sel]);
  gtk_widget_set_sensitive(boxes[cur_sel], FALSE);

  gtk_widget_show(boxes[index]);
  gtk_widget_set_sensitive(boxes[index], TRUE);
}

/* Audio */
static void
audio_setup		(GtkWidget	*page)
{
  GtkWidget *widget;
  GtkWidget *audio_backends =
    lookup_widget(page, "audio_backends");
  GtkWidget **boxes = (GtkWidget**)g_malloc0(num_backends*sizeof(boxes[0]));
  GtkWidget *vbox39 = lookup_widget(page, "vbox39");
  GtkWidget *menuitem;
  GtkWidget *menu = gtk_menu_new();
  gint cur_backend = zcg_int(NULL, "backend");
  gint i;

  /* Hook on dialog destruction so there's no mem leak */
  gtk_object_set_data_full(GTK_OBJECT (audio_backends), "boxes", boxes,
			   (GtkDestroyNotify)g_free);

  for (i=0; i<num_backends; i++)
    {
      menuitem = gtk_menu_item_new_with_label(_(backends[i]->name));
      gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			 GTK_SIGNAL_FUNC(on_backend_activate),
			 GINT_TO_POINTER(i));
      gtk_menu_append(GTK_MENU(menu), menuitem);
      gtk_object_set_user_data(GTK_OBJECT(menuitem), audio_backends);
      gtk_widget_show(menuitem);

      boxes[i] = gtk_vbox_new(FALSE, 3);
      if (backends[i]->add_props)
	backends[i]->add_props(GTK_BOX(boxes[i]));
      gtk_box_pack_start(GTK_BOX(vbox39), boxes[i], FALSE, FALSE, 0);
      if (i == cur_backend)
	gtk_widget_show(boxes[i]);
      else
	gtk_widget_set_sensitive(boxes[i], FALSE);
    }

  gtk_option_menu_set_menu(GTK_OPTION_MENU (audio_backends), menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU (audio_backends),
			      cur_backend);
  gtk_object_set_data(GTK_OBJECT (audio_backends), "cur_sel",
		      GINT_TO_POINTER(cur_backend));

  /* Avoid noise while changing channels */
  widget = lookup_widget(page, "checkbutton1");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/main/avoid_noise"));

  /* Start zapping muted */
  widget = lookup_widget(page, "checkbutton3");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/main/start_muted"));
}

static void
audio_apply		(GtkWidget	*page)
{
  GtkWidget *widget;
  GtkWidget *audio_backends;
  gint selected;
  GtkWidget **boxes;

  widget = lookup_widget(page, "checkbutton1"); /* avoid noise */
  zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/main/avoid_noise");

  widget = lookup_widget(page, "checkbutton3"); /* start muted */
  zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/main/start_muted");  

  /* Apply the properties */
  audio_backends = lookup_widget(page, "audio_backends");
  selected = z_option_menu_get_active(audio_backends);
  boxes = (GtkWidget**)gtk_object_get_data(GTK_OBJECT(audio_backends),
					   "boxes");

  if (backends[selected]->apply_props)
    backends[selected]->apply_props(GTK_BOX(boxes[selected]));

  zcs_int(selected, "backend");
}

static void
help		(GnomeDialog	*dialog,
		 GtkWidget	*page)
{
  void (*page_help)(GtkWidget *page) =
    gtk_object_get_data(GTK_OBJECT(page), "help");

  if (page_help)
    page_help(page);

  ShowBox("No help written yet",
	  GNOME_MESSAGE_BOX_WARNING);
}

static void
add				(GnomeDialog	*dialog)
{
  SidebarEntry general_options[] = {
    { N_("Audio"), ICON_GNOME, "gnome-grecord.png", "vbox39",
      audio_setup, audio_apply }
  };
  SidebarGroup groups[] = {
    { N_("General Options"), general_options, acount(general_options) }
  };
  gint i, j;

  for (i = 0; i<acount(groups); i++)
    {
      append_properties_group(dialog, _(groups[i].label));

      for (j = 0; j<groups[i].num_items; j++)
	{
	  const gchar *icon_name = groups[i].items[j].icon_name;
	  gchar *pixmap_path = (groups[i].items[j].icon_source ==
				ICON_ZAPPING) ?
	    g_strdup_printf("%s/%s", PACKAGE_PIXMAPS_DIR, icon_name) :
	    g_strdup(gnome_pixmap_file(icon_name)); /* FIXME: leak?? */
	  GtkWidget *pixmap = z_pixmap_new_from_file(pixmap_path);
	  GtkWidget *page = build_widget(groups[i].items[j].widget,
					 PACKAGE_DATA_DIR "/zapping.glade");

	  groups[i].items[j].setup(page);

	  gtk_object_set_data(GTK_OBJECT(page), "apply",
			      groups[i].items[j].apply);

	  append_properties_page(dialog, _(groups[i].label),
				 _(groups[i].items[j].label),
				 pixmap, page);

	  g_free(pixmap_path);
	}
    }

  open_properties_group(GTK_WIDGET(dialog), _("General Options"));  
}

static void
apply		(GnomeDialog	*dialog,
		 GtkWidget	*page)
{
  void (*page_apply)(GtkWidget *page) =
    gtk_object_get_data(GTK_OBJECT(page), "apply");

  g_assert(page_apply != NULL);

  page_apply(page);
}

void startup_audio ( void )
{
  gint i;

  property_handler2 audio_handler =
  {
    add: add,
    apply: apply,
    help: help
  };

  register_property_handler2(&audio_handler);

  zcc_int(0, "Default audio backend", "backend");

  for (i=0; i<num_backends; i++)
    if (backends[i]->init)
      backends[i]->init();
}

void shutdown_audio ( void )
{
  gint i;

  for (i=0; i<num_backends; i++)
    if (backends[i]->shutdown)
      backends[i]->shutdown();
}
