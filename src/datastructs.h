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

#ifndef DATASTRUCT_H
#define DATASTRUCT_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "globals.h"
#include "prot_types.h"

typedef struct
{
  GdkColor color;
  gboolean preferred;
} ColorHashItem;


/* clears the proto hash */
void  protohash_clear(void);

/* returns the proto color */
GdkColor protohash_color(const gchar *protoname);

/* returns the preferred flag */
gboolean protohash_is_preferred(const gchar *protoname);

/* fills the hash from a pref vector */
gboolean protohash_read_prefvect(gchar **colors);

/* resets the cycle color to start of list */
void protohash_reset_cycle(void);

/* compacts the array of colors/protocols mappings by collapsing identical
 * colors - frees the input array */
gchar **protohash_compact(gchar **colors);

/* removes all spaces from str (in place). Returns str */
gchar *remove_spaces(gchar *str);

typedef struct
{
  port_type_t port;
  gchar *name;
  gboolean preferred;   /* true if has to be favored when choosing proto name */
} port_service_t;

/* service mappers */
void services_init(void);
void services_clear(void);

const port_service_t *services_tcp_find(port_type_t port);
const port_service_t *services_udp_find(port_type_t port);
const port_service_t *services_port_find(const gchar *name);

#endif
