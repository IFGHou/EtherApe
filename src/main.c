

/* Program Name
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
#include "capture.h"
#include "math.h"

guint averaging_time = 10000000;	/* Microseconds of time we consider to
					 * calculate traffic averages */
GTree *canvas_nodes;		/* We don't use the nodes tree directly in order to 
				 * separate data from presentation: that is, we need to
				 * keep a list of CanvasItems, but we do not want to keep
				 * that info on the nodes tree itself */
typedef struct
  {
    guint8 *ether_addr;
    node_t *node;
    GnomeCanvasItem *node_item;
    GnomeCanvasItem *text_item;
    GnomeCanvasGroup *group_item;
  }
canvas_node_t;

extern gint ether_compare (gconstpointer a, gconstpointer b);


GdkFont *fixed_font;


gint 
reposition_canvas_nodes (guint8 *ether_addr, canvas_node_t *canvas_node, GtkWidget *canvas)
{
   static gfloat angle=0.0;
   static guint node_i=0,n_nodes=0;
   GnomeCanvasGroup *group;
   double x,y,xmax,ymax,rad_max=150.0;
   
   if (!n_nodes) 
     {
	n_nodes=node_i=g_tree_nnodes(canvas_nodes);
     }

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
	angle=0.0;
	n_nodes=0;
     }
   
   return FALSE;
}


gint 
update_canvas_nodes (guint8 *ether_addr, canvas_node_t *canvas_node, GtkWidget *canvas)
{
   node_t *node;
   node=canvas_node->node;
   
   node->average = node->accumulated * 1000000 / averaging_time;


   gnome_canvas_item_set (canvas_node->node_item,
			  "x1", -(double)node->average/2,
			  "x2", (double) node->average/2,
			  "y1", -(double)node->average/2,
			  "y2", (double) node->average/2,
			  NULL);
   
   return FALSE;

}

gint
check_new_node (guint8 *ether_addr, node_t * node, GtkWidget * canvas)
{
  canvas_node_t *new_canvas_node;
  GnomeCanvasGroup *group;


  if (!g_tree_lookup (canvas_nodes, ether_addr))
    {
      group = gnome_canvas_root (GNOME_CANVAS (canvas));

      new_canvas_node = g_malloc (sizeof (canvas_node_t));
      new_canvas_node->ether_addr = ether_addr;
      new_canvas_node->node = node;
      node->average = node->accumulated * 1000000 / averaging_time;

      group = GNOME_CANVAS_GROUP (gnome_canvas_item_new (group,
							  gnome_canvas_group_get_type (),
							  "x", 0.0,
							  "y", 0.0,
							  NULL));
       
      new_canvas_node->node_item = gnome_canvas_item_new (group, 
							    GNOME_TYPE_CANVAS_ELLIPSE,
							    "x1", 0.0,
							    "x2", (double) node->average,
							    "y1", 0.0,
							    "y2", (double) node->average,
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
							  "font","-misc-fixed-medium-r-*-*-*-140-*-*-*-*-*-*",
							  "fill_color", "black",
							  NULL);
      new_canvas_node->group_item=group;

      g_tree_insert (canvas_nodes, ether_addr, new_canvas_node);
      g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, \
	     _ ("Creating canvas_node: %s. Number of nodes %d"), \
	     ether_to_str (new_canvas_node->ether_addr), \
	     g_tree_nnodes (canvas_nodes));

    }
   
   return FALSE;
}

guint
update_diagram (GtkWidget * canvas)
{
  static GnomeCanvasItem *circle = NULL;
  GnomeCanvasGroup *group;

  /* Check if there are any new nodes */
  g_tree_traverse (nodes, check_new_node, G_IN_ORDER, canvas);
   
  /* Reposition canvas_nodes 
   * TODO: This should be conditional. Look for a way to know
   * whether the canvas needs updating, that is, a new node has been added
   */
  g_tree_traverse (canvas_nodes, reposition_canvas_nodes, G_IN_ORDER, canvas);
   
  /* Update nodes aspect */
  g_tree_traverse (canvas_nodes, update_canvas_nodes, G_IN_ORDER, canvas);
   
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

int
main (int argc, char *argv[])
{
  GtkWidget *app1;

#ifdef ENABLE_NLS
  bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
  textdomain (PACKAGE);
#endif

  init_capture ();
  canvas_nodes = g_tree_new (ether_compare);

  gnome_init ("etherape", VERSION, argc, argv);

  fixed_font = gdk_font_load ("-misc-fixed-medium-r-*-*-*-140-*-*-*-*-*-*");

  app1 = create_app1 ();
  gtk_widget_show (app1);

  gtk_timeout_add (100 /*ms */ ,
		   update_diagram,
		   lookup_widget (GTK_WIDGET (app1), "canvas1"));

  gtk_main ();
  return 0;
}
