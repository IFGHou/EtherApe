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

void node_info_window_create(const node_id_t * node_id);
guint update_info_windows (void);
void node_protocols_window_create(const node_id_t * node_id);
void link_info_window_create(const link_id_t * link_id);


/* callbacks */
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
void on_protocols_check_activate (GtkCheckMenuItem * menuitem, gpointer user_data);
void on_prot_color_column_activate (GtkMenuItem * gm, gpointer * user_data);
void on_protocol_column_activate (GtkMenuItem * gm, gpointer * user_data);
void on_instant_column_activate (GtkMenuItem * gm, gpointer * user_data);
void on_accumulated_column_activate (GtkMenuItem * gm, gpointer * user_data);
void on_heard_column_activate (GtkMenuItem * gm, gpointer * user_data);
void on_packets_column_activate (GtkMenuItem * gm, gpointer * user_data);
