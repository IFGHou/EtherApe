/* EtherApe
 * Copyright (C) 2001 Juan Toledo, 2005 Riccardo Ghetta
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
#include "traffic_stats.h"
#include "ui_utils.h"

/***************************************************************************
 *
 * traffic_stats_t implementation
 *
 **************************************************************************/

static void traffic_stats_list_item_delete(gpointer data, gpointer dum)
{
  packet_list_item_delete(data);
}

/* initializes counters */
void traffic_stats_init(traffic_stats_t *pkt_stat)
{
  g_assert(pkt_stat);

  pkt_stat->pkt_list = NULL;
  pkt_stat->n_packets = 0;

  basic_stats_reset(&pkt_stat->stats);
  basic_stats_reset(&pkt_stat->stats_in);
  basic_stats_reset(&pkt_stat->stats_out);
  
  protocol_stack_open(&pkt_stat->stats_protos);
}

/* releases memory */
void traffic_stats_reset(traffic_stats_t *pkt_stat)
{
  g_assert(pkt_stat);

  /* release items and free list */
  g_list_foreach(pkt_stat->pkt_list, traffic_stats_list_item_delete, NULL);
  g_list_free(pkt_stat->pkt_list);
  pkt_stat->pkt_list = NULL;
  pkt_stat->n_packets = 0;

  /* purges protos */
  protocol_stack_reset(&pkt_stat->stats_protos);

  basic_stats_reset(&pkt_stat->stats);
  basic_stats_reset(&pkt_stat->stats_in);
  basic_stats_reset(&pkt_stat->stats_out);
}

/* adds a packet */
void 
traffic_stats_add_packet(traffic_stats_t *pkt_stat, 
                        packet_info_t *new_pkt, 
                        packet_direction dir)
{
  packet_list_item_t *newit;
  
  g_assert(pkt_stat);
  g_assert(new_pkt);

  /* creates a new item, incrementing refcount of new_pkt */
  newit = packet_list_item_create(new_pkt, dir);

  /* adds to list */
  pkt_stat->pkt_list = g_list_prepend (pkt_stat->pkt_list, newit);
  pkt_stat->n_packets++;

  basic_stats_add(&pkt_stat->stats, newit->info->size);
  if (newit->direction != OUTBOUND)
    basic_stats_add(&pkt_stat->stats_in, newit->info->size); /* in or either */
  if (newit->direction != INBOUND)
    basic_stats_add(&pkt_stat->stats_out, newit->info->size);/* out or either */

  /* adds also to protocol stack */
  protocol_stack_add_pkt(&pkt_stat->stats_protos, newit->info);

  /* note: averages are calculated later, by update_packet_list */
}

void
traffic_stats_purge_expired_packets(traffic_stats_t *pkt_stat, double pkt_expire_time, double proto_expire_time)
{
  struct timeval result;
  GList *packet_l_e = NULL;	/* Packets is a list of packets.
				 * packet_l_e is always the latest (oldest)
				 * list element */

  packet_l_e = g_list_last (pkt_stat->pkt_list);
  while (packet_l_e)
  {
    packet_list_item_t * packet = packet_l_e->data;

    result = substract_times (now, packet->info->timestamp);
    if (!IS_OLDER (result, pkt_expire_time))
      break; /* packet valid, subsequent packets are younger, no need to go further */

    /* packet expired, remove from stats */
    basic_stats_sub(&pkt_stat->stats, packet->info->size);
    if (packet->direction != OUTBOUND)
      basic_stats_sub(&pkt_stat->stats_in, packet->info->size); /* in or either */
    if (packet->direction != INBOUND)
      basic_stats_sub(&pkt_stat->stats_out, packet->info->size);/* out or either */

    /* and protocol stack */
    protocol_stack_sub_pkt(&pkt_stat->stats_protos, packet->info);

    /* and, finally from packet list - gets the new check position 
     * if this packet is the first of the list, all the previous packets
     * should be already destroyed. We check that remove never returns a
     * NEXT packet */
    GList *next=packet_l_e->next;
    packet_l_e = packet_list_remove(packet_l_e);
    g_assert(packet_l_e == NULL || packet_l_e != next );
    pkt_stat->n_packets--;
  }

  if (!packet_l_e)
    {
      /* removed all packets */
      if (pkt_stat->n_packets)
        g_critical("n_packets != 0 in traffic_stats_purge_expired_packets");
      pkt_stat->n_packets = 0;
      pkt_stat->pkt_list=NULL;
      pkt_stat->stats.average = 0;
      pkt_stat->stats_in.average = 0;
      pkt_stat->stats_out.average = 0;
    }

  /* purge expired protocols */
  protocol_stack_purge_expired(&pkt_stat->stats_protos, proto_expire_time);
}

/* Update stats, purging expired packets - returns FALSE if there are no 
 * active packets */
gboolean
traffic_stats_update(traffic_stats_t *pkt_stat, double pkt_expire_time, double proto_expire_time)
{
  traffic_stats_purge_expired_packets(pkt_stat, pkt_expire_time, proto_expire_time);

  if (pkt_stat->pkt_list)
    {
      struct timeval diff;
      gdouble usecs_from_oldest;	/* usecs since the first valid packet */
      GList *packet_l_e;
      packet_list_item_t *packet;

      /* the last packet of the list is the oldest */
      packet_l_e = g_list_last (pkt_stat->pkt_list);
      packet = (packet_list_item_t *) packet_l_e->data;

      diff = substract_times (now, packet->info->timestamp);
      usecs_from_oldest = diff.tv_sec * 1000000 + diff.tv_usec;

      /* calculate averages */
      basic_stats_avg(&pkt_stat->stats, usecs_from_oldest);
      basic_stats_avg(&pkt_stat->stats_in, usecs_from_oldest);
      basic_stats_avg(&pkt_stat->stats_out, usecs_from_oldest);

      protocol_stack_avg(&pkt_stat->stats_protos, usecs_from_oldest);

      return TRUE; /* there are packets active */
    }

  /* no packet active remaining */
  return FALSE;
}

/* returns a newly allocated string with a dump of pkt_stat */
gchar *traffic_stats_dump(const traffic_stats_t *pkt_stat)
{
  gchar *msg;
  gchar *msg_tot, *msg_in, *msg_out;
  gchar *msg_proto;

  if (!pkt_stat)
    return g_strdup("traffic_stats_t NULL");

  msg_tot = basic_stats_dump(&pkt_stat->stats);
  msg_in = basic_stats_dump(&pkt_stat->stats_in);
  msg_out = basic_stats_dump(&pkt_stat->stats_out);
  msg_proto = protocol_stack_dump(&pkt_stat->stats_protos);
  msg = g_strdup_printf("active_packets: %d\n"
                        "  in : [%s]\n"
                        "  out: [%s]\n"
                        "  tot: [%s]\n"
                        "  protocols:\n"
                        "  %s",
                        pkt_stat->n_packets, msg_in, msg_out, msg_tot, 
                        msg_proto);
  g_free(msg_tot);
  g_free(msg_in);
  g_free(msg_out);
  g_free(msg_proto);
  return msg;
}
