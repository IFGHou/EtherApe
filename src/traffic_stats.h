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

#ifndef TRAFFIC_STATS_H
#define TRAFFIC_STATS_H

#include <sys/time.h>
#include "protocols.h"

typedef struct
{
  GQueue pkt_list;              /* list of packet_list_item_t - private */
  basic_stats_t stats;        /* total traffic stats */
  basic_stats_t stats_in;     /* inbound traffic stats */
  basic_stats_t stats_out;    /* outbound traffic stats */
  protostack_t stats_protos;    /* protocol stack */
}
traffic_stats_t;

void traffic_stats_init(traffic_stats_t *pkt_stat); /* initializes counters */
void traffic_stats_reset(traffic_stats_t *pkt_stat); /* releases memory */
void traffic_stats_add_packet( traffic_stats_t *pkt_stat, 
                              packet_info_t *new_pkt, 
                              packet_direction dir); /* adds a packet */
void traffic_stats_purge_expired_packets(traffic_stats_t *pkt_stat, double pkt_expire_time, double proto_expire_time);
gboolean traffic_stats_update(traffic_stats_t *pkt_stat, double pkt_expire_time, double proto_expire_time);
gchar *traffic_stats_dump(const traffic_stats_t *pkt_stat); 
gchar *traffic_stats_xml(const traffic_stats_t *pkt_stat); 

#endif
