/* EtherApe
 * Copyright (C) 2001 Juan Toledo, Riccardo Ghetta
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

#ifndef ETHERAPE_NODE_H
#define ETHERAPE_NODE_H

#include "traffic_stats.h"

typedef struct
{
  node_id_t node_id;		/* node identification */
  GString *name;		/* String with a readable default name of the node */
  GString *numeric_name;	/* String with a numeric representation of the id */

  gchar *main_prot[STACK_SIZE + 1];	/* Most common protocol for the node */
  traffic_stats_t node_stats;
}
node_t;

node_t *node_create(const node_id_t * node_id, const gchar *node_id_str); /* creates a new node */
void node_delete(node_t *node); /* destroys a node, releasing memory */
gchar *node_dump(const node_t * node);
gboolean node_update(node_id_t * node_id, node_t *node, gpointer delete_list_ptr);

/* methods to handle every new node not yet handled in the main app */
void new_nodes_clear(void);
void new_nodes_add(node_t *node);
void new_nodes_remove(node_t *node);
node_t *new_nodes_pop(void);	/* Returns a new node that hasn't been heard of */

/* nodes catalog methods */
void nodes_catalog_open(void); /* initializes the catalog */
void nodes_catalog_close(void); /* closes the catalog, releasing all nodes */
node_t *nodes_catalog_find(const node_id_t *key); /* finds a node */
void nodes_catalog_insert(node_t *new_node); /* inserts a new node */
void nodes_catalog_remove(const node_id_t *key); /* removes AND DESTROYS the named node from catalog */
gint nodes_catalog_size(void); /* returns the current number of nodes in catalog */
void nodes_catalog_foreach(GTraverseFunc func, gpointer data); /* calls the func for every node */
void nodes_catalog_update_all(void);

/* returns a newly allocated str with a dump of all nodes */
gchar *nodes_catalog_dump(void);

#endif
