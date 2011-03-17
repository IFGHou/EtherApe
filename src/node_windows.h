/* Etherape
 * Copyright (C) 2000-2009 Juan Toledo, Riccardo Ghetta
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
 *
 * node_windows: node-related windows and dialogs
 */

#ifndef NODE_WINDOWS_H
#define NODE_WINDOWS_H

#include "appdata.h"

void nodes_wnd_show(void);
void nodes_wnd_hide(void);
void nodes_wnd_update(void);

/* gtk callbacks */
gboolean on_nodes_wnd_delete_event(GtkWidget * wdg, GdkEvent * evt, gpointer ud);
void on_nodes_table_row_activated(GtkTreeView *gv,
                                  GtkTreePath *sorted_path,
                                  GtkTreeViewColumn *column,
                                  gpointer data);

#endif
