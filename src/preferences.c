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

#include "preferences.h"

void
on_preferences1_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  GtkEditable *entry;
  gint position = 0;

  entry = GTK_EDITABLE (glade_xml_get_widget (xml, "filter_entry"));
  gtk_editable_delete_text (entry, 0, -1);
  if (filter)
    gtk_editable_insert_text (entry, filter, strlen (filter), &position);
  else
    gtk_editable_insert_text (entry, "", 0, &position);
  gtk_widget_show (diag_pref);
  gdk_window_raise (diag_pref->window);
}

void
on_node_radius_slider_adjustment_changed (GtkAdjustment * adj)
{

  node_radius_multiplier = exp ((double) adj->value * log (10));
  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	 _("Adjustment value: %g. Radius multiplier %g"),
	 adj->value, node_radius_multiplier);

}

void
on_link_width_slider_adjustment_changed (GtkAdjustment * adj)
{

  link_width_multiplier = exp ((double) adj->value * log (10));
  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	 _("Adjustment value: %g. Radius multiplier %g"),
	 adj->value, link_width_multiplier);

}

void
on_averaging_spin_adjustment_changed (GtkAdjustment * adj)
{
  averaging_time = adj->value * 1000;	/* Control in ms, value in us */
}

void
on_refresh_spin_adjustment_changed (GtkAdjustment * adj, GtkWidget * canvas)
{
  refresh_period = adj->value;
  /* When removing the source (which could either be an idle or a timeout
   * function, I'm also forcing the callback for the corresponding 
   * destroying function, which in turn will install a timeout or idle
   * function using the new refresh_period. It might take a while for it
   * to settle down, but I think it works now */
  g_source_remove (diagram_timeout);
}

void
on_node_to_spin_adjustment_changed (GtkAdjustment * adj)
{
  node_timeout_time = adj->value * 1000;	/* Control in ms, value in us */
}

void
on_link_to_spin_adjustment_changed (GtkAdjustment * adj)
{
  link_timeout_time = adj->value * 1000;	/* Control in ms, value in us */
}

void
on_font_button_clicked (GtkButton * button, gpointer user_data)
{
  static GtkWidget *fontsel = NULL;
  if (!fontsel)
    fontsel = glade_xml_get_widget (xml, "fontselectiondialog1");
  gtk_font_selection_dialog_set_font_name (GTK_FONT_SELECTION_DIALOG
					   (fontsel), fontname);
  gtk_widget_show (fontsel);
}


void
on_ok_button1_clicked (GtkButton * button, gpointer user_data)
{
  GtkWidget *fontsel;
  gchar *str;
  fontsel = glade_xml_get_widget (xml, "fontselectiondialog1");
  str =
    gtk_font_selection_dialog_get_font_name (GTK_FONT_SELECTION_DIALOG
					     (fontsel));
  if (str)
    {
      if (fontname)
	g_free (fontname);
      fontname = g_strdup (str);
      g_free (str);
      need_reposition = TRUE;
    }

  gtk_widget_hide (fontsel);
}


void
on_cancel_button1_clicked (GtkButton * button, gpointer user_data)
{
  GtkWidget *fontsel;
  fontsel = glade_xml_get_widget (xml, "fontselectiondialog1");
  gtk_widget_hide (fontsel);
}				/* on_cancel_button1_clicked */


void
on_apply_button1_clicked (GtkButton * button, gpointer user_data)
{
  GtkWidget *fontsel;
  gchar *str;

  fontsel = glade_xml_get_widget (xml, "fontselectiondialog1");
  str =
    gtk_font_selection_dialog_get_font_name (GTK_FONT_SELECTION_DIALOG
					     (fontsel));
  if (str)
    {
      if (fontname)
	g_free (fontname);
      fontname = g_strdup (str);
      g_free (str);
      need_reposition = TRUE;
    }
}				/* on_apply_button1_clicked */

void
on_size_mode_menu_selected (GtkMenuShell * menu_shell, gpointer data)
{
  GtkWidget *active_item;

  active_item = gtk_menu_get_active (GTK_MENU (menu_shell));
  /* Beware! Size mode is an enumeration. The menu options
   * must much the enumaration values */
  size_mode = g_list_index (menu_shell->children, active_item);

}				/* on_size_mode_menu_selected */

void
on_node_size_optionmenu_selected (GtkMenuShell * menu_shell, gpointer data)
{
  GtkWidget *active_item;

  active_item = gtk_menu_get_active (GTK_MENU (menu_shell));
  /* Beware! Size mode is an enumeration. The menu options
   * must much the enumaration values */
  node_size_variable = g_list_index (menu_shell->children, active_item);

}				/* on_node_size_optionmenu_selected */

void
on_stack_level_menu_selected (GtkMenuShell * menu_shell, gpointer data)
{
  GtkWidget *active_item;

  active_item = gtk_menu_get_active (GTK_MENU (menu_shell));
  stack_level = g_list_index (menu_shell->children, active_item);

}				/* on_stack_level_menu_selected */

void
on_save_pref_button_clicked (GtkButton * button, gpointer user_data)
{
  save_config ("/Etherape/");
}				/* on_save_pref_button_clicked */


void
on_diagram_only_toggle_toggled (GtkToggleButton * togglebutton,
				gpointer user_data)
{

  diagram_only = gtk_toggle_button_get_active (togglebutton);
  need_reposition = TRUE;

}				/* on_diagram_only_toggle_toggled */


void
on_ok_pref_button_clicked (GtkButton * button, gpointer user_data)
{
  on_apply_pref_button_clicked (button, NULL);
  gtk_widget_hide (diag_pref);

}				/* on_ok_pref_button_clicked */

void
on_apply_pref_button_clicked (GtkButton * button, gpointer user_data)
{
  GtkWidget *widget = NULL;

  widget = glade_xml_get_widget (xml, "filter_entry");
  on_filter_entry_changed (GTK_EDITABLE (widget), NULL);
  widget = glade_xml_get_widget (xml, "filter_gnome_entry");

  update_history (GNOME_ENTRY (widget), filter, FALSE);

}				/* on_apply_pref_button_clicked */

void
on_cancel_pref_button_clicked (GtkButton * button, gpointer user_data)
{
  gtk_widget_hide (diag_pref);

}				/* on_cancel_pref_button_clicked */

/* Makes a new filter */
void
on_filter_entry_changed (GtkEditable * editable, gpointer user_data)
{
  gchar *str;
  /* TODO should make sure that for each mode the filter is set up
   * correctly */
  str = gtk_editable_get_chars (editable, 0, -1);
  if (filter)
    g_free (filter);
  filter = g_strdup (str);
  g_free (str);
  /* TODO We should look at the error code from set_filter and pop
   * up a window accordingly */
  set_filter (filter, NULL);
}				/* on_filter_entry_changed */
