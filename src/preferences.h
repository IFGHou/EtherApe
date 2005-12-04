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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include "globals.h"
#include "math.h"

/* Extern functions */
extern void save_config (gchar * prefix);

void on_preferences1_activate (GtkMenuItem * menuitem, gpointer user_data);
void on_averaging_spin_adjustment_changed (GtkAdjustment * adj);
void on_refresh_spin_adjustment_changed (GtkAdjustment * adj,
					 GtkWidget * canvas);
void on_node_radius_slider_adjustment_changed (GtkAdjustment * adj);
void on_link_width_slider_adjustment_changed (GtkAdjustment * adj);
void on_node_to_spin_adjustment_changed (GtkAdjustment * adj);
void on_gui_node_to_spin_adjustment_changed (GtkAdjustment * adj);
void on_link_to_spin_adjustment_changed (GtkAdjustment * adj);
void on_font_button_clicked (GtkButton * button, gpointer user_data);
void on_ok_button1_clicked (GtkButton * button, gpointer user_data);
void on_cancel_button1_clicked (GtkButton * button, gpointer user_data);
void on_apply_button1_clicked (GtkButton * button, gpointer user_data);
void on_size_mode_menu_selected (GtkMenuShell * menu_shell, gpointer data);
void on_node_size_optionmenu_selected (GtkMenuShell * menu_shell,
				       gpointer data);
void on_stack_level_menu_selected (GtkMenuShell * menu_shell, gpointer data);
void on_save_pref_button_clicked (GtkButton * button, gpointer user_data);
void on_diagram_only_toggle_toggled (GtkToggleButton * togglebutton,
				     gpointer user_data);
void on_filter_entry_changed (GtkEditable * editable, gpointer user_data);
void on_apply_pref_button_clicked (GtkButton * button, gpointer user_data);
void on_ok_pref_button_clicked (GtkButton * button, gpointer user_data);
void on_cancel_pref_button_clicked (GtkButton * button, gpointer user_data);

void on_group_unk_check_toggled (GtkToggleButton * togglebutton, gpointer);
void on_aa_check_toggled (GtkToggleButton * togglebutton, gpointer user_data);
void on_fade_toggle_toggled (GtkToggleButton * togglebutton, gpointer udata);
void on_cycle_toggle_toggled (GtkToggleButton * togglebutton, gpointer udata);

/* name tab callbacks */
gboolean on_name_clist_select_row (GtkTreeView * gv, gboolean arg1, gpointer user_data);
void on_dns_toggle_toggled (GtkToggleButton * togglebutton, gpointer user_data);
void on_numeric_toggle_toggled (GtkToggleButton * togglebutton, gpointer user_data);
void on_protocol_add_button_clicked (GtkButton * button, gpointer user_data);
void on_protocol_remove_button_clicked (GtkButton * button, gpointer user_data);
void on_protocol_move_up_clicked (GtkButton * button, gpointer user_data);
void on_protocol_move_down_clicked (GtkButton * button, gpointer user_data);

/* proto color tab callbacks */
void on_color_add_button_clicked (GtkButton * button, gpointer user_data);
void on_color_remove_button_clicked (GtkButton * button, gpointer user_data);
void on_colordiag_ok_clicked (GtkButton * button, gpointer user_data);
void on_protocol_edit_button_clicked (GtkButton * button, gpointer user_data);
void on_protocol_edit_dialog_show (GtkWidget * wdg, gpointer user_data);
void on_protocol_edit_ok_clicked (GtkButton * button, gpointer user_data);
