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

typedef struct
{
  guint8 *node_id;
  GtkWidget *window;
}
node_info_window_t;

GList *node_info_windows = NULL;

gboolean on_prot_table_button_press_event (GtkWidget * widget,
					   GdkEventButton * event,
					   gpointer user_data);
static void update_node_info_window (node_info_window_t * node_info_window);
static gint node_info_compare (gconstpointer a, gconstpointer b);

void on_node_info_delete_event (GtkWidget * node_info, gpointer user_data);
