/* Etherape
 * Copyright (C) 2000 Juan Toledo
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

#include "globals.h"

/* Function definitions */

gdouble get_node_size (gdouble average);
gdouble get_link_size (gdouble average);
gint reposition_canvas_nodes (guint8 * ether_addr,
			      canvas_node_t * canvas_node,
			      GtkWidget * canvas);
gint update_canvas_links (guint8 * ether_link, canvas_link_t * canvas_link,
			  GtkWidget * canvas);
gint update_canvas_nodes (guint8 * ether_addr, canvas_node_t * canvas_node,
			  GtkWidget * canvas);
gint check_new_link (guint8 * ether_link, link_t * link, GtkWidget * canvas);
gint check_new_node (guint8 * ether_addr, node_t * node, GtkWidget * canvas);
guint update_diagram (GtkWidget * canvas);
void init_diagram ();
