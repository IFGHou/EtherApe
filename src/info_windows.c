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
#include "appdata.h"
#include "info_windows.h"
#include "diagram.h"
#include "node.h"
#include "datastructs.h"
#include "protocols.h"
#include "capture.h"
#include "preferences.h"
#include "prot_types.h"
#include "ui_utils.h"
#include "node_windows.h"

typedef enum 
{
  PROTO_COLUMN_COLOR = 0,
  PROTO_COLUMN_NAME,
  PROTO_COLUMN_PORT,
  PROTO_COLUMN_INSTANTANEOUS,
  PROTO_COLUMN_ACCUMULATED,
  PROTO_COLUMN_AVGSIZE,
  PROTO_COLUMN_LASTHEARD,
  PROTO_COLUMN_PACKETS,
  PROTO_COLUMN_N        /* must be the last entry */
} PROTO_COLUMN;

typedef struct
{
  gchar *prot_name;
  GtkWidget *window;
}
prot_info_window_t;

typedef struct protocol_list_item_t_tag
{
  gchar *name; /* protocol name */
  uint port;     /* protocol port */
  GdkColor color; /* protocol color */
  basic_stats_t rowstats; 
} protocol_list_item_t;


/* list of active node/link info windows */
static GList *stats_info_windows = NULL;

/* list of active protocol info windows */
static GList *prot_info_windows = NULL;

/* private functions */
static void update_prot_info_windows (void);
static void update_prot_info_window (prot_info_window_t * prot_info_window);
static gint node_info_window_compare (gconstpointer a, gconstpointer b);
static gint prot_info_compare (gconstpointer a, gconstpointer b);
static void update_stats_info_windows (void);
static void update_node_protocols_window (GtkWidget *node_window);
static void update_link_info_window (GtkWidget *window);

/* ----------------------------------------------------------

   Generic Protocol Info Detail window functions (prot_info_windows)
   Used to display nodes and links detail data (and protos)

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
	glade_xml_new (appdata.glade_file, "prot_info", NULL);
      if (!xml_info_window)
	{
	  g_error (_("We could not load the interface! (%s)"),
		   appdata.glade_file);
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
  gchar *str;

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
  str = timeval_to_str (prot->stats.last_time);
  gtk_label_set_text (GTK_LABEL (widget), str);
  g_free(str);
  widget = g_object_get_data (G_OBJECT (window), "average");
  str = traffic_to_str (prot->stats.average, TRUE);
  gtk_label_set_text (GTK_LABEL (widget), str);
  g_free(str);
  widget = g_object_get_data (G_OBJECT (window), "accumulated");
  str = traffic_to_str (prot->stats.accumulated, FALSE);
  gtk_label_set_text (GTK_LABEL (widget), str);
  g_free(str);
  gtk_widget_queue_resize (GTK_WIDGET (prot_info_window->window));

}				/* update_prot_info_window */

/* cycles on all proto detail windows updating them */
void
update_prot_info_windows (void)
{
  GList *list_item = NULL, *remove_item;
  prot_info_window_t *prot_info_window = NULL;
  enum status_t status;

  status = get_capture_status();

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
  gdouble diffms;
  gint idcol;
  GtkSortType order;

  /* reads the proto ptr from 6th columns */
  const protocol_list_item_t *prot1, *prot2;
  gtk_tree_model_get (gs, a, PROTO_COLUMN_N, &prot1, -1);
  gtk_tree_model_get (gs, b, PROTO_COLUMN_N, &prot2, -1);
  if (!prot1 || !prot2)
    return 0;			/* error */

  /* gets sort info */
  gtk_tree_sortable_get_sort_column_id (GTK_TREE_SORTABLE (gs), &idcol,
					&order);

  switch (idcol)
    {
    case PROTO_COLUMN_COLOR: /* color */
    case PROTO_COLUMN_PORT: /* port */
    case PROTO_COLUMN_NAME: /* name */
    default:
      ret = strcmp (prot1->name, prot2->name);
      break;
    case PROTO_COLUMN_INSTANTANEOUS:
      t1 = prot1->rowstats.average;
      t2 = prot2->rowstats.average;
      if (t1 == t2)
	ret = 0;
      else if (t1 < t2)
	ret = -1;
      else
	ret = 1;
      break;
    case PROTO_COLUMN_ACCUMULATED:
      t1 = prot1->rowstats.accumulated;
      t2 = prot2->rowstats.accumulated;
      if (t1 == t2)
	ret = 0;
      else if (t1 < t2)
	ret = -1;
      else
	ret = 1;
      break;
    case PROTO_COLUMN_AVGSIZE:
      t1 = prot1->rowstats.avg_size;
      t2 = prot2->rowstats.avg_size;
      if (t1 == t2)
	ret = 0;
      else if (t1 < t2)
	ret = -1;
      else
	ret = 1;
      break;
    case PROTO_COLUMN_LASTHEARD:
      diffms = substract_times_ms(&prot1->rowstats.last_time, 
                                  &prot2->rowstats.last_time);
      if (diffms == 0)
	ret = 0;
      else if (diffms < 0)
	ret = -1;
      else
	ret = 1;
      break;
    case PROTO_COLUMN_PACKETS:
      if (prot1->rowstats.accu_packets == prot2->rowstats.accu_packets)
	ret = 0;
      else if (prot1->rowstats.accu_packets < prot2->rowstats.accu_packets)
	ret = -1;
      else
	ret = 1;
      break;
    }

  return ret;
}

static GtkListStore *create_protocols_table (GtkWidget *window)
{
  GtkTreeView *gv;
  GtkListStore *gs;
  GtkTreeViewColumn *gc;
  int i;

  /* get the treeview, if present */
  gv = retrieve_treeview(window);
  if (!gv)
    return NULL; 

  gs = GTK_LIST_STORE (gtk_tree_view_get_model (gv));
  if (gs)
    return gs;

  /* create the store  - it uses 9 values, 7 displayable, one proto color 
     and the data pointer */
  gs = gtk_list_store_new (9, GDK_TYPE_COLOR, G_TYPE_STRING, G_TYPE_STRING,
			   G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, 
			   G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
  gtk_tree_view_set_model (gv, GTK_TREE_MODEL (gs));

  /* the view columns and cell renderers must be also created ... 
     the first column is the proto color; is a text column wich renders
     only a space with a colored background. We must set some special properties
   */
  
  gc = gtk_tree_view_column_new_with_attributes
    (" ", gtk_cell_renderer_text_new(), "background-gdk", PROTO_COLUMN_COLOR, 
     NULL);
  g_object_set (G_OBJECT (gc), "resizable", TRUE, 
                               "reorderable", TRUE, 
                               NULL);
//  gtk_tree_view_column_set_sort_column_id(gc, PROTO_COLUMN_COLOR);
  gtk_tree_view_append_column (gv, gc);


  create_add_text_column(gv, "Protocol", PROTO_COLUMN_NAME, FALSE);
  create_add_text_column(gv, "Port", PROTO_COLUMN_PORT, TRUE);
  create_add_text_column(gv, "Inst Traffic", PROTO_COLUMN_INSTANTANEOUS, FALSE);
  create_add_text_column(gv, "Accum Traffic", PROTO_COLUMN_ACCUMULATED, FALSE);
  create_add_text_column(gv, "Avg Size", PROTO_COLUMN_AVGSIZE, FALSE);
  create_add_text_column(gv, "Last Heard", PROTO_COLUMN_LASTHEARD, FALSE);
  create_add_text_column(gv, "Packets", PROTO_COLUMN_PACKETS, FALSE);

  /* the sort functions ... */
  for (i = 0 ; i < PROTO_COLUMN_N ; ++i)
    gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (gs), i,
                                    protocols_table_compare, gs, NULL);

  /* initial sort order is by protocol */
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (gs), 
                                        PROTO_COLUMN_NAME,
					GTK_SORT_ASCENDING);

  return gs;
}

static void protocols_table_clear(GtkListStore *gs)
{
  gboolean res;
  GtkTreeIter it;

  res = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (gs), &it);
  while (res)
    {
      protocol_list_item_t *row_proto = NULL;
      gtk_tree_model_get (GTK_TREE_MODEL (gs), &it, PROTO_COLUMN_N, 
                          &row_proto, -1);
      if (row_proto)
        {
          g_free(row_proto->name);
          g_free(row_proto);
          res = gtk_list_store_remove (gs, &it);
        }
    }
}

static void update_protocols_row(GtkListStore *gs, GtkTreeIter *it, 
                                 const protocol_list_item_t *row_proto)
{
  gchar *ga, *gb, *gc, *gd, *ge, *gf;
  const port_service_t *ps;
  
  ga = traffic_to_str (row_proto->rowstats.accumulated, FALSE);
  gb = g_strdup_printf ("%lu", row_proto->rowstats.accu_packets);
  ps = services_port_find(row_proto->name);
  if (ps)
    gc = g_strdup_printf ("%d", ps->port);
  else
    gc = g_strdup("-");
  gd = traffic_to_str (row_proto->rowstats.average, TRUE);
  ge = timeval_to_str (row_proto->rowstats.last_time);
  gf = traffic_to_str (row_proto->rowstats.avg_size, FALSE);

  gtk_list_store_set (gs, it, 
                      PROTO_COLUMN_ACCUMULATED, ga, 
                      PROTO_COLUMN_PACKETS, gb,
                      PROTO_COLUMN_PORT, gc,
                      PROTO_COLUMN_INSTANTANEOUS, gd,
                      PROTO_COLUMN_LASTHEARD, ge, 
                      PROTO_COLUMN_AVGSIZE, gf, 
                      -1);

  g_free(ga);
  g_free(gb);
  g_free(gc);
  g_free(gd);
  g_free(ge);
  g_free(gf);
}

static void update_protocols_table(GtkWidget *window, const protostack_t *pstk)
{
  GtkListStore *gs;
  GList *item;
  gboolean res;
  GtkTreeIter it;

  gs = create_protocols_table (window);
  if (!gs)
    return; /* nothing to do */

  if (pstk)
    item = pstk->protostack[pref.stack_level];
  else
    item = NULL;
  
  res = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (gs), &it);
  while (res || item)
    {
      const protocol_t *stack_proto = NULL;
      protocol_list_item_t *row_proto = NULL;

      /* retrieve current row proto (if present) */
      if (res)
        gtk_tree_model_get (GTK_TREE_MODEL (gs), &it, PROTO_COLUMN_N, 
                            &row_proto, -1);

      if ( !item )
        {
          /* no more protos on stack, del current row (remove moves to next) */
          if (res)
            {
              g_free(row_proto->name);
              g_free(row_proto);
              res = gtk_list_store_remove (gs, &it);
            }
          continue;
        }

      /* retrieve current stack proto */
      stack_proto = item->data;

      if (!res)
        {
          /* current protocol missing on list, create a new row */
	  row_proto = g_malloc(sizeof(protocol_list_item_t));
          g_assert(row_proto);
          
          row_proto->name = g_strdup(stack_proto->name);
	  row_proto->color = protohash_color(stack_proto->name);

	  gtk_list_store_append(gs, &it);
	  gtk_list_store_set (gs, &it, 
                              PROTO_COLUMN_NAME, row_proto->name, 
                              PROTO_COLUMN_COLOR, &row_proto->color, 
                              PROTO_COLUMN_N, row_proto, 
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
      row_proto->rowstats = stack_proto->stats;
      update_protocols_row(gs, &it, row_proto);

      if (res)
        res = gtk_tree_model_iter_next (GTK_TREE_MODEL (gs), &it);

      item = item->next;
    }
}

/* ----------------------------------------------------------

   General Protocol Info window functions (protocols_window)
   Displays the global protocol table

   ---------------------------------------------------------- */

static void
update_protocols_window (void)
{
  GtkWidget *window;
  GtkTreeView *gv;

  window = glade_xml_get_widget (appdata.xml, "protocols_window");
  gv = retrieve_treeview(window);
  if (!gv)
    {
      /* register gv */
      gv = GTK_TREE_VIEW (glade_xml_get_widget (appdata.xml, "prot_clist"));
      if (!gv)
        {
          g_critical("can't find prot_clist");
          return;
        }
      register_treeview(window, gv);
    }
  
  update_protocols_table(window, protocol_summary_stack());
}				/* update_protocols_window */

void
toggle_protocols_window (void)
{
  GtkWidget *protocols_check = glade_xml_get_widget (appdata.xml, "protocols_check");
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
  GtkWidget *protocols_window = glade_xml_get_widget (appdata.xml, "protocols_window");
  if (!protocols_window)
    return;
  if (gtk_check_menu_item_get_active (menuitem))
    {
      gtk_widget_show (protocols_window);
      gdk_window_raise (protocols_window->window);
      update_protocols_window ();
    }
  else
    {
      /* retrieve view and model (store) */
      GtkListStore *gs;
      GtkTreeView *gv = GTK_TREE_VIEW(glade_xml_get_widget(appdata.xml, "prot_clist"));
      if (gv)
        {
          gs = GTK_LIST_STORE (gtk_tree_view_get_model (gv));
          if (gs)
             protocols_table_clear(gs);
        }
      gtk_widget_hide (protocols_window);
    }
}				/* on_protocols_check_activate */

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

  gtk_tree_model_get (GTK_TREE_MODEL (gs), &it, PROTO_COLUMN_N, &protocol, -1);
  create_prot_info_window (protocol);

  return TRUE;
}				/* on_prot_clist_select_row */




/* ----------------------------------------------------------

   Node Info window functions

   ---------------------------------------------------------- */
/* Comparison function used to compare stats_info_windows */
static gint
node_info_window_compare(gconstpointer a, gconstpointer b)
{
  const node_id_t *id_a;
  g_assert (a != NULL);
  g_assert (b != NULL);

  id_a = g_object_get_data (G_OBJECT (a), "node_id");
  if (!id_a)
    return -1; /* not a node window */

  return node_id_compare( id_a, b);
}

guint
update_info_windows (void)
{
  enum status_t status;

  status = get_capture_status();
  if (status != PLAY && status != STOP)
    return TRUE;

  gettimeofday (&appdata.now, NULL);

  update_protocols_window ();
  update_stats_info_windows ();
  update_prot_info_windows ();
  nodes_wnd_update();

  return TRUE;			/* Keep on calling this function */

}				/* update_info_windows */

/* It's called when a node info window is closed by the user 
 * It has to free memory and delete the window from the list
 * of windows */
gboolean
on_node_info_delete_event (GtkWidget * node_info, GdkEvent * evt,
			   gpointer user_data)
{
  stats_info_windows = g_list_remove(stats_info_windows, node_info);

  g_free(g_object_get_data (G_OBJECT (node_info), "node_id"));
  g_free(g_object_get_data (G_OBJECT (node_info), "link_id"));

  gtk_widget_destroy (node_info);

  return FALSE;
}				/* on_node_info_delete_event */


void
update_stats_info_windows (void)
{
  GList *list_item;
  GtkWidget *window;
  enum status_t status;
  double diffms;
  static struct timeval last_update_time = {
    0, 0
  };

  status = get_capture_status();

  /* Update info windows at most twice a second */
  diffms = substract_times_ms(&appdata.now, &last_update_time);
  if (pref.refresh_period < 500)
    if (diffms < 500)
      return;

  list_item = stats_info_windows;
  while (list_item)
    {
      window = list_item->data;
      if (status == STOP)
	{
          GList *remove_item = list_item;
	  list_item = list_item->next;
	  stats_info_windows =
	    g_list_delete_link (stats_info_windows, remove_item);
          g_free(g_object_get_data (G_OBJECT (window), "node_id"));
          g_free(g_object_get_data (G_OBJECT (window), "link_id"));
	  gtk_widget_destroy (GTK_WIDGET (window));
	}
      else
	{
          if (g_object_get_data (G_OBJECT (window), "node_id"))
            {
              /* is a node info window */
              update_node_protocols_window (window);
            }
          else
            update_link_info_window (window);
	  list_item = list_item->next;
	}
    }

  last_update_time = appdata.now;
}				/* update_stats_info_windows */


/****************************************************************************
 *
 * node protocols display
 *
 ***************************************************************************/
static GtkWidget *
stats_info_create(const gchar *idkey, gpointer key)
{
  GtkWidget *window;
  GladeXML *xml_info_window;
  GtkTreeView *gv;

  xml_info_window =
    glade_xml_new (appdata.glade_file, "node_proto_info", NULL);
  if (!xml_info_window)
    {
      g_error (_("We could not load the interface! (%s)"),
               appdata.glade_file);
      return NULL;
    }
  glade_xml_signal_autoconnect (xml_info_window);
  window = glade_xml_get_widget (xml_info_window, "node_proto_info");
  gtk_widget_show (window);

  /* register the widgets in the window */
  register_glade_widget(xml_info_window, G_OBJECT (window), "src_label");
  register_glade_widget(xml_info_window, G_OBJECT (window), "dst_label");
  register_glade_widget(xml_info_window, G_OBJECT (window), "node_iproto_name");
  register_glade_widget(xml_info_window, G_OBJECT (window), 
                          "node_iproto_numeric_name");
  register_glade_widget(xml_info_window, G_OBJECT (window), "node_iproto_name_b");
  register_glade_widget(xml_info_window, G_OBJECT (window), 
                          "node_iproto_numeric_name_b");
  register_glade_widget(xml_info_window, G_OBJECT (window), "total_label");
  register_glade_widget(xml_info_window, G_OBJECT (window), "inbound_label");
  register_glade_widget(xml_info_window, G_OBJECT (window), "outbound_label");
  register_glade_widget(xml_info_window, G_OBJECT (window), "node_iproto_avg");
  register_glade_widget(xml_info_window, G_OBJECT (window), "node_iproto_avg_in");
  register_glade_widget(xml_info_window, G_OBJECT (window), "node_iproto_avg_out");
  register_glade_widget(xml_info_window, G_OBJECT (window), "node_iproto_accum");
  register_glade_widget(xml_info_window, G_OBJECT (window), "node_iproto_accum_in");
  register_glade_widget(xml_info_window, G_OBJECT (window), "node_iproto_accum_out");
  register_glade_widget(xml_info_window, G_OBJECT (window), "node_iproto_avgsize");
  register_glade_widget(xml_info_window, G_OBJECT (window), "node_iproto_avgsize_in");
  register_glade_widget(xml_info_window, G_OBJECT (window), "node_iproto_avgsize_out");

  /* get and register the tree view */
  gv = GTK_TREE_VIEW (glade_xml_get_widget (xml_info_window, "node_iproto_proto"));
  register_treeview(window, gv);

  /* create columns of proto list */
  create_protocols_table (window);

  g_object_unref (xml_info_window);
  
  /* register key info */
  g_object_set_data (G_OBJECT (window), idkey, key);

  /* insert into list */
  stats_info_windows = g_list_prepend (stats_info_windows, window);

  return window;
}


static void
stats_info_update(GtkWidget *window, const traffic_stats_t *stats)
{
  if (!stats)
    {
      update_gtklabel(window, "node_iproto_avg", "X");
      update_gtklabel(window, "node_iproto_accum", "X");
      update_gtklabel(window, "node_iproto_avgsize", "X");
      update_gtklabel(window, "node_iproto_avg_in", "X");
      update_gtklabel(window, "node_iproto_avg_out", "X");
      update_gtklabel(window, "node_iproto_accum_in", "X");
      update_gtklabel(window, "node_iproto_accum_out", "X");
      update_gtklabel(window, "node_iproto_avgsize_in", "X");
      update_gtklabel(window, "node_iproto_avgsize_out", "X");
      update_protocols_table(window, NULL);
      gtk_widget_queue_resize (GTK_WIDGET (window));
    }
  else
    {
      gchar *str;
      str = traffic_to_str (stats->stats.average, TRUE);
      update_gtklabel(window, "node_iproto_avg", str);
      g_free(str);
      str = traffic_to_str (stats->stats.accumulated, FALSE);
      update_gtklabel(window, "node_iproto_accum", str);
      g_free(str);
      str = traffic_to_str (stats->stats.avg_size, FALSE);
      update_gtklabel(window, "node_iproto_avgsize", str);
      g_free(str);
      str = traffic_to_str (stats->stats_in.average, TRUE);
      update_gtklabel(window, "node_iproto_avg_in", str);
      g_free(str);
      str = traffic_to_str (stats->stats_out.average, TRUE);
      update_gtklabel(window, "node_iproto_avg_out", str);
      g_free(str);
      str = traffic_to_str (stats->stats_in.accumulated, FALSE);
      update_gtklabel(window, "node_iproto_accum_in", str);
      g_free(str);
      str = traffic_to_str (stats->stats_out.accumulated, FALSE);
      update_gtklabel(window, "node_iproto_accum_out", str);
      g_free(str);
      str = traffic_to_str (stats->stats_in.avg_size, FALSE);
      update_gtklabel(window, "node_iproto_avgsize_in", str);
      g_free(str);
      str = traffic_to_str (stats->stats_out.avg_size, FALSE);
      update_gtklabel(window, "node_iproto_avgsize_out", str);
      g_free(str);
      /* update protocol table */
      update_protocols_table(window, &stats->stats_protos);
    }
}


void
node_protocols_window_create(const node_id_t * node_id)
{
  GtkWidget *window;
  GList *list_item;

  /* If there is already a window, we don't need to create it again */
  list_item = g_list_find_custom (stats_info_windows, 
                                  node_id, node_info_window_compare);
  if (!list_item)
    {
      window = stats_info_create("node_id", g_memdup(node_id, sizeof(node_id_t)));
      if (!window)
        return; /* creation failed */

      /* nodes don't need secondary names */
      update_gtklabel(window, "node_iproto_name_b", "");
      update_gtklabel(window, "node_iproto_numeric_name_b", "");
    }
  else
    window = list_item->data;
  update_node_protocols_window (window);
}				/* node_protocols_window_create */

static void
update_node_protocols_window (GtkWidget *window)
{
  const node_id_t *node_id;
  const node_t *node;

  node_id = g_object_get_data (G_OBJECT (window), "node_id");
  node = nodes_catalog_find(node_id);
  if (!node)
    {
      /* node expired, clear stats */
      update_gtklabel(window, "node_iproto_numeric_name", _("Node timed out"));
      stats_info_update(window, NULL);
      return;
    }

  gtk_window_set_title (GTK_WINDOW (window), node->name->str);
  
  update_gtklabel(window, "node_iproto_name", node->name->str);
  update_gtklabel(window, "node_iproto_numeric_name", node->numeric_name->str);
  
  stats_info_update(window, &node->node_stats);
}

/****************************************************************************
 *
 * link protocols display
 *
 ***************************************************************************/
/* Comparison function used to compare stats_info_windows */
static gint
link_info_window_compare(gconstpointer a, gconstpointer b)
{
  const link_id_t *id_a;
  g_assert (a != NULL);
  g_assert (b != NULL);

  id_a = g_object_get_data (G_OBJECT (a), "link_id");
  if (!id_a)
    return -1; /* not a link window */

  return link_id_compare( id_a, b);
}

void
link_info_window_create(const link_id_t * link_id)
{
  GtkWidget *window;
  GList *list_item;

  /* If there is already a window, we don't need to create it again */
  list_item = g_list_find_custom (stats_info_windows, 
                                  link_id, link_info_window_compare);
  if (!list_item)
    {
      window = stats_info_create("link_id", g_memdup(link_id, sizeof(link_id_t)));
      if (!window)
        return; /* creation failed */
    }
  else
    window = list_item->data;
  update_link_info_window (window);
}

static void
update_link_info_window (GtkWidget *window)
{
  const link_id_t *link_id;
  const link_t *link;
  const node_t *node;
  gchar *linkname;

  /* updates column descriptions */
  show_widget(window, "src_label");
  show_widget(window, "dst_label");
  update_gtklabel(window, "inbound_label", _("B->A"));
  update_gtklabel(window, "outbound_label", _("A->B"));

  link_id = g_object_get_data (G_OBJECT (window), "link_id");
  link = links_catalog_find(link_id);
  if (!link)
    {
      /* node expired, clear stats */
      update_gtklabel(window, "node_iproto_numeric_name", _("Link timed out"));
      update_gtklabel(window, "node_iproto_numeric_name_b", "");
      stats_info_update(window, NULL);
      return;
    }

  linkname = link_id_node_names(link_id);
  gtk_window_set_title (GTK_WINDOW (window), linkname);
  g_free(linkname);

  node = nodes_catalog_find(&link_id->src);
  if (node)
  {
    update_gtklabel(window, "node_iproto_name", node->name->str);
    update_gtklabel(window, "node_iproto_numeric_name", node->numeric_name->str);
  }
  else
  {
    update_gtklabel(window, "node_iproto_name", _("Node timed out"));
    update_gtklabel(window, "node_iproto_numeric_name", _("Node timed out"));
  }

  node = nodes_catalog_find(&link_id->dst);
  if (node)
  {
    update_gtklabel(window, "node_iproto_name_b", node->name->str);
    update_gtklabel(window, "node_iproto_numeric_name_b", node->numeric_name->str);
  }
  else
  {
    update_gtklabel(window, "node_iproto_name_b", _("Node timed out"));
    update_gtklabel(window, "node_iproto_numeric_name_b", _("Node timed out"));
  }

  stats_info_update(window, &link->link_stats);
}
