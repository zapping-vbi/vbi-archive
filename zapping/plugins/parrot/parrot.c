/* Parrot buffer plugin for Zapping
 * Copyright (C) 2001 Iñaki García Etxebarria
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
#include "src/plugin_common.h"

static const gchar str_canonical_name[] = "parrot";
static const gchar str_descriptive_name[] =
N_("Parrot buffer plugin");
static const gchar str_description[] =
N_("This plugin stores the last frames of video and repeats them "
   "upon request, so you can see again and again and again that "
   "sequence you liked so much.");
static const gchar str_short_description[] = 
N_("Superb parrot landing observatory.");
static const gchar str_author[] = "Iñaki García Etxebarria";
static const gchar str_version[] = "0.1";

/* Active status of the plugin */
static gboolean active = FALSE;

/* saved bundles */
static capture_bundle *parrot_bundles = NULL;
/* number of bundles in parrot_bundles */
static gint num_bundles = 0;
/* r/w indexes */
static gint read_index=0, write_index=0;
/* protects all the above */
static pthread_rwlock_t _rwlock, *rwlock = &_rwlock;

/* the last received format (size estimation in the config dialog) */
static struct tveng_frame_format last_format;

/*
  TODO:
   . Dynamic updating of estimated_size in process_bundle
   . add_gui, remove_gui
   . Init should do nothing and start should start parrot
     recording. Some gui control should start parrot mode.
*/

static void
update_size_estimation			(GtkWidget	*size_estimation,
					 gint		nbundles)
{
  size_t est_size;
  gchar *buffer, *buffer2;

  est_size = nbundles*last_format.sizeimage;
  if (est_size > 1e9)
    buffer2 = g_strdup_printf(_("%.3g Gb"), ((double)est_size) /
			      ((double)1024*1024*1024));
  else if (est_size > 1e6)
    buffer2 = g_strdup_printf(_("%.3g Mb"), ((double)est_size) /
			      (1024*1024));
  else if (est_size)
    buffer2 = g_strdup_printf(_("%.3g Kb"), ((double)est_size) / 1024);
  else
    buffer2 = g_strdup(_("n/a (capture mode not active)"));
  buffer = g_strdup_printf(_("%s: %s"),
			   _("Estimated parrot buffer size"),
			   buffer2);
  g_free(buffer2);
  gtk_label_set_text(GTK_LABEL(size_estimation), buffer);
  g_free(buffer);
}

/* This function is called when some item in the property box changes */
static void
on_property_item_changed              (GtkWidget * changed_widget,
				       GnomePropertyBox *propertybox)
{
  gint t = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(changed_widget));
  update_size_estimation(lookup_widget(changed_widget, "size_estimation"),
			 t);
  gnome_property_box_changed (propertybox);
}

/*
  Declaration of the static symbols of the plugin. Refer to the docs
  to know what does each of these functions do
*/
gint plugin_get_protocol ( void )
{
  /* You don't need to modify this function */
  return PLUGIN_PROTOCOL;
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
    SYMBOL(plugin_read_bundle, 0x1234),
    /* These three shouldn't be exported, since there are no
       configuration options */
    SYMBOL(plugin_add_properties, 0x1234),
    SYMBOL(plugin_activate_properties, 0x1234),
    SYMBOL(plugin_help_properties, 0x1234),
    SYMBOL(plugin_add_gui, 0x1234),
    SYMBOL(plugin_remove_gui, 0x1234),
    SYMBOL(plugin_get_misc_info, 0x1234)
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

static
gboolean plugin_running (void)
{
  return active;
}

static
void plugin_get_info (const gchar ** canonical_name,
		      const gchar ** descriptive_name,
		      const gchar ** description,
		      const gchar ** short_description,
		      const gchar ** author,
		      const gchar ** version)
{
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

static
gboolean plugin_init (PluginBridge bridge, tveng_device_info * info)
{
  gint i;

/* mhs: IMHO not solid enough for daily use,
 temporarily disabled to see if there are any side effects */
  num_bundles = 0;
  return FALSE;

  /* Do any startup you need here, and return FALSE on error */
  pthread_rwlock_init(rwlock, NULL);

  pthread_rwlock_rdlock(rwlock);
  parrot_bundles = g_malloc(num_bundles*sizeof(*parrot_bundles));
  memset(parrot_bundles, 0, num_bundles*sizeof(*parrot_bundles));
  memset(&last_format, 0, sizeof(last_format));
  pthread_rwlock_unlock(rwlock);

  /* If this is set, autostarting is on (we should start now) */
  if (active)
    return plugin_start();
  return TRUE;
}

static
void plugin_close(void)
{
  gint i;

  /* If we were working, stop the work */
  if (active)
    plugin_stop();

  /* Any cleanups would go here (closing fd's and so on) */
  pthread_rwlock_wrlock(rwlock);
  for (i=0; i<num_bundles; i++)
    clear_bundle(parrot_bundles+i);

  g_free(parrot_bundles);
  parrot_bundles = NULL;
  pthread_rwlock_unlock(rwlock);

  pthread_rwlock_destroy(rwlock);
}

static double last_timestamp /* from d->timestamp */,
  last_proper_timestamp /* time we sent last frame */,
  last_faked_timestamp /* time we should have sent the last frame*/,
  last_frame_difference=1.0/25 /* in original time */;

static void
parrot_filler(capture_bundle *bundle, tveng_device_info *info)
{
  /* fake timestamps */
  struct timeval tv;
  gint start_pos = read_index;
  capture_bundle *d;

  bundle->timestamp = current_time(); /* something != 0 */

  pthread_rwlock_rdlock(rwlock);
  do {
    d = parrot_bundles+read_index;
    read_index = (read_index+1) % num_bundles;
  } while ((!bundle_equal(bundle, d)) && (start_pos != read_index));

  /* no available frames */
  if (start_pos == read_index)
    memset(bundle->data, 0, bundle->format.sizeimage);
  else
    {
      double now;

      memcpy(bundle->data, d->data, bundle->format.sizeimage);
      now = current_time();
      
      if (last_timestamp > 0)
	{
	  /* original time difference */
	  int usecs1 = (d->timestamp-last_timestamp)*1e6;
	  /* time we have spent processing events */
	  int usecs2 = (now-last_proper_timestamp)*1e6;

	  /* probably from different streams: "save, parrot, save,
	     parrot", ... */
	  if (usecs1 > (last_frame_difference *5e6))
	    usecs1 = last_frame_difference*1e6;
	  else if (usecs1 > 0)
	    last_frame_difference = d->timestamp - last_timestamp;

	  /* sync (well, sort of :-)) */
	  if (usecs1 > usecs2)
	    {
	      usleep(usecs1 - usecs2);
	      /* update what "now" means */
	      now += (usecs1 - usecs2)/1e6;
	    }

	  last_faked_timestamp += last_frame_difference;
	}
      else
	last_faked_timestamp = now;

      last_proper_timestamp = now;
      last_timestamp = d->timestamp;

      bundle->timestamp = last_faked_timestamp;
    }
  pthread_rwlock_unlock(rwlock);
}

static
gboolean plugin_start (void)
{
  /* In most plugins, you don't want to be started twice */
  if (active)
    return TRUE;

  /* Do any neccessary work to start the plugin here */
  read_index = write_index+1;
  last_timestamp = -1; /* No sync for the first frame */
  capture_lock(); /* we don't want anybody to mess with the format
		     right now */
  set_bundle_filler(parrot_filler);

  /* If everything has been ok, set the active flags and return TRUE
   */
  active = TRUE;
  return TRUE;
}

static
void plugin_stop(void)
{
  /* Most times we cannot be stopped while we are stopped */
  if (!active)
    return;

  set_bundle_filler(NULL); /* default again */
  capture_unlock(); /* We are done */

  /* Stop anything the plugin is doing and set the flag */
  active = FALSE;
}

static
void plugin_load_config (gchar * root_key)
{
  gchar * buffer;

  buffer = g_strconcat(root_key, "autostart", NULL);
  zconf_create_boolean(FALSE,
		       "Whether the plugin should start"
		       " automatically when opening Zapping", buffer);
  active = zconf_get_boolean(&active, buffer);
  g_free(buffer);

  buffer = g_strconcat(root_key, "num_bundles", NULL);
  zconf_create_integer(250, "Number of saved frames", buffer);
  zconf_get_integer(&num_bundles, buffer);
  if (num_bundles < 1)
    num_bundles = 1;
  g_free(buffer);
}

static
void plugin_save_config (gchar * root_key)
{
  gchar * buffer;

  buffer = g_strconcat(root_key, "autostart", NULL);
  zconf_set_boolean(active, buffer);
  g_free(buffer);

  buffer = g_strconcat(root_key, "num_bundles", NULL);
  zconf_set_integer(num_bundles, buffer);
  g_free(buffer);
}

static
void plugin_read_bundle ( capture_bundle * bundle )
{
  capture_bundle *d;
  struct timeval tv;

  if (last_format.sizeimage != bundle->format.sizeimage)
    {
      memcpy(&last_format, &bundle->format, sizeof(last_format));
      /* FIXME: It would be good if we update_size_estimation here too,
       but we have to know if vbox is active... */
    }

  /* when we are active we do nothing here, everything is done by
   the bundle filler */
  if (active)
    return;

  pthread_rwlock_rdlock(rwlock);

  gettimeofday(&tv, NULL);

  d = parrot_bundles + write_index;

  write_index = (write_index+1) %num_bundles;

  if (!bundle_equal(d, bundle))
    {
      clear_bundle(d);
      build_bundle(d, &bundle->format);
      d->timestamp = bundle->timestamp; /* shouldn't be 0 */
    }

  if (bundle_equal(d, bundle))
    {
      d->timestamp = bundle->timestamp;
      memcpy(d->data, bundle->data, d->format.sizeimage);
    }

  pthread_rwlock_unlock(rwlock);
}

static
gboolean plugin_add_properties ( GnomePropertyBox * gpb )
{
  gint page;
  GtkWidget *vbox1;
  GtkWidget *label;
  GtkWidget *nbundles;

  if (!gpb)
    return TRUE;

  vbox1 = build_widget("vbox1", "parrot.glade");
  update_size_estimation(lookup_widget(vbox1, "size_estimation"), num_bundles);
  nbundles = lookup_widget(vbox1, "nbundles");
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(nbundles), num_bundles);
  gtk_signal_connect(GTK_OBJECT(nbundles), "changed",
		     on_property_item_changed, gpb);

  label = gtk_label_new(_("Parrot"));
  gtk_widget_show(label);

  page = gnome_property_box_append_page(gpb, GTK_WIDGET(vbox1), label);

  gtk_widget_show(vbox1);

  gtk_object_set_data(GTK_OBJECT(gpb), "parrot_page",
		      GINT_TO_POINTER( page ));


  return TRUE;
}

static
gboolean plugin_activate_properties ( GnomePropertyBox * gpb, gint page )
{
  gpointer data = gtk_object_get_data(GTK_OBJECT(gpb),
				      "parrot_page");
  GtkWidget *nbundles;
  GtkWidget *parrot_properties;
  gint t, i;

  if (GPOINTER_TO_INT(data) == page)
    {
      parrot_properties =
	gtk_notebook_get_nth_page(GTK_NOTEBOOK(gpb->notebook), page);

      nbundles = lookup_widget(parrot_properties, "nbundles");
      t = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(nbundles));
      if (t < 1)
	t = 1;
      pthread_rwlock_wrlock(rwlock);
      /* remove extra bundles */
      for (i=t; i<num_bundles; i++)
	clear_bundle(parrot_bundles+i);

      parrot_bundles = g_realloc(parrot_bundles, t*sizeof(*parrot_bundles));
      if (t > num_bundles)
	memset(parrot_bundles+num_bundles, 0,
	       (t-num_bundles)*sizeof(*parrot_bundles));
      num_bundles = t;
      pthread_rwlock_unlock(rwlock);

      return TRUE;
    }

  return FALSE;
}

static
gboolean plugin_help_properties ( GnomePropertyBox * gpb, gint page )
{
  gpointer data = gtk_object_get_data(GTK_OBJECT(gpb),
				      "parrot_page");
  /* FIXME: write this */
  gchar *help =
N_("parrot help todo");

  if (GPOINTER_TO_INT(data) == page)
    {
      ShowBox(_(help), GNOME_MESSAGE_BOX_INFO);
      return TRUE;
    }

  return FALSE;
}

static
void plugin_add_gui (GnomeApp * app)
{
  /*
    Define this function only if you are going to do something to the
    main Zapping window.
  */
}

static
void plugin_remove_gui (GnomeApp * app)
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
    10, /* we should run after the rest of the plugins */
    PLUGIN_CATEGORY_GUI | PLUGIN_CATEGORY_DEVICE_CONTROL
  };

  return (&returned_struct);
}
