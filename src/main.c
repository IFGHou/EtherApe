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

#include <gtk/gtk.h>
#include <stdlib.h>

#include <math.h>

#include "interface.h"
#include "support.h"
#include "capture.h"

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

void 
draw_nodes (gpointer ether_addr, node_t *node, draw_nodes_data_t * draw_nodes_data)
{

  static gfloat angle;		/* Angle at which this node is to be drawn */
  gint x,y,xmax,ymax;
  gint rad_max;
   
  xmax = drawing_area->allocation.width;
  ymax = drawing_area->allocation.height;
  rad_max = (xmax > ymax) ? 0.75 * (xmax/2) : 0.75 * (ymax/2);

  if (draw_nodes_data->first_flag)
    {
      draw_nodes_data->first_flag = FALSE;
      angle = 0;
    }
   
  node->average=100;

  x = xmax / 2 + rad_max * cosf (angle) -node->average/2;
  y = ymax / 2 + rad_max * sinf (angle) -node->average/2;
   
  gdk_draw_arc (pixmap,
		drawing_area->style->black_gc,
		TRUE,
		x,
		y,
		node->average,
		node->average,
		0,
		360000);
   
  angle+=2*M_PI/draw_nodes_data->n_nodes;

}

guint
draw_diagram (gpointer data)
{

  GdkRectangle update_rect;
  draw_nodes_data_t draw_nodes_data;
  char texto[2];
  static gint x=0,y=0,frames=0;

  /* Resets the pixmap */
  gdk_draw_rectangle (pixmap,
		      drawing_area->style->white_gc,
		      TRUE,
		      0, 0,
		      drawing_area->allocation.width,
		      drawing_area->allocation.height);

  update_rect.x = x += 5;
  update_rect.y = y += 5;
  update_rect.width = 10;
  update_rect.height = 10;

  gdk_draw_rectangle (pixmap,
		      drawing_area->style->black_gc,
		      TRUE,
		      update_rect.x, update_rect.y,
		      update_rect.width, update_rect.height);

  texto[0] = frames++;
  texto[1] = '/0';

  gdk_draw_text (pixmap,
		 fixed_font,
		 drawing_area->style->black_gc,
		 drawing_area->allocation.width / 2,
		 drawing_area->allocation.height / 2,
		 texto,
		 1);

  draw_nodes_data.n_nodes = g_hash_table_size (nodes);
  draw_nodes_data.first_flag = TRUE;
  g_hash_table_foreach (nodes, draw_nodes, &draw_nodes_data);

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
  GtkWidget *window_main;


  init_capture ();

  gtk_set_locale ();
  gtk_init (&argc, &argv);

  add_pixmap_directory (PACKAGE_DATA_DIR "/pixmaps");
  add_pixmap_directory (PACKAGE_SOURCE_DIR "/pixmaps");

  fixed_font = gdk_font_load ("-misc-fixed-medium-r-*-*-*-140-*-*-*-*-*-*");

  /*
   * The following code was added by Glade to create one of each component
   * (except popup menus), just so that you see something after building
   * the project. Delete any components that you don't want shown initially.
   */
  window_main = create_window_main ();
  gtk_widget_show (window_main);

  gtk_timeout_add (100 /*ms */ ,
		   draw_diagram,
		   NULL);
  gtk_main ();
  return 0;
}
