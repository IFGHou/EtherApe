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
      if (err_str)
         g_string_free(err_str, TRUE);
      return;
    }
  if (err_str)
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

  g_my_info ("%s", info_string->str);
  if (info_string)
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

void
on_export_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  GtkWidget *dialog;

  dialog = gtk_file_chooser_dialog_new ("Export to XML File",
				      NULL,
				      GTK_FILE_CHOOSER_ACTION_SAVE,
				      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				      GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
				      NULL);
  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);

  if (pref.export_file)
    gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), pref.export_file);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
      GtkRecentManager *manager;
      manager = gtk_recent_manager_get_default ();
      gtk_recent_manager_add_item (manager, gtk_file_chooser_get_uri(GTK_FILE_CHOOSER (dialog)));

      g_free(pref.export_file);
      pref.export_file = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
      gtk_widget_destroy (dialog);

      dump_xml(pref.export_file);
    }
  else
    gtk_widget_destroy (dialog);
}

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
  apemode_t new_mode = APEMODE_DEFAULT;
  const gchar *menuname = NULL;
  gchar *filter;

  if (in_start_capture)
    return;			/* Disregard when called because
				 * of interface look change from
				 * start_capture */

  menuname = gtk_widget_get_name (GTK_WIDGET (menuitem));
  g_assert (menuname);
  g_my_debug ("Initial mode in on_mode_radio_activate %s",
	      (gchar *) menuname);

  if (!strcmp ("link_radio", menuname))
    new_mode = LINK6;
  else if (!strcmp ("ip_radio", menuname))
    new_mode = IP;
  else if (!strcmp ("tcp_radio", menuname))
    new_mode = TCP;
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

  if (!has_linklevel() && new_mode == LINK6)
    return;

  if (!gui_stop_capture ())
     return;

  /* if old filter was still default, we can change to default of new mode */
  filter = get_default_filter(pref.mode);
  if (!strcmp(pref.filter, filter))
    {
      g_free(pref.filter);
      pref.filter = get_default_filter(new_mode);
    }
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
  gchar *msg;
  about = glade_xml_get_widget (xml, "about1");

  gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(about), VERSION);
#ifdef PACKAGE_SCM_REV
  msg = g_strdup_printf("HG revision: %s", 
                        (*PACKAGE_SCM_REV) ? PACKAGE_SCM_REV : _("-unknown-"));
  gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(about), msg);
  g_free(msg);
#endif
  gtk_widget_show (about);
}				/* on_about1_activate */


void
on_help_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  GError *err = NULL;
  gboolean success = FALSE;

#if GTK_CHECK_VERSION(2, 13, 1)
  success = gtk_show_uri (NULL, "ghelp:" PACKAGE_NAME, GDK_CURRENT_TIME, &err);
#else
  success = gnome_help_display (PACKAGE_NAME ".xml", NULL, &err);
#endif
}

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

  /* Enable and disable link layer menu */
  widget = glade_xml_get_widget (xml, "link_radio");
  if (!has_linklevel())
    gtk_widget_set_sensitive (widget, FALSE);
  else
    gtk_widget_set_sensitive (widget, TRUE);

  /* Set active mode in GUI */
  switch (pref.mode)
    {
    case LINK6:
      widget = glade_xml_get_widget (xml, "link_radio");
      break;
    case IP:
      widget = glade_xml_get_widget (xml, "ip_radio");
      break;
    case TCP:
      widget = glade_xml_get_widget (xml, "tcp_radio");
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
    case LINK6:
      g_string_append (status_string, _(" in Data Link mode"));
      break;
    case IP:
      g_string_append (status_string, _(" in IP mode"));
      break;
    case TCP:
      g_string_append (status_string, _(" in TCP mode"));
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
  dump_stats(0);
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
/*  widget = glade_xml_get_widget (xml, "stop_button");
  gtk_widget_set_sensitive (widget, FALSE);
  widget = glade_xml_get_widget (xml, "stop_menuitem");
  gtk_widget_set_sensitive (widget, FALSE);*/
  widget = glade_xml_get_widget (xml, "pause_button");
  gtk_widget_set_sensitive (widget, FALSE);
  widget = glade_xml_get_widget (xml, "pause_menuitem");
  gtk_widget_set_sensitive (widget, FALSE);

  /* Sets the statusbar */
  status_string = g_string_new ("");
  g_string_printf(status_string, _("Replay from file '%s' completed."), pref.input_file);
  set_statusbar_msg (status_string->str);
  g_string_free (status_string, TRUE);
}				/* gui_stop_capture */


/* Sets up the GUI to reflect changes and calls stop_capture() */
gboolean
gui_stop_capture (void)
{
  GtkWidget *widget;
  GString *status_string = NULL;

  stop_requested = FALSE;
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
    {
      stop_requested = TRUE;
      return FALSE;
    }

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
  dump_stats(0);
  return TRUE;
}				/* gui_stop_capture */

void
fatal_error_dialog (const gchar * message)
{
  GtkWidget *error_messagebox;

  error_messagebox = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
					     GTK_MESSAGE_ERROR,
					     GTK_BUTTONS_OK, "%s", message);
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

void dump_xml(gchar *ofile)
{
  FILE *fout;
  gchar *xml;
  gchar *oldlocale;
  
  if (!ofile)
    return;

  // we want to dump in a known locale, so force it as 'C'
  oldlocale = g_strdup(setlocale(LC_ALL, NULL));
  setlocale(LC_ALL, "C");
  
  xml = nodes_catalog_xml();
  fout = fopen(ofile, "wb");
  if (fout)
    {
      fprintf(fout, "<?xml version=\"1.0\"?>\n");
      fprintf(fout, "<!-- traffic data in bytes. last_heard in seconds from dump time -->\n", xml);
      fprintf(fout, "<etherape>\n%s</etherape>", xml);
      fclose(fout);
    }

  // reset user locale
  setlocale(LC_ALL, oldlocale);

  g_free(xml);
  g_free(oldlocale);
}
