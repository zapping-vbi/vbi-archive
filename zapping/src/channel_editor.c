/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2002 Iñaki García Etxebarria
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

/* $Id: channel_editor.c,v 1.37.2.5 2002-12-27 04:14:31 mschimek Exp $ */

/*
  TODO:
  * input type icon
  * dnd
  * write lock channel list
  * notify other modules (ttx bookmarks, schedule etc) about changes
  * device column
  * wizard, better country selection
  * channel merging e.g.
    12 FooBar +              key
       + dev1 tuner E5 fine  key
       + dev2 tuner E5 fine  key
 */

#include "../site_def.h"
#include "../config.h"

#include <gnome.h>

#define ZCONF_DOMAIN "/zapping/internal/properties/"
#include "zconf.h"
#include "zmisc.h"
#include "remote.h"
#include "frequencies.h"
#include "globals.h"
#include "zvbi.h"
#include "v4linterface.h"

#include "channel_editor.h"

typedef struct station_search station_search;

struct station_search
{
  GtkDialog *		station_search;
  GtkLabel *		label;
  GtkProgressBar *	progressbar;

  guint			timeout_handle;
  guint			channel;
  guint			found;
  guint			iteration;
  gint			freq;
  gint			strength;
  gint			afc;
};

enum {
  FL_NAME,
  FL_FREQ,
  FL_NUM_COLUMNS
};

typedef struct channel_editor channel_editor;

struct channel_editor
{
  GtkDialog *		channel_editor;

  GtkBox *		vbox;

  GtkOptionMenu *	country_menu;

  GtkButton *		channel_search;
  GtkButton *		add_all_channels;

  GtkTreeView *		freq_treeview;
  GtkListStore *	freq_model;
  GtkTreeSelection *	freq_selection;

  GtkTreeView *		channel_treeview;
  GtkListStore *	channel_model;
  GtkTreeSelection *	channel_selection;

  GtkWidget *		channel_up;
  GtkWidget *		channel_down;
  GtkWidget *		channel_add;
  GtkWidget *		channel_remove;

  GtkTable *		entry_table;
  GtkEntry *		entry_name;
  GtkWidget *		entry_fine_tuning;		/* spinslider */
  GtkAdjustment *	spinslider_adj;
  GtkOptionMenu *	entry_standard;
  GtkOptionMenu *	entry_input;
  GtkWidget *		entry_accel;			/* z_key_entry */

  GtkTooltips *		tooltips;

  tveng_tuned_channel *	old_channel_list;

  station_search *	search;

  gboolean		have_tuners;
};

#define DONT_CHANGE 0

#define BLOCK(object, signal)						\
  g_signal_handlers_block_by_func (G_OBJECT (ce->object),		\
    (gpointer) on_ ## object ## _ ## signal, (gpointer) ce)

#define UNBLOCK(object, signal)						\
  g_signal_handlers_unblock_by_func (G_OBJECT (ce->object),		\
    (gpointer) on_ ## object ## _ ## signal, (gpointer) ce)

static GtkMenu *
create_standard_menu		(channel_editor *	ce);

static GtkListStore *
create_freq_list_model		(const tveng_rf_table *	rf);

static void
on_channel_selection_changed	(GtkTreeSelection *	selection,
				 channel_editor *	ce);

static channel_editor *		dialog_running;

/*
 *  Misc helpers
 */

static gboolean
tunable_input			(channel_editor *	ce,
				 const tveng_device_info *info,
				 const tveng_tuned_channel *tc)
{
  struct tveng_enum_input *input;

  if (!ce->have_tuners)
    return FALSE;

  if (tc->input == DONT_CHANGE)
    return TRUE;

  input = tveng_find_input_by_hash (tc->input, info);

  return (input && input->tuners > 0);
}

#define VALID_ITER(iter, list_store)					\
  ((iter) != NULL							\
   && (iter)->user_data != NULL						\
   && ((GTK_LIST_STORE (list_store))->stamp == (iter)->stamp))

static inline guint
tree_model_index		(GtkTreeModel *		model,
				 GtkTreeIter *		iter)
{
  GtkTreePath *path;
  guint row;

  path = gtk_tree_model_get_path (model, iter);

  row = gtk_tree_path_get_indices (path)[0];

  gtk_tree_path_free (path);

  return row;
}

static inline tveng_tuned_channel *
tree_model_tuned_channel	(GtkTreeModel *		model,
				 GtkTreeIter *		iter)
{
  return tveng_tuned_channel_nth (global_channel_list,
				  tree_model_index (model, iter));
}

/* function where are thou? */
static gboolean
tree_model_get_iter_last	(GtkTreeModel *		model,
				 GtkTreeIter *		iter)
{
  GtkTreeIter iter2;

  if (!gtk_tree_model_get_iter_first (model, iter))
    return FALSE;

  iter2 = *iter;

  do *iter = iter2;
  while (gtk_tree_model_iter_next (model, &iter2));

  return TRUE;
}

static gboolean
tree_model_iter_prev		(GtkTreeModel *		model,
				 GtkTreeIter *		iter)
{
  GtkTreePath *path;
  gboolean r;

  path = gtk_tree_model_get_path (model, iter);

  if ((r = gtk_tree_path_prev (path)))
    gtk_tree_model_get_iter (model, iter, path);

  gtk_tree_path_free (path);

  return r;
}

/*
 *  Channel list helpers
 */

static inline guint
channel_list_index		(const channel_editor *	ce,
				 GtkTreeIter *		iter)
{
  return tree_model_index (GTK_TREE_MODEL (ce->channel_model), iter);
}

static void
channel_list_scroll_to_cell	(const channel_editor *	ce,
				 GtkTreeIter *		iter,
				 gfloat			row_align)
{
  GtkTreePath *path;

  if ((path = gtk_tree_model_get_path (GTK_TREE_MODEL (ce->channel_model), iter)))
    {
      gtk_tree_view_scroll_to_cell (ce->channel_treeview, path, NULL,
				    /* use_align */ TRUE, row_align, 0.0);
      gtk_tree_path_free (path);
    }
}

static void
channel_list_rows_changed	(channel_editor *	ce,
				 GtkTreeIter *		first_iter,
				 GtkTreeIter *		last_iter)
{
  GtkTreeModel *model = GTK_TREE_MODEL (ce->channel_model);
  GtkTreeIter iter;
  GtkTreePath *path, *last_path;

  if (!last_iter)
    last_iter = first_iter;

  iter = *first_iter;

  path = gtk_tree_model_get_path (model, first_iter);
  last_path = gtk_tree_model_get_path (model, last_iter);

  do
    {
      gtk_tree_model_row_changed (model, path, &iter);

      if (!gtk_tree_model_iter_next (model, &iter))
	break;

      gtk_tree_path_next (path);
    }
  while (gtk_tree_path_compare (path, last_path) <= 0);

  gtk_tree_path_free (last_path);
  gtk_tree_path_free (path);
}

static tveng_tuned_channel *
channel_list_get_tuned_channel	(const channel_editor *	ce,
				 GtkTreeIter *		iter)
{
  tveng_tuned_channel *tc;
  guint index;

  index = channel_list_index (ce, iter);

  tc = tveng_tuned_channel_nth (global_channel_list, index);

  g_assert (tc != NULL);

  return tc;
}

static gboolean
channel_list_get_selection	(const channel_editor *	ce,
				 GtkTreeIter *		iter_first,
				 GtkTreeIter *		iter_last,
				 tveng_tuned_channel **	tc_first,
				 tveng_tuned_channel **	tc_last)
{
  GtkTreeIter iter, last;

  if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (ce->channel_model), &iter))
    return FALSE; /* empty list */

  while (!gtk_tree_selection_iter_is_selected (ce->channel_selection, &iter))
    if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (ce->channel_model), &iter))
      return FALSE; /* nothing selected */

  if (iter_first)
    *iter_first = iter;

  if (tc_first)
    *tc_first = channel_list_get_tuned_channel (ce, &iter);

  if (iter_last || tc_last)
    {
      if (!iter_last)
	iter_last = &last;
      
      do
	{
	  *iter_last = iter;
	  
	  if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (ce->channel_model), &iter))
	    break;
	}
      while (gtk_tree_selection_iter_is_selected (ce->channel_selection, &iter));
      
      if (tc_last)
	*tc_last = channel_list_get_tuned_channel (ce, iter_last);
    }

  return TRUE;
}

static gchar *
rf_channel_string		(const tveng_tuned_channel *tc)
{
  return g_strdup_printf ("%s  %s  %.2f MHz",
			  _(tc->country),
			  tc->rf_name,
			  tc->freq / 1000.0);
}

static void
channel_list_add_tuned_channel	(channel_editor *	ce,
				 tveng_tuned_channel **	list,
				 const tveng_tuned_channel *tc)
{
  GtkTreeModel *model = GTK_TREE_MODEL (ce->channel_model);
  GtkTreeIter iter;
  GtkTreePath *path;
  tveng_tuned_channel *tci;
  guint i;

  /*
   *  Don't add when this channel (by tuner freq) is already
   *  listed. Eventually update the station name.
   */
  for (i = 0; (tci = tveng_tuned_channel_nth (*list, i)); i++)
    {
      if (tci->input != tc->input)
	continue;

      if (tunable_input (ce, main_info, tc)
	  && (tci->freq - tc->freq) > 3000)
	continue;

      gtk_tree_model_iter_nth_child (model, &iter, NULL, i);
      path = gtk_tree_model_get_path (model, &iter);
      gtk_tree_view_scroll_to_cell (ce->channel_treeview, path, NULL,
                                    /* use_align */ TRUE, 0.5, 0.0);
      gtk_tree_path_free (path);

      if (0 != strcmp (tci->name, tci->rf_name))
	return; /* user changed station name */

      if (0 == strcmp (tc->name, tci->rf_name))
	return; /* we have no station name */
      /*
      gtk_list_store_set (ce->channel_model, &iter,
			  CL_NAME, tc->name, -1);
      */
      g_free (tci->name);
      tci->name = g_strdup (tc->name);

      return;
    }

  tveng_tuned_channel_insert (list, tveng_tuned_channel_new (tc), G_MAXINT);

  gtk_list_store_append (ce->channel_model, &iter);

  channel_list_scroll_to_cell (ce, &iter, 0.5);
}

/*
 *  Dialog helpers
 */

static void
entry_fine_tuning_set		(channel_editor *	ce,
				 const tveng_device_info *info,
				 guint			freq)
{
  GtkAdjustment *spin_adj = z_spinslider_get_spin_adj (ce->entry_fine_tuning);
  GtkAdjustment *hscale_adj = z_spinslider_get_hscale_adj (ce->entry_fine_tuning);

  if (freq > 0)
    {
      double dfreq;

      freq += 25;
      freq -= freq % 50;

      dfreq = freq * 1e-3; /* MHz */

      spin_adj->value = dfreq;
      spin_adj->lower = 5;
      spin_adj->upper = 1999;
      spin_adj->step_increment = 0.05;
      spin_adj->page_increment = 1;
      spin_adj->page_size = 0;

      hscale_adj->value = dfreq;
      hscale_adj->lower = dfreq - 4;
      hscale_adj->upper = dfreq + 4;
      hscale_adj->step_increment = 0.05;
      hscale_adj->page_increment = 1;
      hscale_adj->page_size = 0;

      gtk_adjustment_changed (spin_adj);
      gtk_adjustment_value_changed (spin_adj);
      gtk_adjustment_changed (hscale_adj);
      gtk_adjustment_value_changed (hscale_adj);

      z_spinslider_set_reset_value (ce->entry_fine_tuning, dfreq);
    }

  if (freq == 0
      || info->num_inputs == 0
      || info->inputs[info->cur_input].tuners == 0)
    gtk_widget_set_sensitive (ce->entry_fine_tuning, FALSE);
  else
    gtk_widget_set_sensitive (ce->entry_fine_tuning, TRUE);
}

static void
no_channel_selected		(channel_editor *	ce)
{
  gtk_widget_set_sensitive (ce->channel_up, FALSE);
  gtk_widget_set_sensitive (ce->channel_down, FALSE);
  gtk_widget_set_sensitive (ce->channel_remove, FALSE);

  gtk_entry_set_text (ce->entry_name, "");
  z_option_menu_set_active (GTK_WIDGET (ce->entry_standard), 0);
  z_option_menu_set_active (GTK_WIDGET (ce->entry_input), 0);
  z_key_entry_set_key (ce->entry_accel, Z_KEY_NONE);
  gtk_widget_set_sensitive (GTK_WIDGET (ce->entry_table), FALSE);  
}

static void
channel_buttons_set_sensitive	(channel_editor *	ce,
				 gboolean		any_selected)
{
  GtkTreeIter iter;

  if (!gtk_tree_model_get_iter_first
      (GTK_TREE_MODEL (ce->channel_model), &iter))
    {
      no_channel_selected (ce);
      return;
    }

  gtk_widget_set_sensitive (ce->channel_up,
			    !gtk_tree_selection_iter_is_selected
			    (ce->channel_selection, &iter));

  tree_model_get_iter_last (GTK_TREE_MODEL (ce->channel_model), &iter);

  gtk_widget_set_sensitive (ce->channel_down,
			    !gtk_tree_selection_iter_is_selected
			    (ce->channel_selection, &iter));

  gtk_widget_set_sensitive (ce->channel_remove, any_selected);
}

/*
 *  Signals
 */

static void
on_country_menu_changed		(GtkOptionMenu *	country_menu,
				 channel_editor *	ce)
{
  tveng_rf_table *rf;
  guint index;

  index = z_option_menu_get_active (GTK_WIDGET (country_menu));
  rf = tveng_get_country_tune_by_id (index);

  g_assert (rf != NULL);

  current_country = rf;

  ce->freq_model = create_freq_list_model (rf);
  gtk_tree_view_set_model (ce->freq_treeview, GTK_TREE_MODEL (ce->freq_model));
}

static void
on_station_search_cancel_clicked (GtkButton *		cancel,
				  channel_editor *	ce)
{
  if (ce->search)
    gtk_widget_destroy (GTK_WIDGET (ce->search->station_search));
}

static void
on_station_search_destroy	(GtkObject *		unused,
				 channel_editor *	ce)
{
  if (ce->search)
    {
      gtk_timeout_remove (ce->search->timeout_handle);

      g_free (ce->search);
      ce->search = NULL;
    }

  gtk_widget_set_sensitive (GTK_WIDGET (ce->vbox), TRUE);
}

static gboolean
station_search_timeout		(gpointer		p)
{
  channel_editor *ce = p;
  station_search *cs = ce->search;
  tveng_rf_channel *ch;
  tveng_tuned_channel tc;
  gchar *station_name;
  gint strength, afc;

  if (!(cs = ce->search))
    return FALSE;

  if (cs->iteration == 0)
    {
      gchar *buf;

      /* Next channel */

      if (cs->channel >= current_country->channel_count)
	{
	  gtk_widget_destroy (GTK_WIDGET (cs->station_search));
	  return FALSE; /* remove timer */
	}

      gtk_progress_bar_set_fraction (cs->progressbar,
				     cs->channel / (gdouble)
				     current_country->channel_count);

      ch = tveng_get_channel_by_id (cs->channel, current_country);

      buf = g_strdup_printf (_("Channel: %s   Found: %u"),
			     ch->name, cs->found);
      gtk_label_set_text (cs->label, buf);
      g_free (buf);

      cs->freq = ch->freq;
      cs->strength = 0;

      if (-1 == tveng_tune_input (cs->freq, main_info))
	goto next_channel;

#ifdef HAVE_LIBZVBI
      /* zvbi should store the station name if known from now */
      zvbi_name_unknown();
#endif
      cs->iteration = 1;
    }
  else
    {
      /* Probe */

      if (-1 == tveng_get_signal_strength (&strength, &afc, main_info))
	goto next_channel;

      if (strength > 0)
	{
	  cs->strength = strength;
	}
      else if (cs->iteration >= 5
	       && cs->strength == 0)
	{
	  goto next_channel; /* no signal after 0.5 sec */
	}

      if (afc && (afc != -cs->afc))
	{
	  cs->afc = afc;
	  cs->freq += afc * 25; /* should be afc*50, but won't harm */

	  /* error ignored */
	  tveng_tune_input (cs->freq += afc * 25, main_info);
	}

#ifdef HAVE_LIBZVBI
      if (zconf_get_boolean(NULL, "/zapping/options/vbi/use_vbi"))
	{
	  if ((station_name = zvbi_get_name ()))
	    goto add_station;

	  /* How long for XDS? */
	  if (cs->iteration >= 25)
	    goto add_default; /* no name after 2.5 sec */
	}
      else
#endif
	if (cs->iteration >= 10)
	  goto add_default; /* after 1 sec afc */

      cs->iteration++;
    }

  return TRUE; /* continue */

 add_default:
  ch = tveng_get_channel_by_id (cs->channel, current_country);
  station_name = g_strdup (ch->name);

 add_station:
  ch = tveng_get_channel_by_id (cs->channel, current_country);

  memset (&tc, 0, sizeof (tc));
  tc.name = station_name;
  tc.rf_name = (gchar *) ch->name;
  tc.country = (gchar *) current_country->name;
  tc.freq = cs->freq;

  channel_list_add_tuned_channel (ce, &global_channel_list, &tc);

  g_free (station_name);
  cs->found++;

 next_channel:
  cs->channel++;
  cs->iteration = 0;

  return TRUE; /* continue */
}

static void
on_channel_search_clicked	(GtkButton *		search,
				 channel_editor *	ce)
{
  station_search *cs;
  GtkWidget *dialog_vbox;
  GtkWidget *vbox;
  GtkWidget *dialog_action_area;
  GtkWidget *cancel;
  guint id;

  if (ce->search)
    return;

  cs = g_malloc (sizeof (station_search));
  ce->search = cs;
  cs->found = 0;

  cs->station_search = GTK_DIALOG (gtk_dialog_new ());
  gtk_window_set_title (GTK_WINDOW (cs->station_search), _("Searching..."));
  g_signal_connect (G_OBJECT (cs->station_search), "destroy",
                    G_CALLBACK (on_station_search_destroy), ce);

  dialog_vbox = cs->station_search->vbox;
  gtk_widget_show (dialog_vbox);

  vbox = gtk_vbox_new (FALSE, 3);
  gtk_widget_show (vbox);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), vbox, TRUE, TRUE, 0);

  cs->label = GTK_LABEL (gtk_label_new (""));
  gtk_widget_show (GTK_WIDGET (cs->label));
  gtk_box_pack_start (GTK_BOX (vbox),
		      GTK_WIDGET (cs->label),
		      FALSE, FALSE, 0);

  cs->progressbar = GTK_PROGRESS_BAR (gtk_progress_bar_new ());
  gtk_widget_show (GTK_WIDGET (cs->progressbar));
  gtk_progress_bar_set_fraction (cs->progressbar, 0.0);
  gtk_box_pack_start (GTK_BOX (vbox),
		      GTK_WIDGET (cs->progressbar),
		      FALSE, FALSE, 0);

  dialog_action_area = cs->station_search->action_area;
  gtk_widget_show (dialog_action_area);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  cancel = gtk_button_new_from_stock (GTK_STOCK_STOP);
  gtk_widget_show (cancel);
  gtk_dialog_add_action_widget (cs->station_search, cancel, 0);
  g_signal_connect (G_OBJECT (cancel), "clicked",
		    G_CALLBACK (on_station_search_cancel_clicked), ce);

  gtk_widget_show (GTK_WIDGET (cs->station_search));

  gtk_widget_set_sensitive (GTK_WIDGET (ce->vbox), FALSE);

  cs->channel = 0;
  cs->iteration = 0;

  for (id = 0; id < main_info->num_inputs; id++)
    if (main_info->inputs[id].tuners > 0)
      break;

  g_assert (id < main_info->num_inputs);

  tveng_set_input_by_id (id, main_info); /* XXX consider tuners > 1 */

  cs->timeout_handle = gtk_timeout_add (100 /* ms */,
					station_search_timeout, ce);
}

static void
on_add_all_channels_clicked	(GtkButton *		add_all_channels,
				 channel_editor *	ce)
{
  tveng_rf_channel *ch;
  tveng_tuned_channel tc;
  guint i;

  for (i = 0; (ch = tveng_get_channel_by_id (i, current_country)); i++)
    {
      memset (&tc, 0, sizeof (tc));

      tc.name = (gchar *) ch->name;
      tc.rf_name = (gchar *) ch->name;
      tc.country = (gchar *) current_country->name;
      tc.freq = ch->freq;

      channel_list_add_tuned_channel (ce, &global_channel_list, &tc);
    }
}

static void
on_freq_selection_changed	(GtkTreeSelection *	selection,
				 channel_editor *	ce)
{
  GtkTreeIter first, last;
  GtkTreeIter freq_iter;
  tveng_tuned_channel *tc, *tc_first, *tc_last;
  tveng_rf_channel *ch;
  gchar *name;

  if (!gtk_tree_selection_get_selected (selection, NULL, &freq_iter))
    return;

  if (!channel_list_get_selection (ce, &first, &last, &tc_first, &tc_last))
    return;

  gtk_tree_model_get (GTK_TREE_MODEL (ce->freq_model), &freq_iter,
		      FL_NAME, &name, -1);

  ch = tveng_get_channel_by_name (name, current_country);

  g_free (name);

  if (!ch)
    return;

  for (tc = tc_first;; tc = tc->next)
    {
      if (0 != strcmp (tc->country, current_country->name))
	{
	  g_free (tc->country);
	  tc->country = g_strdup (current_country->name);
	}

      if (0 != strcmp (tc->rf_name, ch->name))
	{
	  g_free (tc->rf_name);
	  tc->rf_name = g_strdup (ch->name);
	}

      tc->freq = ch->freq;

      if (tc == tc_last)
	break;
    }

  if (tunable_input (ce, main_info, tc_first))
    {
      entry_fine_tuning_set (ce, main_info, ch->freq);
      tveng_tune_input (ch->freq, main_info);
    }

  channel_list_rows_changed (ce, &first, &last);
}

static void
on_channel_up_clicked		(GtkButton *		channel_up,
				 channel_editor *	ce)
{
  GtkTreeIter dummy, first, last, first_prev;
  tveng_tuned_channel *tc;
  guint index;

  if (!channel_list_get_selection (ce, &first, &last, NULL, NULL))
    return;

  if (!tree_model_iter_prev (GTK_TREE_MODEL (ce->channel_model), &first))
    return; /* nothing above */

  tc = channel_list_get_tuned_channel (ce, &first);
  index = channel_list_index (ce, &last) + 1;
  tveng_tuned_channel_move (&global_channel_list, tc, index);

  gtk_list_store_insert_after (ce->channel_model, &dummy, &last);
  gtk_list_store_remove (ce->channel_model, &first);

  channel_list_get_selection (ce, &first, NULL, NULL, NULL);
  first_prev = first;
  if (!tree_model_iter_prev (GTK_TREE_MODEL (ce->channel_model), &first_prev))
    first_prev = first;
  channel_list_scroll_to_cell (ce, &first_prev, 0.01);

  channel_buttons_set_sensitive	(ce, TRUE);
}

static void
on_channel_down_clicked		(GtkButton *		channel_down,
				 channel_editor *	ce)
{
  GtkTreeIter dummy, first, last, last_next;
  tveng_tuned_channel *tc;
  guint index;

  if (!channel_list_get_selection (ce, &first, &last, NULL, NULL))
    return;

  if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (ce->channel_model), &last))
    return; /* nothing below */

  tc = channel_list_get_tuned_channel (ce, &last);
  index = channel_list_index (ce, &first);
  tveng_tuned_channel_move (&global_channel_list, tc, index);

  gtk_list_store_remove (ce->channel_model, &last);
  gtk_list_store_insert_before (ce->channel_model, &dummy, &first);

  channel_list_get_selection (ce, NULL, &last, NULL, NULL);
  last_next = last;
  if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (ce->channel_model), &last_next))
    last_next = last;
  channel_list_scroll_to_cell (ce, &last_next, 0.99);

  channel_buttons_set_sensitive	(ce, TRUE);
}

static void
on_channel_add_clicked		(GtkButton *		channel_add,
				 channel_editor *	ce)
{
  tveng_tuned_channel tc;
  GtkTreeIter iter;

  memset (&tc, 0, sizeof (tc));

  tc.name = "";
  tc.rf_name = "";
  tc.country = "";

  if (channel_list_get_selection (ce, &iter, NULL, NULL, NULL))
    {
      tveng_tuned_channel_insert (&global_channel_list,
				  tveng_tuned_channel_new (&tc),
				  channel_list_index (ce, &iter));
      gtk_list_store_insert_before (ce->channel_model, &iter, &iter);
    }
  else
    {
      tveng_tuned_channel_insert (&global_channel_list,
				  tveng_tuned_channel_new (&tc),
				  G_MAXINT);
      gtk_list_store_append (ce->channel_model, &iter);
    }

  gtk_tree_selection_unselect_all (ce->channel_selection);
  gtk_tree_selection_select_iter (ce->channel_selection, &iter);

  channel_list_scroll_to_cell (ce, &iter, 0.5);

  channel_buttons_set_sensitive	(ce, TRUE);
}

static void
on_channel_remove_clicked	(GtkButton *		channel_remove,
				 channel_editor *	ce)
{
  tveng_tuned_channel *tc, *tc_next;
  GtkTreeIter iter;

  if (!channel_list_get_selection (ce, &iter, NULL, &tc, NULL))
    return;

  BLOCK (channel_selection, changed);

  while (VALID_ITER (&iter, ce->channel_model))
    {
      if (!gtk_tree_selection_iter_is_selected (ce->channel_selection, &iter))
	break;

      gtk_list_store_remove (ce->channel_model, &iter);

      tc_next = tc->next;
      tveng_tuned_channel_remove (&global_channel_list, tc);
      tveng_tuned_channel_delete (tc);
      tc = tc_next;
    }

  UNBLOCK (channel_selection, changed);

  if (VALID_ITER (&iter, ce->channel_model))
    {
      gtk_tree_selection_select_iter (ce->channel_selection, &iter);
      channel_list_scroll_to_cell (ce, &iter, 0.5);
    }
  else
    {
      no_channel_selected (ce);
    }
}

static void
on_entry_name_changed		(GtkEditable *		channel_name,
				 channel_editor *	ce)
{
  tveng_tuned_channel *tc, *tc_last;
  GtkTreeIter first, last;
  gchar *name;

  if (!channel_list_get_selection (ce, &first, &last, &tc, &tc_last))
    return;

  name = gtk_editable_get_chars (channel_name, 0, -1);

  g_free (tc->name);
  tc->name = name;

  while (tc != tc_last)
    {
      tc = tc->next;

      g_free (tc->name);
      tc->name = g_strdup (name);
    }

  channel_list_rows_changed (ce, &first, &last);
}

static void
on_entry_input_changed		(GtkOptionMenu *	entry_input,
				 channel_editor *	ce)
{
  tveng_tuned_channel *tc, *tc_last;
  GtkTreeIter first, last;
  guint id;

  if (!channel_list_get_selection (ce, &first, &last, &tc, &tc_last))
    return;

  id = z_option_menu_get_active (GTK_WIDGET (entry_input));

  if (id == DONT_CHANGE)
    {
      tc->input = 0;
    }
  else
    {
      tc->input = main_info->inputs[id - 1].hash;
      tveng_set_input_by_id (id - 1, main_info);
    }

  for (; tc_last != tc; tc_last = tc_last->prev)
    tc_last->input = tc->input;

  channel_list_rows_changed (ce, &first, &last);
}

static void
on_entry_fine_tuning_value_changed (GtkAdjustment *	spin_adj,
				    channel_editor *	ce)
{
  tveng_tuned_channel *tc, *tc_last;
  GtkTreeIter first, last;

  if (!channel_list_get_selection (ce, &first, &last, &tc, &tc_last))
    return;

  tc->freq = (guint)(spin_adj->value * 1000);

  tveng_tune_input (tc->freq, main_info);

  for (; tc_last != tc; tc_last = tc_last->prev)
    {
      if (0 != strcmp (tc->rf_name, tc_last->rf_name))
	{
	  g_free (tc_last->rf_name);
	  tc_last->rf_name = g_strdup (tc->rf_name);
	}

      tc_last->freq = tc->freq;
    }

  channel_list_rows_changed (ce, &first, &last);
}

static void
on_entry_standard_changed	(GtkOptionMenu *	entry_standard,
				 channel_editor *	ce)
{
  tveng_tuned_channel *tc, *tc_last;
  GtkTreeIter first, last;
  guint id;

  if (!channel_list_get_selection (ce, &first, &last, &tc, &tc_last))
    return;

  id = z_option_menu_get_active (GTK_WIDGET (entry_standard));

  if (id == DONT_CHANGE)
    {
      tc->standard = 0;
    }
  else
    {
      tc->standard = main_info->standards[id - 1].hash;
      tveng_set_standard_by_id (id - 1, main_info);
    }

  for (; tc_last != tc; tc_last = tc_last->prev)
    tc_last->standard = tc->standard;

  channel_list_rows_changed (ce, &first, &last);
}

static void
on_entry_accel_changed		(GtkEditable *		editable,
				 channel_editor *	ce)
{
  tveng_tuned_channel *tc, *tc_last;
  GtkTreeIter first, last;
  z_key key;

  if (!channel_list_get_selection (ce, &first, &last, &tc, &tc_last))
    return;

  key = z_key_entry_get_key (ce->entry_accel);

  for (;;)
    {
      tc->accel = key;
      if (tc == tc_last)
	break;
      tc = tc->next;
    }

  channel_list_rows_changed (ce, &first, &last);
}

static void
on_channel_selection_changed	(GtkTreeSelection *	selection,
				 channel_editor *	ce)
{
  GtkTreeIter iter;
  tveng_tuned_channel *tc;

  if (!channel_list_get_selection (ce, &iter, NULL, &tc, NULL))
    {
      no_channel_selected (ce);
      return;
    }

  z_switch_channel (tc, main_info);

  {
    BLOCK (entry_name, changed);

    gtk_entry_set_text (ce->entry_name, tc->name);

    UNBLOCK (entry_name, changed);
  }

  {
    struct tveng_enum_input *input;
    guint id = 0;

    BLOCK (entry_input, changed);

    if (tc->input != DONT_CHANGE)
      if ((input = tveng_find_input_by_hash (tc->input, main_info)))
	id = input->index + 1;

    z_option_menu_set_active (GTK_WIDGET (ce->entry_input), id);

    UNBLOCK (entry_input, changed);
  }

  {
    g_signal_handlers_block_by_func (G_OBJECT (ce->spinslider_adj),
				     (gpointer) on_entry_fine_tuning_value_changed,
				     (gpointer) ce);

    entry_fine_tuning_set (ce, main_info, tc->freq);

    g_signal_handlers_unblock_by_func (G_OBJECT (ce->spinslider_adj),
				       (gpointer) on_entry_fine_tuning_value_changed,
				       (gpointer) ce);
  }

  {
    struct tveng_enumstd *standard;
    guint id = 0;

    BLOCK (entry_standard, changed);

    /* Standards depend on current input */
    gtk_widget_destroy (gtk_option_menu_get_menu (ce->entry_standard));
    gtk_option_menu_set_menu (ce->entry_standard,
			      GTK_WIDGET (create_standard_menu (ce)));

    if (tc->standard != DONT_CHANGE)
      if ((standard = tveng_find_standard_by_hash (tc->standard, main_info)))
	id = standard->index + 1;

    z_option_menu_set_active (GTK_WIDGET (ce->entry_standard), id);

    UNBLOCK (entry_standard, changed);
  }

  {
    g_signal_handlers_block_by_func (G_OBJECT (z_key_entry_entry (ce->entry_accel)),
				     (gpointer) on_entry_accel_changed, (gpointer) ce);

    z_key_entry_set_key (ce->entry_accel, tc->accel);

    g_signal_handlers_unblock_by_func (G_OBJECT (z_key_entry_entry (ce->entry_accel)),
				       (gpointer) on_entry_accel_changed, (gpointer) ce);
  }

  gtk_widget_set_sensitive (GTK_WIDGET (ce->entry_table), TRUE);

  channel_buttons_set_sensitive (ce, TRUE);
}

static void
on_ok_clicked			(GtkButton *		ok,
				 channel_editor *	ce)
{
  tveng_tuned_channel_list_delete (&ce->old_channel_list);

  gtk_widget_destroy (GTK_WIDGET (ce->channel_editor));
}

static void
on_cancel_clicked		(GtkButton *		cancel,
				 channel_editor *	ce)
{
  gtk_widget_destroy (GTK_WIDGET (ce->channel_editor));
}

static void
on_channel_editor_destroy	(GtkObject *		unused,
				 channel_editor *	ce)
{
  if (ce->search)
    gtk_widget_destroy (GTK_WIDGET (ce->search->station_search));

  if (ce->old_channel_list)
    {
      tveng_tuned_channel_list_delete (&global_channel_list);
      global_channel_list = ce->old_channel_list;
      ce->old_channel_list = NULL;
    }

  g_free (ce);

  /* Update menus. XXX should rebuild automatically when
     opened after any change. */
  zmodel_changed (z_input_model);

  dialog_running = NULL;
}

/*
 * channel_list GtkTreeCellDataFuncs
 */

static void
set_func_index			(GtkTreeViewColumn *	column,
				 GtkCellRenderer *	cell,
				 GtkTreeModel *		model,
				 GtkTreeIter *		iter,
				 gpointer		data)
{
  tveng_tuned_channel *tc = tree_model_tuned_channel (model, iter);
  gchar buf[32];

  g_snprintf (buf, sizeof (buf) - 1, "%u", tc->index);

  g_object_set (GTK_CELL_RENDERER (cell), "text", buf, NULL);
}

static void
set_func_name			(GtkTreeViewColumn *	column,
				 GtkCellRenderer *	cell,
				 GtkTreeModel *		model,
				 GtkTreeIter *		iter,
				 gpointer		data)
{
  tveng_tuned_channel *tc = tree_model_tuned_channel (model, iter);

  g_object_set (GTK_CELL_RENDERER (cell), "text", tc->name, NULL);
}

static void
set_func_input			(GtkTreeViewColumn *	column,
				 GtkCellRenderer *	cell,
				 GtkTreeModel *		model,
				 GtkTreeIter *		iter,
				 gpointer		data)
{
  tveng_tuned_channel *tc = tree_model_tuned_channel (model, iter);
  struct tveng_enum_input *input;
  gchar *input_name = NULL;

  if (tc->input != DONT_CHANGE)
    if ((input = tveng_find_input_by_hash (tc->input, main_info)))
      input_name = input->name;

  g_object_set (GTK_CELL_RENDERER (cell), "text", input_name, NULL);
}

static void
set_func_channel		(GtkTreeViewColumn *	column,
				 GtkCellRenderer *	cell,
				 GtkTreeModel *		model,
				 GtkTreeIter *		iter,
				 gpointer		data)
{
  channel_editor *ce = data;
  tveng_tuned_channel *tc = tree_model_tuned_channel (model, iter);

  if (tunable_input (ce, main_info, tc)
      && tc->freq != 0)
    {
      gchar *rf_name;

      rf_name = rf_channel_string (tc);
      g_object_set (GTK_CELL_RENDERER (cell), "text", rf_name, NULL);
      g_free (rf_name);
    }
  else
    {
      g_object_set (GTK_CELL_RENDERER (cell), "text", NULL, NULL);
    }
}

static void
set_func_standard		(GtkTreeViewColumn *	column,
				 GtkCellRenderer *	cell,
				 GtkTreeModel *		model,
				 GtkTreeIter *		iter,
				 gpointer		data)
{
  tveng_tuned_channel *tc = tree_model_tuned_channel (model, iter);
  struct tveng_enumstd *standard;
  gchar *standard_name = NULL;

  if (tc->standard != DONT_CHANGE)
    if ((standard = tveng_find_standard_by_hash (tc->standard, main_info)))
      standard_name = standard->name;

  g_object_set (GTK_CELL_RENDERER (cell), "text", standard_name, NULL);
}

static void
set_func_key			(GtkTreeViewColumn *	column,
				 GtkCellRenderer *	cell,
				 GtkTreeModel *		model,
				 GtkTreeIter *		iter,
				 gpointer		data)
{
  tveng_tuned_channel *tc = tree_model_tuned_channel (model, iter);
  gchar *key_name;

  key_name = z_key_name (tc->accel);

  g_object_set (GTK_CELL_RENDERER (cell), "text", key_name, NULL);

  g_free (key_name);
}

#define LABEL(name, x, y)						\
  label = gtk_label_new (_(name));					\
  gtk_widget_show (label);						\
  gtk_table_attach (ce->entry_table, label, x, x + 1, y, y + 1,		\
		    (GtkAttachOptions) (GTK_FILL),			\
		    (GtkAttachOptions) (0), 0, 0);			\
  gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);			\
  gtk_misc_set_padding (GTK_MISC (label), 3, 0);

#define BUTTON(name, stock, sensitive)					\
  ce->name = gtk_button_new_from_stock (stock);				\
  gtk_widget_show (ce->name);						\
  gtk_widget_set_sensitive (ce->name, sensitive);			\
  gtk_box_pack_start (GTK_BOX (vbox), ce->name, FALSE, FALSE, 0);	\
  CONNECT (name, clicked);

#define CONNECT(object, signal)						\
  g_signal_connect (G_OBJECT (ce->object), #signal,			\
		    G_CALLBACK (on_ ## object ## _ ## signal), ce)

static GtkWidget *
create_country_menu		(channel_editor *	ce)
{
  GtkWidget *country_menu;
  GtkWidget *menu;
  tveng_rf_table *rf;
  guint i;

  country_menu = gtk_option_menu_new ();
  gtk_widget_show (country_menu);
  gtk_tooltips_set_tip (ce->tooltips, country_menu,
			_("Select here the frequency table "
			  "used in your country"), NULL);

  menu = gtk_menu_new ();
  gtk_option_menu_set_menu (GTK_OPTION_MENU (country_menu), menu);

  for (i = 0; (rf = tveng_get_country_tune_by_id (i)); i++)
    {
      GtkWidget *item;

      item = gtk_menu_item_new_with_label (_(rf->name));
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);
    }

  g_assert (i > 0);

  gtk_option_menu_set_history (GTK_OPTION_MENU (country_menu),
			       tveng_get_id_of_country_tune
			       (current_country));
  return country_menu;
}

static GtkListStore *
create_freq_list_model		(const tveng_rf_table *	rf)
{
  GtkListStore *model;
  tveng_rf_channel *ch;
  guint i;

  model = gtk_list_store_new (FL_NUM_COLUMNS,
			      G_TYPE_STRING,	/* name */
			      G_TYPE_STRING);	/* freq */

  for (i = 0; (ch = tveng_get_channel_by_id (i, rf)); i++)
    {
      gchar freq[256];
      GtkTreeIter iter;

      g_snprintf (freq, sizeof (freq) - 1, "%.2f", ch->freq / 1000.0);

      gtk_list_store_append (model, &iter);
      gtk_list_store_set (model, &iter,
			  FL_NAME, ch->name,
			  FL_FREQ, freq, -1);
    }

  return model;
}

static GtkWidget *
create_freq_treeview		(channel_editor *	ce)
{
  GtkWidget *scrolledwindow;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (scrolledwindow);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
				  GTK_POLICY_NEVER,
				  GTK_POLICY_AUTOMATIC);

  ce->freq_treeview = GTK_TREE_VIEW (gtk_tree_view_new ());
  gtk_widget_show (GTK_WIDGET (ce->freq_treeview));
  gtk_container_add (GTK_CONTAINER (scrolledwindow), GTK_WIDGET (ce->freq_treeview));
  gtk_tree_view_set_rules_hint (ce->freq_treeview, TRUE);

  ce->freq_selection = gtk_tree_view_get_selection (ce->freq_treeview);
  gtk_tree_selection_set_mode (ce->freq_selection, GTK_SELECTION_SINGLE);
  CONNECT (freq_selection, changed);

  ce->freq_model = create_freq_list_model (current_country);
  gtk_tree_view_set_model (ce->freq_treeview, GTK_TREE_MODEL (ce->freq_model));

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes
    (_("Name"), renderer, "text", FL_NAME, NULL);
  gtk_tree_view_append_column (ce->freq_treeview, column);  
	  
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes
    (_("Freq (MHz)"), renderer, "text", FL_FREQ, NULL);
  gtk_tree_view_append_column (ce->freq_treeview, column);  

  return scrolledwindow;
}

static GtkListStore *
create_channel_list_model	(const tveng_tuned_channel *list)
{
  GtkListStore *model;
  GtkTreeIter iter;
  guint i;

  model = gtk_list_store_new (1, G_TYPE_UINT);

  for (i = tveng_tuned_channel_num (global_channel_list); i; i--)
    gtk_list_store_append (model, &iter);

  return model;
}

static GtkWidget *
create_channel_treeview		(channel_editor *	ce)
{
  GtkWidget *scrolledwindow;

  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (scrolledwindow);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
				  GTK_POLICY_NEVER,
				  GTK_POLICY_AUTOMATIC);

  ce->channel_treeview = GTK_TREE_VIEW (gtk_tree_view_new ());
  gtk_widget_show (GTK_WIDGET (ce->channel_treeview));
  gtk_container_add (GTK_CONTAINER (scrolledwindow),
		     GTK_WIDGET (ce->channel_treeview));
  gtk_tree_view_set_rules_hint (ce->channel_treeview, TRUE);
  /*
  gtk_tree_view_set_search_column (ce->channel_treeview, CL_NAME);
  */
  ce->channel_selection = gtk_tree_view_get_selection (ce->channel_treeview);
  gtk_tree_selection_set_mode (ce->channel_selection, GTK_SELECTION_MULTIPLE);
  CONNECT (channel_selection, changed);

  ce->channel_model = create_channel_list_model (global_channel_list); // XXX
  gtk_tree_view_set_model (ce->channel_treeview,
			   GTK_TREE_MODEL (ce->channel_model));

  gtk_tree_view_insert_column_with_data_func
    (ce->channel_treeview, -1 /* append */, "",
     gtk_cell_renderer_text_new (), set_func_index, ce, NULL);

  gtk_tree_view_insert_column_with_data_func
    (ce->channel_treeview, -1 /* append */, _("Name"),
     gtk_cell_renderer_text_new (), set_func_name, ce, NULL);

  gtk_tree_view_insert_column_with_data_func
    (ce->channel_treeview, -1 /* append */, _("Input"),
     gtk_cell_renderer_text_new (), set_func_input, ce, NULL);

  gtk_tree_view_insert_column_with_data_func
    (ce->channel_treeview, -1 /* append */, _("Channel"),
     gtk_cell_renderer_text_new (), set_func_channel, ce, NULL);

  gtk_tree_view_insert_column_with_data_func
    (ce->channel_treeview, -1 /* append */, _("Standard"),
     gtk_cell_renderer_text_new (), set_func_standard, ce, NULL);
  
  gtk_tree_view_insert_column_with_data_func
    (ce->channel_treeview, -1 /* append */, _("Key"),
     gtk_cell_renderer_text_new (), set_func_key, ce, NULL);
  
  return scrolledwindow;
}

static GtkMenu *
create_input_menu		(channel_editor *	ce)
{
  GtkMenu *menu;
  GtkWidget *menu_item;
  guint i;

  menu = GTK_MENU (gtk_menu_new ());

  menu_item = gtk_menu_item_new_with_label (_("Do not change input"));
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

  for (i = 0; i < main_info->num_inputs; i++)
    {
      menu_item = gtk_menu_item_new_with_label (main_info->inputs[i].name);
      gtk_widget_show (menu_item);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
    }

  return menu;
}

static GtkMenu *
create_standard_menu		(channel_editor *	ce)
{
  GtkMenu *menu;
  GtkWidget *menu_item;
  guint i;

  menu = GTK_MENU (gtk_menu_new ());

  menu_item = gtk_menu_item_new_with_label (_("Do not change standard"));
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

  for (i = 0; i < main_info->num_standards; i++)
    {
      menu_item = gtk_menu_item_new_with_label (main_info->standards[i].name);
      gtk_widget_show (menu_item);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
    }

  return menu;
}

static GtkWidget *
create_channel_editor		(void)
{
  struct channel_editor *ce;
  guint id;

  ce = g_malloc (sizeof (*ce));

  ce->old_channel_list = tveng_tuned_channel_list_new (global_channel_list);

  ce->search = NULL;

  for (id = 0; id < main_info->num_inputs; id++)
    if (main_info->inputs[id].tuners > 0)
      break;

  ce->have_tuners = (id < main_info->num_inputs);

  /* Build dialog */

  ce->tooltips = gtk_tooltips_new ();

  ce->channel_editor = GTK_DIALOG (gtk_dialog_new ());
  gtk_window_set_title (GTK_WINDOW (ce->channel_editor), _("Channel editor"));
  CONNECT (channel_editor, destroy);

  {
    GtkWidget *dialog_vbox;

    dialog_vbox = GTK_DIALOG (ce->channel_editor)->vbox;
    gtk_widget_show (dialog_vbox);

    ce->vbox = GTK_BOX (gtk_vbox_new (FALSE, 3));
    gtk_widget_show (GTK_WIDGET (ce->vbox));
    gtk_box_pack_start (GTK_BOX (dialog_vbox),
			GTK_WIDGET (ce->vbox), TRUE, TRUE, 0);

    {
      GtkWidget *vpaned;

      vpaned = gtk_vpaned_new ();
      gtk_widget_show (vpaned);
      gtk_box_pack_start (ce->vbox, vpaned, TRUE, TRUE, 0);

      {
	GtkWidget *frame;

	frame = gtk_frame_new (_("Region"));
	gtk_widget_show (frame);
	gtk_container_add (GTK_CONTAINER (vpaned), frame);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 3);
	gtk_widget_set_sensitive (frame, ce->have_tuners);

	{
	  GtkWidget *hbox;
	  GtkWidget *scrolledwindow;
	  
	  hbox = gtk_hbox_new (FALSE, 0);
	  gtk_widget_show (hbox);
	  gtk_container_add (GTK_CONTAINER (frame), hbox);
	
	  {
	    GtkWidget *vbox;
	    GtkWidget *label;
	    
	    vbox = gtk_vbox_new (FALSE, 3);
	    gtk_widget_show (vbox);
	    gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
	    
	    ce->country_menu = GTK_OPTION_MENU (create_country_menu (ce));
	    gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (ce->country_menu),
				FALSE, FALSE, 0);
	    CONNECT (country_menu, changed);
	    
	    {
	      GtkWidget *hbox;
	      
	      hbox = gtk_hbox_new (TRUE, 3);
	      gtk_widget_show (hbox);
	      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	      
	      ce->channel_search = GTK_BUTTON (gtk_button_new_with_mnemonic
					       (_("Automatic station _search")));
	      gtk_widget_show (GTK_WIDGET (ce->channel_search));
	      gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (ce->channel_search),
				  TRUE, TRUE, 0);
	      CONNECT (channel_search, clicked);
	      gtk_tooltips_set_tip (ce->tooltips, GTK_WIDGET (ce->channel_search),
				    _("Select a suitable frequency table, then "
				      "click here to search through all channels "
				      "in the table and add the received stations "
				      "to the channel list."), NULL);
	      
	      ce->add_all_channels = GTK_BUTTON (gtk_button_new_with_mnemonic
						 (_("Add all _channels")));
	      gtk_widget_show (GTK_WIDGET (ce->add_all_channels));
	      gtk_box_pack_start (GTK_BOX (hbox),	GTK_WIDGET (ce->add_all_channels),
				  TRUE, TRUE, 0);
	      CONNECT (add_all_channels, clicked);
	      gtk_tooltips_set_tip (ce->tooltips,
				    GTK_WIDGET (ce->add_all_channels),
				    _("Add all channels in the frequency "
				      "table to the channel list."), NULL);
	    }
	    
	    label = gtk_label_new (_("When your country or the required table "
				     "is not present please notify the author,\n"
				     "adding the table if possible, for inclusion "
				     "in the next release of this program."));
	    gtk_widget_show (label);
	    gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
	  }
	  
	  scrolledwindow = create_freq_treeview (ce);
	  gtk_box_pack_start (GTK_BOX (hbox), scrolledwindow, TRUE, TRUE, 0);
	}
      }
      
      {
	GtkWidget *frame;
	
	frame = gtk_frame_new (_("Channel List"));
	gtk_widget_show (frame);
	gtk_container_add (GTK_CONTAINER (vpaned), frame);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 3);
	
	{
	  GtkWidget *hbox;
	  GtkWidget *scrolledwindow;
	  
	  hbox = gtk_hbox_new (FALSE, 3);
	  gtk_widget_show (hbox);
	  gtk_container_add (GTK_CONTAINER (frame), hbox);
	  
	  scrolledwindow = create_channel_treeview (ce);
	  gtk_box_pack_start (GTK_BOX (hbox), scrolledwindow, TRUE, TRUE, 0);
	  
	  {
	    GtkWidget *vbox;
	    
	    vbox = gtk_vbox_new (FALSE, 3);
	    gtk_widget_show (vbox);
	    gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
	    
	    BUTTON (channel_up,	  GTK_STOCK_GO_UP,   FALSE);
	    BUTTON (channel_down,	  GTK_STOCK_GO_DOWN, FALSE);
	    BUTTON (channel_add,	  GTK_STOCK_ADD,     TRUE);
	    BUTTON (channel_remove, GTK_STOCK_REMOVE,  FALSE);
	  }
	}
      }
    }

    {
      GtkWidget *frame;

      frame = gtk_frame_new (_("Edit channel"));
      gtk_widget_show (frame);
      gtk_box_pack_start (ce->vbox, frame, FALSE, FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (frame), 3);

      {
	GtkWidget *label;
	GtkWidget *menu;
	  
	ce->entry_table = GTK_TABLE (gtk_table_new (5, 2, FALSE));
	gtk_widget_show (GTK_WIDGET (ce->entry_table));
	gtk_widget_set_sensitive (GTK_WIDGET (ce->entry_table), FALSE);
	gtk_container_add (GTK_CONTAINER (frame), GTK_WIDGET (ce->entry_table));
	gtk_table_set_row_spacings (ce->entry_table, 3);
	gtk_table_set_col_spacings (ce->entry_table, 3);

	LABEL ("Name:", 0, 0);
  
	ce->entry_name = GTK_ENTRY (gtk_entry_new ());
	gtk_widget_show (GTK_WIDGET (ce->entry_name));
	CONNECT (entry_name, changed);
	gtk_table_attach (ce->entry_table, GTK_WIDGET (ce->entry_name), 1, 2, 0, 1,
			  (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
			  (GtkAttachOptions)(0), 0, 0);

	LABEL ("Input:", 0, 1);

	ce->entry_input = GTK_OPTION_MENU (gtk_option_menu_new ());
	gtk_widget_show (GTK_WIDGET (ce->entry_input));
	CONNECT (entry_input, changed);
	gtk_table_attach (ce->entry_table, GTK_WIDGET (ce->entry_input), 1, 2, 1, 2,
			  (GtkAttachOptions)(GTK_FILL),
			  (GtkAttachOptions)(0), 0, 0);
	gtk_option_menu_set_menu (ce->entry_input,
				  GTK_WIDGET (create_input_menu (ce)));

	LABEL ("Fine tuning:", 0, 2);

	ce->spinslider_adj = GTK_ADJUSTMENT
	  (gtk_adjustment_new (0, 0, 0, 0, 0, 0));

	ce->entry_fine_tuning = z_spinslider_new (ce->spinslider_adj, NULL, _("MHz"), 0, 2);
	gtk_widget_show (ce->entry_fine_tuning);
	gtk_widget_set_sensitive (ce->entry_fine_tuning, FALSE);
	gtk_table_attach (ce->entry_table, ce->entry_fine_tuning, 1, 2, 2, 3,
			  (GtkAttachOptions)(GTK_FILL),
			  (GtkAttachOptions)(GTK_FILL), 0, 0);
	g_signal_connect (G_OBJECT (ce->spinslider_adj), "value-changed",
			  G_CALLBACK (on_entry_fine_tuning_value_changed), ce);

	LABEL ("Video standard:", 0, 3);

	ce->entry_standard = GTK_OPTION_MENU (gtk_option_menu_new ());
	gtk_widget_show (GTK_WIDGET (ce->entry_standard));
	CONNECT (entry_standard, changed);
	gtk_table_attach (ce->entry_table, GTK_WIDGET (ce->entry_standard), 1, 2, 3, 4,
			  (GtkAttachOptions)(GTK_FILL),
			  (GtkAttachOptions)(0), 0, 0);
	menu = gtk_menu_new ();
	gtk_option_menu_set_menu (ce->entry_standard, menu);

	LABEL ("Keyboard shortcut:", 0, 4);

	ce->entry_accel = z_key_entry_new ();
	gtk_widget_show (ce->entry_accel);
	gtk_table_attach (ce->entry_table, ce->entry_accel, 1, 2, 4, 5,
			  (GtkAttachOptions)(GTK_FILL),
			  (GtkAttachOptions)(GTK_FILL), 0, 0);
	g_signal_connect (G_OBJECT (z_key_entry_entry (ce->entry_accel)),
			  "changed", G_CALLBACK (on_entry_accel_changed), ce);
      }
    }
  }
 
  {
    GtkWidget *dialog_action_area;

    dialog_action_area = GTK_DIALOG (ce->channel_editor)->action_area;
    gtk_widget_show (dialog_action_area);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

    {
      GtkWidget *hbox;
      GtkWidget *ok;
      GtkWidget *cancel;
  
      hbox = gtk_hbox_new (TRUE, 15);
      gtk_widget_show (hbox);
      gtk_container_add (GTK_CONTAINER (dialog_action_area), hbox);
      
      cancel = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
      gtk_widget_show (cancel);
      gtk_box_pack_start (GTK_BOX (hbox), cancel, FALSE, TRUE, 0);
      g_signal_connect (G_OBJECT (cancel), "clicked",
			G_CALLBACK (on_cancel_clicked), ce);

      ok = gtk_button_new_from_stock (GTK_STOCK_OK);
      gtk_widget_show (ok);
      gtk_box_pack_start (GTK_BOX (hbox), ok, FALSE, TRUE, 0);
      g_signal_connect (G_OBJECT (ok), "clicked",
			G_CALLBACK (on_ok_clicked), ce);
    }
  }

  return GTK_WIDGET (ce->channel_editor);
}

static PyObject *
py_channel_editor		(PyObject *		self,
				 PyObject *		args)
{
  if (!dialog_running)
    gtk_widget_show (create_channel_editor ());

  py_return_true;
}

void
startup_channel_editor		(void)
{
  cmd_register ("channel_editor", py_channel_editor, METH_VARARGS,
		_("Opens the channel editor"), "zapping.channel_editor()");
}

void
shutdown_channel_editor		(void)
{
}
