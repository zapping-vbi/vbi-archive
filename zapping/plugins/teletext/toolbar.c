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

/* $Id: toolbar.c,v 1.4 2007-08-30 14:14:33 mschimek Exp $ */

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixdata.h>

#include "src/zmisc.h"
#include "src/remote.h"

#include "pixmaps/left.h"
#include "pixmaps/down.h"
#include "pixmaps/up.h"
#include "pixmaps/right.h"
#include "pixmaps/reveal.h"

#include "toolbar.h"

void
teletext_toolbar_set_url	(TeletextToolbar *	toolbar,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno)
{
  gchar buffer[16];

  if ((guint) subno > 0x99)
    subno = 0; /* 0, VBI3_ANY_SUBNO, 0x2359, bug */

  snprintf (buffer, 16, "%3x.%02x", pgno & 0xFFF, subno);

  gtk_label_set_text (toolbar->url, buffer);
}

static void
on_hold_toggled			(GtkToggleButton *	button,
				 gpointer		user_data _unused_)
{
  python_command_printf (GTK_WIDGET (button),
			 "zapping.ttx_hold(%u)",
			 gtk_toggle_button_get_active (button));
}

static void
on_reveal_toggled		(GtkToggleToolButton *	button,
				 gpointer		user_data _unused_)
{
  python_command_printf (GTK_WIDGET (button),
			 "zapping.ttx_reveal(%u)",
			 gtk_toggle_tool_button_get_active (button));
}

static GtkWidget *
button_new_from_pixdata		(const GdkPixdata *	pixdata,
				 const gchar *		tooltip,
				 GtkReliefStyle		relief_style,
				 const gchar *		py_cmd)
{
  GtkWidget *button;
  GtkWidget *icon;

  icon = z_gtk_image_new_from_pixdata (pixdata);
  gtk_widget_show (icon);

  button = gtk_button_new ();
  gtk_widget_show (button);

  gtk_container_add (GTK_CONTAINER (button), icon);
  gtk_button_set_relief (GTK_BUTTON (button), relief_style);
  z_tooltip_set (button, tooltip);

  z_signal_connect_python (G_OBJECT (button), "clicked", py_cmd);

  return button;
}

/* XXX How can we do this with GtkActions? */

static void
on_orientation_changed		(GtkToolbar *		toolbar,
				 GtkOrientation		orientation,
				 TeletextToolbar *	t)
{
  GtkReliefStyle button_relief;
  GList *glist;
  GtkWidget *up;
  GtkWidget *down;
  GtkWidget *left;
  GtkWidget *right;

  while ((glist = t->box1->children))
    gtk_container_remove (GTK_CONTAINER (t->box1),
			  ((GtkBoxChild *) glist->data)->widget);

  while ((glist = t->box2->children))
    gtk_container_remove (GTK_CONTAINER (t->box2),
			  ((GtkBoxChild *) glist->data)->widget);

  button_relief = GTK_RELIEF_NORMAL;
  gtk_widget_style_get (GTK_WIDGET (toolbar), "button_relief",
			&button_relief, NULL);

  up = button_new_from_pixdata
    (&up_png, _("Next page"),
     button_relief, "zapping.ttx_page_incr(1)");
  down = button_new_from_pixdata
    (&down_png, _("Previous page"),
     button_relief, "zapping.ttx_page_incr(-1)");
  left = button_new_from_pixdata
    (&left_png, _("Previous subpage"),
     button_relief, "zapping.ttx_subpage_incr(-1)");
  right = button_new_from_pixdata
    (&right_png, _("Next subpage"),
     button_relief, "zapping.ttx_subpage_incr(1)");

  switch (orientation)
    {
    case GTK_ORIENTATION_HORIZONTAL:
      SWAP (up, left);

      /* fall through */

    case GTK_ORIENTATION_VERTICAL:
      gtk_box_pack_start (t->box1, up,    FALSE, FALSE, 0);
      gtk_box_pack_start (t->box1, down,  FALSE, FALSE, 0);
      gtk_box_pack_start (t->box2, left,  FALSE, FALSE, 0);
      gtk_box_pack_start (t->box2, right, FALSE, FALSE, 0);

      break;
    }
}

static void
instance_init			(GTypeInstance *	instance _unused_,
				 gpointer		g_class _unused_)
{
  /*  TeletextToolbar *toolbar = (TeletextToolbar *) instance; */
}

/* ugh that should take a TeletextView* */
GtkWidget *
teletext_toolbar_new		(GtkActionGroup *action_group)
{
  TeletextToolbar *toolbar;
  GtkWidget *widget;
  GtkReliefStyle button_relief;
  GtkToolItem *tool_item;

  toolbar = TELETEXT_TOOLBAR (g_object_new (TYPE_TELETEXT_TOOLBAR, NULL));

  widget = GTK_WIDGET (toolbar);
  button_relief = GTK_RELIEF_NORMAL;
  gtk_widget_ensure_style (widget);
  gtk_widget_style_get (widget, "button_relief", &button_relief, NULL);

  widget = gtk_action_create_tool_item
    (gtk_action_group_get_action (action_group, "HistoryBack"));
  gtk_toolbar_insert (&toolbar->toolbar, GTK_TOOL_ITEM (widget), APPEND);

  widget = gtk_action_create_tool_item
    (gtk_action_group_get_action (action_group, "HistoryForward"));
  gtk_toolbar_insert (&toolbar->toolbar, GTK_TOOL_ITEM (widget), APPEND);

  widget = gtk_action_create_tool_item
    (gtk_action_group_get_action (action_group, "Home"));
  gtk_toolbar_insert (&toolbar->toolbar, GTK_TOOL_ITEM (widget), APPEND);

  /* XXX should use NewTeletextView action. */
  tool_item = gtk_tool_button_new_from_stock (GTK_STOCK_NEW);
  z_tooltip_set (GTK_WIDGET (tool_item), _("Open new Teletext window"));
  z_signal_connect_python (tool_item, "clicked",
			   "zapping.ttx_open_new()");
  gtk_toolbar_insert (&toolbar->toolbar, tool_item, APPEND);

  widget = gtk_action_create_tool_item
    (gtk_action_group_get_action (action_group, "Search"));
  gtk_toolbar_insert (&toolbar->toolbar, GTK_TOOL_ITEM (widget), APPEND);

  widget = gtk_hbox_new (FALSE, 0);
  tool_item = gtk_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (tool_item), widget);
  gtk_widget_show_all (GTK_WIDGET (tool_item));
  toolbar->box1 = GTK_BOX (widget);
  gtk_toolbar_insert (&toolbar->toolbar, tool_item, APPEND);

  {
    GtkWidget *frame;

    widget = gtk_toggle_button_new ();
    tool_item = gtk_tool_item_new ();
    gtk_container_add (GTK_CONTAINER (tool_item), widget);
    z_tooltip_set (GTK_WIDGET (tool_item), _("Hold the current subpage"));
    gtk_widget_show_all (GTK_WIDGET (tool_item));
    toolbar->hold = GTK_TOGGLE_BUTTON (widget);
    gtk_button_set_relief (GTK_BUTTON (widget), button_relief);
    gtk_toolbar_insert (&toolbar->toolbar, tool_item, APPEND);

    g_signal_connect (G_OBJECT (widget), "clicked",
		      G_CALLBACK (on_hold_toggled), toolbar);

    frame = gtk_frame_new (NULL);
    gtk_widget_show (frame);
    gtk_container_add (GTK_CONTAINER (widget), frame);

    widget = gtk_label_new ("888.88");
    gtk_widget_show (widget);
    toolbar->url = GTK_LABEL (widget);
    gtk_container_add (GTK_CONTAINER (frame), widget);
  }

  widget = gtk_hbox_new (FALSE, 0);
  tool_item = gtk_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (tool_item), widget);
  gtk_widget_show_all (GTK_WIDGET (tool_item));
  toolbar->box2 = GTK_BOX (widget);
  gtk_toolbar_insert (&toolbar->toolbar, tool_item, APPEND);

  widget = z_gtk_image_new_from_pixdata (&reveal_png);
  tool_item = gtk_toggle_tool_button_new ();
  toolbar->reveal = GTK_TOGGLE_TOOL_BUTTON (tool_item);
  gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (tool_item), widget);
  gtk_tool_button_set_label (GTK_TOOL_BUTTON (tool_item), _("Reveal"));
  z_tooltip_set (GTK_WIDGET (tool_item), _("Reveal concealed text"));
  gtk_widget_show_all (GTK_WIDGET (tool_item));
  gtk_toggle_tool_button_set_active (toolbar->reveal, FALSE);
  gtk_toolbar_insert (&toolbar->toolbar, tool_item, APPEND);
  g_signal_connect (tool_item, "toggled",
		    G_CALLBACK (on_reveal_toggled), toolbar);

  g_signal_connect (G_OBJECT (&toolbar->toolbar), "orientation-changed",
		    G_CALLBACK (on_orientation_changed), toolbar);

  on_orientation_changed (&toolbar->toolbar,
			  gtk_toolbar_get_orientation (&toolbar->toolbar),
			  toolbar);

  return GTK_WIDGET (toolbar);
}

GType
teletext_toolbar_get_type		(void)
{
  static GType type = 0;
  
  if (!type)
    {
      GTypeInfo info;

      CLEAR (info);

      info.class_size = sizeof (TeletextToolbarClass);
      info.instance_size = sizeof (TeletextToolbar);
      info.instance_init = instance_init;

      type = g_type_register_static (GTK_TYPE_TOOLBAR,
				     "TeletextToolbar",
				     &info, (GTypeFlags) 0);
    }

  return type;
}

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
