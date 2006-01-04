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

#ifndef ETHERAPE_LINKS_H
#define ETHERAPE_LINKS_H

#include "node.h"

/* a link identification */
typedef struct
{
  node_id_t src;
  node_id_t dst;
} 
link_id_t;
gint link_id_compare (const link_id_t *a, const link_id_t *b);

/* Link information */
typedef struct
{
  link_id_t link_id;		/* src and dest addresses of link */
  gchar *src_name;
  gchar *dst_name;
  gdouble average;
  gdouble accumulated;
  guint n_packets;
  struct timeval last_time;	/* Timestamp of the last packet added */
  GList *link_packets;		/* List of packets heard on this link */

  gchar *main_prot[STACK_SIZE + 1];	/* Most common protocol for the link */
  protostack_t link_protos;
}
link_t;
link_t *link_create(const link_id_t *link_id); /* creates a new link object */
void link_delete(link_t *link); /* destroys a link, releasing memory */

/* link catalog methods */
void links_catalog_open(void);
void links_catalog_close(void); /* closes the catalog, releasing all links */
void links_catalog_insert(link_t *new_link); /* insert a new link */
void links_catalog_remove(const link_id_t *key); /* removes AND DESTROYS the named link from catalog */
link_t *links_catalog_find(const link_id_t *key); /* finds a link */
link_t *links_catalog_find_create(const link_id_t *key); /* finds a link, creating one if necessary */
gint links_catalog_size(void); /* returns the current number of links in catalog */
void links_catalog_foreach(GTraverseFunc func, gpointer data);  /* calls the func for every link */
void links_catalog_update_all(void);


#endif
