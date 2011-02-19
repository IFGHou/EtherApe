
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

#ifndef DECODE_PROTO_H
#define DECODE_PROTO_H

#include "pkt_info.h"
#include "node_id.h"

gboolean has_linklevel(void); /* true if current device captures l2 data */ 
gboolean setup_link_type(unsigned int linktype);
void packet_acquired(guint8 * packet, guint raw_size, guint pkt_size);

#endif
