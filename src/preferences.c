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
  if (pref.filter)
    gtk_editable_insert_text (entry, pref.filter, strlen (pref.filter),
			      &position);
  else
    gtk_editable_insert_text (entry, "", 0, &position);
  gtk_widget_show (diag_pref);
  gdk_window_raise (diag_pref->window);
}

void
on_node_radius_slider_adjustment_changed (GtkAdjustment * adj)
{

  pref.node_radius_multiplier = exp ((double) adj->value * log (10));
  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	 _("Adjustment value: %g. Radius multiplier %g"),
	 adj->value, pref.node_radius_multiplier);

}

void
on_link_width_slider_adjustment_changed (GtkAdjustment * adj)
{

  pref.link_width_multiplier = exp ((double) adj->value * log (10));
  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	 _("Adjustment value: %g. Radius multiplier %g"),
	 adj->value, pref.link_width_multiplier);

}

void
on_averaging_spin_adjustment_changed (GtkAdjustment * adj)
{
  pref.averaging_time = adj->value;	/* Control and value in ms */
}

void
on_refresh_spin_adjustment_changed (GtkAdjustment * adj, GtkWidget * canvas)
{
  pref.refresh_period = adj->value;
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
  pref.node_timeout_time = adj->value;	/* Control and value in ms */
}				/* on_node_to_spin_adjustment_changed */

void
on_gui_node_to_spin_adjustment_changed (GtkAdjustment * adj)
{
  pref.gui_node_timeout_time = adj->value;	/* Control and value in ms */
}				/* on_gui_node_to_spin_adjustment_changed */

void
on_link_to_spin_adjustment_changed (GtkAdjustment * adj)
{
  pref.link_timeout_time = adj->value;	/* Control and value in ms */
}

void
on_font_button_clicked (GtkButton * button, gpointer user_data)
{
  static GtkWidget *fontsel = NULL;
  if (!fontsel)
    fontsel = glade_xml_get_widget (xml, "fontselectiondialog1");
  gtk_font_selection_dialog_set_font_name (GTK_FONT_SELECTION_DIALOG
					   (fontsel), pref.fontname);
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
      if (pref.fontname)
	g_free (pref.fontname);
      pref.fontname = g_strdup (str);
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
      if (pref.fontname)
	g_free (pref.fontname);
      pref.fontname = g_strdup (str);
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
  pref.size_mode = g_list_index (menu_shell->children, active_item);

}				/* on_size_mode_menu_selected */

void
on_node_size_optionmenu_selected (GtkMenuShell * menu_shell, gpointer data)
{
  GtkWidget *active_item;

  active_item = gtk_menu_get_active (GTK_MENU (menu_shell));
  /* Beware! Size mode is an enumeration. The menu options
   * must much the enumaration values */
  pref.node_size_variable = g_list_index (menu_shell->children, active_item);

}				/* on_node_size_optionmenu_selected */

void
on_stack_level_menu_selected (GtkMenuShell * menu_shell, gpointer data)
{
  GtkWidget *active_item;

  active_item = gtk_menu_get_active (GTK_MENU (menu_shell));
  pref.stack_level = g_list_index (menu_shell->children, active_item);

  delete_gui_protocols ();

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

  pref.diagram_only = gtk_toggle_button_get_active (togglebutton);
  need_reposition = TRUE;

}				/* on_diagram_only_toggle_toggled */

void
on_group_unk_check_toggled (GtkToggleButton * togglebutton,
			    gpointer user_data)
{
  enum status_t old_status = status;

  if ((status == PLAY) || (status == PAUSE))
    gui_stop_capture ();

  pref.group_unk = gtk_toggle_button_get_active (togglebutton);

  if (old_status == PLAY)
    gui_start_capture ();

}				/* on_group_unk_check_toggled */


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

  update_history (GNOME_ENTRY (widget), pref.filter, FALSE);

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
  if (pref.filter)
    g_free (pref.filter);
  pref.filter = g_strdup (str);
  g_free (str);
  /* TODO We should look at the error code from set_filter and pop
   * up a window accordingly */
  set_filter (pref.filter, NULL);
}				/* on_filter_entry_changed */

void
on_color_add_button_clicked (GtkButton * button, gpointer user_data)
{
  static GtkWidget *colorseldiag = NULL;
  if (!colorseldiag)
    colorseldiag = glade_xml_get_widget (xml, "colorselectiondialog");
  gtk_widget_show (colorseldiag);
}				/* on_color_add_button_clicked */

void
on_color_remove_button_clicked (GtkButton * button, gpointer user_data)
{
  static GtkWidget *color_clist = NULL;
  gint row_number;

  if (!color_clist)
    color_clist = glade_xml_get_widget (xml, "color_clist");

  if (!
      (row_number =
       GPOINTER_TO_INT (gtk_object_get_data
			(GTK_OBJECT (color_clist), "row"))))
    return;

  gtk_clist_remove (GTK_CLIST (color_clist), row_number - 1);

  /*
   * We have removed the selected row. We'll make sure a new row has to be selected
   * before more are deleted
   */
  gtk_object_set_data (GTK_OBJECT (color_clist), "row", GINT_TO_POINTER (0));

}				/* on_color_remove_button_clicked */

void
on_protocol_edit_button_clicked (GtkButton * button, gpointer user_data)
{
  static GtkWidget *protocol_edit_dialog = NULL;
  if (!protocol_edit_dialog)
    protocol_edit_dialog = glade_xml_get_widget (xml, "protocol_edit_dialog");
  gtk_widget_show (protocol_edit_dialog);
}				/* on_protocol_edit_button_clicked */


void
on_colordiag_ok_clicked (GtkButton * button, gpointer user_data)
{
  static GtkWidget *colorseldiag = NULL;
  static GtkWidget *color_clist = NULL;
  gchar *row[2] = { NULL, NULL };
  gint row_number;
  GtkWidget *colorsel;
  gdouble colors[4];
  GdkColor gdk_color;
  GdkColormap *colormap;
  GtkStyle *style;

  if (!colorseldiag)
    colorseldiag = glade_xml_get_widget (xml, "colorselectiondialog");

  colorsel = GTK_COLOR_SELECTION_DIALOG (colorseldiag)->colorsel;
  gtk_color_selection_get_color (GTK_COLOR_SELECTION (colorsel), colors);

  if (!color_clist)
    color_clist = glade_xml_get_widget (xml, "color_clist");

  /*
   * We save row_number + 1, so we have to remove one here
   * if row_number == -1, no row was selected.
   */
  row_number =
    GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (color_clist), "row")) -
    1;

  row[0] = "hola";
  row[1] = g_strdup_printf ("row_number %d", row_number);

  colormap = gtk_widget_get_colormap (color_clist);

  gdk_color.red = (guint16) (colors[0] * 65535.0);
  gdk_color.green = (guint16) (colors[1] * 65535.0);
  gdk_color.blue = (guint16) (colors[2] * 65535.0);

  gdk_color_alloc (colormap, &gdk_color);

  style = gtk_style_new ();
  style->base[GTK_STATE_NORMAL] = gdk_color;

  /*
   * If an insertion point was defined, then
   */
  if (row_number >= 0)
    {
      gtk_clist_insert (GTK_CLIST (color_clist), row_number, row);
      gtk_clist_set_cell_style (GTK_CLIST (color_clist), row_number, 0,
				style);
      /*
       * Since we have added a row, the selected row has been pushed one step down
       * Then we add one more to distinguish non-existance from row number zero.
       */
      gtk_object_set_data (GTK_OBJECT (color_clist), "row",
			   GINT_TO_POINTER (row_number + 1 + 1));
    }
  else
    {
      gtk_clist_insert (GTK_CLIST (color_clist), 0, row);
      gtk_clist_set_cell_style (GTK_CLIST (color_clist), 0, 0, style);
    }

}

void
on_color_clist_select_row (GtkCList * clist,
			   gint row,
			   gint column, GdkEvent * event, gpointer user_data)
{
  /*
   * To distinguish non-existance from row number zero, we save the row
   * number plus one
   */
  gtk_object_set_data (GTK_OBJECT (clist), "row", GINT_TO_POINTER (row + 1));
}
