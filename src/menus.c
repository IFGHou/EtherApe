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

#include <string.h>
#include <gtk/gtkcontainer.h>
#include <glib.h>
#include "menus.h"
#include "util.h"
#include "diagram.h"
#include "info_windows.h"
#include "capture.h"


static gboolean in_start_capture = FALSE;

static void set_active_interface (void);

void
init_menus (void)
{
  GtkWidget *widget = NULL, *item = NULL;
  GList *interfaces;
  GSList *group = NULL;
  GString *info_string = NULL;
  GString *err_str = g_string_new ("");

  interfaces = interface_list_create(err_str);
  g_my_info (_("get_interface result: '%s'"), err_str->str);
  if (!interfaces)
    {
      g_my_info (_("No suitables interfaces for capture have been found"));
      g_string_free(err_str, TRUE);
      return;
    }
  g_string_free(err_str, TRUE);

  widget = glade_xml_get_widget (xml, "interfaces_menu");

  info_string = g_string_new (_("Available interfaces for capture:"));

  /* Set up a hidden dummy interface to set when there is no active
   * interface */
  item = gtk_radio_menu_item_new_with_label (group, "apedummy");
  group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));
  gtk_menu_shell_append (GTK_MENU_SHELL (widget), item);

  /* Set up the real interfaces menu entries */
  while (interfaces)
    {
      item =
	gtk_radio_menu_item_new_with_label (group,
					    (gchar *) (interfaces->data));
      group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));
      gtk_menu_shell_append (GTK_MENU_SHELL (widget), item);
      gtk_widget_show (item);
      g_signal_connect_swapped (G_OBJECT (item), "activate",
				GTK_SIGNAL_FUNC
				(on_interface_radio_activate),
				(gpointer) g_strdup (interfaces->data));
      g_string_append (info_string, " ");
      g_string_append (info_string, (gchar *) (interfaces->data));
      interfaces = interfaces->next;
    }

  g_my_info (info_string->str);
  g_string_free(info_string, TRUE);

  interface_list_free(interfaces);
}				/* init_menus */

/* FILE MENU */

void
on_open_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  GtkWidget *dialog;

  if (!gui_stop_capture ())
    return;

  dialog = gtk_file_chooser_dialog_new ("Open Capture File",
				      NULL,
				      GTK_FILE_CHOOSER_ACTION_OPEN,
				      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				      NULL);
  if (pref.input_file)
    gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), pref.input_file);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
      GtkRecentManager *manager;
      manager = gtk_recent_manager_get_default ();
      gtk_recent_manager_add_item (manager, gtk_file_chooser_get_uri(GTK_FILE_CHOOSER (dialog)));

      g_free(pref.input_file);
      pref.input_file = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
      gtk_widget_destroy (dialog);

      g_free (pref.interface);
      pref.interface = NULL;
      
      gui_start_capture ();
    }
  else
    gtk_widget_destroy (dialog);
}				/* on_open_activate */

/* Capture menu */

void
on_interface_radio_activate (gchar * gui_device)
{
  g_assert (gui_device != NULL);

  if (pref.interface && !strcmp (gui_device, pref.interface))
    return;

  if (in_start_capture)
    return;			/* Disregard when called because
				 * of interface look change from
				 * start_capture */

  if (!gui_stop_capture ())
     return;

  if (pref.input_file)
    g_free (pref.input_file);
  pref.input_file = NULL;

  if (pref.interface)
    g_free (pref.interface);
  pref.interface = g_strdup (gui_device);

  gui_start_capture ();

  g_my_info (_("Capture interface set to %s in GUI"), gui_device);
}				/* on_interface_radio_activate */

void
on_mode_radio_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  apemode_t new_mode = DEFAULT;
  const gchar *menuname = NULL;


  if (in_start_capture)
    return;			/* Disregard when called because
				 * of interface look change from
				 * start_capture */

  menuname = gtk_widget_get_name (GTK_WIDGET (menuitem));
  g_assert (menuname);
  g_my_debug ("Initial mode in on_mode_radio_activate %s",
	      (gchar *) menuname);

  if (!strcmp ("ieee802_radio", menuname))
    new_mode = IEEE802;
  else if (!strcmp ("fddi_radio", menuname))
    new_mode = FDDI;
  else if (!strcmp ("ethernet_radio", menuname))
    new_mode = ETHERNET;
  else if (!strcmp ("ip_radio", menuname))
    new_mode = IP;
  else if (!strcmp ("tcp_radio", menuname))
    new_mode = TCP;
  else if (!strcmp ("udp_radio", menuname))
    new_mode = UDP;
  else
    {
      g_my_critical ("Unsopported mode in on_mode_radio_activate");
      exit (1);
    }

  if (new_mode == pref.mode)
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
      return;
    }

  if (!gui_stop_capture ())
     return;
  pref.mode = new_mode;
  g_my_info (_("Mode set to %s in GUI"), (gchar *) menuitem);
  gui_start_capture ();

}				/* on_mode_radio_activate */

void
on_start_menuitem_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  g_my_debug ("on_start_menuitem_activate called");
  gui_start_capture ();
}				/* on_start_menuitem_activate */

void
on_pause_menuitem_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  g_my_debug ("on_pause_menuitem_activate called");
  gui_pause_capture ();

}				/* on_pause_menuitem_activate */

void
on_stop_menuitem_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  g_my_debug ("on_stop_menuitem_activate called");
  gui_stop_capture ();
}				/* on_stop_menuitem_activate */



/* View menu */

void
on_full_screen_activate (GtkCheckMenuItem * menuitem, gpointer user_data)
{
  if (menuitem->active)
    gtk_window_fullscreen((GtkWindow *)app1);
  else
    gtk_window_unfullscreen((GtkWindow *)app1);
} /* on_full_screen_activate */


void
on_toolbar_check_activate (GtkCheckMenuItem * menuitem, gpointer user_data)
{
  GtkWidget *widget;

  widget = glade_xml_get_widget (xml, "handlebox_toolbar");
  if (menuitem->active)
    gtk_widget_show (widget);
  else
    gtk_widget_hide (widget);
}				/* on_toolbar_check_activate */

void
on_legend_check_activate (GtkCheckMenuItem * menuitem, gpointer user_data)
{
  GtkWidget *widget;

  widget = glade_xml_get_widget (xml, "handlebox_legend");
  if (menuitem->active)
    gtk_widget_show (widget);
  else
    gtk_widget_hide (widget);
}				/* on_legend_check_activate */

void
on_status_bar_check_activate (GtkCheckMenuItem * menuitem, gpointer user_data)
{
  if (menuitem->active)
    gtk_widget_show (GTK_WIDGET(statusbar));
  else
    gtk_widget_hide (GTK_WIDGET(statusbar));
}				/* on_status_bar_check_activate */



/* Help menu */



void
on_about1_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  GtkWidget *about;
  about = glade_xml_get_widget (xml, "about1");

  gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(about), VERSION);

  gtk_widget_show (about);
}				/* on_about1_activate */


/* Helper functions */

/* Sets up the GUI to reflect changes and calls start_capture() */
void
gui_start_capture (void)
{
  GtkWidget *widget;
  gchar *errorbuf = NULL;
  GString *status_string = NULL;

  if (get_capture_status() == CAP_EOF)
    if (!gui_stop_capture ())
      return;

  if (get_capture_status() == STOP)
    if ((errorbuf = init_capture ()) != NULL)
      {
	fatal_error_dialog (errorbuf);
	return;
      }

  if (get_capture_status() == PLAY)
    {
      g_warning (_("Status already PLAY at gui_start_capture"));
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
#ifdef DLT_LINUX_SLL
    case L_LINUX_SLL:
#endif
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
#ifdef DLT_IEEE802_11
    case L_IEEE802_11:
#endif
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
      g_warning (_("Invalid link type %d"), linktype);
      return;
    }

  /* Set active mode in GUI */

  switch (pref.mode)
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
      g_warning (_("Invalid mode: %d"), pref.mode);
      return;
    }
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget), TRUE);

  /* Set the interface in GUI */
  set_active_interface ();

  /* Sets the statusbar */
  status_string = g_string_new (_("Reading data from "));

  if (pref.input_file)
    g_string_append (status_string, pref.input_file);
  else if (pref.interface)
    g_string_append (status_string, pref.interface);
  else
    g_string_append (status_string, _("default interface"));

  switch (pref.mode)
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
      g_critical (_("Invalid mode: %d"), pref.mode);
      return;
    }

  set_statusbar_msg (status_string->str);
  g_string_free (status_string, TRUE);

  in_start_capture = FALSE;

  g_my_info (_("Diagram started"));
}				/* gui_start_capture */

void
gui_pause_capture (void)
{
  GtkWidget *widget;
  GString *status_string = NULL;

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

  set_statusbar_msg (_("Paused"));

  g_my_info (_("Diagram paused"));

}				/* gui_pause_capture */


/* reached eof on a file replay */
void gui_eof_capture(void)
{
  GtkWidget *widget;
  GString *status_string = NULL;

  if (get_capture_status() == STOP)
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

  /* Sets the statusbar */
  status_string = g_string_new ("");
  g_string_printf(status_string, _("Replay from file '%s' completed."), pref.input_file);
  set_statusbar_msg (status_string->str);
  g_string_free (status_string, TRUE);

  g_my_info (_("Diagram stopped"));
}				/* gui_stop_capture */


/* Sets up the GUI to reflect changes and calls stop_capture() */
gboolean
gui_stop_capture (void)
{
  GtkWidget *widget;
  GString *status_string = NULL;

  if (get_capture_status() == STOP)
    return TRUE;

  /*
   * gui_stop_capture needs to call update_diagram in order to
   * delete all canvas_nodes and nodes. But since a slow running
   * update_diagram will yield to pending events, gui_stop_capture
   * might end up being called below another update_diagram. We can't
   * allow two simultaneous calls, so we fail
   */
  if (already_updating)
    return FALSE;

  if (!stop_capture ())
    return FALSE;

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

  widget = glade_xml_get_widget (xml, "canvas1");
  update_diagram (widget);

  /* Now update info windows */
  update_info_windows ();

  /* Sets the statusbar */
  status_string = g_string_new (_("Ready to capture from "));

  if (pref.input_file)
    g_string_append (status_string, pref.input_file);
  else if (pref.interface)
    g_string_append (status_string, pref.interface);
  else
    g_string_append (status_string, _("default interface"));

  set_statusbar_msg (status_string->str);
  g_string_free (status_string, TRUE);

  g_my_info (_("Diagram stopped"));

  return TRUE;
}				/* gui_stop_capture */

void
fatal_error_dialog (const gchar * message)
{
  GtkWidget *error_messagebox;

  error_messagebox = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
					     GTK_MESSAGE_ERROR,
					     GTK_BUTTONS_OK, message);
  gtk_dialog_run (GTK_DIALOG (error_messagebox));
  gtk_widget_destroy (error_messagebox);

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

      if (pref.input_file)
	{
	  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget), TRUE);
	  return;
	}

      label = GTK_LABEL (GTK_BIN (widget)->child)->label;

      if (!strcmp (label, pref.interface))
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget), TRUE);

      menu_items = menu_items->next;
    }

}				/* set_active_interface */
