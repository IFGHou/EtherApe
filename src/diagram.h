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

typedef struct
{
  guint8 *canvas_node_id;
  node_t *node;
  GnomeCanvasItem *node_item;
  GnomeCanvasItem *text_item;
  GnomeCanvasGroup *group_item;
  GdkColor color;
  gboolean is_new;
  gboolean shown;		/* True if it is to be displayed. */
  gboolean debug;		/* Debugging variable */
}
canvas_node_t;

typedef struct
{
  guint8 *canvas_link_id;
  link_t *link;
  GnomeCanvasItem *link_item;
  GdkColor color;
}
canvas_link_t;

GTree *canvas_nodes;		/* We don't use the nodes tree directly in order to 
				 * separate data from presentation: that is, we need to
				 * keep a list of CanvasItems, but we do not want to keep
				 * that info on the nodes tree itself */
GTree *canvas_links;		/* See above */
GList *legend_protocols;

static gboolean is_idle = FALSE;
static guint displayed_nodes;

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
static gboolean display_node (node_t * node);
static void limit_nodes (void);
static gint add_ordered_node (guint8 * node_id,
			      canvas_node_t * canvas_node,
			      GTree * ordered_nodes);
static gint check_ordered_node (gdouble * traffic, canvas_node_t * node,
				guint * count);
static gint traffic_compare (gconstpointer a, gconstpointer b);
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
