/* EtherApe
 * Copyright (C) 2001 Juan Toledo
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include "appdata.h"

void on_open_activate (GtkMenuItem * menuitem, gpointer user_data);
void on_export_activate (GtkMenuItem * menuitem, gpointer user_data);
void on_interface_radio_activate (gchar * gui_device);
void on_mode_radio_activate (GtkMenuItem * menuitem, gpointer user_data);
void on_start_menuitem_activate (GtkMenuItem * menuitem, gpointer user_data);
void on_stop_menuitem_activate (GtkMenuItem * menuitem, gpointer user_data);
void on_pause_menuitem_activate (GtkMenuItem * menuitem, gpointer user_data);
void on_next_menuitem_activate (GtkMenuItem * menuitem, gpointer user_data);

void on_full_screen_activate (GtkCheckMenuItem * menuitem, gpointer user_data);
void on_toolbar_check_activate (GtkCheckMenuItem * menuitem,
				gpointer user_data);
void on_legend_check_activate (GtkCheckMenuItem * menuitem,
			       gpointer user_data);
void on_status_bar_check_activate (GtkCheckMenuItem * menuitem,
				   gpointer user_data);

void on_about1_activate (GtkMenuItem * menuitem, gpointer user_data);
void on_help_activate (GtkMenuItem * menuitem, gpointer user_data);

void init_menus (void);
void fatal_error_dialog (const gchar * message);
void gui_start_capture (void);
void gui_pause_capture (void);
void gui_eof_capture (void);
gboolean gui_stop_capture (void);	/* gui_stop_capture might fail. For instance,
					 * it can't run if diagram_update is running */

