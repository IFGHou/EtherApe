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
init_menus (void)
{
  GtkWidget *widget = NULL, *item = NULL;
  gint err;
  gchar err_str[300];
  GList *interfaces;
  GSList *group = NULL;
  GString *info_string = NULL;

  /* It seems libglade is not acknowledging the "Use gnome help" option in the 
   * glade file and so it is not automatically adding the help items in the help
   * menu. Thus I must add it manually here */

  widget = glade_xml_get_widget (xml, "help1_menu");
  gnome_app_fill_menu ((GtkMenuShell *) widget, help_submenu,
		       gtk_accel_group_get_default (), TRUE, 1);

  interface_list = get_interface_list (&err, err_str);

  interfaces = interface_list;

  if (!interfaces)
    {
      g_my_info (_("No suitables interfaces for capture have been found"));
      return;
    }

  widget = glade_xml_get_widget (xml, "interfaces_menu");

  info_string = g_string_new (_("Available interfaces for capture:"));

  /* Set up a hidden dummy interface to set when there is no active
   * interface */
  item = gtk_radio_menu_item_new_with_label (group, "apedummy");
  group = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (item));
  gtk_menu_append (GTK_MENU (widget), item);

  /* Set up the real interfaces menu entries */
  while (interfaces)
    {
      item =
	gtk_radio_menu_item_new_with_label (group,
					    (gchar *) (interfaces->data));
      group = gtk_radio_menu_item_group (GTK_RADIO_MENU_ITEM (item));
      gtk_menu_append (GTK_MENU (widget), item);
      gtk_widget_show (item);
      gtk_signal_connect_object (GTK_OBJECT (item), "activate",
				 GTK_SIGNAL_FUNC
				 (on_interface_radio_activate),
				 (gpointer) g_strdup (interfaces->data));
      g_string_append (info_string, " ");
      g_string_append (info_string, (gchar *) (interfaces->data));
      interfaces = interfaces->next;
    }


  g_my_info (info_string->str);

}				/* init_menus */

/* FILE MENU */

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

  input_file = g_strdup (str);
  g_free (str);

}				/* on_file_combo_entry_changed */

void
on_file_ok_button_clicked (GtkButton * button, gpointer user_data)
{
  GtkWidget *widget;
  gchar *str = NULL;

  if (status != STOP)
    gui_stop_capture ();

  if (interface)
    {
      g_free (interface);
      interface = NULL;
    }

  widget = glade_xml_get_widget (xml, "file_combo_entry");
  str = gtk_editable_get_chars (GTK_EDITABLE (widget), 0, -1);

  widget = glade_xml_get_widget (xml, "open_file_dialog");
  gtk_widget_hide (widget);


  if (input_file)
    g_free (input_file);
  input_file = g_strdup (str);
  if (str)
    g_free (str);

  widget = glade_xml_get_widget (xml, "fileentry");
  update_history (GNOME_ENTRY
		  (gnome_file_entry_gnome_entry (GNOME_FILE_ENTRY (widget))),
		  input_file, TRUE);

  gui_start_capture ();

}				/* on_file_ok_button_clicked */


void
on_exit1_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  gtk_exit (0);
}				/* on_exit1_activate */



/* Capture menu */

void
on_interface_radio_activate (gchar * gui_device)
{
  g_assert (gui_device != NULL);

  if (interface && !strcmp (gui_device, interface))
    return;

  if (in_start_capture)
    return;			/* Disregard when called because
				 * of interface look change from
				 * start_capture */

  if (status != STOP)
    gui_stop_capture ();

  if (input_file)
    g_free (input_file);
  input_file = NULL;

  if (interface)
    g_free (interface);
  interface = g_strdup (gui_device);

  gui_start_capture ();

  g_my_info (_("Capture interface set to %s in GUI"), gui_device);
}				/* on_interface_radio_activate */

void
on_mode_radio_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  apemode_t new_mode = DEFAULT;

  g_assert (user_data != NULL);

  if (in_start_capture)
    return;			/* Disregard when called because
				 * of interface look change from
				 * start_capture */

  g_my_debug ("Initial mode in on_mode_radio_activate %s",
	      (gchar *) user_data);

  if (!strcmp ("IEEE802", user_data))
    new_mode = IEEE802;
  else if (!strcmp ("FDDI", user_data))
    new_mode = FDDI;
  else if (!strcmp ("Ethernet", user_data))
    new_mode = ETHERNET;
  else if (!strcmp ("IP", user_data))
    new_mode = IP;
  else if (!strcmp ("TCP", user_data))
    new_mode = TCP;
  else if (!strcmp ("UDP", user_data))
    new_mode = UDP;
  else
    {
      g_my_critical ("Unsopported mode in on_mode_radio_activate");
      exit (1);
    }

  if (new_mode == mode)
    return;

  /* I don't know why, but this menu item is called twice, instead
   * of once. This forces me to make sure we are not trying to set
   * anything impossible */

  g_my_debug ("Mode menuitem active: %d",
	      GTK_CHECK_MENU_ITEM (menuitem)->active);

  switch (linktype)
    {
    case L_NULL:
    case L_RAW:
      if ((new_mode == ETHERNET) || (new_mode == FDDI)
	  || (new_mode == IEEE802))
	return;
      break;
    case L_EN10MB:
      if ((new_mode == FDDI) || (new_mode == IEEE802))
	return;
      break;
    case L_FDDI:
      if (new_mode == ETHERNET || (new_mode == IEEE802))
	return;
      break;
    case L_IEEE802:
      if (new_mode == ETHERNET || (new_mode == FDDI))
	return;
      break;
    default:
    }
  if (status != STOP)
    gui_stop_capture ();
  mode = new_mode;
  g_my_info (_("Mode set to %s in GUI"), (gchar *) user_data);
  gui_start_capture ();

}				/* on_mode_radio_activate */

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



/* View menu */



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



/* Help menu */



void
on_about1_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  GtkWidget *about;
  GladeXML *xml_about;

  xml_about = glade_xml_new (GLADEDIR "/" ETHERAPE_GLADE_FILE, "about2");
  if (!xml_about)
    {
      g_error (_("We could not load the interface! (%s)"),
	       GLADEDIR "/" ETHERAPE_GLADE_FILE);
      return;
    }
  about = glade_xml_get_widget (xml_about, "about2");
  gtk_object_destroy (GTK_OBJECT (xml_about));

  gtk_widget_show (about);

}				/* on_about1_activate */



/* Helper functions */



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
  GString *status_string = NULL;

  if ((status == PAUSE) && end_of_file)
    gui_stop_capture ();

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

  in_start_capture = TRUE;

  /* Enable and disable control buttons */
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

  /* Enable and disable mode buttons */

  switch (linktype)
    {
    case L_NULL:
    case L_RAW:
      widget = glade_xml_get_widget (xml, "ieee802_radio");
      gtk_widget_set_sensitive (widget, FALSE);
      widget = glade_xml_get_widget (xml, "fddi_radio");
      gtk_widget_set_sensitive (widget, FALSE);
      widget = glade_xml_get_widget (xml, "ethernet_radio");
      gtk_widget_set_sensitive (widget, FALSE);
      break;
    case L_FDDI:
      widget = glade_xml_get_widget (xml, "fddi_radio");
      gtk_widget_set_sensitive (widget, TRUE);
      widget = glade_xml_get_widget (xml, "ethernet_radio");
      gtk_widget_set_sensitive (widget, FALSE);
      widget = glade_xml_get_widget (xml, "ieee802_radio");
      gtk_widget_set_sensitive (widget, FALSE);
      break;
    case L_EN10MB:
      widget = glade_xml_get_widget (xml, "fddi_radio");
      gtk_widget_set_sensitive (widget, FALSE);
      widget = glade_xml_get_widget (xml, "ethernet_radio");
      gtk_widget_set_sensitive (widget, TRUE);
      widget = glade_xml_get_widget (xml, "ieee802_radio");
      gtk_widget_set_sensitive (widget, FALSE);
      break;
    case L_IEEE802:
      widget = glade_xml_get_widget (xml, "fddi_radio");
      gtk_widget_set_sensitive (widget, FALSE);
      widget = glade_xml_get_widget (xml, "ethernet_radio");
      gtk_widget_set_sensitive (widget, FALSE);
      widget = glade_xml_get_widget (xml, "ieee802_radio");
      gtk_widget_set_sensitive (widget, TRUE);
      break;
    default:
    }

  /* Set active mode in GUI */

  switch (mode)
    {
    case IEEE802:
      widget = glade_xml_get_widget (xml, "ieee802_radio");
      break;
    case FDDI:
      widget = glade_xml_get_widget (xml, "fddi_radio");
      break;
    case ETHERNET:
      widget = glade_xml_get_widget (xml, "ethernet_radio");
      break;
    case IP:
      widget = glade_xml_get_widget (xml, "ip_radio");
      break;
    case TCP:
      widget = glade_xml_get_widget (xml, "tcp_radio");
      break;
    case UDP:
      widget = glade_xml_get_widget (xml, "udp_radio");
      break;
    default:
    }
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget), TRUE);

  /* Set the interface in GUI */
  set_active_interface ();

  /* Sets the appbar */

  status_string = g_string_new (_("Reading data from "));

  if (input_file)
    g_string_append (status_string, input_file);
  else if (interface)
    g_string_append (status_string, interface);
  else
    g_string_append (status_string, _("default interface"));

  switch (mode)
    {
    case IEEE802:
      g_string_append (status_string, _(" in Token Ring mode"));
      break;
    case FDDI:
      g_string_append (status_string, _(" in FDDI mode"));
      break;
    case ETHERNET:
      g_string_append (status_string, _(" in Ethernet mode"));
      break;
    case IP:
      g_string_append (status_string, _(" in IP mode"));
      break;
    case TCP:
      g_string_append (status_string, _(" in TCP mode"));
      break;
    case UDP:
      g_string_append (status_string, _(" in UDP mode"));
      break;
    default:
    }

  set_appbar_status (status_string->str);
  g_string_free (status_string, TRUE);

  in_start_capture = FALSE;

  g_my_info (_("Diagram started"));
}				/* gui_start_capture */



void
gui_pause_capture (void)
{
  GtkWidget *widget;
  GString *status_string = NULL;

  if (status == PAUSE)
    {
      g_warning (_("Status not PLAY at gui_pause_capture"));
      return;
    }

  /*
   * Make sure the data in the info windows is updated
   * so that it is consistent
   */
  update_info_windows ();

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


  /* Sets the appbar */

  status_string = g_string_new (_("Paused"));

  set_appbar_status (status_string->str);
  g_string_free (status_string, TRUE);

  g_my_info (_("Diagram paused"));

}				/* gui_pause_capture */


/* Sets up the GUI to reflect changes and calls stop_capture() */
void
gui_stop_capture (void)
{
  GtkWidget *widget;
  GString *status_string = NULL;

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

  /* Delete and free protocol information */
  delete_gui_protocols ();

  /* By calling update_diagram, we are forcing node_update
   * and link_update, thus deleting all nodes and links since
   * status=STOP. Then the diagram is redrawn */
  widget = glade_xml_get_widget (xml, "canvas1");
  update_diagram (widget);

  /* Now update info windows */
  update_info_windows ();

  /* Sets the appbar */

  status_string = g_string_new (_("Ready to capture from "));

  if (input_file)
    g_string_append (status_string, input_file);
  else if (interface)
    g_string_append (status_string, interface);
  else
    g_string_append (status_string, _("default interface"));

  set_appbar_status (status_string->str);
  g_string_free (status_string, TRUE);

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

void
set_active_interface ()
{
  GtkWidget *widget;
  GList *menu_items = NULL;
  gchar *label;


  widget = glade_xml_get_widget (xml, "interfaces_menu");

  menu_items = GTK_MENU_SHELL (widget)->children;

  while (menu_items)
    {
      widget = (GtkWidget *) (menu_items->data);

      if (input_file)
	{
	  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget), TRUE);
	  return;
	}

      label = GTK_LABEL (GTK_BIN (widget)->child)->label;

      if (!strcmp (label, interface))
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget), TRUE);

      menu_items = menu_items->next;
    }

#if 0
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
#endif

}				/* set_active_interface */
