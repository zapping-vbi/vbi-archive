/* Zapping (TV viewer for the Gnome Desktop)
 * Video Widget
 * Copyright (C) 2003 Michael H. Schimek
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

/* $Id: zvideo.c,v 1.1.2.5 2003-10-07 18:38:15 mschimek Exp $ */

#include "site_def.h"

#ifndef ZVIDEO_EXPOSE_TEST
#define ZVIDEO_EXPOSE_TEST 0
#endif

#include "zmisc.h"
#include "zvideo.h"
#include "zmarshalers.h"

#define MAX_SIZE 16384

enum {
  CURSOR_BLANKED,
  NUM_SIGNALS
};

static GtkDrawingAreaClass *	parent_class = NULL;
static guint			signals[NUM_SIGNALS];

/* Cursor blanking routines */

static void
cursor_blanked			(ZVideo *		video)
{
  gdk_window_set_cursor (GTK_WIDGET (video)->window,
			 Z_VIDEO_GET_CLASS (video)->blank_cursor);
}

static gboolean
blank_cursor_timeout		(gpointer		p)
{
  ZVideo *video = (ZVideo *) p;

  g_assert (video->blank_cursor_timeout > 0);

  g_signal_emit (video, signals[CURSOR_BLANKED], 0);

  video->blank_cursor_timeout_handle = 0;

  return FALSE; /* don't call again */
}

/**
 * z_video_set_cursor:
 * @video: a #ZVideo object
 * @cursor_type::
 *
 * Changes the cursor appearance. When pointer hiding is enabled,
 * makes the cursor visible and resets the blank timer.
 */
gboolean
z_video_set_cursor		(ZVideo *		video,
				 GdkCursorType		cursor_type)
{
  GdkCursor *cursor;

  cursor_type = MIN (cursor_type, GDK_LAST_CURSOR - 2) & ~1;

  if ((cursor = gdk_cursor_new (cursor_type)))
    {
      gdk_window_set_cursor (GTK_WIDGET (video)->window, cursor);
      gdk_cursor_unref (cursor);

      video->blanked_cursor_type = cursor_type;

      if (video->blank_cursor_timeout_handle != 0)
	gtk_timeout_remove (video->blank_cursor_timeout_handle);

      if (video->blank_cursor_timeout > 0)
	video->blank_cursor_timeout_handle =
	  gtk_timeout_add (video->blank_cursor_timeout,
			   blank_cursor_timeout, video);
      else
	video->blank_cursor_timeout_handle = 0;

      return TRUE;
    }

  return FALSE;
}

/**
 * z_video_blank_cursor:
 * @video: a #ZVideo object
 * @timeout: time in milliseconds after which the pointer should
 *   disappear. 0 to disable pointer hiding.
 *
 * Enable or disable pointer hiding.
 */
void
z_video_blank_cursor		(ZVideo *		video,
				 guint			timeout)
{
  g_return_if_fail (Z_IS_VIDEO (video));

  if (video->blank_cursor_timeout == timeout)
    return;

  video->blank_cursor_timeout = timeout;

  if (timeout > 0)
    video->blank_cursor_timeout_handle =
      gtk_timeout_add (video->blank_cursor_timeout,
		       blank_cursor_timeout, video);
  else
    z_video_set_cursor (video, video->blanked_cursor_type);
}

#undef FLOOR
#define FLOOR(val, base) ({						\
  guint _val = val;							\
  _val -= _val % (base);						\
})

#undef CEIL
#define CEIL(val, base) FLOOR((val) + (base) - 1, base)

static __inline__ GtkWidget *
get_toplevel_window		(ZVideo *		video)
{
  GtkWidget *toplevel;

  if ((toplevel = gtk_widget_get_toplevel (GTK_WIDGET (video))))
    if (!GTK_WIDGET_TOPLEVEL (toplevel)
	|| !GTK_IS_WINDOW (toplevel))
      return NULL;

  return toplevel;
}

static void
size_allocate			(GtkWidget *		widget,
				 GtkAllocation *	allocation)
{
  ZVideo *video = (ZVideo *) widget;
  GtkWidget *toplevel;
  GtkAllocation darea_alloc;

  g_return_if_fail (Z_IS_VIDEO (widget));
  g_return_if_fail (allocation != NULL);

  /* FIXME: fails after 
     gdk_window_resize(main_window->window, 0, 0);
     How's that possible despite proper minimum
     size_requisition and geometry hints? */
  //  g_assert (allocation->width >= video->min_width);
  //  g_assert (allocation->height >= video->min_height);

  toplevel = get_toplevel_window (video);

  /* Our objective is to automatically resize the drawing area as the
     user resizes the window, and to keep the size fixed otherwise.

     The problem is gtk has no concept of a fixed size widget. A
     size_request() determines the desired (minimum) size of the
     widget, and in turn of the toplevel window. When other widgets
     such as a toolbar are dynamically added gtk expands the window
     if the required size exceeds toplevel window->requisition.
     Removing shrinks the window (by setting window manager geometry
     hint max size to requisition) provided resizing is disabled for
     this window.

     Obviously we can't use these mechanisms. GTK_WIDGET (video)->
     requisition is our minimum size as usual and we compute geometry
     hints ourselves. The lines below check for the adding/removing
     case and resize the window instead of adjusting the widget. */

  if (toplevel
      && video->window_alloc.width == toplevel->allocation.width
      && video->window_alloc.height == toplevel->allocation.height
      && (allocation->width != widget->allocation.width
	  || allocation->height != widget->allocation.height))
    {
      gdk_window_resize (toplevel->window,
                         toplevel->allocation.width - allocation->width
			 + widget->allocation.width,
			 toplevel->allocation.height - allocation->height
			 + widget->allocation.height);
      return;
    }

  /* The drawing area size shall be constrained by minimum size,
     maximum size, size increment and aspect ratio parameters. We cannot
     expect gtk to allocate a suitable size, so here we compute the
     largest possible size and center the drawing area within the
     allocated space. Roughly the effect GtkAlignment & GtkAspectFrame &
     GtkDrawingArea would give.

     Actually I'm afraid this is the only feature we can count on. The
     code tinkering with the toplevel window probably works only by
     chance.

     Of course the padding space is not painted. Set the background
     attribute or whatever, of the parent widget if you care. */

  darea_alloc = *allocation;

  if (video->ratio > 0.0)
    {
      if (video->ratio * allocation->height > allocation->width)
	darea_alloc.height = allocation->width / video->ratio + 0.5;
      else
	darea_alloc.width = video->ratio * allocation->height + 0.5;
    }

  darea_alloc.width = FLOOR (darea_alloc.width, video->mod_x);
  darea_alloc.height = FLOOR (darea_alloc.height, video->mod_y);

  darea_alloc.width =
    CLAMP (darea_alloc.width, video->min_width, video->max_width);
  darea_alloc.height =
    CLAMP (darea_alloc.height, video->min_height, video->max_height);

  darea_alloc.x = allocation->x
    + ((allocation->width - darea_alloc.width) >> 1);
  darea_alloc.y = allocation->y
    + ((allocation->height - darea_alloc.height) >> 1);

  GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, &darea_alloc);

  /* Supposedly geometry hints tell the WM about our constraints,
     such that the user can only resize to a suitable size in the
     first place, avoiding ugly padding. There are a number of
     problems though.

     According to the ICCCM the hints.base*, hints.*aspect and
     hints.*inc define a preferred window size of:
     window_w = base_w + i * w_inc, window_h = base_h + j * h_inc
     with i and j >= 0 such that
     min_aspect <= (i * w_inc) / (j * h_inc) <= max_aspect and
     min_w <= window_w <= max_w, min_h <= window_h <= max_h.

     The intention seems clear, but I still couldn't find a single
     WM working this way. Even xf86's own twm and gdk follow the
     bad practice of min_aspect <= window_w / window_h <= max_aspect,
     and limits are inconsistently applied.

     We can work around the wrong limits, and when video->ratio == 0.0
     or base_width == base_height == 0 the geometry hints should work
     just fine. Adjusting the aspect while or after the user resizes is
     not permitted by the ICCCM and would indeed require plenty of WM
     specific work-arounds, so I opted against it. Alternatively we can
     ignore video->ratio or accept a wrong aspect value including or
     excluding base or something in between. This will nearly always
     cause padding, so I guess not forcing a window ratio is still the
     most user friendly option.

     Actually I believe the WM should just call with a pointer position
     and let the client calculate a suitable size. Then we could also
     snap to the capture sizes really supported by the hardware. Sigh.
  */

  if (toplevel)
    {
      GdkWindowHints hints_mask;
      GdkGeometry hints;
      GtkRequisition min;
      GtkRequisition curr;
      guint changed;

      hints.base_width = toplevel->allocation.width - allocation->width;
      hints.base_height = toplevel->allocation.height - allocation->height;

      changed  = hints.base_width ^ video->old_hints.base_width;
      changed |= hints.base_height ^ video->old_hints.base_height;

      curr = widget->requisition;
      widget->requisition.width = video->min_width;
      widget->requisition.height = video->min_height;

      /* Probably there's a better way but I'm tired of this crap. */
      gtk_widget_size_request (toplevel, &min);

      hints.min_width = hints.base_width
	+ CEIL (min.width - hints.base_width, video->mod_x);
      hints.min_height = hints.base_height
	+ CEIL (min.height - hints.base_height, video->mod_y);

      widget->requisition = curr;

      gtk_widget_size_request (toplevel, &min);

      /* video->min* should have the correct aspect, but to fit
	 all widgets the minimum window size can be larger than
	 this, so we have to re-aspect. */

      if (video->ratio > 0.0)
	{
	  if (video->ratio * hints.min_height > hints.min_width)
	    hints.min_width = hints.min_height * video->ratio + 0.5;
	  else
	    hints.min_height = hints.min_width / video->ratio + 0.5;
	}

      changed |= hints.min_width ^ video->old_hints.min_width;
      changed |= hints.min_height ^ video->old_hints.min_height;

      hints.max_width = hints.base_width
	+ FLOOR (video->max_width, video->mod_x);
      hints.max_width = MAX (hints.max_width, hints.min_width);
      changed |= hints.max_width ^ video->old_hints.max_width;

      hints.max_height = hints.base_height
	+ FLOOR (video->max_height, video->mod_y);
      hints.max_height = MAX (hints.max_height, hints.min_height);
      changed |= hints.max_height ^ video->old_hints.max_height;

      if (changed)
	{
	  video->old_hints = hints;

	  hints_mask = 0
	    | GDK_HINT_BASE_SIZE
	    | GDK_HINT_MIN_SIZE
	    | GDK_HINT_MAX_SIZE;

	  if (video->mod_x | video->mod_y)
	    {
	      hints.width_inc = video->mod_x;
	      hints.height_inc = video->mod_y;
	      hints_mask |= GDK_HINT_RESIZE_INC;
	    }

	  /* As of GTK+ 2.2 the geometry hints are broken, requesting
	     the inverse of the desired aspect. Has been fixed. */

	  /* Sawfish 1.2 doesn't seem to support the aspect hint and
	     behaves strangely when we call gtk_window_set_geometry_-
	     hints() with aspect while the user resizes. Needs
	     further examination. */

	  if ((gtk_major_version > 2
	       || (gtk_major_version == 2
		   && (gtk_minor_version > 2)))
	      && 0
	      && video->ratio > 0.0
	      && (hints.base_width | hints.base_height) == 0)
	    {
	      hints.min_aspect = video->ratio;
	      hints.max_aspect = video->ratio;
	      hints_mask |= GDK_HINT_ASPECT;
	    }

	  if (0)
	    fprintf (stderr, "set_geometry_hints: min=%d,%d max=%d,%d "
		     "inc=%d,%d aspect=%f\n",
		     hints.min_width, hints.min_height,
		     hints.max_width, hints.max_height,
		     hints.width_inc, hints.height_inc,
		     hints.min_aspect);

	  /* I guess geometry_widget should be video, not toplevel. But
	     somehow the function doesn't initialize hints.base* as we
	     do. Well, after computing base* ourselves we had to
	     initialize minimum and maximum too, so we have to call
	     this anyway. */

	  gtk_window_set_geometry_hints (GTK_WINDOW (toplevel),
					 /* geometry_widget */ toplevel,
					 &hints, hints_mask);
	}
    }
}

static void
size_request			(GtkWidget *		widget,
				 GtkRequisition *	requisition)
{
  ZVideo *video = (ZVideo *) widget;

  GTK_WIDGET_CLASS (parent_class)->size_request (widget, requisition);

  requisition->width = CLAMP (requisition->width,
			      video->min_width, video->max_width);
  requisition->height = CLAMP (requisition->height,
			       video->min_height, video->max_height);
}

static void        
window_size_allocate		(GtkWidget *		widget,
                                 GtkAllocation *	allocation,
				 gpointer		user_data)
{
  ZVideo *video = (ZVideo *) user_data;

  video->window_alloc = *allocation;
}

/**
 * z_video_set_min_size:
 * @video: a #ZVideo object
 * @width:
 * @height:
 *
 * Set the desired minimum size of the drawing area. Width
 * and height will be rounded up to the desired size increment,
 * so you may want to set that first.
 */
void
z_video_set_min_size		(ZVideo *		video,
				 guint			width,
				 guint			height)
{
  video->min_width = CEIL (MIN (width, MAX_SIZE), video->mod_x);
  video->min_height = CEIL (MIN (height, MAX_SIZE), video->mod_y);

  video->max_width = MAX (video->max_width, video->min_width); 
  video->max_height = MAX (video->max_height, video->min_height); 

  video->old_hints.base_width = ~0; /* force update */
  gtk_widget_queue_resize (GTK_WIDGET (video));
}

/**
 * z_video_set_max_size:
 * @video: a #ZVideo object
 * @width:
 * @height:
 *
 * Set the desired maximum size of the drawing area. Width
 * and height will be rounded down to the desired size increment,
 * so you may want to set that first.
 */
void
z_video_set_max_size		(ZVideo *		video,
				 guint			width,
				 guint			height)
{
  video->max_width = FLOOR (MIN (width, MAX_SIZE), video->mod_x);
  video->max_height = FLOOR (MIN (height, MAX_SIZE), video->mod_y);

  video->min_width = MIN (video->min_width, video->max_width); 
  video->min_height = MIN (video->min_height, video->max_height); 

  video->old_hints.base_width = ~0; /* force update */
  gtk_widget_queue_resize (GTK_WIDGET (video));
}

/**
 * z_video_set_size_inc:
 * @video: a #ZVideo object
 * @width:
 * @height:
 *
 * Set the desired size increment of the drawing area. Minimum
 * and maximum size will be rounded up and down accordingly, so you
 * may want to set those first.
 *
 * You should choose a size increment matching the desired aspect
 * ratio, for example 32/24 or 32/18.
 *
 * The default size increment is 1/1.
 */
void
z_video_set_size_inc		(ZVideo *		video,
				 guint			width,
				 guint			height)
{
  video->mod_x = width;
  video->mod_y = height;

  video->min_width = CEIL (video->min_width, video->mod_x);
  video->min_height = CEIL (video->min_height, video->mod_y);

  video->max_width = FLOOR (video->max_width, video->mod_x);
  video->max_height = FLOOR (video->max_height, video->mod_y);

  video->max_width = MAX (video->max_width, video->min_width); 
  video->max_height = MAX (video->max_height, video->min_height); 

  video->old_hints.base_width = ~0; /* force update */
  gtk_widget_queue_resize (GTK_WIDGET (video));
}

/**
 * z_video_set_aspect:
 * @video: a #ZVideo object
 * @ratio: x/y, 0.0 to accept any ratio.
 *
 * Set the desired aspect ratio of the drawing area. You should
 * choose a ratio matching the desired size increment, minimum
 * and maximum size.
 *
 * The default aspect ratio is 0.0.
 */
void
z_video_set_aspect		(ZVideo *		video,
				 gfloat			ratio)
{
  video->ratio = ratio;

  video->old_hints.base_width = ~0; /* force update */
  gtk_widget_queue_resize (GTK_WIDGET (video));
}


/* Miscellaneous */

static gint
events				(GtkWidget *		widget,
				 GdkEvent *		event,
				 gpointer		unused)
{
  ZVideo *video = (ZVideo *) widget;

  if (0)
    fprintf (stderr, "on_events: GDK_%s\n", z_gdk_event_name (event));

  switch (event->type)
    {
    case GDK_EXPOSE:
      if (ZVIDEO_EXPOSE_TEST)
	{
	  fprintf (stderr, "z_video expose\n");
	  gdk_draw_arc (widget->window,
			widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
			TRUE, 0, 0,
			widget->allocation.width,
			widget->allocation.height,
			0, 64 * 360);
	}

      break;

    case GDK_MOTION_NOTIFY:
      /* Note there is a VidMode event, but we already get this event
	 when the VidMode changed, by Ctrl-Alt-nk+/- anyway. Why this
	 happens I can only speculate. Well, it is a desired sideeffect
	 in fullscreen mode, connecting to the cursor-blanked signal to
	 recenter the video. */

      /* fall through */

    case GDK_BUTTON_PRESS:
      if (video->blank_cursor_timeout > 0)
	z_video_set_cursor (video, video->blanked_cursor_type);
      break;

    /* This prevents a flicker when the pointer reenters the
       drawing area window and the cursor is still blanked. */
    case GDK_LEAVE_NOTIFY:
      if (video->blank_cursor_timeout > 0)
        {
	  guint timeout = video->blank_cursor_timeout;

	  /* Don't restart timer */
	  video->blank_cursor_timeout = 0;

	  z_video_set_cursor (video, video->blanked_cursor_type);

	  video->blank_cursor_timeout = timeout;
	}

      break;

    default:
      break;
    }

  return 0; /* propagate the event */
}

static void
destroy				(GtkObject *		object)
{
  ZVideo *video = (ZVideo *) object;
  GtkWidget *toplevel;

  if (video->blank_cursor_timeout_handle != 0)
    gtk_timeout_remove (video->blank_cursor_timeout_handle);

  if ((toplevel = get_toplevel_window (video)))
    g_signal_handlers_disconnect_by_func
      (toplevel, window_size_allocate, video);

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
realize				(GtkWidget *		widget,
				 gpointer		user_data)
{
  ZVideo *video = (ZVideo *) widget;
  GtkWidget *toplevel;

  toplevel = get_toplevel_window (video);

  g_assert (toplevel != NULL);

  g_signal_connect (toplevel, "size-allocate",
		    G_CALLBACK (window_size_allocate), video);
}

static void
instance_init			(GTypeInstance *	instance,
				 gpointer		g_class)
{
  ZVideo *video = (ZVideo *) instance;

  video->mod_x = 1;
  video->mod_y = 1;

  video->ratio = 0.0; /* any */

  video->min_width = video->mod_x;
  video->min_height = video->mod_y;

  video->max_width = FLOOR (MAX_SIZE, video->mod_x);
  video->max_height = FLOOR (MAX_SIZE, video->mod_y);

  video->blank_cursor_timeout = 0;
  video->blanked_cursor_type = GDK_LEFT_PTR;

  gtk_widget_add_events (GTK_WIDGET (video), 0
			 | GDK_POINTER_MOTION_MASK
			 | GDK_BUTTON_PRESS_MASK
			 | GDK_LEAVE_NOTIFY_MASK);

  g_signal_connect (video, "event",
		    G_CALLBACK (events), NULL);

  video->window_alloc.width = ~0;
  video->old_hints.base_width = ~0;

  g_signal_connect_after (video, "realize",
			  G_CALLBACK (realize), NULL);
}

GtkWidget *
z_video_new			(void)
{
  ZVideo *video;

  D();

  video = g_object_new (Z_TYPE_VIDEO, NULL);

  instance_init ((GTypeInstance *) video, G_OBJECT_GET_CLASS (video));

  return GTK_WIDGET (video);
}

static void
class_init			(gpointer		g_class,
				 gpointer		class_data)
{
  ZVideoClass *class = g_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  
  parent_class = g_type_class_peek_parent (class);

  object_class = GTK_OBJECT_CLASS (class);
  object_class->destroy = destroy;

  widget_class = GTK_WIDGET_CLASS (class);
  widget_class->size_allocate = size_allocate;
  widget_class->size_request = size_request;

  {
    unsigned char empty_cursor [16 * 16 / 8];
    GdkColor fg = { 0, 0, 0, 0 };
    GdkPixmap *pixmap;

    CLEAR (empty_cursor);
    pixmap = gdk_bitmap_create_from_data (NULL, empty_cursor, 16, 16);

    /* XXX unref? */
    class->blank_cursor =
      gdk_cursor_new_from_pixmap (/* source */ pixmap,
                                  /* mask */ pixmap,
				  &fg, &fg, 8, 8);

    g_object_unref (G_OBJECT (pixmap));
  }

  class->cursor_blanked = cursor_blanked;

  signals[CURSOR_BLANKED] =
    g_signal_new ("cursor-blanked",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (ZVideoClass, cursor_blanked),
                  NULL, NULL,
                  z_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

GType
z_video_get_type		(void)
{
  static GType video_type = 0;
  
  if (!video_type)
    {
      static const GTypeInfo video_info =
      {
	.class_size	= sizeof (ZVideoClass),
	.class_init	= class_init,
	.instance_size	= sizeof (ZVideo),
	.instance_init	= instance_init,
      };
      
      video_type =
	g_type_register_static (GTK_TYPE_DRAWING_AREA,
				"ZVideo", &video_info, 0);
    }
  
  return video_type;
}
