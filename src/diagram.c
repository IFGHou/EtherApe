/* Etherape
 * Copyright (C) 2000 Juan Toledo
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

#include "interface.h"
#include "support.h"
#include "math.h"
#include "resolv.h"
#include "diagram.h"

/* Global application parameters */

double node_radius_multiplier = 100;	/* used to calculate the radius of the
					 * displayed nodes. So that the user can
					 * select with certain precision this
					 * value, the GUI uses the log10 of the
					 * multiplier*/
double link_width_multiplier = 100;	/* Same explanation as above */

gchar *node_color = "red", *link_color = "tan", *text_color = "black";


GTree *canvas_nodes;		/* We don't use the nodes tree directly in order to 
				 * separate data from presentation: that is, we need to
				 * keep a list of CanvasItems, but we do not want to keep
				 * that info on the nodes tree itself */
GTree *canvas_links;		/* See above */

/* Extern global variables */

extern double averaging_time;
extern double link_timeout_time;
extern double node_timeout_time;
extern guint32 refresh_period;

/* Extern functions declarations */

extern gint node_id_compare (gconstpointer a, gconstpointer b);
extern gint link_id_compare (gconstpointer a, gconstpointer b);
extern gboolean diagram_only;
extern gboolean interape;


/* Local functions definitions */

gdouble
get_node_size (gdouble average)
{
  return (double) 5 + node_radius_multiplier * average;
}

gdouble
get_link_size (gdouble average)
{
  return (double) 1 + link_width_multiplier * average;
}

static gint
node_item_event (GnomeCanvasItem * item, GdkEvent * event, canvas_node_t * canvas_node)
{

  double item_x, item_y;
  static GtkWidget *node_popup;
  GtkLabel *label;


  item_x = event->button.x;
  item_y = event->button.y;
  gnome_canvas_item_w2i (item->parent, &item_x, &item_y);

  switch (event->type)
    {

    case GDK_BUTTON_PRESS:
      node_popup = create_node_popup ();
      label = (GtkLabel *) lookup_widget (GTK_WIDGET (node_popup), "name");
      gtk_label_set_text (label, canvas_node->node->name->str);
      label = (GtkLabel *) lookup_widget (GTK_WIDGET (node_popup), "ip_str");
      gtk_label_set_text (label, canvas_node->node->ip_str->str);
      label = (GtkLabel *) lookup_widget (GTK_WIDGET (node_popup), "ip_numeric_str");
      gtk_label_set_text (label, canvas_node->node->ip_numeric_str->str);
      label = (GtkLabel *) lookup_widget (GTK_WIDGET (node_popup), "ether_str");
      gtk_label_set_text (label, canvas_node->node->ether_str->str);
      label = (GtkLabel *) lookup_widget (GTK_WIDGET (node_popup), "ether_numeric_str");
      gtk_label_set_text (label, canvas_node->node->ether_numeric_str->str);
      label = (GtkLabel *) lookup_widget (GTK_WIDGET (node_popup), "accumulated");
      gtk_label_set_text (label,
		    g_strdup_printf ("%g", canvas_node->node->accumulated));
      label = (GtkLabel *) lookup_widget (GTK_WIDGET (node_popup), "average");
      gtk_label_set_text (label,
	      g_strdup_printf ("%g", canvas_node->node->average * 1000000));
      gtk_widget_show (GTK_WIDGET (node_popup));
      break;
    case GDK_BUTTON_RELEASE:
      gtk_widget_destroy (GTK_WIDGET (node_popup));
    default:
      break;
    }

  return FALSE;

}

gint
reposition_canvas_nodes (guint8 * ether_addr, canvas_node_t * canvas_node, GtkWidget * canvas)
{
  static gfloat angle = 0.0;
  static guint node_i = 0, n_nodes = 0;
  double x, y, xmin, ymin, xmax, ymax, rad_max, text_compensation = 50;


  gnome_canvas_get_scroll_region (GNOME_CANVAS (canvas),
				  &xmin,
				  &ymin,
				  &xmax,
				  &ymax);
  if (!n_nodes)
    {
      n_nodes = node_i = g_tree_nnodes (canvas_nodes);
    }

  xmin += text_compensation;
  xmax -= text_compensation;	/* Reduce the drawable area so that
				 * the node name is not lost
				 * TODO: Need a function to calculate
				 * text_compensation depending on font size */
  rad_max = ((xmax - xmin) > (ymax - ymin)) ? 0.9 * (y = (ymax - ymin)) / 2 : 0.9 * (x = (xmax - xmin)) / 2;
  x = rad_max * cos (angle);
  y = rad_max * sin (angle);

  gnome_canvas_item_set (GNOME_CANVAS_ITEM (canvas_node->group_item),
			 "x", x,
			 "y", y,
			 NULL);
  if (diagram_only)
    {
      gnome_canvas_item_hide (canvas_node->text_item);
    }
  else
    {
      gnome_canvas_item_show (canvas_node->text_item);
      gnome_canvas_item_request_update (canvas_node->text_item);
      gnome_canvas_item_request_update (canvas_node->node_item);

    }

  node_i--;

  if (node_i)
    {
      angle += 2 * M_PI / n_nodes;
    }
  else
    {
      angle = 0.0;
      n_nodes = 0;
    }

  return FALSE;
}				/* reposition_canvas_nodes */


gint
update_canvas_links (guint8 * link_id, canvas_link_t * canvas_link, GtkWidget * canvas)
{
  link_t *link;
  GnomeCanvasPoints *points;
  canvas_node_t *canvas_node;
  GtkArg args[2];
  gdouble link_size;

  link = canvas_link->link;


  /* First we check whether the link has timed out */

  if (link->packets)
    update_packet_list (link->packets, LINK);

  if (link->n_packets == 0)
    {
      if (link_timeout_time)
	{

	  gtk_object_destroy (GTK_OBJECT (canvas_link->link_item));

	  g_tree_remove (canvas_links, link_id);

	  if (interape)
	    g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	     _ ("Removing link and canvas_link: %s-%s. Number of links %d"),
		   ip_to_str (link_id),
		   ip_to_str (link_id + 4),
		   g_tree_nnodes (canvas_links));
	  else
	    g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	     _ ("Removing link and canvas_link: %s-%s. Number of links %d"),
		   get_ether_name (link_id + 6),
		   get_ether_name (link_id),
		   g_tree_nnodes (canvas_links));


	  link_id = link->link_id;	/* Since we are freeing the link
					 * we must free its members as well 
					 * but if we free the id then we will
					 * not be able to find the link again 
					 * to free it, thus the intermediate variable */
	  g_free (link);
	  g_tree_remove (links, link_id);
	  g_free (link_id);

	  return TRUE;		/* I've checked it's not safe to traverse 
				 * while deleting, so we return TRUE to stop
				 * the traversion (Does that word exist? :-) */
	}
      else
	link->packets = NULL;
      link->accumulated = 0;
    }


  args[0].name = "x";
  args[1].name = "y";

  points = gnome_canvas_points_new (2);

  /* If either source or destination has disappeared, we hide the link
   * until it can be show again */
  /* TODO: This is a dirty hack. Redo this again later by properly 
   * deleting the link */

  /* We get coords from source node */
  canvas_node = g_tree_lookup (canvas_nodes, link_id);
  if (!canvas_node)
    {
      gnome_canvas_item_hide (canvas_link->link_item);
      return FALSE;
    }
  gtk_object_getv (GTK_OBJECT (canvas_node->group_item),
		   2,
		   args);
  points->coords[0] = args[0].d.double_data;
  points->coords[1] = args[1].d.double_data;

  /* And then for the destination node */
  if (interape)
    canvas_node = g_tree_lookup (canvas_nodes, link_id + 4);
  else
    canvas_node = g_tree_lookup (canvas_nodes, link_id + 6);
  if (!canvas_node)
    {
      gnome_canvas_item_hide (canvas_link->link_item);
      return FALSE;
    }
  gtk_object_getv (GTK_OBJECT (canvas_node->group_item),
		   2,
		   args);
  points->coords[2] = args[0].d.double_data;
  points->coords[3] = args[1].d.double_data;

  /* If we got this far, the link can be shown. Make sure it is */
  gnome_canvas_item_show (canvas_link->link_item);

  link_size = get_link_size (link->average);

  gnome_canvas_item_set (canvas_link->link_item,
			 "points", points,
			 "fill_color", link_color,
			 "width_units", link_size,
			 NULL);

  gnome_canvas_points_unref (points);

  return FALSE;

}				/* update_canvas_links */

gint
update_canvas_nodes (guint8 * node_id, canvas_node_t * canvas_node, GtkWidget * canvas)
{
  node_t *node;
  gdouble node_size;
  node = canvas_node->node;

  /* First we check whether the link has timed out */

  if (node->packets)
    update_packet_list (node->packets, NODE);

  if (node->n_packets == 0)
    {
      if (node_timeout_time)
	{

	  gtk_object_destroy (GTK_OBJECT (canvas_node->group_item));

	  g_tree_remove (canvas_nodes, node_id);

	  if (interape)
	    g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
		 _ ("Removing node and canvas_node: %s. Number of node %d"),
		   ip_to_str (node_id),
		   g_tree_nnodes (canvas_nodes));
	  else
	    g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
		_ ("Removing node and canvas_node: %s. Number of nodes %d"),
		   get_ether_name (node_id),
		   g_tree_nnodes (canvas_nodes));


	  node_id = node->node_id;	/* Since we are freeing the node
					 * we must free its members as well 
					 * but if we free the id then we will
					 * not be able to find the link again 
					 * to free it, thus the intermediate variable */
	  g_free (node);
	  g_tree_remove (nodes, node_id);
	  g_free (node_id);

	  return TRUE;		/* I've checked it's not safe to traverse 
				 * while deleting, so we return TRUE to stop
				 * the traversion (Does that word exist? :-) */
	}
      else
	node->packets = NULL;
      node->accumulated = 0;
    }


  node_size = get_node_size (node->average);

  gnome_canvas_item_set (canvas_node->node_item,
			 "x1", -node_size / 2,
			 "x2", node_size / 2,
			 "y1", -node_size / 2,
			 "y2", node_size / 2,
			 NULL);

  return FALSE;

}				/* update_canvas_nodes */

gint
check_new_link (guint8 * link_id, link_t * link, GtkWidget * canvas)
{
  canvas_link_t *new_canvas_link;
  GnomeCanvasGroup *group;
  GnomeCanvasPoints *points;

  GtkArg args[2];
  args[0].name = "x";
  args[1].name = "y";



  if (!g_tree_lookup (canvas_links, link_id))
    {
      group = gnome_canvas_root (GNOME_CANVAS (canvas));

      new_canvas_link = g_malloc (sizeof (canvas_link_t));
      new_canvas_link->canvas_link_id = link_id;
      new_canvas_link->link = link;

      /* We set the lines position using groups positions */
      points = gnome_canvas_points_new (2);

      points->coords[0] = points->coords[1] = points->coords[2] = points->coords[3] = 0.0;

      new_canvas_link->link_item = gnome_canvas_item_new (group,
					      gnome_canvas_line_get_type (),
							  "points", points,
						   "fill_color", link_color,
							  "width_units", 0.0,
							  NULL);


      g_tree_insert (canvas_links, link_id, new_canvas_link);
      gnome_canvas_item_lower_to_bottom (new_canvas_link->link_item);

      gnome_canvas_points_unref (points);

      if (interape)
	g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	       _ ("Creating canvas_link: %s-%s. Number of links %d"),
	       ip_to_str ((new_canvas_link->canvas_link_id) + 4),
	       ip_to_str (new_canvas_link->canvas_link_id),
	       g_tree_nnodes (canvas_links));
      else
	g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	       _ ("Creating canvas_link: %s-%s. Number of links %d"),
	       get_ether_name ((new_canvas_link->canvas_link_id) + 6),
	       get_ether_name (new_canvas_link->canvas_link_id),
	       g_tree_nnodes (canvas_links));

    }

  return FALSE;
}				/* check_new_link */


/* Checks if there is a canvas_node per each node. If not, one canvas_node
 * must be created and initiated */
gint
check_new_node (guint8 * node_id, node_t * node, GtkWidget * canvas)
{
  canvas_node_t *new_canvas_node;
  GnomeCanvasGroup *group;

  if (!g_tree_lookup (canvas_nodes, node_id))
    {
      group = gnome_canvas_root (GNOME_CANVAS (canvas));

      new_canvas_node = g_malloc (sizeof (canvas_node_t));
      new_canvas_node->canvas_node_id = node_id;
      new_canvas_node->node = node;

      group = GNOME_CANVAS_GROUP (gnome_canvas_item_new (group,
					     gnome_canvas_group_get_type (),
							 "x", 0.0,
							 "y", 0.0,
							 NULL));

      new_canvas_node->node_item = gnome_canvas_item_new (group,
						  GNOME_TYPE_CANVAS_ELLIPSE,
							  "x1", 0.0,
							  "x2", 0.0,
							  "y1", 0.0,
							  "y2", 0.0,
						   "fill_color", node_color,
						   "outline_color", "black",
							  "width_pixels", 0,
							  NULL);
      new_canvas_node->text_item = gnome_canvas_item_new (group
						     ,GNOME_TYPE_CANVAS_TEXT
						    ,"text", node->name->str
							  ,"x", 0.0
							  ,"y", 0.0
						,"anchor", GTK_ANCHOR_CENTER
		      ,"font", "-misc-fixed-medium-r-*-*-*-140-*-*-*-*-*-*",
						   "fill_color", text_color,
							  NULL);
      new_canvas_node->group_item = group;

      gnome_canvas_item_raise_to_top (GNOME_CANVAS_ITEM (new_canvas_node->text_item));
      gtk_signal_connect (GTK_OBJECT (new_canvas_node->group_item), "event",
			  (GtkSignalFunc) node_item_event,
			  new_canvas_node);

      g_tree_insert (canvas_nodes, node_id, new_canvas_node);
      g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, \
	     _ ("Creating canvas_node: %s. Number of nodes %d"), \
	     new_canvas_node->node->name->str, \
	     g_tree_nnodes (canvas_nodes));

    }

  return FALSE;
}				/* check_new_node */

guint
update_diagram (GtkWidget * canvas)
{
  static GnomeAppBar *appbar=NULL;
  static guint n_nodes = 0, n_nodes_new;
  guint n_links = 0, n_links_new = 1;

/* Now we update the status bar with the number of present nodes 
 * TODO Find a nice use for the status bar. I can thik of very little */
  if (!appbar) appbar = GNOME_APPBAR(lookup_widget (GTK_WIDGET(canvas), "appbar1"));
  
   
  /* Check if there are any new nodes */
  g_tree_traverse (nodes,
		   (GTraverseFunc) check_new_node,
		   G_IN_ORDER,
		   canvas);


  /* Update nodes look and delete outdated nodes*/
  /* TODO: shouldn't we do the same thing we do with
   * the links down there, to go on until no nodes are delete? */
  g_tree_traverse (canvas_nodes,
		   (GTraverseFunc) update_canvas_nodes,
		   G_IN_ORDER,
		   canvas);

  /* Reposition canvas_nodes and update status bar if a node has been
   * added or deleted */

  if (n_nodes != (n_nodes_new = g_tree_nnodes (nodes)))
    {
       g_tree_traverse (canvas_nodes,
			(GTraverseFunc) reposition_canvas_nodes,
			G_IN_ORDER,
			canvas);
       gnome_appbar_pop (appbar);

       /* TODO: Am I leaking here with each call to g_strdup_printf?
	* How should I do this if so? */
       gnome_appbar_push (appbar,  g_strconcat (_("Number of nodes: "),
						g_strdup_printf ("%d", n_nodes_new),
						NULL));
       n_nodes = n_nodes_new;
    }


  /* Check if there are any new links */
  g_tree_traverse (links,
		   (GTraverseFunc) check_new_link,
		   G_IN_ORDER,
		   canvas);

  /* Update links look 
   * We also delete timedout links, and when we do that we stop
   * traversing, so we need to go on until we have finished updating */
   
  do
    {
      n_links = g_tree_nnodes (links);
      g_tree_traverse (canvas_links,
		       (GTraverseFunc) update_canvas_links,
		       G_IN_ORDER,
		       canvas);
      n_links_new = g_tree_nnodes (links);
    }
  while (n_links != n_links_new);


  return TRUE;			/* Keep on calling this function */

}				/* update_diagram */

void
init_diagram (GtkWidget *app1)
{
  GtkScale *scale;
  GtkSpinButton *spin;
   
  /* Creates trees */
  canvas_nodes = g_tree_new (node_id_compare);
  canvas_links = g_tree_new (link_id_compare);
   
  /* Updates controls from values of variables */
  scale = GTK_SCALE(lookup_widget (GTK_WIDGET (app1), "node_radius_slider"));
  gtk_adjustment_set_value (GTK_RANGE (scale)->adjustment, log(node_radius_multiplier)/log(10));
  gtk_signal_emit_by_name (GTK_OBJECT (GTK_RANGE (scale)->adjustment), "changed");

  scale = GTK_SCALE(lookup_widget (GTK_WIDGET (app1), "link_width_slider"));
  gtk_adjustment_set_value (GTK_RANGE (scale)->adjustment, log(link_width_multiplier)/log(10));
  gtk_signal_emit_by_name (GTK_OBJECT (GTK_RANGE (scale)->adjustment), "changed");

  spin = GTK_SPIN_BUTTON(lookup_widget (GTK_WIDGET (app1), "averaging_spin"));
  gtk_spin_button_set_value (spin, averaging_time/1000);
  
  spin = GTK_SPIN_BUTTON(lookup_widget (GTK_WIDGET (app1), "refresh_spin"));
  gtk_spin_button_set_value (spin, refresh_period);
  
  spin = GTK_SPIN_BUTTON(lookup_widget (GTK_WIDGET (app1), "node_to_spin"));
  gtk_spin_button_set_value (spin, node_timeout_time/1000);
  
  spin = GTK_SPIN_BUTTON(lookup_widget (GTK_WIDGET (app1), "link_to_spin"));
  gtk_spin_button_set_value (spin, link_timeout_time/1000);
  

}
