

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

typedef struct _draw_nodes_data
  {
    gboolean first_flag;
    guint n_nodes;		/* Number of nodes
				 * We put it here so that it does not have to be calculated
				 * everytime in draw_nodes */
  }
draw_nodes_data_t;

extern GdkPixmap *pixmap;
extern GtkWidget *drawing_area;
GdkFont *fixed_font;

gint
draw_nodes (gpointer ether_addr, node_t *node, draw_nodes_data_t *draw_nodes_data)
{

  static gfloat angle;		/* Angle at which this node is to be drawn */
  gint x, y, xmax, ymax;
  gint rad_max;

  xmax = drawing_area->allocation.width;
  ymax = drawing_area->allocation.height;
  rad_max = (xmax < ymax) ? 0.75 * (xmax / 2) : 0.75 * (ymax / 2);

  if (draw_nodes_data->first_flag)
    {
      draw_nodes_data->first_flag = FALSE;
      angle = 0;
    }

  x = xmax / 2 + rad_max * cosf (angle) - node->average / node->n_packets / 2;
  y = ymax / 2 + rad_max * sinf (angle) - node->average / node->n_packets / 2;

  gdk_draw_arc (pixmap,
		drawing_area->style->black_gc,
		TRUE,
		x,
		y,
		node->average / node->n_packets,
		node->average / node->n_packets,
		0,
		360000);

  gdk_draw_text (pixmap,
		 fixed_font,
		 drawing_area->style->black_gc,
		 x, y,
		 ether_to_str (node->ether_addr),
		 17);		/*Size of text */


  angle += 2 * M_PI / draw_nodes_data->n_nodes;

  return FALSE;			/* Continue with traverse function */
}

guint
draw_diagram (gpointer data)
{

  GdkRectangle update_rect;
  draw_nodes_data_t draw_nodes_data;
  static gint x = 0, y = 0;

  /* Resets the pixmap */
  gdk_draw_rectangle (pixmap,
		      drawing_area->style->white_gc,
		      TRUE,
		      0, 0,
		      drawing_area->allocation.width,
		      drawing_area->allocation.height);

  draw_nodes_data.n_nodes = g_tree_nnodes (nodes);
  draw_nodes_data.first_flag = TRUE;

  g_tree_traverse (nodes, draw_nodes, G_IN_ORDER, &draw_nodes_data);

  /* Tells gtk_main to update the whole widget area */
  update_rect.x = update_rect.y = 0;
  update_rect.width = drawing_area->allocation.width;
  update_rect.height = drawing_area->allocation.height;

  gtk_widget_draw (drawing_area, &update_rect);

  return TRUE;
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

  gnome_init ("etherape", VERSION, argc, argv);

  fixed_font = gdk_font_load ("-misc-fixed-medium-r-*-*-*-140-*-*-*-*-*-*");

  app1 = create_app1 ();
  gtk_widget_show (app1);

  gtk_timeout_add (100 /*ms */ ,
		   draw_diagram,
		   NULL);

  gtk_main ();
  return 0;
}
