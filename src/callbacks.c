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

#include "callbacks.h"
#include "math.h"

#include "globals.h"

/* Extern functions */
extern void save_config (gchar * prefix);

void
on_file1_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  GtkWidget *messagebox;
  messagebox = glade_xml_get_widget(xml_app1, "messagebox1");
  gtk_widget_show (messagebox);
}


void
on_new_file1_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  GtkWidget *messagebox;
  messagebox = glade_xml_get_widget(xml_app1, "messagebox1");
  gtk_widget_show (messagebox);
}


void
on_open1_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  GtkWidget *messagebox;
  messagebox = glade_xml_get_widget(xml_app1, "messagebox1");
  gtk_widget_show (messagebox);
}


void
on_save1_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  GtkWidget *messagebox;
  messagebox = glade_xml_get_widget(xml_app1, "messagebox1");
  gtk_widget_show (messagebox);
}


void
on_save_as1_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  GtkWidget *messagebox;
  messagebox = glade_xml_get_widget(xml_app1, "messagebox1");
  gtk_widget_show (messagebox);
}


void
on_exit1_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  gtk_exit (0);
}


void
on_cut1_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  GtkWidget *messagebox;
  messagebox = glade_xml_get_widget(xml_app1, "messagebox1");
  gtk_widget_show (messagebox);

}


void
on_copy1_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  GtkWidget *messagebox;
  messagebox = glade_xml_get_widget(xml_app1, "messagebox1");
  gtk_widget_show (messagebox);

}


void
on_paste1_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  GtkWidget *messagebox;
  messagebox = glade_xml_get_widget(xml_app1, "messagebox1");
  gtk_widget_show (messagebox);

}


void
on_clear1_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  GtkWidget *messagebox;
  messagebox = glade_xml_get_widget(xml_app1, "messagebox1");
  gtk_widget_show (messagebox);

}


void
on_properties1_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  GtkWidget *messagebox;
  messagebox = glade_xml_get_widget(xml_app1, "messagebox1");
  gtk_widget_show (messagebox);

}


void
on_preferences1_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  GtkEditable *entry;
  gint position = 0;

  entry = GTK_EDITABLE (glade_xml_get_widget(xml_diag_pref,
				       "filter_entry"));
  gtk_editable_delete_text (entry, 0, -1);
  gtk_editable_insert_text (entry, filter, strlen (filter), &position);
  gtk_widget_show (diag_pref);
  gdk_window_raise (diag_pref->window);
}


void
on_about1_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  GtkWidget *about;
  about = glade_xml_get_widget(xml_app1, "about2");
  gtk_widget_show (about);

}


gboolean
on_app1_delete_event (GtkWidget * widget,
		      GdkEvent * event, gpointer user_data)
{
  gtk_exit (0);
  return FALSE;
}


void
on_canvas1_size_allocate (GtkWidget * widget,
			  GtkAllocation * allocation, gpointer user_data)
{

  GtkWidget *canvas;
  gnome_canvas_set_scroll_region (GNOME_CANVAS (widget),
				  -widget->allocation.width / 2,
				  -widget->allocation.height / 2,
				  widget->allocation.width / 2,
				  widget->allocation.height / 2);
  need_reposition = TRUE;
  canvas = glade_xml_get_widget(xml_app1, "canvas1");
  update_diagram (canvas);
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

gboolean
on_node_popup_motion_notify_event (GtkWidget * widget,
				   GdkEventMotion * event, gpointer user_data)
{

  gtk_widget_destroy (widget);
  return FALSE;
}


gboolean
on_name_motion_notify_event (GtkWidget * widget,
			     GdkEventMotion * event, gpointer user_data)
{

  g_message ("Motion in name label");
  return FALSE;
}

void
on_toolbar_check_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  static gboolean active = TRUE;
  GnomeDock *dock;
  GnomeDockItem *dock_item;
  guint num_band_return, band_position_return, offset_return;
  GnomeDockPlacement placement_return;

  dock = GNOME_DOCK (glade_xml_get_widget(xml_app1, "dock1"));
  if (active)
    {
      dock_item = gnome_dock_get_item_by_name (dock,
					       "toolbar",
					       &placement_return,
					       &num_band_return,
					       &band_position_return,
					       &offset_return);
      gtk_widget_hide (GTK_WIDGET (dock_item));
      gtk_container_queue_resize (GTK_CONTAINER (dock));
      active = FALSE;
    }
  else
    {
      dock_item = gnome_dock_get_item_by_name (dock,
					       "toolbar",
					       &placement_return,
					       &num_band_return,
					       &band_position_return,
					       &offset_return);
      gtk_widget_show (GTK_WIDGET (dock_item));
      active = TRUE;
    }
}

void
on_legend_check_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  static gboolean active = TRUE;
  GnomeDock *dock;
  GnomeDockItem *dock_item;
  guint num_band_return, band_position_return, offset_return;
  GnomeDockPlacement placement_return;

  dock = GNOME_DOCK (glade_xml_get_widget(xml_app1, "dock1"));
  if (active)
    {
      dock_item = gnome_dock_get_item_by_name (dock,
					       "legend_table",
					       &placement_return,
					       &num_band_return,
					       &band_position_return,
					       &offset_return);
      gtk_widget_hide (GTK_WIDGET (dock_item));
      gtk_container_queue_resize (GTK_CONTAINER (dock));
      active = FALSE;
    }
  else
    {
      dock_item = gnome_dock_get_item_by_name (dock,
					       "legend_table",
					       &placement_return,
					       &num_band_return,
					       &band_position_return,
					       &offset_return);
      gtk_widget_show (GTK_WIDGET (dock_item));
      active = TRUE;
    }

}


void
on_status_bar_check_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  static gboolean active = TRUE;
  GtkWidget *widget;
  widget = glade_xml_get_widget(xml_app1, "appbar1");
  if (active)
    {
      gtk_widget_hide (widget);
      gtk_container_queue_resize (GTK_CONTAINER (app1));
      active = FALSE;
    }
  else
    {
      gtk_widget_show (widget);
      active = TRUE;
    }
}



void
on_font_button_clicked (GtkButton * button, gpointer user_data)
{
  static GtkWidget *fontsel = NULL;
  if (!fontsel)
    fontsel = glade_xml_get_widget(xml_app1, "fontselectiondialog1");
  gtk_font_selection_dialog_set_font_name (GTK_FONT_SELECTION_DIALOG
					   (fontsel), fontname);
  gtk_widget_show (fontsel);
}


void
on_ok_button1_clicked (GtkButton * button, gpointer user_data)
{
  GtkWidget *fontsel;
  gchar *str;
  /* TODO fix libglade */
#if 0
  fontsel = lookup_widget (GTK_WIDGET (button), "fontselectiondialog1");
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
#endif   
}


void
on_cancel_button1_clicked (GtkButton * button, gpointer user_data)
{
  GtkWidget *fontsel;
  /* TODO fix libglade */
#if 0
  fontsel = lookup_widget (GTK_WIDGET (button), "fontselectiondialog1");
  gtk_widget_hide (fontsel);
#endif
}


void
on_apply_button1_clicked (GtkButton * button, gpointer user_data)
{
  GtkWidget *fontsel;
  gchar *str;
  /* TODO fix libglade */
#if 0

  fontsel = lookup_widget (GTK_WIDGET (button), "fontselectiondialog1");
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
#endif
}

void
on_size_mode_menu_selected (GtkMenuShell * menu_shell, gpointer data)
{
  GtkWidget *active_item;

  active_item = gtk_menu_get_active (GTK_MENU (menu_shell));
  /* Beware! Size mode is an enumeration. The menu options
   * must much the enumaration values */
  size_mode = g_list_index (menu_shell->children, active_item);

}

void
on_stack_level_menu_selected (GtkMenuShell * menu_shell, gpointer data)
{
  GtkWidget *active_item;

  active_item = gtk_menu_get_active (GTK_MENU (menu_shell));
  stack_level = g_list_index (menu_shell->children, active_item);

}

void
on_save_pref_button_clicked (GtkButton * button, gpointer user_data)
{
  save_config ("/Etherape/");
}


void
on_diagram_only_toggle_toggled (GtkToggleButton * togglebutton,
				gpointer user_data)
{

  diagram_only = gtk_toggle_button_get_active (togglebutton);
  need_reposition = TRUE;

}


void
on_ok_pref_button_clicked (GtkButton * button, gpointer user_data)
{
#if 0   
  GtkWidget *dialog;
#endif   
  on_apply_pref_button_clicked (button, NULL);
#if 0   
  dialog = lookup_widget (GTK_WIDGET (button), "diag_pref");
#endif
  gtk_widget_hide (diag_pref);

}

void
on_apply_pref_button_clicked (GtkButton * button, gpointer user_data)
{
  GtkWidget *widget;
  widget = glade_xml_get_widget(xml_diag_pref, "filter_entry");
  on_filter_entry_changed (GTK_EDITABLE (widget), NULL);
  widget = glade_xml_get_widget(xml_diag_pref, "filter_gnome_entry");
  /* TODO should only be done if the filter is not already
   * in the history */
  gnome_entry_prepend_history (GNOME_ENTRY (widget), TRUE, filter);
  gnome_entry_save_history (GNOME_ENTRY (widget));

}

void
on_cancel_pref_button_clicked (GtkButton * button, gpointer user_data)
{
#if 0   
  GtkWidget *dialog;
  dialog = glade_xml_get_widget(xml_diag_pref, "diag_pref");
#endif   
  gtk_widget_hide (diag_pref);

}

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
}
