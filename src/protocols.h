
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

#include "prot_types.h"

/* Funcions declarations */

static void get_eth_type (void);
static void get_fddi_type (void);
static void get_eth_II (etype_t etype);

static void get_ip (void);
static void get_tcp (void);
static gint tcp_compare (gconstpointer a, gconstpointer b);
static void get_udp (void);
static gint udp_compare (gconstpointer a, gconstpointer b);
