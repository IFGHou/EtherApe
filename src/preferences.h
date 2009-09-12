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

void load_config (const char *prefix);
void save_config (const char * prefix);

void init_config(struct pref_struct *cfg);
struct pref_struct *duplicate_config(const struct pref_struct *src);
void free_config(struct pref_struct *t);
void copy_config(struct pref_struct *tgt, const struct pref_struct *src);

void initialize_pref_controls(void);

/* callbacks */
void on_preferences1_activate (GtkMenuItem * menuitem, gpointer user_data);
void on_averaging_spin_adjustment_changed (GtkAdjustment * adj);
void on_refresh_spin_adjustment_changed (GtkAdjustment * adj,
					 GtkWidget * canvas);
void on_node_radius_slider_adjustment_changed (GtkAdjustment * adj);
void on_link_width_slider_adjustment_changed (GtkAdjustment * adj);
void on_node_to_spin_adjustment_changed (GtkAdjustment * adj);
void on_gui_node_to_spin_adjustment_changed (GtkAdjustment * adj);
void on_proto_node_to_spin_adjustment_changed (GtkAdjustment * adj);
void on_link_to_spin_adjustment_changed (GtkAdjustment * adj);
void on_gui_link_to_spin_adjustment_changed (GtkAdjustment * adj);
void on_proto_link_to_spin_adjustment_changed (GtkAdjustment * adj);
void on_proto_to_spin_adjustment_changed (GtkAdjustment * adj);
void on_save_pref_button_clicked (GtkButton * button, gpointer user_data);
void on_diagram_only_toggle_toggled (GtkToggleButton * togglebutton,
				     gpointer user_data);
void on_ok_pref_button_clicked (GtkButton * button, gpointer user_data);
void on_cancel_pref_button_clicked (GtkButton * button, gpointer user_data);

void on_group_unk_check_toggled (GtkToggleButton * togglebutton, gpointer);
void on_cycle_toggle_toggled (GtkToggleButton * togglebutton, gpointer udata);
void on_numeric_toggle_toggled (GtkToggleButton * togglebutton, gpointer user_data);

/* proto color tab callbacks */
void on_color_add_button_clicked (GtkButton * button, gpointer user_data);
void on_color_change_button_clicked (GtkButton * button, gpointer user_data);
void on_color_remove_button_clicked (GtkButton * button, gpointer user_data);
void on_colordiag_ok_clicked (GtkButton * button, gpointer user_data);
void on_protocol_edit_button_clicked (GtkButton * button, gpointer user_data);
void on_protocol_edit_dialog_show (GtkWidget * wdg, gpointer user_data);
void on_protocol_edit_ok_clicked (GtkButton * button, gpointer user_data);

/* removes all spaces from str (in place). Returns str */
gchar *remove_spaces(gchar *str);
  
