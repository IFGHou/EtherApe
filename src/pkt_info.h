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

#ifndef PKT_INFO_H
#define PKT_INFO_H

#include "common.h"

#define STACK_SIZE 5		/* How many protocol levels to keep
				 * track of (+1 for the topmost one) */

/* Flag used in node packets to indicate whether this packet was
 * inbound or outbound for the node. Links and protocols use
 * "eitherbound" */
typedef enum
{
  EITHERBOUND=-1, INBOUND = 0, OUTBOUND = 1
}
packet_direction;

/* stack of protocol names, one protocol per level */
typedef struct
{
  gchar *protonames[STACK_SIZE + 1];
} 
packet_protos_t;
/* init/delete of a packet_protos_t */
packet_protos_t *packet_protos_init(void);
void packet_protos_delete(packet_protos_t *pt);
/* returns a newly allocated string with a dump of pt */
gchar *packet_protos_dump(const packet_protos_t *pt);

/* Information about each packet heard on the network */
typedef struct
{
  guint size;			/* Size in bytes of the packet */
  struct timeval timestamp;	/* Time at which the packet was heard */
  packet_protos_t *prot_desc;	/* Packet protocol tree */
  guint ref_count;		/* How many structures are referencing this 
				 * packet. When the count reaches zero the packet
				 * is deleted */
}
packet_info_t;

/* items of a packet list. The "direction" item is used to update in/out 
 * stats */
typedef struct
{
  packet_info_t *info;
  packet_direction direction;         /* packet direction - used for nodes */
}
packet_list_item_t;

packet_list_item_t *packet_list_item_create(packet_info_t *i, packet_direction d);
void packet_list_item_delete(packet_list_item_t *pli);
long packet_list_item_count(void);

#endif
