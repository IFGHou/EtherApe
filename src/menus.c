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

#include "menus.h"

#if 0
/* This is here just as an example in case I need it again */
void
on_file1_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  GtkWidget *messagebox;
  GladeXML *xml_messagebox;
  xml_messagebox =
    glade_xml_new (GLADEDIR "/" ETHERAPE_GLADE_FILE, "messagebox1");
  messagebox = glade_xml_get_widget (xml_messagebox, "messagebox1");
  if (!xml)
    {
      g_error (_("We could not load the interface! (%s)"),
	       GLADEDIR "/" ETHERAPE_GLADE_FILE);
      return;
    }
  gtk_widget_show (messagebox);
  gtk_object_unref (GTK_OBJECT (xml_messagebox));
}
#endif

void
on_open_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  static GtkWidget *open_file_dialog = NULL;
  if (!open_file_dialog)
    open_file_dialog = glade_xml_get_widget (xml, "open_file_dialog");
  gtk_widget_show (open_file_dialog);
}				/* on_open_activate */

void
on_file_cancel_button_clicked (GtkButton * button, gpointer user_data)
{
  GtkWidget *open_file_dialog;
  open_file_dialog = glade_xml_get_widget (xml, "open_file_dialog");
  gtk_widget_hide (open_file_dialog);
}				/* on_file_cancel_button_clicked */

/* Sets up the new input capture file */
void
on_file_combo_entry_changed (GtkEditable * editable, gpointer user_data)
{
  gchar *str;
  str = gtk_editable_get_chars (editable, 0, -1);
  if (input_file)
    g_free (input_file);
  if (interface)
    {
      g_free (interface);
      interface = NULL;
    }

  input_file = g_strdup (str);
  g_free (str);

}				/* on_file_combo_entry_changed */

void
on_file_ok_button_clicked (GtkButton * button, gpointer user_data)
{
  GtkWidget *widget;

  widget = glade_xml_get_widget (xml, "file_combo_entry");
  on_file_combo_entry_changed (GTK_EDITABLE (widget), NULL);

  widget = glade_xml_get_widget (xml, "fileentry");
  update_history (GNOME_ENTRY
		  (gnome_file_entry_gnome_entry (GNOME_FILE_ENTRY (widget))),
		  input_file, TRUE);

  widget = glade_xml_get_widget (xml, "open_file_dialog");
  gtk_widget_hide (widget);

  if (status!=STOP)
     gui_stop_capture ();
  gui_start_capture ();

}				/* on_file_ok_button_clicked */


void
on_exit1_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  gtk_exit (0);
}				/* on_exit1_activate */

void
on_toolbar_check_activate (GtkCheckMenuItem * menuitem, gpointer user_data)
{
  GnomeDock *dock;
  GtkWidget *widget;

  dock = GNOME_DOCK (glade_xml_get_widget (xml, "dock1"));
  widget = glade_xml_get_widget (xml, "dock_toolbar");
  if (menuitem->active)
    gtk_widget_show (widget);
  else
    gtk_widget_hide (widget);

  gtk_container_queue_resize (GTK_CONTAINER (dock));
}				/* on_toolbar_check_activate */

void
on_legend_check_activate (GtkCheckMenuItem * menuitem, gpointer user_data)
{
  GnomeDock *dock;
  GtkWidget *widget;

  dock = GNOME_DOCK (glade_xml_get_widget (xml, "dock1"));
  widget = glade_xml_get_widget (xml, "dock_legend");
  if (menuitem->active)
    gtk_widget_show (widget);
  else
    gtk_widget_hide (widget);

  gtk_container_queue_resize (GTK_CONTAINER (dock));

}				/* on_legend_check_activate */

void
on_status_bar_check_activate (GtkCheckMenuItem * menuitem, gpointer user_data)
{
  GtkWidget *widget;
  widget = glade_xml_get_widget (xml, "appbar1");
  if (menuitem->active)
    gtk_widget_show (widget);
  else
    gtk_widget_hide (widget);

  gtk_container_queue_resize (GTK_CONTAINER (app1));
}				/* on_status_bar_check_activate */


void
on_about1_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  GtkWidget *about;
  GladeXML *xml_about;
  xml_about = glade_xml_new (GLADEDIR "/" ETHERAPE_GLADE_FILE, "about2");
  about = glade_xml_get_widget (xml_about, "about2");
  if (!xml)
    {
      g_error (_("We could not load the interface! (%s)"),
	       GLADEDIR "/" ETHERAPE_GLADE_FILE);
      return;
    }
  gtk_widget_show (about);
  gtk_object_unref (GTK_OBJECT (xml_about));
}				/* on_about1_activate */

void
on_start_menuitem_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  gui_start_capture ();

}				/* on_start_menuitem_activate */

void
on_pause_menuitem_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  gui_pause_capture ();

}				/* on_pause_menuitem_activate */

void
on_stop_menuitem_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  gui_stop_capture ();

}				/* on_stop_menuitem_activate */

/* Saves the history of a gnome_entry making sure there are no duplicates */
void
update_history (GnomeEntry * gentry, const gchar * str, gboolean is_fileentry)
{
  GList *entry_items = NULL;
  gboolean is_new = TRUE, normal_case = TRUE;

  /* The combo fills up even if there are duplicate values. I think
   * this is a bug in gnome and might be fixed in the future, but
   * right now I'll just make sure the history is fine and reload that*/

  /* TODO There really should be a better way of getting all values from
   * the drop down list than this casting mess :-( */

  /* The file entry introduces a copy of the file name at the end of the
   * list if the browse button has been used. There goes another hack to
   * work around it */
  entry_items = GTK_LIST (GTK_COMBO (gentry)->list)->children;
  while (entry_items)
    {
      gchar *history_value;
      history_value = GTK_LABEL (GTK_BIN (entry_items->data)->child)->label;
      if (!strcmp (history_value, str) && !is_fileentry
	  && (entry_items->next != NULL))
	is_new = FALSE;
      normal_case = !(is_fileentry && !(entry_items->next));
      entry_items = g_list_next (entry_items);
    }

  if (is_new)
    {
      g_my_info ("New entry history item: %s", str);
      if (normal_case)
	gnome_entry_prepend_history (gentry, TRUE, str);
      gnome_entry_save_history (gentry);
    }

  gnome_entry_load_history (gentry);
}				/* update_history */

/* Sets up the GUI to reflect changes and calls start_capture() */
void
gui_start_capture (void)
{
  GtkWidget *widget;
  gchar *errorbuf = NULL;

  if (status == STOP)
    if ((errorbuf = init_capture ()) != NULL)
      {
	fatal_error_dialog (errorbuf);
	return;
      }

  if (status == PLAY)
    {
      g_warning (_("Status not STOP or PAUSE at gui_start_capture"));
      return;
    }

  if (!start_capture ())
    return;

   
  widget = glade_xml_get_widget (xml, "stop_button");
  gtk_widget_set_sensitive (widget, TRUE);
  widget = glade_xml_get_widget (xml, "stop_menuitem");
  gtk_widget_set_sensitive (widget, TRUE);
  widget = glade_xml_get_widget (xml, "start_button");
  gtk_widget_set_sensitive (widget, FALSE);
  widget = glade_xml_get_widget (xml, "start_menuitem");
  gtk_widget_set_sensitive (widget, FALSE);
  widget = glade_xml_get_widget (xml, "pause_button");
  gtk_widget_set_sensitive (widget, TRUE);
  widget = glade_xml_get_widget (xml, "pause_menuitem");
  gtk_widget_set_sensitive (widget, TRUE);

  g_my_info (_("Diagram started"));
}				/* gui_start_capture */

void
gui_pause_capture (void)
{
  GtkWidget *widget;

  if (status == PAUSE)
    {
      g_warning (_("Status not PLAY at gui_pause_capture"));
      return;
    }
   
  if (!pause_capture ())
    return;

  widget = glade_xml_get_widget (xml, "stop_button");
  gtk_widget_set_sensitive (widget, TRUE);
  widget = glade_xml_get_widget (xml, "stop_menuitem");
  gtk_widget_set_sensitive (widget, TRUE);
  widget = glade_xml_get_widget (xml, "start_button");
  gtk_widget_set_sensitive (widget, TRUE);
  widget = glade_xml_get_widget (xml, "start_menuitem");
  gtk_widget_set_sensitive (widget, TRUE);
  widget = glade_xml_get_widget (xml, "pause_button");
  gtk_widget_set_sensitive (widget, FALSE);
  widget = glade_xml_get_widget (xml, "pause_menuitem");
  gtk_widget_set_sensitive (widget, FALSE);
   
  g_my_info (_("Diagram paused"));

}				/* gui_pause_capture */


/* Sets up the GUI to reflect changes and calls stop_capture() */
void
gui_stop_capture (void)
{
  GtkWidget *widget;

  if (status == STOP)
    {
      g_warning (_("Status not PLAY or PAUSE at gui_stop_capture"));
      return;
    }

  if (!stop_capture ())
    return;

  widget = glade_xml_get_widget (xml, "start_button");
  gtk_widget_set_sensitive (widget, TRUE);
  widget = glade_xml_get_widget (xml, "start_menuitem");
  gtk_widget_set_sensitive (widget, TRUE);
  widget = glade_xml_get_widget (xml, "stop_button");
  gtk_widget_set_sensitive (widget, FALSE);
  widget = glade_xml_get_widget (xml, "stop_menuitem");
  gtk_widget_set_sensitive (widget, FALSE);
  widget = glade_xml_get_widget (xml, "pause_button");
  gtk_widget_set_sensitive (widget, FALSE);
  widget = glade_xml_get_widget (xml, "pause_menuitem");
  gtk_widget_set_sensitive (widget, FALSE);


  /* By calling update_diagram, we are forcing node_update
   * and link_update, thus deleting all nodes and links since
   * status=STOP. Then the diagram is redrawn */
  widget = glade_xml_get_widget (xml, "canvas1");
  g_my_info ("Antes del update diagram");
  update_diagram (widget);
  g_my_info ("Después del update diagram");

  g_my_info (_("Diagram stopped"));
}				/* gui_stop_capture */

void
fatal_error_dialog (const gchar * message)
{
  GtkWidget *error_messagebox;

  /* TODO I am not very sure I am not leaking with this, but this is 
   * extraordinary and would hardly be noticeable */

  error_messagebox = gnome_message_box_new (message,
					    GNOME_MESSAGE_BOX_ERROR,
					    GNOME_STOCK_BUTTON_OK, NULL);
  gtk_widget_show (error_messagebox);

}				/* fatal_error_dialog */
