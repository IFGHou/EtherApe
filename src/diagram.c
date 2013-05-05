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

#include <gnome.h>

#include "appdata.h"
#include "diagram.h"
#include "pref_dialog.h"
#include "node.h"
#include "info_windows.h"
#include "protocols.h"
#include "datastructs.h"
#include "ip-cache.h"
#include "menus.h"
#include "capture.h"
#include "conversations.h"
#include "preferences.h"
#include "export.h"

/* maximum node and link size */
#define MAX_NODE_SIZE 5000
#define MAX_LINK_SIZE (MAX_NODE_SIZE/4)


/***************************************************************************
 *
 * canvas_node_t definition and methods
 *
 **************************************************************************/
typedef struct
{
  node_id_t canvas_node_id;
  GnomeCanvasItem *node_item;
  GnomeCanvasItem *text_item;
  GnomeCanvasGroup *group_item;
  GdkColor color;
  gboolean is_new;
  gboolean shown;		/* True if it is to be displayed. */
  gboolean centered;            /* true if is a center node */
}
canvas_node_t;
static gint canvas_node_compare(const node_id_t *a, const node_id_t *b, 
                                gpointer dummy);
static void canvas_node_delete(canvas_node_t *cn);
static gint canvas_node_update(node_id_t  * ether_addr,
				 canvas_node_t * canvas_node,
				 GList ** delete_list);


/***************************************************************************
 *
 * canvas_link_t definition and methods
 *
 **************************************************************************/
typedef struct
{
  link_id_t canvas_link_id; /* id of the link */
  GnomeCanvasItem *src_item;    /* triangle for src side */
  GnomeCanvasItem *dst_item;    /* triangle for dst side */
  GdkColor color;
}
canvas_link_t;
static gint canvas_link_compare(const link_id_t *a, const link_id_t *b, 
                                gpointer dummy);
static void canvas_link_delete(canvas_link_t *canvas_link);
static gint canvas_link_update(link_id_t * link_id,
				 canvas_link_t * canvas_link,
				 GList ** delete_list);


typedef struct 
{
  GtkWidget * canvas;
  gfloat angle;
  guint node_i;
  guint n_nodes;
  gdouble xmin;
  gdouble ymin;
  gdouble xmax;
  gdouble ymax;
  gdouble x_rad_max;
  gdouble y_rad_max;
  gdouble x_inner_rad_max;
  gdouble y_inner_rad_max;
  
} reposition_node_t;




/***************************************************************************
 *
 * local variables
 *
 **************************************************************************/


static GTree *canvas_nodes;	/* We don't use the nodes tree directly in order to 
				 * separate data from presentation: that is, we need to
				 * keep a list of CanvasItems, but we do not want to keep
				 * that info on the nodes tree itself */
static GTree *canvas_links;	/* See canvas_nodes */

static guint known_protocols = 0;


static gboolean is_idle = FALSE;
static guint displayed_nodes;
static GdkColor black_color;
static gboolean need_reposition = TRUE;	/* Force a diagram relayout */
static gboolean need_font_refresh = TRUE;/* Force font refresh during layout */
static gint diagram_timeout;	/* Descriptor of the diagram timeout function
				 * (Used to change the refresh_period in the callback */

static long canvas_obj_count = 0; /* counter of canvas objects */

/***************************************************************************
 *
 * local Function definitions
 *
 **************************************************************************/
static void diagram_update_nodes(GtkWidget * canvas); /* updates ALL nodes */
static void diagram_update_links(GtkWidget * canvas); /* updates ALL links */

static void check_new_protocol (GtkWidget *prot_table, const protostack_t *pstk);
static gint check_new_node (node_t * node, GtkWidget * canvas);
static gboolean display_node (node_t * node);
static void limit_nodes (void);
static gint add_ordered_node (node_id_t * node_id,
			      canvas_node_t * canvas_node,
			      GTree * ordered_nodes);
static gint check_ordered_node (gdouble * traffic, canvas_node_t * node,
				guint * count);
static gint traffic_compare (gconstpointer a, gconstpointer b);
static gint reposition_canvas_nodes (node_id_t * node_id,
				     canvas_node_t * canvas_node,
				     reposition_node_t *data);
static gint check_new_link (link_id_t * link_id,
			    link_t * link, GtkWidget * canvas);
static gdouble get_node_size (gdouble average);
static gdouble get_link_size(const basic_stats_t *link_stats);
static gint link_item_event (GnomeCanvasItem * item,
			     GdkEvent * event, canvas_link_t * canvas_link);
static gint node_item_event (GnomeCanvasItem * item,
			     GdkEvent * event, canvas_node_t * canvas_node);
static void update_legend(void);
static void draw_oneside_link(double xs, double ys, double xd, double yd,
                              const basic_stats_t *link_data, 
                              guint32 scaledColor, GnomeCanvasItem *item);
static void init_reposition(reposition_node_t *data,
                            GtkWidget * canvas, 
                            guint total_nodes);


void ask_reposition(gboolean r_font)
{
  need_reposition = TRUE;
  need_font_refresh = r_font;
}

void dump_stats(guint32 diff_msecs)
{
  gchar *status_string;
  long ipc=ipcache_active_entries();
  status_string = g_strdup_printf (
    _("Nodes: %d (on canvas: %d, shown: %u), Links: %d, Conversations: %ld, "
      "names %ld, protocols %ld. Total Packets seen: %lu (in memory: %ld, "
      "on list %ld). IP cache entries %ld. Canvas objs: %ld. Refreshed: %u ms"),
                                   node_count(), 
                                   g_tree_nnodes(canvas_nodes), displayed_nodes, 
                                   links_catalog_size(), active_conversations(), 
                                   active_names(), protocol_summary_size(),
                                   appdata.n_packets, appdata.total_mem_packets, 
                                   packet_list_item_count(), ipc,
                                   canvas_obj_count,
                                   (unsigned int) diff_msecs);
  
  g_my_info ("%s", status_string);
  g_free(status_string);
}

/* called when a watched object is finalized */
static void finalize_callback(gpointer data, GObject *obj)
{
  --canvas_obj_count;
}
/* increase reference to object and optionally register a callback to check 
 * for reference leaks */
static void addref_canvas_obj(GObject *obj)
{
  g_assert(obj);
  g_object_ref_sink(obj);

  if (INFO_ENABLED)
    {
      /* to check for resource leaks, we ask for a notify ... */
      g_object_weak_ref(obj, finalize_callback, NULL);
      ++canvas_obj_count;
    }
}

/* It updates controls from values of variables, and connects control
 * signals to callback functions */
void
init_diagram (GladeXML *xml)
{
  GtkStyle *style;
  GtkWidget *canvas;

  /* Creates trees */
  canvas_nodes = g_tree_new_full ( (GCompareDataFunc)canvas_node_compare, 
                            NULL, NULL, (GDestroyNotify)canvas_node_delete);
  canvas_links = g_tree_new_full( (GCompareDataFunc)canvas_link_compare,
                            NULL, NULL, (GDestroyNotify)canvas_link_delete);

  initialize_pref_controls();
  
  /* Sets canvas background to black */
  canvas = glade_xml_get_widget (appdata.xml, "canvas1");

  gdk_color_parse ("black", &black_color);
  gdk_colormap_alloc_color (gdk_colormap_get_system (), &black_color,
			    TRUE, TRUE);
  style = gtk_style_new ();
  style->bg[GTK_STATE_NORMAL] = black_color;
  style->base[GTK_STATE_NORMAL] = black_color;

  gtk_widget_set_style (canvas, style);
  gtk_style_set_background (canvas->style, canvas->window, GTK_STATE_NORMAL);

  /* Initialize the known_protocols table */
  delete_gui_protocols ();

  /* Set the already_updating global flag */
  already_updating = FALSE;
  stop_requested = FALSE;
}				/* init_diagram */


void
destroying_timeout (gpointer data)
{
  diagram_timeout = g_idle_add_full (G_PRIORITY_DEFAULT,
				     (GtkFunction) update_diagram,
				     data, (GDestroyNotify) destroying_idle);
  is_idle = TRUE;
}

void
destroying_idle (gpointer data)
{
  diagram_timeout = g_timeout_add_full (G_PRIORITY_DEFAULT,
					pref.refresh_period,
					(GtkFunction) update_diagram,
					data,
					(GDestroyNotify) destroying_timeout);
  is_idle = FALSE;
}

/* delete the specified canvas node */
static void 
canvas_node_delete(canvas_node_t *canvas_node)
{
  if (canvas_node->node_item)
    {
      gtk_object_destroy (GTK_OBJECT (canvas_node->node_item));
      g_object_unref (G_OBJECT (canvas_node->node_item));
      canvas_node->node_item = NULL;
    }
  if (canvas_node->text_item)
    {
      gtk_object_destroy (GTK_OBJECT (canvas_node->text_item));
      g_object_unref (G_OBJECT (canvas_node->text_item));
      canvas_node->text_item = NULL;
    }
  if (canvas_node->group_item)
    {
      gtk_object_destroy (GTK_OBJECT (canvas_node->group_item));
      g_object_unref (G_OBJECT (canvas_node->group_item));
      canvas_node->group_item = NULL;
    }

  g_free (canvas_node);
}

/* used to remove nodes */
static void 
gfunc_remove_canvas_node(gpointer data, gpointer user_data)
{
  g_tree_remove (canvas_nodes, (const node_id_t *)data);
}

/* used to remove links */
static void 
gfunc_remove_canvas_link(gpointer data, gpointer user_data)
{
  g_tree_remove (canvas_links, (const link_id_t *)data);
}

static void
diagram_update_nodes(GtkWidget * canvas)
{
  GList *delete_list = NULL;
  node_t *new_node = NULL;

  /* Deletes all nodes and updates traffic values */
  /* TODO To reduce CPU usage, I could just as well update each specific
   * node in update_canvas_nodes and create a new timeout function that would
   * make sure that old nodes get deleted by calling update_nodes, but
   * not as often as with diagram_refresh_period */
  nodes_catalog_update_all();

  /* Check if there are any new nodes */
  while ((new_node = new_nodes_pop()))
    check_new_node (new_node, canvas);

  /* Update nodes look and queue outdated canvas_nodes for deletion */
  g_tree_foreach(canvas_nodes,
	       (GTraverseFunc) canvas_node_update,
	       &delete_list);

  /* delete all canvas nodes queued */
  g_list_foreach(delete_list, gfunc_remove_canvas_node, NULL);

  /* free the list - list items are already destroyed */
  g_list_free(delete_list);

  /* Limit the number of nodes displayed, if a limit has been set */
  /* TODO check whether this is the right function to use, now that we have a more
   * general display_node called in update_canvas_nodes */
  limit_nodes ();

  /* Reposition canvas_nodes */
  if (need_reposition)
    {
      reposition_node_t rdata;
      init_reposition(&rdata, canvas, displayed_nodes);
      g_tree_foreach(canvas_nodes,
                    (GTraverseFunc) reposition_canvas_nodes,
                    &rdata);
      need_reposition = FALSE;
      need_font_refresh = FALSE;
    }
}

static void
diagram_update_links(GtkWidget * canvas)
{
  GList *delete_list = NULL;

  /* Delete old capture links and update capture link variables */
  links_catalog_update_all();

  /* Check if there are any new links */
  links_catalog_foreach((GTraverseFunc) check_new_link, canvas);

  /* Update links look 
   * We also queue timedout links for deletion */
  delete_list = NULL;
  g_tree_foreach(canvas_links,
                   (GTraverseFunc) canvas_link_update,
                   &delete_list);

  /* delete all canvas links queued */
  g_list_foreach(delete_list, gfunc_remove_canvas_link, NULL);

  /* free the list - list items are already destroyed */
  g_list_free(delete_list);
}

/* Refreshes the diagram. Called each refresh_period ms
 * 1. Checks for new protocols and displays them
 * 2. Updates nodes looks
 * 3. Updates links looks
 */
guint update_diagram(GtkWidget * canvas)
{
  static struct timeval last_refresh_time = { 0, 0 };
  double diffms;
  enum status_t status;

  /* if requested and enabled, dump to xml */
  if (appdata.request_dump && appdata.export_file_signal)
    {
      g_warning (_("SIGUSR1 received: exporting to %s"), appdata.export_file_signal);
      dump_xml(appdata.export_file_signal);
      appdata.request_dump = FALSE; 
    }
  
  status = get_capture_status();
  if (status == PAUSE)
    return FALSE;

  if (status == CAP_EOF)
    {
      gui_eof_capture ();
      return FALSE;
    }
  
  /* 
   * It could happen that during an intensive calculation, in order
   * to update the GUI and make the application responsive gtk_main_iteration
   * is called. But that could also trigger this very function's timeout.
   * If we let it run twice many problems could come up. Thus,
   * we are preventing it with the already_updating variable
   */

  if (already_updating)
    {
      g_my_debug ("update_diagram called while already updating");
      return FALSE;
    }

  already_updating = TRUE;
  gettimeofday (&appdata.now, NULL);

  /* update nodes */
  diagram_update_nodes(canvas);

  /* update links */
  diagram_update_links(canvas);

  /* Update protocol information */
  protocol_summary_update_all();

  /* update proto legend */
  update_legend();

  /* Now update info windows */
  update_info_windows ();

  /* With this we make sure that we don't overload the
   * CPU with redraws */

  if ((last_refresh_time.tv_sec == 0) && (last_refresh_time.tv_usec == 0))
    last_refresh_time = appdata.now;

  /* Force redraw */
  while (gtk_events_pending ())
    gtk_main_iteration ();

  gettimeofday (&appdata.now, NULL);
  diffms = substract_times_ms(&appdata.now, &last_refresh_time);
  last_refresh_time = appdata.now;

  already_updating = FALSE;

  if (!is_idle)
    {
      if (diffms > pref.refresh_period * 1.2)
        return FALSE;		/* Removes the timeout */
    }
  else
    {
      if (diffms < pref.refresh_period)
        return FALSE;		/* removes the idle */
    }

  if (stop_requested)
    gui_stop_capture();
  
  return TRUE;			/* Keep on calling this function */
}				/* update_diagram */

static void
purge_expired_legend_protocol(GtkWidget *widget, gpointer data)
{
  GtkLabel *lab = GTK_LABEL(widget);
  if (lab &&
      !protocol_summary_find(pref.stack_level, gtk_label_get_label(lab)))
    {
      /* protocol expired, remove */
      gtk_widget_destroy(widget);
      known_protocols--;
    }
}

/* updates the legend */
static void
update_legend()
{
  GtkWidget *prot_table;

  /* first, check if there are expired protocols */
  prot_table = glade_xml_get_widget (appdata.xml, "prot_table");
  if (!prot_table)
    return;

  gtk_container_foreach(GTK_CONTAINER(prot_table), 
                        (GtkCallback)purge_expired_legend_protocol, NULL);

  /* then search for new protocols */
  check_new_protocol(prot_table, protocol_summary_stack());
}


/* Checks whether there is already a legend entry for each known 
 * protocol. If not, create it */
static void
check_new_protocol (GtkWidget *prot_table, const protostack_t *pstk)
{
  const GList *protocol_item;
  const protocol_t *protocol;
  GdkColor color;
  GtkStyle *style;
  GtkLabel *lab;
  GtkWidget *newlab;
  GList *childlist;

  if (!pstk)
    return; /* nothing to do */

  childlist = gtk_container_get_children(GTK_CONTAINER(prot_table));
  protocol_item = pstk->protostack[pref.stack_level];
  while (protocol_item)
    {
      const GList *cur;
      protocol = protocol_item->data;

      /* prepare next */
      protocol_item = protocol_item->next; 

      /* First, we check whether the diagram already knows about this protocol,
       * checking whether it is shown on the legend. */
      cur = childlist;
      while (cur)
        {
          lab = GTK_LABEL(cur->data);
          if (lab && !strcmp(protocol->name, gtk_label_get_label(lab)))
            break; /* found */
          cur = cur->next;
        }
      
      if (cur)
          continue; /* found, skip to next */

      g_my_debug ("Protocol '%s' not found. Creating legend item", 
                  protocol->name);
    
      /* It's not, so we build a new entry on the legend */
    
      /* we add the new label widgets */
      newlab = gtk_label_new (protocol->name);
      gtk_widget_show (newlab);
      gtk_misc_set_alignment(GTK_MISC(newlab), 0, 0);

      color = protohash_color(protocol->name);
      if (!gdk_colormap_alloc_color
          (gdk_colormap_get_system(), &color, FALSE, TRUE))
        g_warning (_("Unable to allocate color for new protocol %s"),
                   protocol->name);

      style = gtk_style_new ();
      style->fg[GTK_STATE_NORMAL] = color;
      gtk_widget_set_style (newlab, style);
      g_object_unref (style);

      gtk_container_add(GTK_CONTAINER(prot_table), newlab);
      known_protocols++;
    }
  g_list_free(childlist);
}				/* check_new_protocol */

/* empties the table of protocols */
void
delete_gui_protocols (void)
{
  GList *item;
  GtkContainer *prot_table;

  known_protocols = 0;

  /* restart color cycle */
  protohash_reset_cycle();

  /* remove proto labels from legend */
  prot_table = GTK_CONTAINER (glade_xml_get_widget (appdata.xml, "prot_table"));
  item = gtk_container_get_children (GTK_CONTAINER (prot_table));
  while (item)
    {
      gtk_container_remove (prot_table, item->data);
      item = item->next;
    }

  /* resize legend */
  gtk_container_resize_children(prot_table);
  gtk_widget_queue_resize (GTK_WIDGET (appdata.app1));
}				/* delete_gui_protocols */

/* Checks if there is a canvas_node per each node. If not, one canvas_node
 * must be created and initiated */
static gint
check_new_node (node_t * node, GtkWidget * canvas)
{
  canvas_node_t *new_canvas_node;
  GnomeCanvasGroup *group;

  if (!node)
    return FALSE;

  if (display_node (node) && !g_tree_lookup (canvas_nodes, &node->node_id))
    {
      new_canvas_node = g_malloc (sizeof (canvas_node_t));
      g_assert(new_canvas_node);
      
      new_canvas_node->canvas_node_id = node->node_id;

      /* Create a new group to hold the node and its labels */
      group = gnome_canvas_root (GNOME_CANVAS (canvas));
      group = GNOME_CANVAS_GROUP (gnome_canvas_item_new (group,
							 GNOME_TYPE_CANVAS_GROUP,
							 "x", 100.0, 
                                                         "y", 100.0, NULL));
      addref_canvas_obj(G_OBJECT (group));
      new_canvas_node->group_item = group;

      new_canvas_node->node_item
	= gnome_canvas_item_new (group,
				 GNOME_TYPE_CANVAS_ELLIPSE,
				 "x1", 0.0,
				 "x2", 0.0,
				 "y1", 0.0,
				 "y2", 0.0,
				 "fill_color", "white",
				 "outline_color", "black",
				 "width_pixels", 0, NULL);
      addref_canvas_obj(G_OBJECT (new_canvas_node->node_item));

      new_canvas_node->text_item =
	gnome_canvas_item_new (group, GNOME_TYPE_CANVAS_TEXT,
			       "text", node->name->str,
			       "x", 0.0,
			       "y", 0.0,
			       "anchor", GTK_ANCHOR_CENTER,
			       "font", pref.fontname,
			       "fill_color", pref.text_color, NULL);
      addref_canvas_obj(G_OBJECT (new_canvas_node->text_item));

      gnome_canvas_item_raise_to_top (GNOME_CANVAS_ITEM
				      (new_canvas_node->text_item));
      g_signal_connect (G_OBJECT (new_canvas_node->group_item), "event",
			(GtkSignalFunc) node_item_event, new_canvas_node);

      if (!new_canvas_node->node_item || !new_canvas_node->text_item)
        g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, _("Canvas node null"));
      
      /*
       * We hide them until we are sure that they will get a proper position
       * in reposition_nodes
       */
      gnome_canvas_item_hide (new_canvas_node->node_item);
      gnome_canvas_item_hide (new_canvas_node->text_item);

      new_canvas_node->is_new = TRUE;
      new_canvas_node->shown = TRUE;
      new_canvas_node->centered = FALSE;

      g_tree_insert (canvas_nodes,
		     &new_canvas_node->canvas_node_id, new_canvas_node);
      g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	     _("Creating canvas_node: %s. Number of nodes %d"),
	     node->name->str, g_tree_nnodes (canvas_nodes));

      need_reposition = TRUE;
    }

  return FALSE;			/* False to keep on traversing */
}				/* check_new_node */


/* - updates sizes, names, etc */
static gint
canvas_node_update(node_id_t * node_id, canvas_node_t * canvas_node,
		     GList **delete_list)
{
  node_t *node;
  gdouble node_size;
  static clock_t start = 0;
  clock_t end;
  gdouble cpu_time_used;
  char *nametmp = NULL;

  node = nodes_catalog_find(node_id);

  /* Remove node if node is too old or if capture is stopped */
  if (!node || !display_node (node))
    {
      /* adds current to list of canvas nodes to delete */
      *delete_list = g_list_prepend( *delete_list, node_id);
      g_my_debug ("Queing canvas node to remove.");
      need_reposition = TRUE;
      return FALSE;
    }

  switch (pref.node_size_variable)
    {
    case INST_TOTAL:
      node_size = get_node_size (node->node_stats.stats.average);
      break;
    case INST_INBOUND:
      node_size = get_node_size (node->node_stats.stats_in.average);
      break;
    case INST_OUTBOUND:
      node_size = get_node_size (node->node_stats.stats_out.average);
      break;
    case INST_PACKETS:
      node_size = get_node_size (node->node_stats.pkt_list.length);
      break;
    case ACCU_TOTAL:
      node_size = get_node_size (node->node_stats.stats.accumulated);
      break;
    case ACCU_INBOUND:
      node_size = get_node_size (node->node_stats.stats_in.accumulated);
      break;
    case ACCU_OUTBOUND:
      node_size = get_node_size (node->node_stats.stats_out.accumulated);
      break;
    case ACCU_PACKETS:
      node_size = get_node_size (node->node_stats.stats.accu_packets);
      break;
    case ACCU_AVG_SIZE:
      node_size = get_node_size (node->node_stats.stats.avg_size);
      break;
    default:
      node_size = get_node_size (node->node_stats.stats_out.average);
      g_warning (_("Unknown value or node_size_variable"));
    }

  /* limit the maximum size to avoid overload */
  if (node_size > MAX_NODE_SIZE)
    node_size = MAX_NODE_SIZE; 

  if (node->main_prot[pref.stack_level])
    {
      canvas_node->color = protohash_color(node->main_prot[pref.stack_level]);

      gnome_canvas_item_set (canvas_node->node_item,
			     "x1", -node_size / 2,
			     "x2", node_size / 2,
			     "y1", -node_size / 2,
			     "y2", node_size / 2,
			     "fill_color_gdk", &(canvas_node->color), NULL);
    }
  else
    {
      guint32 black = 0x000000ff;
      gnome_canvas_item_set (canvas_node->node_item,
			     "x1", -node_size / 2,
			     "x2", node_size / 2,
			     "y1", -node_size / 2,
			     "y2", node_size / 2,
			     "fill_color_rgba", black, NULL);
    }

  /* We check the name of the node, and update the canvas node name
   * if it has changed (useful for non blocking dns resolving) */
  /*TODO why is it exactly that sometimes it is NULL? */
  if (canvas_node->text_item)
    {
      g_object_get (G_OBJECT (canvas_node->text_item), 
                    "text", &nametmp,
		    NULL);
      if (strcmp (nametmp, node->name->str))
	{
	  gnome_canvas_item_set (canvas_node->text_item,
				 "text", node->name->str, 
                                 NULL);
	  gnome_canvas_item_request_update (canvas_node->text_item);
	}
      g_free (nametmp);
    }

  /* Processor time check. If too much time has passed, update the GUI */
  end = clock ();
  cpu_time_used = ((gdouble) (end - start)) / CLOCKS_PER_SEC;
  if (cpu_time_used > 0.05)
    {
      /* Force redraw */
      while (gtk_events_pending ())
	gtk_main_iteration ();
      start = end;
    }
  return FALSE;			/* False means keep on calling the function */

}				/* update_canvas_nodes */


/* Returns whether the node in question should be displayed in the
 * diagram or not */
static gboolean
display_node (node_t * node)
{
  double diffms;

  if (!node)
    return FALSE;

  diffms = substract_times_ms(&appdata.now, &node->node_stats.stats.last_time);

  /* There are problems if a canvas_node is deleted if it still
   * has packets, so we have to check that as well */

  /* Remove canvas_node if node is too old */
  if (diffms >= pref.gui_node_timeout_time
      && pref.gui_node_timeout_time 
      && !node->node_stats.pkt_list.length)
    return FALSE;

#if 1
  if ((pref.gui_node_timeout_time == 1) && !node->node_stats.pkt_list.length)
    g_my_critical ("Impossible situation in display node");
#endif

  return TRUE;
}				/* display_node */

/* Sorts canvas nodes with the criterium set in preferences and sets
 * which will be displayed in the diagram */
static void
limit_nodes (void)
{
  static GTree *ordered_nodes = NULL;
  static guint limit;
  displayed_nodes = 0;		/* We'll increment for each node we don't
				 * limit */

  if (appdata.node_limit < 0)
    {
      displayed_nodes = g_tree_nnodes (canvas_nodes);
      return;
    }

  limit = appdata.node_limit;

  ordered_nodes = g_tree_new (traffic_compare);

  g_tree_foreach(canvas_nodes, (GTraverseFunc) add_ordered_node, ordered_nodes);
  g_tree_foreach(ordered_nodes, (GTraverseFunc) check_ordered_node,
		   &limit);
  g_tree_destroy (ordered_nodes);
  ordered_nodes = NULL;
}				/* limit_nodes */

static gint
add_ordered_node (node_id_t * node_id, canvas_node_t * node,
		  GTree * ordered_nodes)
{
  g_tree_insert (ordered_nodes, node, node);
  g_my_debug ("Adding ordered node. Number of nodes: %d",
	      g_tree_nnodes (ordered_nodes));
  return FALSE;			/* keep on traversing */
}				/* add_ordered_node */

static gint
check_ordered_node (gdouble * traffic, canvas_node_t * node, guint * count)
{
  /* TODO We can probably optimize this by stopping the traversion once
   * the limit has been reached */
  if (*count)
    {
      if (!node->shown)
	need_reposition = TRUE;
      node->shown = TRUE;
      displayed_nodes++;
      (*count)--;
    }
  else
    {
      if (node->shown)
	need_reposition = TRUE;
      node->shown = FALSE;
    }
  return FALSE;			/* keep on traversing */
}				/* check_ordered_node */

/* Comparison function used to order the (GTree *) nodes
 * and canvas_nodes heard on the network */
static gint
traffic_compare (gconstpointer a, gconstpointer b)
{
  node_t *node_a, *node_b;

  g_assert (a != NULL);
  g_assert (b != NULL);

  node_a = (node_t *) a;
  node_b = (node_t *) b;

  if (node_a->node_stats.stats.average < node_b->node_stats.stats.average)
    return 1;
  if (node_a->node_stats.stats.average > node_b->node_stats.stats.average)
    return -1;

  /* If two nodes have the same traffic, we still have
   * to distinguish them somehow. We use the node_id */

  return (node_id_compare (&node_a->node_id, &node_b->node_id));

}				/* traffic_compare */


/* initialize reposition struct */
static void init_reposition(reposition_node_t *data,
                            GtkWidget * canvas, 
                            guint total_nodes)
{
  gdouble text_compensation = 50;

  data->canvas = canvas;
  data->angle = 0.0;
  data->node_i = 0;
  data->n_nodes = 0;
  data->n_nodes = total_nodes;
  data->node_i = total_nodes;

  gnome_canvas_get_scroll_region (GNOME_CANVAS (canvas),
				  &data->xmin, &data->ymin, 
                                  &data->xmax, &data->ymax);


  data->xmin += text_compensation;
  data->xmax -= text_compensation;	/* Reduce the drawable area so that
				 * the node name is not lost
				 * TODO: Need a function to calculate
				 * text_compensation depending on font size */
  data->x_rad_max = 0.9 * (data->xmax - data->xmin) / 2;
  data->y_rad_max = 0.9 * (data->ymax - data->ymin) / 2;
  data->x_inner_rad_max = data->x_rad_max / 2;
  data->y_inner_rad_max = data->y_rad_max / 2;
}

/* Called from update_diagram if the global need_reposition
 * is set. It rearranges the nodes*/
/* TODO I think I should update all links as well, so as not having
 * stale graphics if the diagram has been resized */
static gint reposition_canvas_nodes(node_id_t * node_id, 
                                    canvas_node_t * canvas_node,
                                    reposition_node_t *data)
{
  gdouble x = 0, y = 0;
  gdouble oddAngle;

  if (!canvas_node->shown)
    {
      gnome_canvas_item_hide (canvas_node->node_item);
      gnome_canvas_item_hide (canvas_node->text_item);
      return FALSE;
    }

  oddAngle = data->angle;
  
  /* TODO I've done all the stationary changes in a hurry
   * I should review it an tidy up all this stuff */
  if (pref.stationary)
    {
      if (canvas_node->is_new)
	{
	  static guint count = 0, base = 1;
	  gdouble s_angle = 0;

	  if (count == 0)
	    {
	      s_angle = M_PI * 2.0f;
	      count++;
	    }
	  else
	    {

	      if (count > 2 * base)
		{
		  base *= 2;
		  count = 1;
		}
	      s_angle = M_PI * (gdouble) count / ((gdouble) base);
	      count += 2;
	    }
	  x = data->x_rad_max * cos (s_angle);
	  y = data->y_rad_max * sin (s_angle);
	}
    }
  else
    {
      if (canvas_node->is_new && *pref.center_node) {
        node_t * node;

        /* center node specified, check to see if current node is centered */  
        node = nodes_catalog_find((const node_id_t *)&canvas_node->canvas_node_id);
        /* If it is one of the "centered" nodes change the coordinates */
        if (node) {
          if (!strcmp(node->name->str, pref.center_node) ||
              !strcmp(node->numeric_name->str, pref.center_node)) {
             canvas_node->centered = TRUE;
          }
        }
      }

      if (canvas_node->centered) {
         /* centered node, reset coordinates */
         x = ( data->xmax - data->xmin ) / 2 + data->xmin ;
         y = ( data->ymax - data->ymin ) / 2 + data->ymin ;
         data->angle -= 2 * M_PI / data->n_nodes;
      }
      else {
        if (data->n_nodes % 2 == 0)	/* spacing is better when n_nodes is odd and Y is linear */
            oddAngle = (data->angle * data->n_nodes) / (data->n_nodes + 1);
        if (data->n_nodes > 7)
        {
          x = data->x_rad_max * cos (oddAngle);
          y = data->y_rad_max * asin (sin (oddAngle)) / (M_PI / 2);
        }
        else
        {
          x = data->x_rad_max * cos (data->angle);
          y = data->y_rad_max * sin (data->angle);
        }
      }
    }


  if (!pref.stationary || canvas_node->is_new)
    {
      gnome_canvas_item_set(GNOME_CANVAS_ITEM (canvas_node->group_item),
			     "x", x, "y", y, NULL);
      canvas_node->is_new = FALSE;
    }

  if (need_font_refresh)
    {
      /* We update the text font */
      gnome_canvas_item_set (canvas_node->text_item, 
                             "font", pref.fontname, 
                             "fill_color", pref.text_color, 
                             NULL);
    }
  
  if (pref.diagram_only)
    {
      gnome_canvas_item_hide (canvas_node->text_item);
    }
  else
    {
      gnome_canvas_item_show (canvas_node->text_item);
      gnome_canvas_item_request_update (canvas_node->text_item);
    }

  gnome_canvas_item_show (canvas_node->node_item);
  gnome_canvas_item_request_update (canvas_node->node_item);

  data->node_i--;

  if (data->node_i)
    data->angle += 2 * M_PI / data->n_nodes;
  else
    {
      data->angle = 0.0;
      data->n_nodes = 0;
    }

  return FALSE;
}


/* Goes through all known links and checks whether there already exists
 * a corresponding canvas_link. If not, create it.*/
static gint
check_new_link (link_id_t * link_id, link_t * link, GtkWidget * canvas)
{
  canvas_link_t *new_canvas_link;
  GnomeCanvasGroup *group;
  GnomeCanvasPoints *points;
  guint i = 0;

  if (!g_tree_lookup (canvas_links, link_id))
    {
      group = gnome_canvas_root (GNOME_CANVAS (canvas));

      new_canvas_link = g_malloc (sizeof (canvas_link_t));
      g_assert(new_canvas_link);
      new_canvas_link->canvas_link_id = *link_id;

      /* We set the lines position using groups positions */
      points = gnome_canvas_points_new (3);

      for (; i <= 5; i++)
	points->coords[i] = 0.0;

      new_canvas_link->src_item
	= gnome_canvas_item_new (group,
				 gnome_canvas_polygon_get_type (),
				 "points", points, "fill_color", "tan", NULL);
      addref_canvas_obj(G_OBJECT (new_canvas_link->src_item));

      new_canvas_link->dst_item
	= gnome_canvas_item_new (group,
				 gnome_canvas_polygon_get_type (),
				 "points", points, "fill_color", "tan", NULL);
      addref_canvas_obj(G_OBJECT (new_canvas_link->dst_item));

      g_tree_insert (canvas_links,
		     &new_canvas_link->canvas_link_id, new_canvas_link);
      gnome_canvas_item_lower_to_bottom (new_canvas_link->src_item);
      gnome_canvas_item_lower_to_bottom (new_canvas_link->dst_item);

      gnome_canvas_points_unref (points);

      g_signal_connect (G_OBJECT (new_canvas_link->src_item), "event",
			(GtkSignalFunc) link_item_event, new_canvas_link);
      g_signal_connect (G_OBJECT (new_canvas_link->dst_item), "event",
			(GtkSignalFunc) link_item_event, new_canvas_link);

    }

  return FALSE;
}				/* check_new_link */


/* - calls update_links, so that the related link updates its average
 *   traffic and main protocol, and old links are deleted
 * - caculates link size and color fading */
static gint
canvas_link_update(link_id_t * link_id, canvas_link_t * canvas_link,
		     GList **delete_list)
{
  const link_t *link;
  const canvas_node_t *canvas_dst;
  const canvas_node_t *canvas_src;
  guint32 scaledColor;
  double xs, ys, xd, yd, scale;

  /* We used to run update_link here, but that was a major performance penalty, 
   * and now it is done in update_diagram */
  link = links_catalog_find(link_id);
  if (!link)
    {
      *delete_list = g_list_prepend( *delete_list, link_id);
      g_my_debug ("Queing canvas link to remove.");
      return FALSE;
    }

  /* If either source or destination has disappeared, we hide the link
   * until it can be show again */

  /* We get coords for the destination node */
  canvas_dst = g_tree_lookup (canvas_nodes, &link_id->dst);
  if (!canvas_dst || !canvas_dst->shown)
    {
      gnome_canvas_item_hide (canvas_link->src_item);
      gnome_canvas_item_hide (canvas_link->dst_item);
      return FALSE;
    }

  /* We get coords from source node */
  canvas_src = g_tree_lookup (canvas_nodes, &link_id->src);
  if (!canvas_src || !canvas_src->shown)
    {
      gnome_canvas_item_hide (canvas_link->src_item);
      gnome_canvas_item_hide (canvas_link->dst_item);
      return FALSE;
    }

  /* What if there never is a protocol?
   * I have to initialize canvas_link->color to a known value */
  if (link->main_prot[pref.stack_level])
    {
      double diffms;
      canvas_link->color = protohash_color(link->main_prot[pref.stack_level]);

      /* scale color down to 10% at link timeout */
      diffms = substract_times_ms(&appdata.now, &link->link_stats.stats.last_time);
      scale = pow(0.10, diffms / pref.gui_link_timeout_time);

      scaledColor =
        (((int) (scale * canvas_link->color.red) & 0xFF00) << 16) |
        (((int) (scale * canvas_link->color.green) & 0xFF00) << 8) |
        ((int) (scale * canvas_link->color.blue) & 0xFF00) | 0xFF;
    }
  else
    {
      guint32 black = 0x000000ff;
      scaledColor = black;
    }

  /* retrieve coordinates of node centers */
  g_object_get (G_OBJECT (canvas_src->group_item), "x", &xs, "y", &ys, NULL);
  g_object_get (G_OBJECT (canvas_dst->group_item), "x", &xd, "y", &yd, NULL);

  /* first draw triangle for src->dst */
  draw_oneside_link(xs, ys, xd, yd, &(link->link_stats.stats_out), scaledColor, 
                    canvas_link->src_item);

  /* then draw triangle for dst->src */
  draw_oneside_link(xd, yd, xs, ys, &(link->link_stats.stats_in), scaledColor, 
                    canvas_link->dst_item);

  return FALSE;

}				/* update_canvas_links */

/* given the src and dst node centers, plus a size, draws a triangle in the 
 * specified color on the provided canvas item*/
static void draw_oneside_link(double xs, double ys, double xd, double yd,
                              const basic_stats_t *link_stats, 
                              guint32 scaledColor, GnomeCanvasItem *item)
{
  GnomeCanvasPoints *points;
  gdouble versorx, versory, modulus, link_size;

  link_size = get_link_size(link_stats) / 2;

  /* limit the maximum size to avoid overload */
  if (link_size > MAX_LINK_SIZE)
    link_size = MAX_LINK_SIZE; 

  versorx = -(yd - ys);
  versory = xd - xs;
  modulus = sqrt (pow (versorx, 2) + pow (versory, 2));
  if (modulus == 0)
    {
      link_size = 0;
      modulus = 1;
    }

  points = gnome_canvas_points_new (3);
  points->coords[0] = xd;
  points->coords[1] = yd;
  points->coords[2] = xs + versorx * link_size / modulus;
  points->coords[3] = ys + versory * link_size / modulus;
  points->coords[4] = xs - versorx * link_size / modulus;
  points->coords[5] = ys - versory * link_size / modulus;

  gnome_canvas_item_set (item, 
                          "points", points,
                          "fill_color_rgba", scaledColor, NULL);

  /* If we got this far, the link can be shown. Make sure it is */
  gnome_canvas_item_show (item);
  gnome_canvas_points_unref (points);
}



/* Returs the radius in pixels given average traffic and size mode */
static gdouble
get_node_size (gdouble average)
{
  gdouble result = 0.0;
  switch (pref.size_mode)
    {
    case LINEAR:
      result = average + 1;
      break;
    case LOG:
      result = log (average + 1);
      break;
    case SQRT:
      result = sqrt (average + 1);
      break;
    }
  return 5.0 + pref.node_radius_multiplier * result;
}

/* Returs the width in pixels given average traffic and size mode */
static gdouble get_link_size (const basic_stats_t *link_stats)
{
  gdouble result = 0.0;
  gdouble data;

  /* since links are one-sided, there's no distinction between inbound/outbound
     data.   */
  switch (pref.node_size_variable)
    {
    case INST_TOTAL:
    case INST_INBOUND:
    case INST_OUTBOUND:
    case INST_PACKETS: /* active packets stat not available */
      data = link_stats->average;
      break;
    case ACCU_TOTAL:
    case ACCU_INBOUND:
    case ACCU_OUTBOUND:
      data = link_stats->accumulated;
      break;
    case ACCU_PACKETS:
      data = link_stats->accu_packets;
      break;
    case ACCU_AVG_SIZE:
      data = link_stats->avg_size;
      break;
    default:
      data = link_stats->average;
      g_warning (_("Unknown value for link_size_variable"));
    }
  switch (pref.size_mode)
    {
    case LINEAR:
      result = data + 1;
      break;
    case LOG:
      result = log(data + 1);
      break;
    case SQRT:
      result = sqrt(data + 1);
      break;
    }
  return 1.0 + pref.node_radius_multiplier * pref.link_node_ratio * result;
}				/* get_link_size */



/* Called for every event a link receives. Right now it's used to 
 * set a message in the statusbar */
static gint
link_item_event (GnomeCanvasItem * item, GdkEvent * event,
		 canvas_link_t * canvas_link)
{
  gchar *str;
  const link_t *link=NULL;

  switch (event->type)
    {
    case GDK_2BUTTON_PRESS:
      if (canvas_link)
        link_info_window_create( &canvas_link->canvas_link_id );
      break;

    case GDK_ENTER_NOTIFY:
      if (canvas_link)
        link = links_catalog_find(&canvas_link->canvas_link_id);
      if (link && link->main_prot[pref.stack_level])
	str = g_strdup_printf (_("Link main protocol: %s"),
			   link->main_prot[pref.stack_level]);
      else
	str = g_strdup_printf (_("Link main protocol unknown"));
      gtk_statusbar_push(appdata.statusbar, 1, str);
      g_free (str);
      break;
    case GDK_LEAVE_NOTIFY:
      gtk_statusbar_pop(appdata.statusbar, 1);
      break;
    default:
      break;
    }

  return FALSE;
}				/* link_item_event */


/* Called for every event a node receives. Right now it's used to 
 * set a message in the statusbar and launch the popup timeout */
static gint
node_item_event (GnomeCanvasItem * item, GdkEvent * event,
		 canvas_node_t * canvas_node)
{

  gdouble item_x, item_y;
  const node_t *node = NULL;

  /* This is not used yet, but it will be. */
  item_x = event->button.x;
  item_y = event->button.y;
  gnome_canvas_item_w2i (item->parent, &item_x, &item_y);

  switch (event->type)
    {

    case GDK_2BUTTON_PRESS:
      if (canvas_node)
        node = nodes_catalog_find(&canvas_node->canvas_node_id);
      if (node)
        {
          node_protocols_window_create( &canvas_node->canvas_node_id );
          g_my_info ("Nodes: %d (shown %u)", nodes_catalog_size(),
                     displayed_nodes);
          if (DEBUG_ENABLED)
            {
              gchar *msg = node_dump(node);
              g_my_debug("%s", msg);
              g_free(msg);
            }
        }
      break;
    default:
      break;
    }

  return FALSE;

}				/* node_item_event */

/* Pushes a string into the statusbar stack */
void
set_statusbar_msg (gchar * str)
{
  static gchar *status_string = NULL;

  if (status_string)
    g_free (status_string);

  status_string = g_strdup (str);

  gtk_statusbar_pop(appdata.statusbar, 0);
  gtk_statusbar_push(appdata.statusbar, 0, status_string);
}				/* set_statusbar_msg */


static gint canvas_node_compare(const node_id_t *a, const node_id_t *b,
                                gpointer dummy)
{
  g_assert (a != NULL);
  g_assert (b != NULL);
  return node_id_compare(a, b);
}

static gint canvas_link_compare(const link_id_t *a, const link_id_t *b,
                                gpointer dummy)
{
  g_assert (a != NULL);
  g_assert (b != NULL);
  return link_id_compare(a, b);
}

static void 
canvas_link_delete(canvas_link_t *canvas_link)
{
   /* Right now I'm not very sure in which cases there could be a canvas_link but not a link_item, but
   * I had a not in update_canvas_nodes that if the test is not done it can lead to corruption */
  if (canvas_link->src_item)
    {
      gtk_object_destroy (GTK_OBJECT (canvas_link->src_item));
      g_object_unref (G_OBJECT (canvas_link->src_item));
      canvas_link->src_item = NULL;
    }
  if (canvas_link->dst_item)
    {
      gtk_object_destroy (GTK_OBJECT (canvas_link->dst_item));
      g_object_unref (G_OBJECT (canvas_link->dst_item));
      canvas_link->dst_item = NULL;
    }

  g_free (canvas_link);
}

void timeout_changed(void)
{
  /* When removing the source (which could either be an idle or a timeout
   * function, I'm also forcing the callback for the corresponding 
   * destroying function, which in turn will install a timeout or idle
   * function using the new refresh_period. It might take a while for it
   * to settle down, but I think it works now */
  g_source_remove (diagram_timeout);
}
