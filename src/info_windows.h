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

#include <gtk/gtkbutton.h>
#include <gtk/gtkclist.h>
#include <gtk/gtkcheckmenuitem.h>

typedef struct
{
  guint8 *node_id;
  GtkWidget *window;
}
node_info_window_t;

typedef struct
{
  gchar *prot_name;
  GtkWidget *window;
}
prot_info_window_t;


gboolean on_prot_table_button_press_event (GtkWidget * widget,
					   GdkEventButton * event,
					   gpointer user_data);

gboolean on_node_info_delete_event (GtkWidget *, GdkEvent *, gpointer);
gboolean on_prot_info_delete_event (GtkWidget *, GdkEvent *, gpointer);
void toggle_protocols_window (void);
gboolean on_prot_list_select_row (GtkTreeView * gv, gboolean arg1,
				  gpointer ud);
gboolean on_delete_protocol_window (GtkWidget * wdg, GdkEvent * e,
				    gpointer ud);
