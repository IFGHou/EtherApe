#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>

#include "interface.h"
#include "support.h"
#include "math.h"
#include "resolv.h"
#include "diagram.h"


guint averaging_time = 2000000;	/* Microseconds of time we consider to
				   * calculate traffic averages */
GTree *canvas_nodes;		/* We don't use the nodes tree directly in order to 
				 * separate data from presentation: that is, we need to
				 * keep a list of CanvasItems, but we do not want to keep
				 * that info on the nodes tree itself */
GTree *canvas_links;		/* See above */


extern gint ether_compare (gconstpointer a, gconstpointer b);
extern gint link_compare (gconstpointer a, gconstpointer b);

double
get_node_size (glong accumulated)
{
  return (double) 5 + 1000 * accumulated / averaging_time;
}

double
get_link_size (glong accumulated)
{
  return (double) 1000 *accumulated / averaging_time;
}

gint
reposition_canvas_nodes (guint8 * ether_addr, canvas_node_t * canvas_node, GtkWidget * canvas)
{
  static gfloat angle = 0.0;
  static guint node_i = 0, n_nodes = 0;
  GnomeCanvasGroup *group;
  double x, y, xmin, ymin, xmax, ymax, rad_max, text_compensation = 50;


  gnome_canvas_get_scroll_region (canvas,
				  &xmin,
				  &ymin,
				  &xmax,
				  &ymax);
/*  gnome_canvas_window_to_world (canvas, xmin, ymin, &xmin, &ymin);
   gnome_canvas_window_to_world (canvas, xmax, ymax, &xmax, &ymax); */

/*   gnome_canvas_c2w (canvas, xmin, ymin, &xmin, &ymin);
   gnome_canvas_c2w (canvas, xmax, ymax, &xmax, &ymax); */

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
  x = rad_max * cosf (angle);
  y = rad_max * sinf (angle);

  gnome_canvas_item_set (canvas_node->group_item,
			 "x", x,
			 "y", y,
			 NULL);
  gnome_canvas_item_request_update (canvas_node->text_item);

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
update_canvas_links (guint8 * ether_link, canvas_link_t * canvas_link, GtkWidget * canvas)
{
  link_t *link;
  GnomeCanvasPoints *points;
  canvas_node_t *canvas_node;
  GtkArg args[2];

  args[0].name = "x";
  args[1].name = "y";
  link = canvas_link->link;

  points = gnome_canvas_points_new (2);

  /* We get coords from source node */
  canvas_node = g_tree_lookup (canvas_nodes, ether_link);
  gtk_object_getv (canvas_node->group_item,
		   2,
		   args);
  points->coords[0] = args[0].d.double_data;
  points->coords[1] = args[1].d.double_data;

  /* And then for the destination node */
  canvas_node = g_tree_lookup (canvas_nodes, ether_link + 6);
  gtk_object_getv (canvas_node->group_item,
		   2,
		   args);
  points->coords[2] = args[0].d.double_data;
  points->coords[3] = args[1].d.double_data;

  link->average = get_link_size (link->accumulated);

  gnome_canvas_item_set (canvas_link->link_item,
			 "points", points,
			 "fill_color", "tan",
			 "outline_color", "black",
			 "width_units", link->average,
			 NULL);

  gnome_canvas_points_unref (points);

  return FALSE;

}				/* update_canvas_links */

gint
update_canvas_nodes (guint8 * ether_addr, canvas_node_t * canvas_node, GtkWidget * canvas)
{
  node_t *node;
  node = canvas_node->node;

  node->average = get_node_size (node->accumulated);


  gnome_canvas_item_set (canvas_node->node_item,
			 "x1", -node->average / 2,
			 "x2", node->average / 2,
			 "y1", -node->average / 2,
			 "y2", node->average / 2,
			 NULL);

  return FALSE;

}				/* update_canvas_nodes */

gint
check_new_link (guint8 * ether_link, link_t * link, GtkWidget * canvas)
{
  canvas_link_t *new_canvas_link;
  canvas_node_t *canvas_node;
  GnomeCanvasGroup *group;
  GnomeCanvasPoints *points;
  node_t *node;
  double x, y;
  GtkArg args[2];
  args[0].name = "x";
  args[1].name = "y";



  if (!g_tree_lookup (canvas_links, ether_link))
    {
      group = gnome_canvas_root (GNOME_CANVAS (canvas));

      new_canvas_link = g_malloc (sizeof (canvas_link_t));
      new_canvas_link->ether_link = ether_link;
      new_canvas_link->link = link;

      /* We set the lines position using groups positions */
      points = gnome_canvas_points_new (2);

      /* We get coords from source node */
      canvas_node = g_tree_lookup (canvas_nodes, ether_link);
      gtk_object_getv (canvas_node->group_item,
		       2,
		       args);
      points->coords[0] = args[0].d.double_data;
      points->coords[1] = args[1].d.double_data;

      /* And then for the destination node */
      canvas_node = g_tree_lookup (canvas_nodes, ether_link + 6);
      gtk_object_getv (canvas_node->group_item,
		       2,
		       args);
      points->coords[2] = args[0].d.double_data;
      points->coords[3] = args[1].d.double_data;

      link->average = get_link_size (link->accumulated);

      new_canvas_link->link_item = gnome_canvas_item_new (group,
					   gnome_canvas_polygon_get_type (),
							  "points", points,
						      "fill_color", "green",
						   "outline_color", "green",
					       "width_units", link->average,
							  NULL);


      g_tree_insert (canvas_links, ether_link, new_canvas_link);
      gnome_canvas_item_lower_to_bottom (new_canvas_link->link_item);

      gnome_canvas_points_unref (points);

      g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	     _ ("Creating canvas_link: %s-%s. Number of links %d"),
	     get_ether_name (new_canvas_link->ether_link),
	     get_ether_name ((new_canvas_link->ether_link) + 6),
	     g_tree_nnodes (canvas_links));

    }

  return FALSE;
}				/* check_new_link */



gint
check_new_node (guint8 * ether_addr, node_t * node, GtkWidget * canvas)
{
  canvas_node_t *new_canvas_node;
  GnomeCanvasGroup *group;


  if (!g_tree_lookup (canvas_nodes, ether_addr))
    {
      group = gnome_canvas_root (GNOME_CANVAS (canvas));

      new_canvas_node = g_malloc (sizeof (canvas_node_t));
      new_canvas_node->ether_addr = ether_addr;
      new_canvas_node->node = node;
      node->average = get_node_size (node->accumulated);

      group = GNOME_CANVAS_GROUP (gnome_canvas_item_new (group,
					     gnome_canvas_group_get_type (),
							 "x", 0.0,
							 "y", 0.0,
							 NULL));

      new_canvas_node->node_item = gnome_canvas_item_new (group,
						  GNOME_TYPE_CANVAS_ELLIPSE,
							  "x1", 0.0,
							"x2", node->average,
							  "y1", 0.0,
							"y2", node->average,
					      "fill_color_rgba", 0xFF0000FF,
						   "outline_color", "black",
							  "width_pixels", 0,
							  NULL);
      new_canvas_node->text_item = gnome_canvas_item_new (group,
						     GNOME_TYPE_CANVAS_TEXT,
						    "text", node->name->str,
							  "x", 0.0,
							  "y", 0.0,
						"anchor", GTK_ANCHOR_CENTER,
		       "font", "-misc-fixed-medium-r-*-*-*-140-*-*-*-*-*-*",
						      "fill_color", "black",
							  NULL);
/*      new_canvas_node->accu_str = g_strdup_printf ("%f",node->accumulated);
   new_canvas_node->accu_item = gnome_canvas_item_new (group,
   GNOME_TYPE_CANVAS_TEXT,
   "text", new_canvas_node->accu_str,
   "x", 0.0,
   "y", 10.0,
   "anchor", GTK_ANCHOR_CENTER,
   "font","-misc-fixed-medium-r-*-*-*-140-*-*-*-*-*-*",
   "fill_color", "black",
   NULL); */
      new_canvas_node->group_item = group;

      gnome_canvas_item_raise_to_top (new_canvas_node->text_item);

      g_tree_insert (canvas_nodes, ether_addr, new_canvas_node);
      g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, \
	     _ ("Creating canvas_node: %s. Number of nodes %d"), \
	     get_ether_name (new_canvas_node->ether_addr), \
	     g_tree_nnodes (canvas_nodes));

    }

  return FALSE;
}				/* check_new_node */

guint
update_diagram (GtkWidget * canvas)
{
  static GnomeCanvasItem *circle = NULL;
  GnomeCanvasGroup *group;
  static guint n_nodes = 0, n_nodes_new;

  /* Check if there are any new nodes */
  g_tree_traverse (nodes, check_new_node, G_IN_ORDER, canvas);

  /* Reposition canvas_nodes 
   * TODO: This should be conditional. Look for a way to know
   * whether the canvas needs updating, that is, a new node has been added
   */
  if (n_nodes != (n_nodes_new = g_tree_nnodes (nodes)))
    {
      g_tree_traverse (canvas_nodes, reposition_canvas_nodes, G_IN_ORDER, canvas);
      n_nodes = n_nodes_new;
    }

  /* Update nodes aspect */
  g_tree_traverse (canvas_nodes, update_canvas_nodes, G_IN_ORDER, canvas);

  /* Check if there are any new links */
  g_tree_traverse (links, check_new_link, G_IN_ORDER, canvas);

  /* Update links aspect */
  g_tree_traverse (canvas_links, update_canvas_links, G_IN_ORDER, canvas);

  group = gnome_canvas_root (GNOME_CANVAS (canvas));

#if 0
  if (!circle)
    {
      circle = gnome_canvas_item_new (group,
				      GNOME_TYPE_CANVAS_ELLIPSE,
				      "x1", 0.0, "x2", 100.0,
				      "y1", 0.0, "y2", 100.0,
				      "fill_color_rgba", 0x00FF00FF,
				      "outline_color", "black",
				      "width_pixels", 0,
				      NULL);
    }
#endif

}

void
init_diagram (void)
{
  canvas_nodes = g_tree_new (ether_compare);
  canvas_links = g_tree_new (link_compare);
}
