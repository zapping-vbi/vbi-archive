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

/* $Id: audio.c,v 1.12.2.10 2003-08-24 23:51:43 mschimek Exp $ */

#include "../site_def.h"

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include "../common/fifo.h" // current_time()
#include <math.h>
#include <unistd.h>

#include "audio.h"
#define ZCONF_DOMAIN "/zapping/options/audio/"
#include "zconf.h"
#include "properties.h"
#include "interface.h"
#include "zmisc.h"
#include "mixer.h"
#include "osd.h"
#include "remote.h"
#include "callbacks.h"
#include "globals.h"
#include "v4linterface.h"


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

typedef struct mhandle {
  gint		owner; /* backend owning this handle */
  gpointer	handle; /* for the backend */
} mhandle;

void mixer_setup ( void )
{
#warning
  if (mixer && mixer_line)
    {
      if (mixer->rec_gain)
	tv_mixer_line_set_volume (mixer->rec_gain,
				  mixer->rec_gain->reset,
				  mixer->rec_gain->reset);

      tv_mixer_line_record (mixer_line, /* exclusive */ TRUE);
    }
}

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
	      GTK_MESSAGE_WARNING);
      return NULL;
    }

  mhandle = (struct mhandle *) g_malloc0(sizeof(*mhandle));
  mhandle->handle = handle;
  mhandle->owner = cur_backend;

  /* make sure we record from the appropiate source */
  mixer_setup();

  return mhandle;
}

void
close_audio_device (gpointer handle)
{
  mhandle *mhandle = (struct mhandle *) handle;

  backends[mhandle->owner]->close(mhandle->handle);

  g_free(mhandle);
}

void
read_audio_data (gpointer handle, gpointer dest, gint num_bytes,
		 double *timestamp)
{
  mhandle *mhandle = (struct mhandle *) handle;

  backends[mhandle->owner]->read(mhandle->handle, dest, num_bytes,
				timestamp);
}

static void
on_backend_activate	(GObject		*menuitem,
			 gpointer		pindex)
{
  gint index = GPOINTER_TO_INT(pindex);
  GObject * audio_backends =
    G_OBJECT(g_object_get_data(menuitem, "user-data"));
  gint cur_sel =
    GPOINTER_TO_INT(g_object_get_data(audio_backends, "cur_sel"));
  GtkWidget **boxes =
    (GtkWidget**)g_object_get_data(audio_backends, "boxes");

  if (cur_sel == index)
    return;

  g_object_set_data(audio_backends, "cur_sel", pindex);
  gtk_widget_hide(boxes[cur_sel]);
  gtk_widget_set_sensitive(boxes[cur_sel], FALSE);

  gtk_widget_show(boxes[index]);
  gtk_widget_set_sensitive(boxes[index], TRUE);
}

#if 0
static void
build_mixer_lines		(GtkWidget	*optionmenu)
{
  GtkMenuShell *menu = GTK_MENU_SHELL
    (gtk_option_menu_get_menu(GTK_OPTION_MENU(optionmenu)));
  GtkWidget *menuitem;
  gchar *label;
  gint i;

  for (i=0;(label = mixer_get_description(i));i++)
    {
      menuitem = gtk_menu_item_new_with_label(label);
      free(label);
      gtk_widget_show(menuitem);
      gtk_menu_shell_append(menu, menuitem);
    }
}

static void
on_record_source_changed	(GtkOptionMenu	*optionmenu,
				 GtkRange	*hscale)
{
  int min, max;
  GtkAdjustment *adj;
  gint cur_sel = z_option_menu_get_active (GTK_WIDGET (optionmenu));

  gtk_widget_set_sensitive(GTK_WIDGET(hscale), cur_sel);

  if (!cur_sel)
    return;

  /* note that it's cur_sel-1, the 0 entry is "Use system settings" */
  g_assert(!mixer_get_bounds(cur_sel-1, &min, &max));
  
  adj = gtk_range_get_adjustment(hscale);

  adj -> lower = min;
  adj -> upper = max;

  gtk_adjustment_changed(adj);
}
#endif

/*
 *  Soundcard mixer selection
 */

#ifndef AUDIO_MIXER_LOG_FP
#define AUDIO_MIXER_LOG_FP 0 /* stderr */
#endif

static void
on_mixer_enable_toggled		(GtkToggleButton *	togglebutton,
				 gpointer		user_data)
{
  GtkWidget *table = user_data;
  gboolean active;

  active = gtk_toggle_button_get_active (togglebutton);
  gtk_widget_set_sensitive (table, active);
}

static void
mixer_select			(GtkWidget *		table,
				 void *			user_data,
				 tv_device_node *	n)
{
  GtkWidget *menu;
  GtkWidget *optionmenu;

  if ((optionmenu = g_object_get_data (G_OBJECT (table), "mixer_lines")))
    {
      gtk_widget_destroy (optionmenu);
      optionmenu = NULL;
    }

  if (n)
    {
      tv_mixer *mixer;
      tv_audio_line *line;
      guint hash = -1;
      guint index;

      mixer = PARENT (n, tv_mixer, node);
      g_assert (mixer->inputs != NULL);

      if (0 == strcmp (zcg_char (NULL, "mixer_device"), n->device))
	hash = zcg_int (NULL, "mixer_input");

      optionmenu = gtk_option_menu_new ();
      gtk_widget_show (optionmenu);

      menu = gtk_menu_new ();
      gtk_widget_show (menu);
      gtk_option_menu_set_menu (GTK_OPTION_MENU (optionmenu), menu);
  
      index = 0;

      for (line = mixer->inputs; line; line = line->_next)
	{
	  GtkWidget *menu_item;
	  
	  menu_item = gtk_menu_item_new_with_label (line->label);
	  gtk_widget_show (menu_item);
	  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

	  if (index == 0 || line->hash == hash)
	    gtk_option_menu_set_history (GTK_OPTION_MENU (optionmenu), index);

	  index++;
	}

      if (mixer->inputs->_next == NULL)
	gtk_widget_set_sensitive (GTK_WIDGET (optionmenu), FALSE);

      gtk_table_attach (GTK_TABLE (table), optionmenu, 1, 1 + 1, 3, 3 + 1,
			GTK_FILL | GTK_EXPAND, 0, 0, 0);
    }

  g_object_set_data (G_OBJECT (table), "mixer_lines", optionmenu);
}

static tv_device_node *
mixer_open			(GtkWidget *		table,
				 void *			user_data,
				 tv_device_node *	list,
				 const char *		name)
{
  tv_mixer *m;
  tv_device_node *n;

  if ((n = tv_device_node_find (list, name)))
    return n;

  /* FIXME report errors */
  if ((m = tv_mixer_open (AUDIO_MIXER_LOG_FP, name)))
    {
      if (m->inputs != NULL)
	return &m->node;

      tv_mixer_close (m);
    }

  return NULL;
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
  guint cur_backend = zcg_int(NULL, "backend");
  guint i;

  /* Hook on dialog destruction so there's no mem leak */
  g_object_set_data_full(G_OBJECT (audio_backends), "boxes", boxes,
			   (GtkDestroyNotify)g_free);

  for (i=0; i<num_backends; i++)
    {
      menuitem = gtk_menu_item_new_with_label(_(backends[i]->name));
      g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(on_backend_activate),
			 GINT_TO_POINTER(i));
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
      g_object_set_data(G_OBJECT(menuitem), "user-data",
			audio_backends);
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
  g_object_set_data(G_OBJECT (audio_backends), "cur_sel",
		      GINT_TO_POINTER(cur_backend));

  /* Avoid noise while changing channels */
  widget = lookup_widget(page, "checkbutton1");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/main/avoid_noise"));

  /* Start zapping muted */
  widget = lookup_widget(page, "checkbutton3");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/main/start_muted"));

  {
    tv_mixer *m;
    tv_device_node *list, *n;
    GtkWidget *checkbutton;
    GtkWidget *alignment;
    GtkWidget *table;
    GtkWidget *label;

    /* Mixer selection */

    /* FIXME use abstract mixer */
    m = tv_mixer_scan (AUDIO_MIXER_LOG_FP);
    list = m ? &m->node : NULL;

    /* Only real soundcards. We have to handle video devices differently. */
    for (n = list; n;)
      if (PARENT (n, tv_mixer, node)->inputs == NULL)
	{
	  tv_device_node *next = n->next;
	  tv_device_node_delete (&list, n, FALSE);
	  n = next;
	}
      else
	{
	  n = n->next;
	}

    alignment = lookup_widget (page, "mixer_table_alignment");
    table = z_device_entry_new (NULL, list, zcg_char (NULL, "mixer_device"),
				mixer_open, mixer_select, NULL);
    gtk_widget_show (table);
    gtk_container_add (GTK_CONTAINER (alignment), table);
    register_widget (NULL, table, "mixer_table");

    label = gtk_label_new (_("Input:"));
    gtk_widget_show (label);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
    gtk_misc_set_padding (GTK_MISC (label), 3, 3);
    gtk_table_attach (GTK_TABLE (table), label,
		      0, 1, 3, 3 + 1, GTK_FILL, 0, 0, 0);

    checkbutton = lookup_widget (page, "force_mixer");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton),
				  zcg_bool (NULL, "force_mixer"));
    on_mixer_enable_toggled (GTK_TOGGLE_BUTTON (checkbutton), table);
    g_signal_connect (G_OBJECT (checkbutton), "toggled",
		      G_CALLBACK (on_mixer_enable_toggled), table);
  }
}

static void
audio_apply		(GtkWidget	*page)
{
  GtkWidget *widget;
  GtkWidget *audio_backends;
  gint selected;
  GtkWidget **boxes;
  tv_device_node *n;  

  widget = lookup_widget(page, "checkbutton1"); /* avoid noise */
  zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/main/avoid_noise");

  widget = lookup_widget(page, "force_mixer");
  zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/audio/force_mixer");

  widget = lookup_widget (page, "mixer_table");
  if ((n = z_device_entry_selected (widget)))
    {
      tv_audio_line *l;
      gint index;

      zcs_char (n->device, "mixer_device");
      widget = g_object_get_data (G_OBJECT (widget), "mixer_lines");
      index = z_option_menu_get_active (widget);

      for (l = PARENT (n, tv_mixer, node)->inputs; l; l = l->_next)
	if (index-- == 0)
	  {
	    zcs_int (l->hash, "mixer_input");
	    break;
	  }
    }
  {
    tveng_tc_control *tcc;
    guint num_controls;

    store_control_values (main_info, &tcc, &num_controls);
    startup_mixer ();
    update_control_box (main_info);
    load_control_values (main_info, tcc, num_controls, FALSE);
  }

  widget = lookup_widget(page, "checkbutton3"); /* start muted */
  zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/main/start_muted");  

  /* Apply the properties */
  audio_backends = lookup_widget(page, "audio_backends");
  selected = z_option_menu_get_active(audio_backends);
  boxes = (GtkWidget**)g_object_get_data(G_OBJECT(audio_backends),
					   "boxes");

  if (backends[selected]->apply_props)
    backends[selected]->apply_props(GTK_BOX(boxes[selected]));

  zcs_int(selected, "backend");
}

static void
add				(GtkDialog	*dialog)
{
  SidebarEntry general_options[] = {
    { N_("Audio"), "gnome-grecord.png", "vbox39",
      audio_setup, audio_apply }
  };
  SidebarGroup groups[] = {
    { N_("General Options"), general_options, acount(general_options) }
  };

  standard_properties_add(dialog, groups, acount(groups), "zapping.glade2");
}






static guint			quiet_timeout_handle = 0;

static gboolean
quiet_timeout			(gpointer		user_data)
{
  tv_quiet_set (main_info, FALSE);
  return FALSE; /* don't call again */
}

void
reset_quiet			(tveng_device_info *	info,
				 guint			delay)
{
  if (quiet_timeout_handle)
    gtk_timeout_remove (quiet_timeout_handle);

  quiet_timeout_handle =
    gtk_timeout_add (delay /* ms */, quiet_timeout, NULL);
}









/*
 *  Global mute function. XXX switch to callbacks.
 *  mode: 0=sound, 1=mute, 2=toggle, 3=just update GUI
 *  controls: update controls box
 *  osd: show state on screen
 */
gboolean
set_mute				(gint	        mode,
					 gboolean	controls,
					 gboolean	osd)
{
  static gboolean recursion = FALSE;
  gint mute;

  if (recursion)
    return TRUE;

  recursion = TRUE;

  /* Get current state */

  if (mode >= 2)
    {
      mute = tv_mute_get (main_info, TRUE);
      if (mute == -1)
        goto failure;

      if (mode == 2)
	mute = !mute;
    }
  else
    mute = !!mode;

  /* Set new state */

  if (mode <= 2)
    if (-1 == tv_mute_set (main_info, mute))
      goto failure;

  /* Update GUI */

  {
    GtkWidget *button;
    GtkCheckMenuItem *check;

    button = lookup_widget (main_window, "toolbar-mute");

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)) != mute)
      {
	SIGNAL_HANDLER_BLOCK (button, on_python_command1,
			      gtk_toggle_button_set_active
			      (GTK_TOGGLE_BUTTON (button), mute));
      }

    check = GTK_CHECK_MENU_ITEM (lookup_widget (main_window, "mute2"));

    if (check->active != mute)
      {
	SIGNAL_HANDLER_BLOCK (check, on_python_command1,
			      gtk_check_menu_item_set_active (check, mute));
      }

    /* obsolete, using callbacks now
    if (controls)
      update_control_box (main_info);
    */

    if (osd)
      {
	BonoboDockItem *dock_item;

	dock_item = gnome_app_get_dock_item_by_name
	  (GNOME_APP (main_window), GNOME_APP_TOOLBAR_NAME);

	if (main_info->current_mode == TVENG_CAPTURE_PREVIEW ||
	    !GTK_WIDGET_VISIBLE (GTK_WIDGET (dock_item)))
	  osd_render_markup (NULL, mute ?
			   _("<blue>audio off</blue>") :
			   _("<yellow>AUDIO ON</yellow>"));
      }
  }

  recursion = FALSE;
  return TRUE;

 failure:
  recursion = FALSE;
  return FALSE;
}

static PyObject* py_mute (PyObject *self, PyObject *args)
{
  int value = 2; /* toggle by default */
  int ok = PyArg_ParseTuple (args, "|i", &value);

  if (!ok)
    {
      g_warning ("zapping.mute(|i)");
      py_return_false;
    }

  set_mute (value, TRUE, TRUE);

  py_return_true;
}

void startup_audio ( void )
{
  guint i;

  property_handler audio_handler =
  {
    add: add
  };

  prepend_property_handler(&audio_handler);

  zcc_int(0, "Default audio backend", "backend");
  zcc_char ("", "Mixer device", "mixer_device");
  zcc_int (-1, "Mixer input line (hash)", "mixer_input");

  for (i=0; i<num_backends; i++)
    if (backends[i]->init)
      backends[i]->init();

  cmd_register ("mute", py_mute, METH_VARARGS,
		"Toggles the mute state", "zapping.mute([new_state])");
}

void shutdown_audio ( void )
{
  guint i;

  for (i=0; i<num_backends; i++)
    if (backends[i]->shutdown)
      backends[i]->shutdown();
}
