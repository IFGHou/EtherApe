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

#include "globals.h"
#include "info_windows.h"

guint
update_info_windows (void)
{
  static GtkWidget *protocols_window = NULL;

  if (!protocols_window)
    protocols_window = glade_xml_get_widget (xml, "protocols_window");

  if (status == PAUSE)
    return TRUE;

  gettimeofday (&now, NULL);

  /* Update the protocols window if it is being displayed */
  if (GTK_WIDGET_VISIBLE (protocols_window))
    update_protocols_window ();

  update_node_info_windows ();
  update_prot_info_windows ();

  return TRUE;			/* Keep on calling this function */

}				/* update_info_windows */

void
update_protocols_window (void)
{
  GList *item = NULL;
  protocol_t *legend_protocol = NULL, *info_protocol = NULL, *protocol = NULL;
  static struct timeval last_updated = { 0, 0 };
  struct timeval diff;
  static GtkWidget *prot_clist = NULL;
  static gboolean clist_initiated = FALSE;
  GtkWidget *widget = NULL;
  gchar *row[5] = { NULL, NULL, NULL, NULL }, *str = NULL;
  gint i;

  if (!prot_clist)
    prot_clist = glade_xml_get_widget (xml, "prot_clist");

  if (!clist_initiated)
    {
      /* set the sorting function */
      gtk_clist_set_compare_func (GTK_CLIST (prot_clist),
				  prot_window_compare);
      /* set the callbacks for the column titles */
      for (i = 0; i < GTK_CLIST (prot_clist)->columns; i++)
	{
	  widget =
	    (gtk_clist_get_column_widget (GTK_CLIST (prot_clist), i))->parent;
	  gtk_signal_connect (GTK_OBJECT (widget), "clicked",
			      GTK_SIGNAL_FUNC (prot_clist_button_clicked),
			      GINT_TO_POINTER (i));
	}


      clist_initiated = TRUE;
    }

  gtk_clist_freeze (GTK_CLIST (prot_clist));

  /* Add any new protocols in the legend */
  item = legend_protocols;
  while (item)
    {
      legend_protocol = item->data;
      if (!g_list_find_custom (info_protocols, legend_protocol->name,
			       protocol_compare))
	{
	  protocol = g_malloc (sizeof (protocol_t));
	  protocol->name = g_strdup (legend_protocol->name);
	  protocol->last_heard.tv_sec = protocol->last_heard.tv_usec = 0;
	  info_protocols = g_list_prepend (info_protocols, protocol);

	  row[0] = protocol->name;
	  row[1] = row[2] = row[3] = row[4] = "";
	  gtk_clist_prepend (GTK_CLIST (prot_clist), row);
	  gtk_clist_set_row_data (GTK_CLIST (prot_clist), 0, protocol);
	}
      item = item->next;
    }

  /* Delete protocols not in the legend */
  for (i = GTK_CLIST (prot_clist)->rows - 1; i >= 0; i--)
    {
      protocol = gtk_clist_get_row_data (GTK_CLIST (prot_clist), i);
      if (!g_list_find_custom (legend_protocols, protocol->name,
			       protocol_compare))
	{
	  info_protocols = g_list_remove (info_protocols, protocol);
	  g_free (protocol->name);
	  g_free (protocol);
	  gtk_clist_remove (GTK_CLIST (prot_clist), i);

	}
    }

  /* Update rows */
  for (i = GTK_CLIST (prot_clist)->rows - 1; i >= 0; i--)
    {

      if (!
	  (info_protocol =
	   gtk_clist_get_row_data (GTK_CLIST (prot_clist), i)))
	{
	  g_my_critical
	    ("Unable to extract protocol structure from table in update_protocols_window");
	  return;
	}

      if (!
	  (item =
	   g_list_find_custom (protocols[stack_level], info_protocol->name,
			       protocol_compare)))
	{
	  g_my_critical
	    ("Global protocol not found in update_protocols_window");
	  return;
	}

      protocol = item->data;
      diff =
	substract_times (info_protocol->last_heard, protocol->last_heard);
      if ((diff.tv_usec < 0) || (diff.tv_sec < 0))
	{
	  info_protocol->accumulated = protocol->accumulated;
	  info_protocol->last_heard = protocol->last_heard;
	  info_protocol->n_packets = protocol->n_packets;

	  str = traffic_to_str (protocol->accumulated, FALSE);
	  gtk_clist_set_text (GTK_CLIST (prot_clist), i, 2, str);

	  str = g_strdup_printf ("%d", info_protocol->n_packets);
	  gtk_clist_set_text (GTK_CLIST (prot_clist), i, 4, str);
	  g_free (str);

	}
      /*str = ctime (&(protocol->last_heard.tv_sec)); */
      info_protocol->average = protocol->average;
      str = traffic_to_str (protocol->average, TRUE);
      gtk_clist_set_text (GTK_CLIST (prot_clist), i, 1, str);
      str = timeval_to_str (protocol->last_heard);
      gtk_clist_set_text (GTK_CLIST (prot_clist), i, 3, str);

    }

  gtk_clist_sort (GTK_CLIST (prot_clist));

  gtk_clist_thaw (GTK_CLIST (prot_clist));

#if 0
  /* Clean the list */
  prot_clist = glade_xml_get_widget (xml, "prot_clist");
  gtk_clist_clear (GTK_CLIST (prot_clist));
  /* Fill with data from legend_protocols */
  /* substract_times */
  item = legend_protocols;
  while (item)
    {
      protocol = item->data;
      capture_item = g_list_find_custom (protocols[stack_level],
					 protocol->name, protocol_compare);
      protocol = capture_item->data;
      row[0] = protocol->name;
      row[1] = g_strdup (traffic_to_str (protocol->average, TRUE));
      row[2] = g_strdup (traffic_to_str (protocol->accumulated, FALSE));
      gtk_clist_prepend (GTK_CLIST (prot_clist), row);
      g_free (row[1]);
      g_free (row[2]);
      item = item->next;
    }

#endif
  last_updated = now;
}				/* update_protocols_window */

void
create_node_info_window (canvas_node_t * canvas_node)
{
  GtkWidget *window;
  node_info_window_t *node_info_window;
  GladeXML *xml_info_window;
  GtkWidget *widget;
  GList *list_item;
  /* If there is already a window, we don't need to create it again */
  if (!(list_item =
	g_list_find_custom (node_info_windows,
			    canvas_node->canvas_node_id, node_info_compare)))
    {
      xml_info_window =
	glade_xml_new (GLADEDIR "/" ETHERAPE_GLADE_FILE, "node_info");
      if (!xml_info_window)
	{
	  g_error (_("We could not load the interface! (%s)"),
		   GLADEDIR "/" ETHERAPE_GLADE_FILE);
	  return;
	}
      glade_xml_signal_autoconnect (xml_info_window);
      window = glade_xml_get_widget (xml_info_window, "node_info");
      gtk_widget_show (window);
      widget = glade_xml_get_widget (xml_info_window, "node_info_name_label");
      gtk_object_set_data (GTK_OBJECT (window), "name_label", widget);
      widget =
	glade_xml_get_widget (xml_info_window,
			      "node_info_numeric_name_label");
      gtk_object_set_data (GTK_OBJECT (window), "numeric_name_label", widget);
      widget = glade_xml_get_widget (xml_info_window, "node_info_average");
      gtk_object_set_data (GTK_OBJECT (window), "average", widget);
      widget = glade_xml_get_widget (xml_info_window, "node_info_average_in");
      gtk_object_set_data (GTK_OBJECT (window), "average_in", widget);
      widget =
	glade_xml_get_widget (xml_info_window, "node_info_average_out");
      gtk_object_set_data (GTK_OBJECT (window), "average_out", widget);
      widget =
	glade_xml_get_widget (xml_info_window, "node_info_accumulated");
      gtk_object_set_data (GTK_OBJECT (window), "accumulated", widget);
      widget =
	glade_xml_get_widget (xml_info_window, "node_info_accumulated_in");
      gtk_object_set_data (GTK_OBJECT (window), "accumulated_in", widget);
      widget =
	glade_xml_get_widget (xml_info_window, "node_info_accumulated_out");
      gtk_object_set_data (GTK_OBJECT (window), "accumulated_out", widget);
      gtk_object_destroy (GTK_OBJECT (xml_info_window));
      node_info_window = g_malloc (sizeof (node_info_window_t));
      node_info_window->node_id =
	g_memdup (canvas_node->canvas_node_id, node_id_length);
      gtk_object_set_data (GTK_OBJECT (window), "node_id",
			   node_info_window->node_id);
      node_info_window->window = window;
      node_info_windows =
	g_list_prepend (node_info_windows, node_info_window);
    }
  else
    node_info_window = (node_info_window_t *) list_item->data;
  update_node_info_window (node_info_window);
  if (canvas_node && canvas_node->node)
    {
      g_my_info ("Nodes: %d. Canvas nodes: %d", g_tree_nnodes (nodes),
		 g_tree_nnodes (canvas_nodes));
      dump_node_info (canvas_node->node);
    }

}				/* create_node_info_window */


void
update_node_info_windows (void)
{
  GList *list_item = NULL, *remove_item;
  node_info_window_t *node_info_window = NULL;
  static struct timeval last_time = {
    0, 0
  }
  , diff;
  diff = substract_times (now, last_time);
  /* Update info windows at most twice a second */
  if (refresh_period < 500)
    if (!(IS_OLDER (diff, 500)))
      return;
  list_item = node_info_windows;
  while (list_item)
    {
      node_info_window = (node_info_window_t *) list_item->data;
      if (status == STOP)
	{
	  g_free (node_info_window->node_id);
	  gtk_widget_destroy (GTK_WIDGET (node_info_window->window));
	  remove_item = list_item;
	  list_item = list_item->next;
	  node_info_windows =
	    g_list_remove_link (node_info_windows, remove_item);
	}
      else
	{
	  update_node_info_window (node_info_window);
	  list_item = list_item->next;
	}
    }

  last_time = now;
  return;
}				/* update_node_info_windows */

/* It's called when a node info window is closed by the user 
 * It has to free memory and delete the window from the list
 * of windows */
void
on_node_info_delete_event (GtkWidget * node_info, gpointer user_data)
{
  GList *item = NULL;
  guint8 *node_id = NULL;
  node_info_window_t *node_info_window = NULL;
  node_id = gtk_object_get_data (GTK_OBJECT (node_info), "node_id");
  if (!node_id)
    {
      g_my_critical (_("No node_id in on_node_info_delete_event"));
      return;
    }

  if (!(item =
	g_list_find_custom (node_info_windows, node_id, node_info_compare)))
    {
      g_my_critical (_("No node_info_window in on_node_info_delete_event"));
      return;
    }

  node_info_window = item->data;
  g_free (node_info_window->node_id);
  gtk_widget_destroy (GTK_WIDGET (node_info_window->window));
  node_info_windows = g_list_remove_link (node_info_windows, item);
}				/* on_node_info_delete_event */

/* It's called when a prot info window is closed by the user 
 * It has to free memory and delete the window from the list
 * of windows */
void
on_prot_info_delete_event (GtkWidget * prot_info, gpointer user_data)
{
  GList *item = NULL;
  guint8 *prot_name = NULL;
  prot_info_window_t *prot_info_window = NULL;
  prot_name = gtk_object_get_data (GTK_OBJECT (prot_info), "prot_name");
  if (!prot_name)
    {
      g_my_critical (_("No prot_name in on_prot_info_delete_event"));
      return;
    }

  if (!(item =
	g_list_find_custom (prot_info_windows, prot_name, prot_info_compare)))
    {
      g_my_critical (_("No prot_info_window in on_prot_info_delete_event"));
      return;
    }

  prot_info_window = item->data;
  g_free (prot_info_window->prot_name);
  gtk_widget_destroy (GTK_WIDGET (prot_info_window->window));
  prot_info_windows = g_list_remove_link (prot_info_windows, item);
}				/* on_prot_info_delete_event */

static void
update_node_info_window (node_info_window_t * node_info_window)
{
  guint8 *node_id = NULL;
  node_t *node = NULL;
  GtkWidget *window = NULL;
  GtkWidget *widget = NULL;
  node_id = node_info_window->node_id;
  window = node_info_window->window;
  if (!(node = g_tree_lookup (nodes, node_id)))
    {
      widget =
	gtk_object_get_data (GTK_OBJECT (window), "numeric_name_label");
      gtk_label_set_text (GTK_LABEL (widget), _("No info available"));
      widget = gtk_object_get_data (GTK_OBJECT (window), "average");
      gtk_label_set_text (GTK_LABEL (widget), "X");
      widget = gtk_object_get_data (GTK_OBJECT (window), "average_in");
      gtk_label_set_text (GTK_LABEL (widget), "X");
      widget = gtk_object_get_data (GTK_OBJECT (window), "average_out");
      gtk_label_set_text (GTK_LABEL (widget), "X");
      widget = gtk_object_get_data (GTK_OBJECT (window), "accumulated");
      gtk_label_set_text (GTK_LABEL (widget), "X");
      widget = gtk_object_get_data (GTK_OBJECT (window), "accumulated_in");
      gtk_label_set_text (GTK_LABEL (widget), "X");
      widget = gtk_object_get_data (GTK_OBJECT (window), "accumulated_out");
      gtk_label_set_text (GTK_LABEL (widget), "X");
      gtk_container_queue_resize (GTK_CONTAINER (node_info_window->window));
      return;
    }

  gtk_window_set_title (GTK_WINDOW (window), node->name->str);
  widget = gtk_object_get_data (GTK_OBJECT (window), "name_label");
  gtk_label_set_text (GTK_LABEL (widget), node->name->str);
  widget = gtk_object_get_data (GTK_OBJECT (window), "numeric_name_label");
  gtk_label_set_text (GTK_LABEL (widget), node->numeric_name->str);
  widget = gtk_object_get_data (GTK_OBJECT (window), "average");
  gtk_label_set_text (GTK_LABEL (widget),
		      traffic_to_str (node->average, TRUE));
  widget = gtk_object_get_data (GTK_OBJECT (window), "average_in");
  gtk_label_set_text (GTK_LABEL (widget),
		      traffic_to_str (node->average_in, TRUE));
  widget = gtk_object_get_data (GTK_OBJECT (window), "average_out");
  gtk_label_set_text (GTK_LABEL (widget),
		      traffic_to_str (node->average_out, TRUE));
  widget = gtk_object_get_data (GTK_OBJECT (window), "accumulated");
  gtk_label_set_text (GTK_LABEL (widget),
		      traffic_to_str (node->accumulated, FALSE));
  widget = gtk_object_get_data (GTK_OBJECT (window), "accumulated_in");
  gtk_label_set_text (GTK_LABEL (widget),
		      traffic_to_str (node->accumulated_in, FALSE));
  widget = gtk_object_get_data (GTK_OBJECT (window), "accumulated_out");
  gtk_label_set_text (GTK_LABEL (widget),
		      traffic_to_str (node->accumulated_out, FALSE));
  gtk_container_queue_resize (GTK_CONTAINER (node_info_window->window));
}				/* update_node_info_window */

void
toggle_protocols_window (void)
{
  static GtkWidget *protocols_check = NULL;
  if (!protocols_check)
    protocols_check = glade_xml_get_widget (xml, "protocols_check");
  gtk_menu_item_activate (GTK_MENU_ITEM (protocols_check));
}				/* toggle_protocols_window */

void
on_protocols_check_activate (GtkCheckMenuItem * menuitem, gpointer user_data)
{
  static GtkWidget *protocols_window = NULL;
  if (!protocols_window)
    protocols_window = glade_xml_get_widget (xml, "protocols_window");
  if (menuitem->active)
    {
      gtk_widget_show (protocols_window);
      gdk_window_raise (protocols_window->window);
      update_protocols_window ();
    }
  else
    gtk_widget_hide (protocols_window);
}				/* on_protocols_check_activate */

void
on_prot_column_view_activate (GtkCheckMenuItem * menuitem, gpointer user_data)
{
  static GtkWidget *prot_clist = NULL;
  guint column;
   
  if (!prot_clist)
    prot_clist = glade_xml_get_widget (xml, "prot_clist");
  
  if (!sscanf ((gchar *) user_data, "%d", &column))
     {
	g_warning ("Unable to decode column in on_prot_column_view_activate");
	return;
     }
   
   gtk_clist_set_column_visibility (GTK_CLIST(prot_clist),
				    column,
				    menuitem->active);
   
}				/* on_prot_column_view_activate */

/* Displays the protocols window when the legend is double clicked */
gboolean
on_prot_table_button_press_event (GtkWidget * widget,
				  GdkEventButton * event, gpointer user_data)
{
  switch (event->type)
    {
    case GDK_2BUTTON_PRESS:
      toggle_protocols_window ();
      return TRUE;
    default:
    }

  return FALSE;
}				/* on_prot_table_button_press_event */

static void
prot_clist_button_clicked (GtkButton * button, gpointer func_data)
{
  guint column = GPOINTER_TO_INT (func_data);

  /* If clicked a second time, reverse the order of sorting */
  if (column == prot_clist_sort_column)
    {
      if (prot_clist_reverse_sort == TRUE)
	prot_clist_reverse_sort = FALSE;
      else
	prot_clist_reverse_sort = TRUE;
    }

  prot_clist_sort_column = column;

  update_protocols_window ();
}				/* prot_clist_button_clicked */

void
on_prot_clist_select_row (GtkCList * clist,
			  gint row,
			  gint column, GdkEvent * event, gpointer user_data)
{
  protocol_t *protocol = NULL;

  if (!event)
    return;

  switch (event->type)
    {
    case GDK_2BUTTON_PRESS:
      protocol = gtk_clist_get_row_data (clist, row);
      create_prot_info_window (protocol);
    default:
    }
}				/* on_prot_clist_select_row */

static void
create_prot_info_window (protocol_t * protocol)
{
  GtkWidget *window;
  prot_info_window_t *prot_info_window;
  GladeXML *xml_info_window;
  GtkWidget *widget;
  GList *list_item;

  /* If there is already a window, we don't need to create it again */
  if (!(list_item =
	g_list_find_custom (prot_info_windows,
			    protocol->name, prot_info_compare)))
    {
      xml_info_window =
	glade_xml_new (GLADEDIR "/" ETHERAPE_GLADE_FILE, "prot_info");
      if (!xml_info_window)
	{
	  g_error (_("We could not load the interface! (%s)"),
		   GLADEDIR "/" ETHERAPE_GLADE_FILE);
	  return;
	}
      glade_xml_signal_autoconnect (xml_info_window);
      window = glade_xml_get_widget (xml_info_window, "prot_info");
      gtk_widget_show (window);
      widget = glade_xml_get_widget (xml_info_window, "prot_info_name_label");
      gtk_object_set_data (GTK_OBJECT (window), "name_label", widget);
      widget =
	glade_xml_get_widget (xml_info_window, "prot_info_last_heard_label");
      gtk_object_set_data (GTK_OBJECT (window), "last_heard_label", widget);
      widget = glade_xml_get_widget (xml_info_window, "prot_info_average");
      gtk_object_set_data (GTK_OBJECT (window), "average", widget);
      widget =
	glade_xml_get_widget (xml_info_window, "prot_info_accumulated");
      gtk_object_set_data (GTK_OBJECT (window), "accumulated", widget);
      gtk_object_destroy (GTK_OBJECT (xml_info_window));

      prot_info_window = g_malloc (sizeof (prot_info_window_t));
      prot_info_window->prot_name = g_strdup (protocol->name);
      gtk_object_set_data (GTK_OBJECT (window), "prot_name",
			   prot_info_window->prot_name);
      prot_info_window->window = window;
      prot_info_windows =
	g_list_prepend (prot_info_windows, prot_info_window);
    }
  else
    prot_info_window = (prot_info_window_t *) list_item->data;

  update_prot_info_window (prot_info_window);

}				/* create_prot_info_window */


void
update_prot_info_windows (void)
{
  GList *list_item = NULL, *remove_item;
  prot_info_window_t *prot_info_window = NULL;

  list_item = prot_info_windows;
  while (list_item)
    {
      prot_info_window = (prot_info_window_t *) list_item->data;
      if (status == STOP)
	{
	  g_free (prot_info_window->prot_name);
	  gtk_widget_destroy (GTK_WIDGET (prot_info_window->window));
	  remove_item = list_item;
	  list_item = list_item->next;
	  prot_info_windows =
	    g_list_remove_link (prot_info_windows, remove_item);
	}
      else
	{
	  update_prot_info_window (prot_info_window);
	  list_item = list_item->next;
	}
    }

  return;
}				/* update_prot_info_windows */


static void
update_prot_info_window (prot_info_window_t * prot_info_window)
{
  guint8 *prot_name = NULL;
  protocol_t *prot = NULL;
  GtkWidget *window = NULL;
  GtkWidget *widget = NULL;
  GList *item = NULL;
  prot_name = prot_info_window->prot_name;

  window = prot_info_window->window;
  if (!
      (item =
       g_list_find_custom (info_protocols, prot_name, protocol_compare)))
    {
      widget = gtk_object_get_data (GTK_OBJECT (window), "average");
      gtk_label_set_text (GTK_LABEL (widget), "X");
      widget = gtk_object_get_data (GTK_OBJECT (window), "accumulated");
      gtk_label_set_text (GTK_LABEL (widget), "X");
      gtk_container_queue_resize (GTK_CONTAINER (prot_info_window->window));
      return;
    }

  prot = item->data;

  gtk_window_set_title (GTK_WINDOW (window), prot->name);
  widget = gtk_object_get_data (GTK_OBJECT (window), "name_label");
  gtk_label_set_text (GTK_LABEL (widget), prot->name);
  widget = gtk_object_get_data (GTK_OBJECT (window), "last_heard_label");
  gtk_label_set_text (GTK_LABEL (widget), timeval_to_str (prot->last_heard));
  widget = gtk_object_get_data (GTK_OBJECT (window), "average");
  gtk_label_set_text (GTK_LABEL (widget),
		      traffic_to_str (prot->average, TRUE));
  widget = gtk_object_get_data (GTK_OBJECT (window), "accumulated");
  gtk_label_set_text (GTK_LABEL (widget),
		      traffic_to_str (prot->accumulated, FALSE));
  gtk_container_queue_resize (GTK_CONTAINER (prot_info_window->window));

}				/* update_prot_info_window */


/* Comparison function used to sort the clist */
static gint
prot_window_compare (GtkCList * clist, gconstpointer p1, gconstpointer p2)
{
  gint ret;
  gdouble t1, t2;
  struct timeval time1, time2, diff;

  protocol_t *prot1, *prot2;
  prot1 = ((const GtkCListRow *) p1)->data;
  prot2 = ((const GtkCListRow *) p2)->data;

  switch (prot_clist_sort_column)
    {
    case 0:
      ret = strcmp (prot1->name, prot2->name);
      break;
    case 1:
      t1 = prot1->average;
      t2 = prot2->average;
      if (t1 == t2)
	ret = 0;
      else if (t1 < t2)
	ret = -1;
      else
	ret = 1;
      break;
    case 2:
      t1 = prot1->accumulated;
      t2 = prot2->accumulated;
      if (t1 == t2)
	ret = 0;
      else if (t1 < t2)
	ret = -1;
      else
	ret = 1;
      break;
    case 3:
      time1 = prot1->last_heard;
      time2 = prot2->last_heard;
      diff = substract_times (time1, time2);
      if ((diff.tv_sec == 0) && (diff.tv_usec == 0))
	ret = 0;
      else if ((diff.tv_sec < 0) || (diff.tv_usec < 0))
	ret = -1;
      else
	ret = 1;
      break;
    case 4:
      if (prot1->n_packets == prot2->n_packets)
	ret = 0;
      else if (prot1->n_packets < prot2->n_packets)
	ret = -1;
      else
	ret = 1;
      break;
    default:
      ret = 0;
    }

  if (prot_clist_reverse_sort)
    ret = -ret;

  return ret;
}				/* prot_window_compare */

/* Comparison function used to compare node_info_windows */
static gint
node_info_compare (gconstpointer a, gconstpointer b)
{
  g_assert (a != NULL);
  g_assert (b != NULL);
  return memcmp (((node_info_window_t *) a)->node_id, (guint8 *) b,
		 node_id_length);
}

/* Comparison function used to compare prot_info_windows */
static gint
prot_info_compare (gconstpointer a, gconstpointer b)
{
  g_assert (a != NULL);
  g_assert (b != NULL);
  return strcmp (((prot_info_window_t *) a)->prot_name, (guint8 *) b);
}				/* prot_info_compare */


static gchar *
timeval_to_str (struct timeval last_heard)
{
  static gchar *str = NULL;
  struct timeval diff;
  struct tm broken_time;

  if (str)
    g_free (str);

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
