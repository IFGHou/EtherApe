/* Etherape
 * Copyright (C) 2000 Juan Toledo
 * $Id$
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

#include "callbacks.h"

/* Extern functions */
extern void save_config (gchar * prefix);

gboolean
on_app1_delete_event (GtkWidget * widget,
		      GdkEvent * event, gpointer user_data)
{
  gtk_exit (0);
  return FALSE;
}


void
on_canvas1_size_allocate (GtkWidget * widget,
			  GtkAllocation * allocation, gpointer user_data)
{

  GtkWidget *canvas;
  gnome_canvas_set_scroll_region (GNOME_CANVAS (widget),
				  -widget->allocation.width / 2,
				  -widget->allocation.height / 2,
				  widget->allocation.width / 2,
				  widget->allocation.height / 2);
  need_reposition = TRUE;
  canvas = glade_xml_get_widget (xml, "canvas1");
  update_diagram (canvas);
}


/* TODO this is not necessary, can be set directly in etherape.glade */
gboolean
on_node_popup_motion_notify_event (GtkWidget * widget,
				   GdkEventMotion * event, gpointer user_data)
{

  gtk_widget_destroy (widget);
  return FALSE;
}


gboolean
on_name_motion_notify_event (GtkWidget * widget,
			     GdkEventMotion * event, gpointer user_data)
{

  g_message ("Motion in name label");
  return FALSE;
}
