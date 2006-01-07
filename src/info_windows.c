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
#include <time.h>
#include "globals.h"
#include "info_windows.h"
#include "diagram.h"
#include "node.h"
#include "protocols.h"

/* private static vars */
static GList *info_protocols = NULL;
static GList *node_info_windows = NULL;
static GList *prot_info_windows = NULL;

/* private functions */
static void update_prot_info_windows (void);
static void update_node_info_window (node_info_window_t * node_info_window);
static void update_prot_info_window (prot_info_window_t * prot_info_window);
static gint node_info_window_compare (gconstpointer a, gconstpointer b);
static gint prot_info_compare (gconstpointer a, gconstpointer b);
static void update_node_info_windows (void);

/* 
  
  Helper functions 

*/

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

/* ----------------------------------------------------------

   Protocol Info Detail window functions (prot_info_windows)

   ---------------------------------------------------------- */
/* Comparison function used to compare prot_info_windows */
static gint
prot_info_compare (gconstpointer a, gconstpointer b)
{
  g_assert (a != NULL);
  g_assert (b != NULL);
  return strcmp (((prot_info_window_t *) a)->prot_name, (gchar *) b);
}				/* prot_info_compare */

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
	glade_xml_new (GLADEDIR "/" ETHERAPE_GLADE_FILE, "prot_info", NULL);
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
      g_object_set_data (G_OBJECT (window), "name_label", widget);
      widget =
	glade_xml_get_widget (xml_info_window, "prot_info_last_heard_label");
      g_object_set_data (G_OBJECT (window), "last_heard_label", widget);
      widget = glade_xml_get_widget (xml_info_window, "prot_info_average");
      g_object_set_data (G_OBJECT (window), "average", widget);
      widget =
	glade_xml_get_widget (xml_info_window, "prot_info_accumulated");
      g_object_set_data (G_OBJECT (window), "accumulated", widget);
      g_object_unref (xml_info_window);

      prot_info_window = g_malloc (sizeof (prot_info_window_t));
      prot_info_window->prot_name = g_strdup (protocol->name);
      g_object_set_data (G_OBJECT (window), "prot_name",
			 prot_info_window->prot_name);
      prot_info_window->window = window;
      prot_info_windows =
	g_list_prepend (prot_info_windows, prot_info_window);

    }
  else
    prot_info_window = (prot_info_window_t *) list_item->data;

  update_prot_info_window (prot_info_window);

}				/* create_prot_info_window */


/* It's called when a prot info window is closed by the user 
 * It has to free memory and delete the window from the list
 * of windows */
gboolean
on_prot_info_delete_event (GtkWidget * prot_info, GdkEvent * evt,
			   gpointer user_data)
{
  GList *item = NULL;
  gchar *prot_name = NULL;
  prot_info_window_t *prot_info_window = NULL;

  prot_name = g_object_get_data (G_OBJECT (prot_info), "prot_name");
  if (!prot_name)
    {
      g_my_critical (_("No prot_name in on_prot_info_delete_event"));
      return TRUE;		/* ignore event */
    }

  if (!(item =
	g_list_find_custom (prot_info_windows, prot_name, prot_info_compare)))
    {
      g_my_critical (_("No prot_info_window in on_prot_info_delete_event"));
      return TRUE;		/* ignore event */
    }

  prot_info_window = item->data;
  g_free (prot_info_window->prot_name);
  gtk_widget_destroy (GTK_WIDGET (prot_info_window->window));
  prot_info_windows = g_list_remove_link (prot_info_windows, item);

  return FALSE;
}				/* on_prot_info_delete_event */

/* updates a single proto detail window */
static void
update_prot_info_window (prot_info_window_t * prot_info_window)
{
  gchar *prot_name = NULL;
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
      widget = g_object_get_data (G_OBJECT (window), "average");
      gtk_label_set_text (GTK_LABEL (widget), "X");
      widget = g_object_get_data (G_OBJECT (window), "accumulated");
      gtk_label_set_text (GTK_LABEL (widget), "X");
      gtk_widget_queue_resize (GTK_WIDGET (prot_info_window->window));
      return;
    }

  prot = item->data;

  gtk_window_set_title (GTK_WINDOW (window), prot->name);
  widget = g_object_get_data (G_OBJECT (window), "name_label");
  gtk_label_set_text (GTK_LABEL (widget), prot->name);
  widget = g_object_get_data (G_OBJECT (window), "last_heard_label");
  gtk_label_set_text (GTK_LABEL (widget), timeval_to_str (prot->last_heard));
  widget = g_object_get_data (G_OBJECT (window), "average");
  gtk_label_set_text (GTK_LABEL (widget),
		      traffic_to_str (prot->average, TRUE));
  widget = g_object_get_data (G_OBJECT (window), "accumulated");
  gtk_label_set_text (GTK_LABEL (widget),
		      traffic_to_str (prot->accumulated, FALSE));
  gtk_widget_queue_resize (GTK_WIDGET (prot_info_window->window));

}				/* update_prot_info_window */

/* cycles on all proto detail windows updating them */
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


/* ----------------------------------------------------------

   General Protocol Info window functions (protocols_window) 

   ---------------------------------------------------------- */
#define MAX_C(a,b) ((a) > (b) ? (a) : (b))
#define MIN_C(a,b) ((a) > (b) ? (a) : (b))
#define LUMINANCE(r,g,b) ((MAX_C( (double)(r)/0xFFFF, MAX_C( (double)(g)/0xFFFF, \
                                  (double)(b)/0xFFFF)) + \
                          MIN_C( (double)(r)/0xFFFF, MIN_C( (double)(g)/0xFFFF, \
                                  (double)(b)/0xFFFF))) / 2.0)

/* Comparison functions used to sort the clist */
static gint
prot_window_compare (GtkTreeModel * gs, GtkTreeIter * a, GtkTreeIter * b,
		     gpointer userdata)
{
  gint ret = 0;
  gdouble t1, t2;
  struct timeval time1, time2, diff;
  gint idcol;
  GtkSortType order;


  /* reads the proto ptr from 6th columns */
  protocol_t *prot1, *prot2;
  gtk_tree_model_get (gs, a, 5, &prot1, -1);
  gtk_tree_model_get (gs, b, 5, &prot2, -1);
  if (!prot1 || !prot2)
    return 0;			/* error */

  /* gets sort info */
  gtk_tree_sortable_get_sort_column_id (GTK_TREE_SORTABLE (gs), &idcol,
					&order);

  switch (idcol)
    {
    case 0:
    default:
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
      if (prot1->proto_packets == prot2->proto_packets)
	ret = 0;
      else if (prot1->proto_packets < prot2->proto_packets)
	ret = -1;
      else
	ret = 1;
      break;
    case 6:
      /* compare color by luminosity first, then by rgb... */
      if (LUMINANCE (prot1->color.red, prot1->color.green, prot1->color.blue)
	  < LUMINANCE (prot2->color.red, prot2->color.green,
		       prot2->color.blue))
	ret = -1;
      else
	if (LUMINANCE
	    (prot1->color.red, prot1->color.green,
	     prot1->color.blue) > LUMINANCE (prot2->color.red,
					     prot2->color.green,
					     prot2->color.blue))
	ret = +1;
      else if (prot1->color.red < prot2->color.red)
	ret = -1;
      else if (prot1->color.red > prot2->color.red)
	ret = +1;
      else if (prot1->color.green < prot2->color.green)
	ret = -1;
      else if (prot1->color.green > prot2->color.green)
	ret = +1;
      if (prot1->color.blue < prot2->color.blue)
	ret = -1;
      else if (prot1->color.blue > prot2->color.blue)
	ret = +1;
      else
	ret = 0;
      break;
    }

  return ret;
}

static void
create_protocols_list ()
{
  GtkTreeView *gv;
  GtkListStore *gs;
  GList *item = NULL;
  GtkTreeIter it;
  GtkTreeViewColumn *gc;

  /* create the tree view */
  gv = GTK_TREE_VIEW (glade_xml_get_widget (xml, "prot_clist"));
  if (!gv)
    return;			/* error */

  /* create the store  - it uses 7 values, five displayable, one ptr, and there
     node color */
  gs = gtk_list_store_new (7, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
			   G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER,
			   GDK_TYPE_COLOR);
  gtk_tree_view_set_model (gv, GTK_TREE_MODEL (gs));

  /* the view columns and cell renderers must be also created ... 
     the first column is the proto color  */
  gc = gtk_tree_view_column_new_with_attributes
    (" ", gtk_cell_renderer_text_new (), "background-gdk", 6, NULL);
  g_object_set (G_OBJECT (gc), "resizable", TRUE, "reorderable", TRUE, NULL);
  gtk_tree_view_column_set_sort_column_id (gc, 6);
  gtk_tree_view_append_column (gv, gc);

  gc = gtk_tree_view_column_new_with_attributes
    ("Protocol", gtk_cell_renderer_text_new (), "text", 0, NULL);
  g_object_set (G_OBJECT (gc), "resizable", TRUE, "reorderable", TRUE, NULL);
  gtk_tree_view_column_set_sort_column_id (gc, 0);
  gtk_tree_view_append_column (gv, gc);

  gc = gtk_tree_view_column_new_with_attributes
    ("Inst Traffic", gtk_cell_renderer_text_new (), "text", 1, NULL);
  g_object_set (G_OBJECT (gc), "resizable", TRUE, "reorderable", TRUE, NULL);
  gtk_tree_view_column_set_sort_column_id (gc, 1);
  gtk_tree_view_append_column (gv, gc);

  gc = gtk_tree_view_column_new_with_attributes
    ("Accum Traffic", gtk_cell_renderer_text_new (), "text", 2, NULL);
  g_object_set (G_OBJECT (gc), "resizable", TRUE, "reorderable", TRUE, NULL);
  gtk_tree_view_column_set_sort_column_id (gc, 2);
  gtk_tree_view_append_column (gv, gc);

  gc = gtk_tree_view_column_new_with_attributes
    ("Last Heard", gtk_cell_renderer_text_new (), "text", 3, NULL);
  g_object_set (G_OBJECT (gc), "resizable", TRUE, "reorderable", TRUE, NULL);
  gtk_tree_view_column_set_sort_column_id (gc, 3);
  gtk_tree_view_append_column (gv, gc);

  gc = gtk_tree_view_column_new_with_attributes
    ("Packets", gtk_cell_renderer_text_new (), "text", 4, NULL);
  g_object_set (G_OBJECT (gc), "resizable", TRUE, "reorderable", TRUE, NULL);
  gtk_tree_view_column_set_sort_column_id (gc, 4);
  gtk_tree_view_append_column (gv, gc);

  /* the sort functions ... */
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (gs), 0,
				   prot_window_compare, gs, NULL);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (gs), 1,
				   prot_window_compare, gs, NULL);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (gs), 2,
				   prot_window_compare, gs, NULL);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (gs), 3,
				   prot_window_compare, gs, NULL);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (gs), 4,
				   prot_window_compare, gs, NULL);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (gs), 6,
				   prot_window_compare, gs, NULL);

  /* initial sort order is by protocol */
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (gs), 0,
					GTK_SORT_ASCENDING);

  /* re-adds the current protocols */
  item = info_protocols;
  while (item)
    {
      protocol_t *nproto = (protocol_t *) (item->data);
      gtk_list_store_prepend (gs, &it);
      gtk_list_store_set (gs, &it, 0, nproto->name, 5, nproto, -1);
      item = item->next;
    }

}


static void
update_protocols_window (void)
{
  static gboolean inited = FALSE;
  GtkListStore *gs = NULL;
  GList *item = NULL;
  const protocol_t *protocol = NULL;
  struct timeval diff;
  gchar *str = NULL;
  gboolean res;
  GtkTreeIter it;

  /* retrieve view and model (store) */
  GtkTreeView *gv = GTK_TREE_VIEW (glade_xml_get_widget (xml, "prot_clist"));
  if (!gv || !GTK_WIDGET_VISIBLE (gv))
    return;			/* error or hidden */
  gs = GTK_LIST_STORE (gtk_tree_view_get_model (gv));
  if (!gs)
    create_protocols_list ();
  gs = GTK_LIST_STORE (gtk_tree_view_get_model (gv));
  if (!gs)
    return;			/* error */
  inited = TRUE;

  /* Add any new protocols in the legend */
  item = legend_protocols;
  while (item)
    {
      protocol = item->data;
      if (!g_list_find_custom (info_protocols, protocol->name,
			       protocol_compare))
	{
	  protocol_t *nproto = protocol_t_create(protocol->name);
	  info_protocols = g_list_prepend (info_protocols, nproto);
	  nproto->color = protocol->color;

	  gtk_list_store_prepend (gs, &it);
	  gtk_list_store_set (gs, &it, 0, nproto->name, 5, nproto, 6,
			      &nproto->color, -1);

	}
      item = item->next;
    }

  /* Delete protocols not in the legend */
  res = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (gs), &it);
  while (res)
    {
      gtk_tree_model_get (GTK_TREE_MODEL (gs), &it, 5, &protocol, -1);
      if (!g_list_find_custom (legend_protocols, protocol->name,
			       protocol_compare))
	{
	  info_protocols = g_list_remove (info_protocols, protocol);
	  g_free (protocol->name);
	  g_free ( (protocol_t *)protocol);

	  /* remove row - after, iter points to next row if res = TRUE */
#if GTK_CHECK_VERSION(2,2,0)
	  res = gtk_list_store_remove (gs, &it);
#else
	  /* gtk < 2.2 had gtk_list_store_remove void */
	  gtk_list_store_remove (gs, &it);
	  res = FALSE;		/* to be sure, act like remove() returned FALSE */
#endif
	}
      else
	res = gtk_tree_model_iter_next (GTK_TREE_MODEL (gs), &it);
    }

  /* Update rows */
  for (res = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (gs), &it);
       res; res = gtk_tree_model_iter_next (GTK_TREE_MODEL (gs), &it))
    {
      protocol_t *nproto = NULL;
      gtk_tree_model_get (GTK_TREE_MODEL (gs), &it, 5, &nproto, -1);

      if (!nproto)
	{
	  g_my_critical
	    ("Unable to extract protocol structure from table in update_protocols_window");
	  return;
	}

      protocol = protocol_summary_find(pref.stack_level, nproto->name);
      if (!protocol)
	{
	  g_my_critical
	    ("Global protocol not found in update_protocols_window");
	  return;
	}

      diff = substract_times (nproto->last_heard, protocol->last_heard);
      if ((diff.tv_usec < 0) || (diff.tv_sec < 0))
	{
	  nproto->accumulated = protocol->accumulated;
	  nproto->last_heard = protocol->last_heard;
	  nproto->proto_packets = protocol->proto_packets;
	  nproto->color = protocol->color;

	  str = traffic_to_str (protocol->accumulated, FALSE);
	  gtk_list_store_set (gs, &it, 2, str, 6, &nproto->color, -1);

	  str = g_strdup_printf ("%d", protocol->proto_packets);
	  gtk_list_store_set (gs, &it, 4, str, -1);
	  g_free (str);

	}
      /*str = ctime (&(protocol->last_heard.tv_sec)); */
      nproto->average = protocol->average;
      str = traffic_to_str (protocol->average, TRUE);
      gtk_list_store_set (gs, &it, 1, str, -1);
      str = timeval_to_str (protocol->last_heard);
      gtk_list_store_set (gs, &it, 3, str, -1);
    }
}				/* update_protocols_window */

void
toggle_protocols_window (void)
{
  GtkWidget *protocols_check = glade_xml_get_widget (xml, "protocols_check");
  if (!protocols_check)
    return;
  gtk_menu_item_activate (GTK_MENU_ITEM (protocols_check));
}				/* toggle_protocols_window */

gboolean
on_delete_protocol_window (GtkWidget * wdg, GdkEvent * evt, gpointer ud)
{
  toggle_protocols_window ();
  return TRUE;			/* ignore signal */
}

void
on_protocols_check_activate (GtkCheckMenuItem * menuitem, gpointer user_data)
{
  GtkWidget *protocols_window =
    glade_xml_get_widget (xml, "protocols_window");
  if (!protocols_window)
    return;
  if (gtk_check_menu_item_get_active (menuitem))
    {
      gtk_widget_show (protocols_window);
      gdk_window_raise (protocols_window->window);
      update_protocols_window ();
    }
  else
    gtk_widget_hide (protocols_window);
}				/* on_protocols_check_activate */

/* common function to activate the proto columns */
static void
activate_protocols_info_column (GtkMenuItem * gm, guint column)
{
  GtkTreeViewColumn *gc;
  GtkTreeView *gv = GTK_TREE_VIEW (glade_xml_get_widget (xml, "prot_clist"));
  if (!gv)
    return;			/* no window, no handling */

  gc = gtk_tree_view_get_column (gv, column);
  if (!gc)
    return;
  gtk_tree_view_column_set_visible (gc,
				    gtk_check_menu_item_get_active
				    (GTK_CHECK_MENU_ITEM (gm)));
}				/* on_prot_column_view_activate */


void
on_prot_color_column_activate (GtkMenuItem * gm, gpointer * user_data)
{
  activate_protocols_info_column (gm, 0);
}

void
on_protocol_column_activate (GtkMenuItem * gm, gpointer * user_data)
{
  activate_protocols_info_column (gm, 1);
}

void
on_instant_column_activate (GtkMenuItem * gm, gpointer * user_data)
{
  activate_protocols_info_column (gm, 2);
}

void
on_accumulated_column_activate (GtkMenuItem * gm, gpointer * user_data)
{
  activate_protocols_info_column (gm, 3);
}

void
on_heard_column_activate (GtkMenuItem * gm, gpointer * user_data)
{
  activate_protocols_info_column (gm, 4);
}

void
on_packets_column_activate (GtkMenuItem * gm, gpointer * user_data)
{
  activate_protocols_info_column (gm, 5);
}


/* Displays the protocols window when the legend is double clicked */
gboolean
on_prot_table_button_press_event (GtkWidget * widget,
				  GdkEventButton * event, gpointer user_data)
{
  if (GDK_2BUTTON_PRESS == event->type)
    {
      toggle_protocols_window ();
      return TRUE;
    }

  return FALSE;
}				/* on_prot_table_button_press_event */

/* opens a protocol detail window when the user clicks a proto row */
gboolean
on_prot_list_select_row (GtkTreeView * gv, gboolean arg1, gpointer user_data)
{
  protocol_t *protocol = NULL;
  GtkListStore *gs;
  GtkTreePath *gpath = NULL;
  GtkTreeViewColumn *gcol = NULL;
  GtkTreeIter it;

  /* retrieve the model (store) */
  gs = GTK_LIST_STORE (gtk_tree_view_get_model (gv));
  if (!gs)
    return FALSE;

  /* gets the row (path) at cursor */
  gtk_tree_view_get_cursor (gv, &gpath, &gcol);
  if (!gpath)
    return FALSE;		/* no row selected */

  /* get iterator from path  and removes from store */
  if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (gs), &it, gpath))
    return FALSE;		/* path not found */

  gtk_tree_model_get (GTK_TREE_MODEL (gs), &it, 5, &protocol, -1);
  create_prot_info_window (protocol);

  return TRUE;
}				/* on_prot_clist_select_row */




/* ----------------------------------------------------------

   Node Info window functions

   ---------------------------------------------------------- */
/* Comparison function used to compare node_info_windows */
static gint
node_info_window_compare(gconstpointer a, gconstpointer b)
{
  g_assert (a != NULL);
  g_assert (b != NULL);
  return node_id_compare( & (((node_info_window_t *) a)->node_id), 
                          & (((node_info_window_t *) b)->node_id));
}

guint
update_info_windows (void)
{
  if (status == PAUSE)
    return TRUE;

  gettimeofday (&now, NULL);

  update_protocols_window ();
  update_node_info_windows ();
  update_prot_info_windows ();

  return TRUE;			/* Keep on calling this function */

}				/* update_info_windows */

void
node_info_window_create(const node_id_t * node_id)
{
  GtkWidget *window;
  node_info_window_t *node_info_window;
  GladeXML *xml_info_window;
  GtkWidget *widget;
  GList *list_item;
  node_info_window_t key;

  key.node_id = *node_id;

  /* If there is already a window, we don't need to create it again */
  if (!(list_item =
	g_list_find_custom (node_info_windows, &key, node_info_window_compare)))
    {
      xml_info_window =
	glade_xml_new (GLADEDIR "/" ETHERAPE_GLADE_FILE, "node_info", NULL);
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
      g_object_set_data (G_OBJECT (window), "name_label", widget);
      widget =
	glade_xml_get_widget (xml_info_window,
			      "node_info_numeric_name_label");
      g_object_set_data (G_OBJECT (window), "numeric_name_label", widget);
      widget = glade_xml_get_widget (xml_info_window, "node_info_average");
      g_object_set_data (G_OBJECT (window), "average", widget);
      widget = glade_xml_get_widget (xml_info_window, "node_info_average_in");
      g_object_set_data (G_OBJECT (window), "average_in", widget);
      widget =
	glade_xml_get_widget (xml_info_window, "node_info_average_out");
      g_object_set_data (G_OBJECT (window), "average_out", widget);
      widget =
	glade_xml_get_widget (xml_info_window, "node_info_accumulated");
      g_object_set_data (G_OBJECT (window), "accumulated", widget);
      widget =
	glade_xml_get_widget (xml_info_window, "node_info_accumulated_in");
      g_object_set_data (G_OBJECT (window), "accumulated_in", widget);
      widget =
	glade_xml_get_widget (xml_info_window, "node_info_accumulated_out");
      g_object_set_data (G_OBJECT (window), "accumulated_out", widget);
      g_object_unref (xml_info_window);
      node_info_window = g_malloc (sizeof (node_info_window_t));
      node_info_window->node_id = *node_id;
      g_object_set_data (G_OBJECT (window), "node_id",
			 g_memdup(&node_info_window->node_id, sizeof(node_id_t)));
      node_info_window->window = window;
      node_info_windows =
	g_list_prepend (node_info_windows, node_info_window);
    }
  else
    node_info_window = (node_info_window_t *) list_item->data;
  update_node_info_window (node_info_window);
}				/* node_info_window_create */


void
update_node_info_windows (void)
{
  GList *list_item = NULL, *remove_item;
  node_info_window_t *node_info_window = NULL;
  struct timeval diff;
  static struct timeval last_time = {
    0, 0
  };

  diff = substract_times (now, last_time);
  /* Update info windows at most twice a second */
  if (pref.refresh_period < 500)
    if (!(IS_OLDER (diff, 500)))
      return;
  list_item = node_info_windows;
  while (list_item)
    {
      node_info_window = (node_info_window_t *) list_item->data;
      if (status == STOP)
	{
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
}				/* update_node_info_windows */

/* It's called when a node info window is closed by the user 
 * It has to free memory and delete the window from the list
 * of windows */
gboolean
on_node_info_delete_event (GtkWidget * node_info, GdkEvent * evt,
			   gpointer user_data)
{
  GList *item = NULL;
  node_id_t *node_id = NULL;
  node_info_window_t *node_info_window = NULL;
  node_info_window_t key;
  
  node_id = g_object_get_data (G_OBJECT (node_info), "node_id");
  if (!node_id)
    {
      g_my_critical (_("No node_id in on_node_info_delete_event"));
      return TRUE;		/* ignore signal */
    }

  key.node_id = *node_id;
  if (!(item =
	g_list_find_custom (node_info_windows, &key, node_info_window_compare)))
    {
      g_my_critical (_("No node_info_window in on_node_info_delete_event"));
      return TRUE;		/* ignore signal */
    }

  node_info_window = item->data;
  gtk_widget_destroy (GTK_WIDGET (node_info_window->window));
  node_info_windows = g_list_remove_link (node_info_windows, item);

  return FALSE;
}				/* on_node_info_delete_event */

static void
update_node_info_window (node_info_window_t * node_info_window)
{
  node_t *node = NULL;
  GtkWidget *window = NULL;
  GtkWidget *widget = NULL;
  
  window = node_info_window->window;
  if (!(node = nodes_catalog_find(&node_info_window->node_id)))
    {
      widget = g_object_get_data (G_OBJECT (window), "numeric_name_label");
      gtk_label_set_text (GTK_LABEL (widget), _("No info available"));
      widget = g_object_get_data (G_OBJECT (window), "average");
      gtk_label_set_text (GTK_LABEL (widget), "X");
      widget = g_object_get_data (G_OBJECT (window), "average_in");
      gtk_label_set_text (GTK_LABEL (widget), "X");
      widget = g_object_get_data (G_OBJECT (window), "average_out");
      gtk_label_set_text (GTK_LABEL (widget), "X");
      widget = g_object_get_data (G_OBJECT (window), "accumulated");
      gtk_label_set_text (GTK_LABEL (widget), "X");
      widget = g_object_get_data (G_OBJECT (window), "accumulated_in");
      gtk_label_set_text (GTK_LABEL (widget), "X");
      widget = g_object_get_data (G_OBJECT (window), "accumulated_out");
      gtk_label_set_text (GTK_LABEL (widget), "X");
      gtk_widget_queue_resize (GTK_WIDGET (node_info_window->window));
      return;
    }

  gtk_window_set_title (GTK_WINDOW (window), node->name->str);
  widget = g_object_get_data (G_OBJECT (window), "name_label");
  gtk_label_set_text (GTK_LABEL (widget), node->name->str);
  widget = g_object_get_data (G_OBJECT (window), "numeric_name_label");
  gtk_label_set_text (GTK_LABEL (widget), node->numeric_name->str);
  widget = g_object_get_data (G_OBJECT (window), "average");
  gtk_label_set_text (GTK_LABEL (widget),
		      traffic_to_str (node->node_stats.stats.average, TRUE));
  widget = g_object_get_data (G_OBJECT (window), "average_in");
  gtk_label_set_text (GTK_LABEL (widget),
		      traffic_to_str (node->node_stats.stats_in.average, TRUE));
  widget = g_object_get_data (G_OBJECT (window), "average_out");
  gtk_label_set_text (GTK_LABEL (widget),
		      traffic_to_str (node->node_stats.stats_out.average, TRUE));
  widget = g_object_get_data (G_OBJECT (window), "accumulated");
  gtk_label_set_text (GTK_LABEL (widget),
		      traffic_to_str (node->node_stats.stats.accumulated, FALSE));
  widget = g_object_get_data (G_OBJECT (window), "accumulated_in");
  gtk_label_set_text (GTK_LABEL (widget),
		      traffic_to_str (node->node_stats.stats_in.accumulated, FALSE));
  widget = g_object_get_data (G_OBJECT (window), "accumulated_out");
  gtk_label_set_text (GTK_LABEL (widget),
		      traffic_to_str (node->node_stats.stats_out.accumulated, FALSE));
  gtk_widget_queue_resize (GTK_WIDGET (node_info_window->window));
}				/* update_node_info_window */
