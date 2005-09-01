/*
 * Zapping (TV viewer for the Gnome Desktop)
 * ZStack Container Widget
 *
 * Copyright (C) 2005 Michael H. Schimek
 *
 * Based on GtkFixed widget from:
 * GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "zstack.h"

static GtkContainerClass *	parent_class;

void
z_stack_set_has_window		(ZStack *		stack,
				 gboolean		has_window)
{
  g_return_if_fail (Z_IS_STACK (stack));
  g_return_if_fail (!GTK_WIDGET_REALIZED (stack));

  if (!has_window != GTK_WIDGET_NO_WINDOW (stack))
    {
      if (has_window)
	GTK_WIDGET_UNSET_FLAGS (stack, GTK_NO_WINDOW);
      else
	GTK_WIDGET_SET_FLAGS (stack, GTK_NO_WINDOW);
    }
}

static ZStackChild *
get_child			(ZStack *		stack,
				 GtkWidget *		widget)
{
  GList *children;

  for (children = stack->children; children; children = children->next)
    {
      ZStackChild *child = children->data;

      if (widget == child->widget)
	return child;
    }

  return NULL;
}

static ZStackChild *
get_child_below			(ZStack *		stack,
				 ZStackChild *		above)
{
  GList *children;
  ZStackChild *below;

  below = NULL;

  for (children = stack->children; children; children = children->next)
    {
      ZStackChild *child = children->data;

      if (child->z >= above->z)
	continue;

      if (below && child->z < below->z)
	continue;

      below = child;
    }

  return below;
}

static ZStackChild *
get_child_above			(ZStack *		stack,
				 ZStackChild *		below)
{
  GList *children;
  ZStackChild *above;

  above = NULL;

  for (children = stack->children; children; children = children->next)
    {
      ZStackChild *child = children->data;

      if (child->z <= below->z)
	continue;

      if (above && child->z > above->z)
	continue;

      above = child;
    }

  return above;
}

static void
child_realize			(GtkWidget *		widget,
				 gpointer		user_data)
{
  ZStack *stack = Z_STACK (user_data);
  ZStackChild *child;
  ZStackChild *below;

  child = get_child (stack, widget);
  g_assert (NULL != child);

  if ((below = get_child_below (stack, child)))
    {
      g_assert (child != below);

      if (!GTK_WIDGET_REALIZED (below->widget))
	{
	  gtk_widget_realize (below->widget);
	}

      g_assert (NULL != below->widget->window);

      gtk_widget_set_parent_window (widget, below->widget->window);
    }

  /* Now realize it. */
}

static void
child_unrealize			(GtkWidget *		widget,
				 gpointer		user_data)
{
  ZStack *stack = Z_STACK (user_data);
  ZStackChild *child;
  ZStackChild *above;

  child = get_child (stack, widget);
  g_assert (NULL != child);

  if ((above = get_child_above (stack, child)))
    {
      ZStackChild *below;
      GdkWindow *parent_window;
      GdkWindow *child_window;

      g_assert (child != above);

      child_window = above->widget->window;

      if ((below = get_child_below (stack, child)))
	{
	  g_assert (child != below);
	  g_assert (above != below);

	  parent_window = below->widget->window;
	}
      else
	{
	  GtkWidget *stack_widget = GTK_WIDGET (stack);

	  if (GTK_WIDGET_NO_WINDOW (stack_widget))
	    parent_window = gtk_widget_get_parent_window (stack_widget);
	  else
	    parent_window = stack_widget->window;
	}
  
      g_assert (NULL != parent_window);
      g_assert (NULL != child_window);

      gtk_widget_set_parent_window (above->widget, parent_window);
      gdk_window_reparent (child_window, parent_window,
			   /* x */ 0, /* y */ 0);
      /* gdk_window_reparent() unmaps the child_window. */
      gdk_window_show_unraised (child_window);
    }

  /* Now unrealize it. */
}

void
z_stack_put			(ZStack *		stack,
				 GtkWidget *		widget,
				 gint			z)
{
  GList *children;
  ZStackChild *child;
  GObject *object;

  g_return_if_fail (Z_IS_STACK (stack));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  for (children = stack->children; children; children = children->next)
    {
      ZStackChild *child = children->data;

      g_assert (z != child->z);
    }

  child = g_new (ZStackChild, 1);
  child->widget = widget;
  child->z = z;

  stack->children = g_list_append (stack->children, child);

  object = G_OBJECT (widget);
  g_signal_connect (object, "realize", G_CALLBACK (child_realize), stack);
  g_signal_connect (object, "unrealize", G_CALLBACK (child_unrealize), stack);

  /* Set stack as parent and realize the widget if stack is realized. */
  gtk_widget_set_parent (widget, GTK_WIDGET (stack));
}

/* ------------------------------------------------------------------------- */

static GType
z_stack_child_type		(GtkContainer *		container)
{
  container = container;

  return GTK_TYPE_WIDGET;
}

static void
z_stack_forall			(GtkContainer *		container,
				 gboolean		include_internals,
				 GtkCallback		callback,
				 gpointer		callback_data)
{
  ZStack *stack = Z_STACK (container);
  GList *children;

  include_internals = include_internals;

  g_return_if_fail (callback != NULL);

  children = stack->children;
  while (children)
    {
      ZStackChild *child;

      child = children->data;
      children = children->next;

      callback (child->widget, callback_data);
    }
}

static void
z_stack_remove			(GtkContainer *		container,
				 GtkWidget *		widget)
{
  ZStack *stack = Z_STACK (container);
  GList *children;

  children = stack->children;
  while (children)
    {
      ZStackChild *child;

      child = children->data;

      if (child->widget == widget)
	{
	  gboolean was_visible = GTK_WIDGET_VISIBLE (widget);
	  
	  gtk_widget_unparent (widget);

	  stack->children = g_list_remove_link (stack->children, children);
	  g_list_free (children);
	  g_free (child);

	  if (was_visible && GTK_WIDGET_VISIBLE (container))
	    gtk_widget_queue_resize (GTK_WIDGET (container));

	  break;
	}

      children = children->next;
    }
}

static void
z_stack_add			(GtkContainer *		container,
				 GtkWidget *		widget)
{
  z_stack_put (Z_STACK (container), widget, 0);
}

static void
z_stack_size_allocate		(GtkWidget *		widget,
				 GtkAllocation *	allocation)
{
  ZStack *stack = Z_STACK (widget);
  GList *children;
  guint border_width;

  widget->allocation = *allocation;

  if (!GTK_WIDGET_NO_WINDOW (widget))
    {
      if (GTK_WIDGET_REALIZED (widget))
	gdk_window_move_resize (widget->window,
				allocation->x, 
				allocation->y,
				allocation->width, 
				allocation->height);
    }

  border_width = GTK_CONTAINER (stack)->border_width;

  children = stack->children;
  while (children)
    {
      ZStackChild *child;

      child = children->data;
      children = children->next;

      if (GTK_WIDGET_VISIBLE (child->widget))
	{
	  GtkAllocation child_allocation;

	  child_allocation.x = border_width;
	  child_allocation.y = border_width;

	  if (GTK_WIDGET_NO_WINDOW (widget))
	    {
	      child_allocation.x += widget->allocation.x;
	      child_allocation.y += widget->allocation.y;
	    }
	  
	  child_allocation.width =
	    widget->allocation.width - border_width * 2;
	  child_allocation.height =
	    widget->allocation.height - border_width * 2;

	  gtk_widget_size_allocate (child->widget, &child_allocation);
	}
    }
}

static void
z_stack_size_request		(GtkWidget *		widget,
				 GtkRequisition *	requisition)
{
  ZStack *stack = Z_STACK (widget);
  GList *children;
  guint border_width;

  requisition->width = 0;
  requisition->height = 0;

  children = stack->children;
  while (children)
    {
      ZStackChild *child;

      child = children->data;
      children = children->next;

      if (GTK_WIDGET_VISIBLE (child->widget))
	{
	  GtkRequisition child_requisition;

          gtk_widget_size_request (child->widget, &child_requisition);

          requisition->height = MAX (requisition->height,
                                     child_requisition.height);
          requisition->width = MAX (requisition->width,
                                    child_requisition.width);
	}
    }

  border_width = GTK_CONTAINER (stack)->border_width;
  requisition->height += border_width * 2;
  requisition->width += border_width * 2;
}

static void
z_stack_realize			(GtkWidget *		widget)
{
  GdkWindowAttr attributes;
  gint attributes_mask;

  if (GTK_WIDGET_NO_WINDOW (widget))
    GTK_WIDGET_CLASS (parent_class)->realize (widget);
  else
    {
      GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

      attributes.window_type = GDK_WINDOW_CHILD;
      attributes.x = widget->allocation.x;
      attributes.y = widget->allocation.y;
      attributes.width = widget->allocation.width;
      attributes.height = widget->allocation.height;
      attributes.wclass = GDK_INPUT_OUTPUT;
      attributes.visual = gtk_widget_get_visual (widget);
      attributes.colormap = gtk_widget_get_colormap (widget);
      attributes.event_mask = gtk_widget_get_events (widget);
      attributes.event_mask |= GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK;

      attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
      
      widget->window =
	gdk_window_new (gtk_widget_get_parent_window (widget),
			&attributes, 
			attributes_mask);

      gdk_window_set_user_data (widget->window, widget);
      
      widget->style = gtk_style_attach (widget->style, widget->window);

      gtk_style_set_background (widget->style,
				widget->window,
				GTK_STATE_NORMAL);
    }
}

static void
z_stack_init			(ZStack *		stack)
{
  GTK_WIDGET_SET_FLAGS (stack, GTK_NO_WINDOW);
 
  stack->children = NULL;
}

GtkWidget *
z_stack_new			(void)
{
  return g_object_new (Z_TYPE_STACK, NULL);
}

static void
z_stack_class_init		(ZStackClass *		class)
{
  GtkWidgetClass *widget_class = (GtkWidgetClass *) class;
  GtkContainerClass *container_class = (GtkContainerClass *) class;

  parent_class = g_type_class_peek_parent (class);

  widget_class->realize = z_stack_realize;
  widget_class->size_request = z_stack_size_request;
  widget_class->size_allocate = z_stack_size_allocate;

  container_class->add = z_stack_add;
  container_class->remove = z_stack_remove;
  container_class->forall = z_stack_forall;
  container_class->child_type = z_stack_child_type;
}

GType
z_stack_get_type		(void)
{
  static GType stack_type = 0;

  if (!stack_type)
    {
      static const GTypeInfo stack_info =
	{
	  .class_size		= sizeof (ZStackClass),
	  .class_init		= (GClassInitFunc) z_stack_class_init,
	  .instance_size	= sizeof (ZStack),
	  .instance_init	= (GInstanceInitFunc) z_stack_init,
	};

      stack_type = g_type_register_static (GTK_TYPE_CONTAINER, "ZStack",
					   &stack_info, 0);
    }

  return stack_type;
}
