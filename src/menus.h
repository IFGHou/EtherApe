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

#include <libgnomeui/gnome-popup-menu.h>
#include <libgnomeui/gnome-messagebox.h>
#include <gtk/gtk.h>
#include "globals.h"
#include "util.h"

GList *interface_list = NULL;	/* A list of all usable interface */

void on_open_activate (GtkMenuItem * menuitem, gpointer user_data);
void on_file_cancel_button_clicked (GtkButton * button, gpointer user_data);
void on_file_combo_entry_changed (GtkEditable * editable, gpointer user_data);
void on_file_ok_button_clicked (GtkButton * button, gpointer user_data);

void on_interface_radio_activate (gchar * gui_device);
void on_mode_radio_activate (GtkMenuItem * menuitem, gpointer user_data);
void on_start_menuitem_activate (GtkMenuItem * menuitem, gpointer user_data);
void on_stop_menuitem_activate (GtkMenuItem * menuitem, gpointer user_data);
void on_pause_menuitem_activate (GtkMenuItem * menuitem, gpointer user_data);

void on_toolbar_check_activate (GtkCheckMenuItem * menuitem,
				gpointer user_data);
void on_legend_check_activate (GtkCheckMenuItem * menuitem,
			       gpointer user_data);
void on_status_bar_check_activate (GtkCheckMenuItem * menuitem,
				   gpointer user_data);

void on_about1_activate (GtkMenuItem * menuitem, gpointer user_data);
static void set_active_interface (void);
