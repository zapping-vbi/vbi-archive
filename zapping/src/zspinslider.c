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

/* $Id: zspinslider.c,v 1.1 2004-12-07 17:31:08 mschimek Exp $ */

/*
   SpinSlider widget:

   [12345___|<>] unit [--||--------][Reset]
*/

#include <math.h>
#include "zmisc.h"
#include "zspinslider.h"

static GObjectClass *		parent_class;

gfloat
z_spinslider_get_value		(ZSpinSlider *		sp)
{
  g_return_val_if_fail (Z_IS_SPINSLIDER (sp), 0.0);

  return sp->spin_adj->value;
}

void
z_spinslider_set_value		(ZSpinSlider *		sp,
				 gfloat			value)
{
  g_return_if_fail (Z_IS_SPINSLIDER (sp));

  gtk_adjustment_set_value (sp->spin_adj, value);
  gtk_adjustment_set_value (sp->hscale_adj, value);
}

gint
z_spinslider_get_int_value	(ZSpinSlider *		sp)
{
  gfloat value;

  g_return_val_if_fail (Z_IS_SPINSLIDER (sp), 0);

  value = sp->spin_adj->value;

  if (value < G_MININT || value > G_MAXINT)
    g_warning ("ZSpinSlider value %f not representable as gint.",
	       (double) value);

  return (gint) value;
}

void
z_spinslider_set_int_value	(ZSpinSlider *		sp,
				 gint			value)
{
  g_return_if_fail (Z_IS_SPINSLIDER (sp));

  gtk_adjustment_set_value (sp->spin_adj, value);
  gtk_adjustment_set_value (sp->hscale_adj, value);
}

void
z_spinslider_set_reset_value	(ZSpinSlider *		sp,
				 gfloat			value)
{
  g_return_if_fail (Z_IS_SPINSLIDER (sp));

  sp->history[sp->reset_state] = value;

  gtk_adjustment_set_value (sp->spin_adj, value);
  gtk_adjustment_set_value (sp->hscale_adj, value);
}

void
z_spinslider_adjustment_changed	(ZSpinSlider *		sp)
{
  g_return_if_fail (Z_IS_SPINSLIDER (sp));

  sp->hscale_adj->value = sp->spin_adj->value;
  sp->hscale_adj->lower = sp->spin_adj->lower;
  sp->hscale_adj->upper = sp->spin_adj->upper + sp->spin_adj->page_size;
  sp->hscale_adj->step_increment = sp->spin_adj->step_increment;
  sp->hscale_adj->page_increment = sp->spin_adj->page_increment;
  sp->hscale_adj->page_size = sp->spin_adj->page_size;

  gtk_adjustment_changed (sp->spin_adj);
  gtk_adjustment_changed (sp->hscale_adj);
}

static void
on_hscale_changed		(GtkWidget *		widget _unused_,
				 ZSpinSlider *		sp)
{
  if (sp->spin_adj->value != sp->hscale_adj->value)
    gtk_adjustment_set_value (sp->spin_adj, sp->hscale_adj->value);
}

static void
on_spinbutton_changed		(GtkWidget *		widget _unused_,
				 ZSpinSlider *		sp)
{
  if (!sp->in_reset)
    {
      if (sp->reset_state != 0)
	{
	  sp->history[0] = sp->history[1];
	  sp->history[1] = sp->history[2];
	  sp->reset_state--;
	}

      sp->history[2] = sp->spin_adj->value;
    }

  if (sp->spin_adj->value != sp->hscale_adj->value)
    gtk_adjustment_set_value (sp->hscale_adj, sp->spin_adj->value);
}

static void
on_reset			(GtkWidget *		widget _unused_,
				 ZSpinSlider *		sp)
{
  gfloat current_value;

  current_value = sp->history[2];
  sp->history[2] = sp->history[1];
  sp->history[1] = sp->history[0];
  sp->history[0] = current_value;

  sp->in_reset = TRUE;

  gtk_adjustment_set_value (sp->spin_adj, sp->history[2]);
  gtk_adjustment_set_value (sp->hscale_adj, sp->history[2]);

  sp->in_reset = FALSE;

  if (sp->reset_state == 0
      && fabs (sp->history[0] - sp->history[1]) < 1e-6)
    sp->reset_state = 2;
  else
    sp->reset_state = (sp->reset_state + 1) % 3;
}

static void
instance_finalize		(GObject *		object)
{
  /* ZSpinSlider *sp = Z_SPINSLIDER (object); */

  parent_class->finalize (object);
}

static void
instance_init			(GTypeInstance *	instance _unused_,
				 gpointer		g_class _unused_)
{
  /* ZSpinSlider *sp = (ZSpinSlider *) instance; */
}

#include "pixmaps/reset.h"

GtkWidget *
z_spinslider_new		(GtkAdjustment *	spin_adj,
				 GtkAdjustment *	hscale_adj,
				 const gchar *		unit,
				 gfloat			reset,
				 gint			digits)
{
  ZSpinSlider *sp;
  GtkBox *box;
  GtkWidget *widget;

  g_return_val_if_fail (GTK_IS_ADJUSTMENT (spin_adj), NULL);

  sp = Z_SPINSLIDER (g_object_new (Z_TYPE_SPINSLIDER, NULL));

  sp->spin_adj = spin_adj;
  sp->hscale_adj = hscale_adj;

  if (0)
    fprintf (stderr, "zss_new %f %f...%f  %f %f  %f  %d\n",
	     spin_adj->value, spin_adj->lower, spin_adj->upper,
	     spin_adj->step_increment, spin_adj->page_increment,
	     spin_adj->page_size, digits);

  box = GTK_BOX (&sp->hbox);

  /* Spin button */

  {
    GtkSpinButton *spin_button;

    widget = gtk_spin_button_new (spin_adj,
				  spin_adj->step_increment,
				  (guint) digits);
    gtk_widget_show (widget);

    /* I don't see how to set "as much as needed", so hacking this up */
    gtk_widget_set_size_request (widget, 80, -1);

    spin_button = GTK_SPIN_BUTTON (widget);
    gtk_spin_button_set_update_policy (spin_button, GTK_UPDATE_IF_VALID);
    gtk_spin_button_set_numeric (spin_button, TRUE);
    gtk_spin_button_set_wrap (spin_button, TRUE);
    gtk_spin_button_set_snap_to_ticks (spin_button, TRUE);

    gtk_box_pack_start (box, widget,
			/* expand */ FALSE,
			/* fill */ FALSE,
			/* padding */ 0);

    g_signal_connect (G_OBJECT (spin_adj), "value-changed",
		      G_CALLBACK (on_spinbutton_changed), sp);
  }

  /* Unit name */

  if (unit)
    {
      GtkWidget *label;

      label = gtk_label_new (unit);
      gtk_widget_show (label);

      gtk_box_pack_start (box, label, FALSE, FALSE, 3);
    }

  /* Slider */

  if (!hscale_adj)
    {
      GtkObject *adj;

      /* Necessary to reach spin_adj->upper with slider. */

      adj = gtk_adjustment_new (spin_adj->value,
				spin_adj->lower,
				spin_adj->upper + spin_adj->page_size,
				spin_adj->step_increment,
				spin_adj->page_increment,
				spin_adj->page_size);

      hscale_adj = GTK_ADJUSTMENT (adj);
      sp->hscale_adj = hscale_adj;
    }

  {
    GtkScale *scale;

    widget = gtk_hscale_new (hscale_adj);
    gtk_widget_show (widget);

    /* Another hack */
    gtk_widget_set_size_request (widget, 80, -1);

    scale = GTK_SCALE (widget);
    gtk_scale_set_draw_value (scale, FALSE);
    gtk_scale_set_digits (scale, -digits);

    gtk_box_pack_start (box, widget,
			/* expand */ TRUE,
			/* fill */ TRUE,
			/* padding */ 3);

    g_signal_connect (G_OBJECT (hscale_adj), "value-changed",
		      G_CALLBACK (on_hscale_changed), sp);
  }

  /* Reset button */

  {
    static GdkPixbuf *pixbuf = NULL;
    GtkWidget *button;
    GtkWidget *image;

    sp->history[0] = reset;
    sp->history[1] = reset;
    sp->history[2] = reset;
    sp->reset_state = 0;
    sp->in_reset = FALSE;

    if (!pixbuf)
      pixbuf = gdk_pixbuf_from_pixdata (&reset_png, FALSE, NULL);

    if (pixbuf && (image = gtk_image_new_from_pixbuf (pixbuf)))
      {
	gtk_widget_show (image);
	button = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER (button), image);
	z_tooltip_set (button, _("Reset"));
      }
    else
      {
	button = gtk_button_new_with_label (_("Reset"));
      }

    gtk_widget_show (button);

    gtk_box_pack_start (box, button,
			/* expand */ FALSE,
			/* fill */ FALSE,
			/* padding */ 0);

    g_signal_connect (G_OBJECT (button), "pressed",
		      G_CALLBACK (on_reset), sp);
  }

  return GTK_WIDGET (sp);
}

static void
class_init			(gpointer		g_class,
				 gpointer		class_data _unused_)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (g_class);
  parent_class = g_type_class_peek_parent (g_class);

  object_class->finalize = instance_finalize;
}

GType
z_spinslider_get_type		(void)
{
  static GType type = 0;
  
  if (!type)
    {
      GTypeInfo info;

      CLEAR (info);

      info.class_size = sizeof (ZSpinSliderClass);
      info.class_init = class_init;
      info.instance_size = sizeof (ZSpinSlider);
      info.instance_init = instance_init;

      type = g_type_register_static (GTK_TYPE_HBOX,
				     "ZSpinSlider",
				     &info, (GTypeFlags) 0);
    }

  return type;
}
