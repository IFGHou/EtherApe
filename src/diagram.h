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

static gboolean is_idle = FALSE;

struct popup_data
{
  GtkWidget *node_popup;
  canvas_node_t *canvas_node;
};

/* Function definitions */

static void check_new_protocol (protocol_t * protocol, GtkWidget * canvas);
static gint check_new_node (guint8 * ether_addr,
			    node_t * node, GtkWidget * canvas);
static gint update_canvas_nodes (guint8 * ether_addr,
				 canvas_node_t * canvas_node,
				 GtkWidget * canvas);
static gint reposition_canvas_nodes (guint8 * ether_addr,
				     canvas_node_t * canvas_node,
				     GtkWidget * canvas);
static gint check_new_link (guint8 * ether_link,
			    link_t * link, GtkWidget * canvas);
static gint update_canvas_links (guint8 * ether_link,
				 canvas_link_t * canvas_link,
				 GtkWidget * canvas);
static gchar *get_prot_color (gchar * name);
static gdouble get_node_size (gdouble average);
static gdouble get_link_size (gdouble average);
static gint link_item_event (GnomeCanvasItem * item,
			     GdkEvent * event, canvas_link_t * canvas_link);
static gint node_item_event (GnomeCanvasItem * item,
			     GdkEvent * event, canvas_node_t * canvas_node);
static guint popup_to (struct popup_data *pd);
