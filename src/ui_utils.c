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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>
#include "globals.h"
#include "ui_utils.h"
#include "traffic_stats.h"


/* 
  
  Helper functions 

*/

/* returns a newly allocated string with a timeval in human readable form */
gchar *timeval_to_str (struct timeval last_heard)
{
  gchar *str;
  struct timeval diff;
  struct tm broken_time;

  diff = substract_times (now, last_heard);
  if (!localtime_r ((time_t *) & (last_heard.tv_sec), &broken_time))
    {
      g_my_critical ("Time conversion failed in timeval_to_str");
      return NULL;
    }

  if (diff.tv_sec <= 60)
    {
      /* Meaning "n seconds" ago */
      str = g_strdup_printf (_("%d\" ago"), (int) diff.tv_sec);
    }
  else if (diff.tv_sec < 600)
    {
      /* Meaning "m minutes, n seconds ago" */
      str =
	g_strdup_printf (_("%d'%d\" ago"),
			 (int) floor ((double) diff.tv_sec / 60),
			 (int) diff.tv_sec % 60);
    }
  else if (diff.tv_sec < 3600 * 24)
    {
      str =
	g_strdup_printf ("%d:%d", broken_time.tm_hour, broken_time.tm_min);
    }
  else
    {
      /* Watch out! The first is month, the second day of the month */
      str = g_strdup_printf (_("%d/%d %d:%d"),
			     broken_time.tm_mon, broken_time.tm_mday,
			     broken_time.tm_hour, broken_time.tm_min);
    }

  return str;
}				/* timeval_to_str */

/* registers the named glade widget on the specified object */
void register_glade_widget(GladeXML *xm, GObject *tgt, const gchar *widgetName)
{
  GtkWidget *widget;
  widget = glade_xml_get_widget (xm, widgetName);
  g_object_set_data (tgt, widgetName, widget);
}

void update_gtklabel(GtkWidget *window, const gchar *lblname, const gchar *value)
{
  GtkWidget *widget = g_object_get_data (G_OBJECT (window), lblname);
  gtk_label_set_text (GTK_LABEL (widget), value);
}


void show_widget(GtkWidget *window, const gchar *lblname)
{
  GtkWidget *widget = g_object_get_data (G_OBJECT (window), lblname);
  gtk_widget_show(widget);
}
void hide_widget(GtkWidget *window, const gchar *lblname)
{
  GtkWidget *widget = g_object_get_data (G_OBJECT (window), lblname);
  gtk_widget_hide(widget);
}


/* creates a new text column with a specific title, column number colno and
 * adds it to treeview gv.  If r_just true the column is right justified */
void create_add_text_column(GtkTreeView *gv, const gchar *title, int colno, 
                            gboolean r_just)
{
  GtkTreeViewColumn *gc;
  GtkCellRenderer *gr;

  gr = gtk_cell_renderer_text_new ();
  if (r_just)
    g_object_set (G_OBJECT (gr), "xalign", 1.0, NULL);
  
  gc = gtk_tree_view_column_new_with_attributes(title, gr, "text", colno, NULL);
  g_object_set (G_OBJECT (gc), "resizable", TRUE, 
                               "reorderable", TRUE, 
                               NULL);
  gtk_tree_view_column_set_sort_column_id(gc, colno);
  gtk_tree_view_append_column (gv, gc);
}

/* returns a newly allocated string with a formatted traffic  */
gchar *traffic_to_str (gdouble traffic, gboolean is_speed)
{
  gchar *str;
  if (is_speed)
    {
      if (traffic > 1000000)
	str = g_strdup_printf ("%.3f Mbps", traffic / 1000000);
      else if (traffic > 1000)
	str = g_strdup_printf ("%.3f Kbps", traffic / 1000);
      else
	str = g_strdup_printf ("%.0f bps", traffic);
    }
  else
    {
      /* Debug code for sanity check */
      if (traffic && traffic < 1)
	g_warning ("Ill traffic value in traffic_to_str");

      if (traffic > 1024 * 1024)
	str = g_strdup_printf ("%.3f Mbytes", traffic / 1024 / 1024);
      else if (traffic > 1024)
	str = g_strdup_printf ("%.3f Kbytes", traffic / 1024);
      else
	str = g_strdup_printf ("%.0f bytes", traffic);
    }

  return str;
}				/* traffic_to_str */

/* register/get a treeview to/from a window */
void register_treeview(GtkWidget *window, GtkTreeView *gv)
{
  g_assert(window);
  g_object_set_data ( G_OBJECT(window), "EA_gv", gv);
}
GtkTreeView *retrieve_treeview(GtkWidget *window)
{
  if (!window)
    return NULL;
  return GTK_TREE_VIEW(g_object_get_data ( G_OBJECT(window), "EA_gv"));
}
