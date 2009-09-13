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
#include "node_windows.h"
#include "ui_utils.h"
#include "node.h"

typedef enum 
{
  NODES_COLUMN_NAME = 0,
  NODES_COLUMN_NUMERIC,
  NODES_COLUMN_INSTANTANEOUS,
  NODES_COLUMN_ACCUMULATED,
  NODES_COLUMN_LASTHEARD,
  NODES_COLUMN_PACKETS,
  NODES_COLUMN_N        /* must be the last entry */
} NODES_COLUMN;


static GtkWidget *nodes_wnd = NULL;	       /* Ptr to nodes window */
static GtkCheckMenuItem *nodes_check = NULL;   /* Ptr to nodes menu */

/* private functions */
static void init_all_nodes(GtkWidget *wnd);
static void nodes_table_clear(GtkWidget *window);
static void nodes_table_update(GtkWidget *window);

void nodes_wnd_show(void)
{
  nodes_wnd = glade_xml_get_widget (xml, "nodes_wnd");
  nodes_check = GTK_CHECK_MENU_ITEM(glade_xml_get_widget (xml, "nodes_check"));

  if (!nodes_wnd || GTK_WIDGET_VISIBLE (nodes_wnd))
    return;
  
  gtk_widget_show (nodes_wnd);
  gdk_window_raise (nodes_wnd->window);
  if (nodes_check && !gtk_check_menu_item_get_active(nodes_check))
    gtk_check_menu_item_set_active(nodes_check, TRUE);
}

void nodes_wnd_hide(void)
{
  if (!nodes_wnd || !GTK_WIDGET_VISIBLE (nodes_wnd))
    return;
  gtk_widget_hide (nodes_wnd);
  nodes_table_clear(nodes_wnd);
  if (nodes_check && gtk_check_menu_item_get_active(nodes_check))
    gtk_check_menu_item_set_active(nodes_check, FALSE);
}

void nodes_wnd_update(void)
{
  if (!nodes_wnd || !GTK_WIDGET_VISIBLE (nodes_wnd))
    return;

  nodes_table_update(nodes_wnd);

  /* with viewport, no need to resize 
   gtk_widget_queue_resize (GTK_WIDGET (nodes_wnd)); */
}				/* update_nodes_wnd */

/***************************************************************
 *
 * node table handling functions 
 *
 ***************************************************************/

/* Comparison functions used to sort the clist */
static gint
nodes_table_compare (GtkTreeModel * gls, GtkTreeIter * a, GtkTreeIter * b, 
                     gpointer data)
{
  gint ret = 0;
  gdouble t1, t2;
  struct timeval time1, time2, diff;
  const node_id_t *nodeid1, *nodeid2;
  const node_t *node1, *node2;

  gtk_tree_model_get (gls, a, NODES_COLUMN_N, &nodeid1, -1);
  gtk_tree_model_get (gls, b, NODES_COLUMN_N, &nodeid2, -1);
  if (!nodeid1 || !nodeid2)
    return 0;			/* error */ 
  
  /* retrieve nodes. One or more might be expired */
  node1 = nodes_catalog_find(nodeid1);
  node2 = nodes_catalog_find(nodeid2);
  if (!node1 && !node2)
    return 0;	        
  else if (!node1)      
    return -1;          
  else if (!node2)
    return 1;
  
  /* gets sort info */
  switch (GPOINTER_TO_INT(data))
    {
    case NODES_COLUMN_NAME: /* name */
    default:
      ret = strcmp (node1->name->str, node2->name->str);
      break;
    case NODES_COLUMN_NUMERIC: /* id */
      ret = node_id_compare(&(node1->node_id), &(node2->node_id));
      break;
    case NODES_COLUMN_INSTANTANEOUS:
      t1 = node1->node_stats.stats.average;
      t2 = node2->node_stats.stats.average;
      if (t1 == t2)
	ret = 0;
      else if (t1 < t2)
	ret = -1;
      else
	ret = 1;
      break;
    case NODES_COLUMN_ACCUMULATED:
      t1 = node1->node_stats.stats.accumulated;
      t2 = node2->node_stats.stats.accumulated;
      if (t1 == t2)
	ret = 0;
      else if (t1 < t2)
	ret = -1;
      else
	ret = 1;
      break;
    case NODES_COLUMN_LASTHEARD:
      time1 = node1->node_stats.stats.last_time;
      time2 = node2->node_stats.stats.last_time;
      diff = substract_times (time1, time2);
      if ((diff.tv_sec == 0) && (diff.tv_usec == 0))
	ret = 0;
      else if ((diff.tv_sec < 0) || (diff.tv_usec < 0))
	ret = -1;
      else
	ret = 1;
      break;
    case NODES_COLUMN_PACKETS:
      if (node1->node_stats.stats.accu_packets == node2->node_stats.stats.accu_packets)
	ret = 0;
      else if (node1->node_stats.stats.accu_packets < node2->node_stats.stats.accu_packets)
	ret = -1;
      else
	ret = 1;
      break;
    }

  return ret;
}

/* retrieves the data store creating if needed */
static GtkListStore *nodes_table_create(GtkWidget *window)
{
  GtkTreeView *gv;
  GtkListStore *gls;
  GtkTreeModel *sort_model;
  GtkTreeViewColumn *gc;
  int i;

  /* get the treeview */
  gv = GTK_TREE_VIEW(g_object_get_data ( G_OBJECT(window), "gv"));
  if (!gv)
    {
      gv = GTK_TREE_VIEW (glade_xml_get_widget (xml, "nodes_table"));
      if (!gv)
        {
          g_error("can't find nodes_table");
          return NULL;
        }
      g_object_set_data ( G_OBJECT(window), "gv", gv);
    }

  sort_model = gtk_tree_view_get_model(gv);
  if (sort_model)
    {
      gls = GTK_LIST_STORE(gtk_tree_model_sort_get_model
                            (GTK_TREE_MODEL_SORT(sort_model)));
      if (gls)
        return gls; 
    }
  
  /* create the store. it uses 7 values, six displayable and the data pointer */
  gls = gtk_list_store_new (7, G_TYPE_STRING, G_TYPE_STRING,
			   G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, 
			   G_TYPE_STRING, G_TYPE_POINTER);

  /* to update successfully, the list store must be unsorted. So we wrap it */
  sort_model = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL(gls));
  gtk_tree_view_set_model (gv, GTK_TREE_MODEL (sort_model));

  create_add_text_column(gv, "Name", NODES_COLUMN_NAME, FALSE);
  create_add_text_column(gv, "Address", NODES_COLUMN_NUMERIC, TRUE);
  create_add_text_column(gv, "Inst Traffic", NODES_COLUMN_INSTANTANEOUS, FALSE);
  create_add_text_column(gv, "Accum Traffic", NODES_COLUMN_ACCUMULATED, FALSE);
  create_add_text_column(gv, "Last Heard", NODES_COLUMN_LASTHEARD, FALSE);
  create_add_text_column(gv, "Packets", NODES_COLUMN_PACKETS, FALSE);

  /* the sort functions ... */
  for (i = 0 ; i < NODES_COLUMN_N ; ++i)
    gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (sort_model), i,
                                     nodes_table_compare, 
                                     GINT_TO_POINTER(i), NULL);

  /* initial sort order is by name */
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort_model), 
                                        NODES_COLUMN_NAME,
                                        GTK_SORT_ASCENDING);

  return gls;
}

static void nodes_table_clear(GtkWidget *window)
{
  /* releases the datastore */
  GtkTreeView *gv;
  GtkListStore *gs;
  GtkTreeIter it;
  gboolean res;

  gv = GTK_TREE_VIEW(g_object_get_data ( G_OBJECT(window), "gv"));
  if (!gv)
    return; /* gv not registered, store doesn't exists */
  
  gs = GTK_LIST_STORE (gtk_tree_view_get_model (gv));
  if (!gs)
    return; /* nothing to do */

  res = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (gs), &it);
  while (res)
    {
      node_id_t *rowitem = NULL;
      gtk_tree_model_get (GTK_TREE_MODEL (gs), &it, 
                          NODES_COLUMN_N, &rowitem, -1);
      g_free(rowitem);
      res = gtk_list_store_remove (gs, &it);
    }
}

static void nodes_table_update_row(GtkListStore *gs, GtkTreeIter *it, 
                                   const node_t * node)
{
  gchar *sa, *sb, *sc, *sd;
  const basic_stats_t *stats = (const basic_stats_t *) &(node->node_stats.stats);
  
  sa = traffic_to_str (stats->average, TRUE);
  sb = traffic_to_str (stats->accumulated, FALSE);
  sc = g_strdup_printf ("%.0f", stats->accu_packets);
  sd = timeval_to_str (stats->last_time);

  gtk_list_store_set (gs, it, 
                      NODES_COLUMN_NAME, node->name->str, 
                      NODES_COLUMN_NUMERIC, node->numeric_name->str, 
                      NODES_COLUMN_INSTANTANEOUS, sa,
                      NODES_COLUMN_ACCUMULATED, sb,
                      NODES_COLUMN_PACKETS, sc,
                      NODES_COLUMN_LASTHEARD, sd,
                      -1);
  g_free (sc);
}

struct NTTraverse
{
  GtkListStore *gs;
  GtkTreeIter it;
  gboolean res;
};

static gboolean nodes_table_iterate(gpointer key, gpointer value, gpointer data)
{
  const node_t *node = (const node_t *)value;
  struct NTTraverse *tt = (struct NTTraverse *)data;
  node_id_t *rowitem;

  while (tt->res)
    {
      gtk_tree_model_get (GTK_TREE_MODEL (tt->gs), &(tt->it), 
                          NODES_COLUMN_N, &rowitem, -1);

      switch (node_id_compare(&(node->node_id), rowitem))
        {
        case 0:
            /* curnode==cur row, update results and go next */
            nodes_table_update_row(tt->gs, &(tt->it), node);
            tt->res = gtk_tree_model_iter_next (GTK_TREE_MODEL (tt->gs), 
                                                &(tt->it));
            return FALSE; /* continue with next node */
        case -1:
            /* current node name less than current row means node is new.
               Insert a new node in store before current (it moves to new node),
               fill it and move next */
            gtk_list_store_prepend(tt->gs, &(tt->it));
            rowitem = g_malloc(sizeof(node_id_t));
            *rowitem = node->node_id;
            gtk_list_store_set (tt->gs, &(tt->it), NODES_COLUMN_N, rowitem, -1);
            nodes_table_update_row(tt->gs, &(tt->it), node);
            tt->res = gtk_tree_model_iter_next (GTK_TREE_MODEL (tt->gs), 
                                                &(tt->it));
            return FALSE; /* continue with next node */
        case 1:
            /* current node name greater than current row means deleted row.
             * remove row and loop */
            g_free(rowitem);
            tt->res = gtk_list_store_remove (tt->gs, &(tt->it));
            break;
        }
    }  /* end while */

  /* coming here means node is new - append and move next */
  gtk_list_store_prepend(tt->gs, &(tt->it));
  rowitem = g_malloc(sizeof(node_id_t));
  *rowitem = node->node_id;
  gtk_list_store_set (tt->gs, &(tt->it), NODES_COLUMN_N, rowitem, -1);
  nodes_table_update_row(tt->gs, &(tt->it), node);
  return FALSE;
}

static void nodes_table_update(GtkWidget *window)
{
  GtkListStore *gs;
  struct NTTraverse tt;

  gs = nodes_table_create(window);
  if (!gs)
    return; /* no datastore, exit */

  /* traverse both the full list and the treestore, comparing entries */
  tt.gs = gs;
  tt.res = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (tt.gs), &(tt.it));
  nodes_catalog_foreach(nodes_table_iterate, &tt);

  /* if the iterator is still active, remaining rows must be removed */
  while (tt.res)
    {
      node_id_t *rowitem;
      gtk_tree_model_get (GTK_TREE_MODEL (tt.gs), &(tt.it), 
                          NODES_COLUMN_N, &rowitem, -1);
      g_free(rowitem);
      tt.res = gtk_list_store_remove (tt.gs, &(tt.it));
    }
}

/* ----------------------------------------------------------
   Events
   ---------------------------------------------------------- */

gboolean on_nodes_wnd_delete_event(GtkWidget * wdg, GdkEvent * evt, gpointer ud)
{
  nodes_wnd_hide();
  return TRUE;			/* ignore signal */
}

void on_nodes_check_toggled(GtkCheckMenuItem *checkmenuitem,  gpointer data)
{
  if (gtk_check_menu_item_get_active(checkmenuitem))
    nodes_wnd_show();
  else
    nodes_wnd_hide();
}

/* double click on row */
void on_nodes_table_row_activated(GtkTreeView *gv,
                                  GtkTreePath *sorted_path,
                                  GtkTreeViewColumn *column,
                                  gpointer data)
{
  GtkTreeModel *sort_model;
  GtkTreeModel *gls;
  GtkTreePath *path;
  GtkTreeIter it;

  /* parameters came from sorted model; we need to convert to liststore*/
  sort_model = gtk_tree_view_get_model(gv);
  if (!sort_model)
    return;

  /* liststore from sorted model */
  gls = gtk_tree_model_sort_get_model(GTK_TREE_MODEL_SORT(sort_model));
  if (!gls)
    return; 

  /* liststore path from sorted path */
  path = gtk_tree_model_sort_convert_path_to_child_path
            (GTK_TREE_MODEL_SORT(sort_model), sorted_path);
  if (!path)
    return;

  /* iterator from path */
  if (gtk_tree_model_get_iter (gls, &it, path))
    {
      /* and last, value */
      node_id_t *rowitem;
      gtk_tree_model_get (gls, &it, NODES_COLUMN_N, &rowitem, -1);
      node_protocols_window_create(rowitem);
    }
  
  gtk_tree_path_free(path);
}


