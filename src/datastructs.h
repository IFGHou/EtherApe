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

/* clears the proto hash */
void  protohash_clear(void);

/* adds or replaces the protoname item */
gboolean protohash_set(gchar *protoname, GdkColor protocolor);

/* returns the proto color if exists, NULL otherwise */
GdkColor * protohash_get(gchar *protoname);

/* fills the hash from a pref vector */
gboolean protohash_read_prefvect(gchar **colors, gint n_colors);

/* fills the pref vector from the hash */
gboolean protohash_write_prefvect();

#endif 

