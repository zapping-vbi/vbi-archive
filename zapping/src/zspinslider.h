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

/* $Id: zspinslider.h,v 1.2 2007-08-30 14:14:37 mschimek Exp $ */

#ifndef Z_SPINSLIDER_H
#define Z_SPINSLIDER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define Z_TYPE_SPINSLIDER (z_spinslider_get_type ())
#define Z_SPINSLIDER(obj)						\
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), Z_TYPE_SPINSLIDER, ZSpinSlider))
#define Z_SPINSLIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass),	\
  Z_TYPE_SPINSLIDER, ZSpinSliderClass))
#define Z_IS_SPINSLIDER(obj)						\
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), Z_TYPE_SPINSLIDER))
#define Z_IS_SPINSLIDER_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_TYPE ((klass), Z_TYPE_SPINSLIDER))
#define Z_SPINSLIDER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  Z_TYPE_SPINSLIDER, ZSpinSliderClass))

typedef struct _ZSpinSlider ZSpinSlider;
typedef struct _ZSpinSliderClass ZSpinSliderClass;

struct _ZSpinSlider
{
  GtkHBox		hbox;

  GtkAdjustment *	spin_adj;
  GtkAdjustment *	hscale_adj;

  gfloat		history[3];
  guint			reset_state;
  gboolean		in_reset;
};

struct _ZSpinSliderClass
{
  GtkHBoxClass		parent_class;
};

extern GType
z_spinslider_get_type		(void) G_GNUC_CONST;
GtkWidget *
z_spinslider_new		(GtkAdjustment *	spin_adj,
				 GtkAdjustment *	hscale_adj,
				 const gchar *		unit,
				 gfloat			reset_value,
				 gint			digits);
gfloat
z_spinslider_get_value		(ZSpinSlider *		sp);
void
z_spinslider_set_value		(ZSpinSlider *		sp,
				 gfloat			value);
gint
z_spinslider_get_int_value	(ZSpinSlider *		sp);
void
z_spinslider_set_int_value	(ZSpinSlider *		sp,
				 gint			value);
void
z_spinslider_set_reset_value	(ZSpinSlider *		sp,
				 gfloat			value);
void
z_spinslider_adjustment_changed	(ZSpinSlider *		sp);

G_END_DECLS

#endif /* Z_SPINSLIDER_H */

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
