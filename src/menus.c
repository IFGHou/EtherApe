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
  GtkWidget *widget;
  if (start_capture ())
    {
      widget = glade_xml_get_widget (xml, "stop_button");
      gtk_widget_set_sensitive (widget, TRUE);
      widget = glade_xml_get_widget (xml, "stop_menuitem");
      gtk_widget_set_sensitive (widget, TRUE);
      widget = glade_xml_get_widget (xml, "start_button");
      gtk_widget_set_sensitive (widget, FALSE);
      widget = glade_xml_get_widget (xml, "start_menuitem");
      gtk_widget_set_sensitive (widget, FALSE);
    }
}

void
on_stop_menuitem_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  GtkWidget *widget;
  if (stop_capture ())
    {
      widget = glade_xml_get_widget (xml, "start_button");
      gtk_widget_set_sensitive (widget, TRUE);
      widget = glade_xml_get_widget (xml, "start_menuitem");
      gtk_widget_set_sensitive (widget, TRUE);
      widget = glade_xml_get_widget (xml, "stop_button");
      gtk_widget_set_sensitive (widget, FALSE);
      widget = glade_xml_get_widget (xml, "stop_menuitem");
      gtk_widget_set_sensitive (widget, FALSE);
    }
}
