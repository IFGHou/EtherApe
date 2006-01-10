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
#include "datastructs.h"
#include "protocols.h"

typedef struct
{
  node_id_t node_id;
  GtkWidget *window;
}
node_info_window_t;

typedef struct
{
  gchar *prot_name;
  GtkWidget *window;
}
prot_info_window_t;

typedef struct protocol_list_item_t_tag
{
  gchar *name; /* protocol name */
  GdkColor color; /* protocol color */
} protocol_list_item_t;


/* list of active node info windows */
static GList *node_info_windows = NULL;

/* list of active protocol info windows */
static GList *prot_info_windows = NULL;

/* private functions */
static void update_prot_info_windows (void);
static void update_node_info_window (node_info_window_t * node_info_window);
static void update_prot_info_window (prot_info_window_t * prot_info_window);
static gint node_info_window_compare (gconstpointer a, gconstpointer b);
static gint prot_info_compare (gconstpointer a, gconstpointer b);
static void update_node_info_windows (void);
static void update_node_protocols_window (node_info_window_t * node_info_window);

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

/* registers the named glade widget on the specified object */
static void
register_glade_widget(GladeXML *xm, GObject *tgt, const gchar *widgetName)
{
  GtkWidget *widget;
  widget = glade_xml_get_widget (xm, widgetName);
  g_object_set_data (tgt, widgetName, widget);
}

static void
update_gtklabel(GtkWidget *window, const gchar *lblname, const gchar *value)
{
  GtkWidget *widget = g_object_get_data (G_OBJECT (window), lblname);
  gtk_label_set_text (GTK_LABEL (widget), value);
}

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
  const protocol_t *prot;
  GtkWidget *window;
  GtkWidget *widget;

  window = prot_info_window->window;
  if (!window)
    return;

  prot = protocol_summary_find(pref.stack_level, prot_info_window->prot_name);
  if (!prot)
    {
      /* protocol expired */
      widget = g_object_get_data (G_OBJECT (window), "average");
      gtk_label_set_text (GTK_LABEL (widget), "X");
      widget = g_object_get_data (G_OBJECT (window), "accumulated");
      gtk_label_set_text (GTK_LABEL (widget), "X");
      gtk_widget_queue_resize (GTK_WIDGET (prot_info_window->window));
      return;
    }

  gtk_window_set_title (GTK_WINDOW (window), prot_info_window->prot_name);
  widget = g_object_get_data (G_OBJECT (window), "name_label");
  gtk_label_set_text (GTK_LABEL (widget), prot_info_window->prot_name);
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


/***************************************************************
 *
 * protocol table handling functions 
 *
 ***************************************************************/
#define MAX_C(a,b) ((a) > (b) ? (a) : (b))
#define MIN_C(a,b) ((a) > (b) ? (a) : (b))
#define LUMINANCE(r,g,b) ((MAX_C( (double)(r)/0xFFFF, MAX_C( (double)(g)/0xFFFF, \
                                  (double)(b)/0xFFFF)) + \
                          MIN_C( (double)(r)/0xFFFF, MIN_C( (double)(g)/0xFFFF, \
                                  (double)(b)/0xFFFF))) / 2.0)

/* Comparison functions used to sort the clist */
static gint
protocols_table_compare (GtkTreeModel * gs, GtkTreeIter * a, GtkTreeIter * b,
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
create_protocols_table (GtkWidget *window, GtkTreeView *gv)
{
  GtkListStore *gs;
  GtkTreeViewColumn *gc;

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
				   protocols_table_compare, gs, NULL);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (gs), 1,
				   protocols_table_compare, gs, NULL);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (gs), 2,
				   protocols_table_compare, gs, NULL);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (gs), 3,
				   protocols_table_compare, gs, NULL);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (gs), 4,
				   protocols_table_compare, gs, NULL);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (gs), 6,
				   protocols_table_compare, gs, NULL);

  /* initial sort order is by protocol */
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (gs), 0,
					GTK_SORT_ASCENDING);

  /* register the treeview in the window */
  g_object_set_data ( G_OBJECT(window), "proto_clist", gv);
}

static void
update_protocols_table(GtkListStore *gs, const protostack_t *pstk)
{
  GList *item;
  gchar *str;
  gboolean res;
  GtkTreeIter it;

  if (!gs || !pstk)
    return; /* nothing to do */

  item = pstk->protostack[pref.stack_level];
  res = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (gs), &it);
  while (res || item)
    {
      const protocol_t *stack_proto = NULL;
      protocol_list_item_t *row_proto = NULL;

      /* retrieve current row proto (if present) */
      if (res)
        gtk_tree_model_get (GTK_TREE_MODEL (gs), &it, 5, &row_proto, -1);

      if ( !item )
        {
          /* no more protos on stack, current row (remove moves to next) */
          g_free(row_proto->name);
          g_free(row_proto);
          res = gtk_list_store_remove (gs, &it);
          continue;
        }

      /* retrieve current stack proto */
      stack_proto = item->data;

      if (!res)
        {
          /* current protocol missing on list, create a new row */
	  row_proto = g_malloc(sizeof(protocol_list_item_t));
          row_proto->name = g_strdup(stack_proto->name);
	  row_proto->color = protohash_get(stack_proto->name);

	  gtk_list_store_append(gs, &it);
	  gtk_list_store_set (gs, &it, 
                              0, row_proto->name, 
                              5, row_proto, 
                              6, &row_proto->color, 
                              -1);
        }
      else
        {
          /* we have both items, check names */
          if (strcmp(row_proto->name, stack_proto->name))
            {
              /* names not identical, delete current row  (remove moves to next) */
              g_free(row_proto->name);
              g_free(row_proto);
              res = gtk_list_store_remove (gs, &it);
              continue;
            }
        }

      /* everything ok, update stats */
      str = traffic_to_str (stack_proto->accumulated, FALSE);
      gtk_list_store_set (gs, &it, 2, str, -1);

      str = g_strdup_printf ("%d", stack_proto->proto_packets);
      gtk_list_store_set (gs, &it, 4, str, -1);
      g_free (str);

      str = traffic_to_str (stack_proto->average, TRUE);
      gtk_list_store_set (gs, &it, 1, str, -1);

      str = timeval_to_str (stack_proto->last_heard);
      gtk_list_store_set (gs, &it, 3, str, -1);

      if (res)
        res = gtk_tree_model_iter_next (GTK_TREE_MODEL (gs), &it);

      item = item->next;
    }
}

/* common function to activate the proto columns */
static void
activate_protocols_info_column (GtkMenuItem * gm, guint column)
{
  GtkTreeViewColumn *gc;
  GtkTreeView *gv = GTK_TREE_VIEW (glade_xml_get_widget (xml, "prot_clist"));
//  GtkTreeView *gv = GTK_TREE_VIEW (g_object_get_data (window, "proto_clist"));
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


/* ----------------------------------------------------------

   General Protocol Info window functions (protocols_window) 

   ---------------------------------------------------------- */

static void
update_protocols_window (void)
{
  GtkListStore *gs = NULL;

  /* retrieve view and model (store) */
  GtkTreeView *gv = GTK_TREE_VIEW (glade_xml_get_widget (xml, "prot_clist"));
  if (!gv || !GTK_WIDGET_VISIBLE (gv))
    return;			/* error or hidden */
  gs = GTK_LIST_STORE (gtk_tree_view_get_model (gv));
  if (!gs)
    {
      create_protocols_table (glade_xml_get_widget (xml, "protocols_window"), gv);
      gs = GTK_LIST_STORE (gtk_tree_view_get_model (gv));
      if (!gs)
        return;			/* error */
    }

  update_protocols_table(gs, protocol_summary_stack());

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

      /* register the widgets in the window */
      register_glade_widget(xml_info_window, G_OBJECT (window), "node_info_name_label");
      register_glade_widget(xml_info_window, G_OBJECT (window), 
			      "node_info_numeric_name_label");
      register_glade_widget(xml_info_window, G_OBJECT (window), "node_info_average");
      register_glade_widget(xml_info_window, G_OBJECT (window), "node_info_average_in");
      register_glade_widget(xml_info_window, G_OBJECT (window), "node_info_average_out");
      register_glade_widget(xml_info_window, G_OBJECT (window), "node_info_accumulated");
      register_glade_widget(xml_info_window, G_OBJECT (window), "node_info_accumulated_in");
      register_glade_widget(xml_info_window, G_OBJECT (window), "node_info_accumulated_out");

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
          if (pref.new_infodlg)
            update_node_protocols_window (node_info_window);
          else
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
  node_id_t *node_id;
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
  node_info_windows = g_list_delete_link (node_info_windows, item);
  g_free(node_id);

  return FALSE;
}				/* on_node_info_delete_event */

static void
update_node_info_window (node_info_window_t * node_info_window)
{
  node_t *node = NULL;
  GtkWidget *window;
  
  window = node_info_window->window;
  if (!(node = nodes_catalog_find(&node_info_window->node_id)))
    {
      update_gtklabel(window, "node_info_numeric_name_label", _("Node timed out"));
      update_gtklabel(window, "node_info_average", "X");
      update_gtklabel(window, "node_info_average_in", "X");
      update_gtklabel(window, "node_info_average_out", "X");
      update_gtklabel(window, "node_info_accumulated", "X");
      update_gtklabel(window, "node_info_accumulated_in", "X");
      update_gtklabel(window, "node_info_accumulated_out", "X");
      gtk_widget_queue_resize (GTK_WIDGET (window));
      return;
    }

  gtk_window_set_title (GTK_WINDOW (window), node->name->str);
  
  update_gtklabel(window, "node_info_name_label", node->name->str);
  update_gtklabel(window, "node_info_numeric_name_label", node->numeric_name->str);
  update_gtklabel(window, "node_info_average",
		      traffic_to_str (node->node_stats.stats.average, TRUE));
  update_gtklabel(window, "node_info_average_in",
		      traffic_to_str (node->node_stats.stats_in.average, TRUE));
  update_gtklabel(window, "node_info_average_out",
		      traffic_to_str (node->node_stats.stats_out.average, TRUE));
  update_gtklabel(window, "node_info_accumulated",
		      traffic_to_str (node->node_stats.stats.accumulated, FALSE));
  update_gtklabel(window, "node_info_accumulated_in",
		      traffic_to_str (node->node_stats.stats_in.accumulated, FALSE));
  update_gtklabel(window, "node_info_accumulated_out",
		      traffic_to_str (node->node_stats.stats_out.accumulated, FALSE));
  gtk_widget_queue_resize (GTK_WIDGET (window));
}				/* update_node_info_window */

/****************************************************************************
 *
 * node protocols display
 *
 ***************************************************************************/
void
node_protocols_window_create(const node_id_t * node_id)
{
  GtkWidget *window;
  node_info_window_t *node_info_window;
  GladeXML *xml_info_window;
  GList *list_item;
  node_info_window_t key;
  GtkTreeView *gv;

  key.node_id = *node_id;

  /* If there is already a window, we don't need to create it again */
  if (!(list_item =
	g_list_find_custom (node_info_windows, &key, node_info_window_compare)))
    {
      xml_info_window =
	glade_xml_new (GLADEDIR "/" ETHERAPE_GLADE_FILE, "node_proto_info", NULL);
      if (!xml_info_window)
	{
	  g_error (_("We could not load the interface! (%s)"),
		   GLADEDIR "/" ETHERAPE_GLADE_FILE);
	  return;
	}
      glade_xml_signal_autoconnect (xml_info_window);
      window = glade_xml_get_widget (xml_info_window, "node_proto_info");
      gtk_widget_show (window);

      /* register the widgets in the window */
      register_glade_widget(xml_info_window, G_OBJECT (window), "node_iproto_name");
      register_glade_widget(xml_info_window, G_OBJECT (window), 
			      "node_iproto_numeric_name");
      register_glade_widget(xml_info_window, G_OBJECT (window), "node_iproto_avg");
      register_glade_widget(xml_info_window, G_OBJECT (window), "node_iproto_avg_in");
      register_glade_widget(xml_info_window, G_OBJECT (window), "node_iproto_avg_out");
      register_glade_widget(xml_info_window, G_OBJECT (window), "node_iproto_accum");
      register_glade_widget(xml_info_window, G_OBJECT (window), "node_iproto_accum_in");
      register_glade_widget(xml_info_window, G_OBJECT (window), "node_iproto_accum_out");
      register_glade_widget(xml_info_window, G_OBJECT (window), "node_iproto_proto");

      /* create columns of proto list */
      gv = GTK_TREE_VIEW (glade_xml_get_widget (xml_info_window, "node_iproto_proto"));
      create_protocols_table (window, gv);

      g_object_unref (xml_info_window);
      node_info_window = g_malloc (sizeof (node_info_window_t));
      node_info_window->node_id = *node_id;
      g_object_set_data (G_OBJECT (window), "node_id",
			 g_memdup(&node_info_window->node_id, sizeof(node_id_t)));
      node_info_window->window = window;
      node_info_windows = g_list_prepend (node_info_windows, node_info_window);
    }
  else
    node_info_window = (node_info_window_t *) list_item->data;
  update_node_protocols_window (node_info_window);
}				/* node_info_window_create */


static void
update_node_protocols_window (node_info_window_t * node_info_window)
{
  GtkListStore *gs;
  GtkTreeView *gv;
  node_t *node;
  GtkWidget *window;
  
  window = node_info_window->window;

  /* retrieve view and model (store) */
  gv = GTK_TREE_VIEW (g_object_get_data (G_OBJECT (window), "node_iproto_proto"));
  if (!gv)
    return;			/* error or hidden */
  gs = GTK_LIST_STORE (gtk_tree_view_get_model (gv));
  if (!gs)
     return;			/* error */

  if (!(node = nodes_catalog_find(&node_info_window->node_id)))
    {
      /* node expired, remove */
      update_gtklabel(window, "node_iproto_numeric_name", _("Node timed out"));
      update_gtklabel(window, "node_iproto_avg", "X");
      update_gtklabel(window, "node_iproto_avg_in", "X");
      update_gtklabel(window, "node_iproto_avg_out", "X");
      update_gtklabel(window, "node_iproto_accum", "X");
      update_gtklabel(window, "node_iproto_accum_in", "X");
      update_gtklabel(window, "node_iproto_accum_out", "X");
      gtk_list_store_clear(gs);
      gtk_widget_queue_resize (GTK_WIDGET (window));
      return;
    }

  gtk_window_set_title (GTK_WINDOW (window), node->name->str);
  
  update_gtklabel(window, "node_iproto_name", node->name->str);
  update_gtklabel(window, "node_iproto_numeric_name", node->numeric_name->str);
  update_gtklabel(window, "node_iproto_avg",
		      traffic_to_str (node->node_stats.stats.average, TRUE));
  update_gtklabel(window, "node_iproto_avg_in",
		      traffic_to_str (node->node_stats.stats_in.average, TRUE));
  update_gtklabel(window, "node_iproto_avg_out",
		      traffic_to_str (node->node_stats.stats_out.average, TRUE));
  update_gtklabel(window, "node_iproto_accum",
		      traffic_to_str (node->node_stats.stats.accumulated, FALSE));
  update_gtklabel(window, "node_iproto_accum_in",
		      traffic_to_str (node->node_stats.stats_in.accumulated, FALSE));
  update_gtklabel(window, "node_iproto_accum_out",
		      traffic_to_str (node->node_stats.stats_out.accumulated, FALSE));

  /* update protocol table */
  update_protocols_table(gs, &node->node_stats.stats_protos);
}
