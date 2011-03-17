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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "appdata.h"
#include "traffic_stats.h"
#include "ui_utils.h"
#include "util.h"

/***************************************************************************
 *
 * traffic_stats_t implementation
 *
 **************************************************************************/

/* initializes counters */
void traffic_stats_init(traffic_stats_t *pkt_stat)
{
  g_assert(pkt_stat);

  g_queue_init(&pkt_stat->pkt_list);

  basic_stats_reset(&pkt_stat->stats);
  basic_stats_reset(&pkt_stat->stats_in);
  basic_stats_reset(&pkt_stat->stats_out);
  
  protocol_stack_open(&pkt_stat->stats_protos);
}

/* releases memory */
void traffic_stats_reset(traffic_stats_t *pkt_stat)
{
  gpointer it;

  g_assert(pkt_stat);

  /* release items and free list */
  while ( (it = g_queue_pop_head(&pkt_stat->pkt_list)) != NULL)
      packet_list_item_delete((packet_list_item_t *)it);

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
  g_queue_push_head(&pkt_stat->pkt_list, newit);

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
  double diffms;
  packet_list_item_t* packet;

  /* pkt queue is ordered by arrival time, so older pkts are at tail */
  while (pkt_stat->pkt_list.head)
  {
    packet = (packet_list_item_t *)g_queue_peek_tail(&pkt_stat->pkt_list);
    diffms = substract_times_ms(&appdata.now, &packet->info->timestamp);
    if (diffms < pkt_expire_time)
      break; /* packet valid, subsequent packets are younger, no need to go further */

    /* packet expired, remove from stats */
    basic_stats_sub(&pkt_stat->stats, packet->info->size);
    if (packet->direction != OUTBOUND)
      basic_stats_sub(&pkt_stat->stats_in, packet->info->size); /* in or either */
    if (packet->direction != INBOUND)
      basic_stats_sub(&pkt_stat->stats_out, packet->info->size);/* out or either */

    /* and protocol stack */
    protocol_stack_sub_pkt(&pkt_stat->stats_protos, packet->info);

    /* and, finally, from packet queue */
    g_queue_pop_tail(&pkt_stat->pkt_list);
    packet_list_item_delete(packet);
  }

  if (pkt_stat->pkt_list.head == NULL)
    {
      /* removed all packets */
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
traffic_stats_update(traffic_stats_t *pkt_stat, double avg_time, double proto_expire_time)
{
  traffic_stats_purge_expired_packets(pkt_stat, avg_time, proto_expire_time);

  if (!g_queue_is_empty(&pkt_stat->pkt_list))
    {
      gdouble ms_from_oldest = avg_time;

#if CHECK_EXPIRATION
      /* the last packet of the list is the oldest */
      const packet_list_item_t* packet;
      packet = (const packet_list_item_t *)g_queue_peek_tail(&pkt_stat->pkt_list);
      ms_from_oldest = substract_times_ms (&now, &packet->info->timestamp);
      if (ms_from_oldest < avg_time)
        ms_from_oldest = avg_time;
      else
        g_warning("ms_to_oldest > avg_time: %f", ms_from_oldest);
#endif

      basic_stats_avg(&pkt_stat->stats, ms_from_oldest);
      basic_stats_avg(&pkt_stat->stats_in, ms_from_oldest);
      basic_stats_avg(&pkt_stat->stats_out, ms_from_oldest);
      protocol_stack_avg(&pkt_stat->stats_protos, ms_from_oldest);

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
  msg = g_strdup_printf("active_packets: %u\n"
                        "  in : [%s]\n"
                        "  out: [%s]\n"
                        "  tot: [%s]\n"
                        "  protocols:\n"
                        "  %s",
                        pkt_stat->pkt_list.length, 
                        msg_in, msg_out, msg_tot, msg_proto);
  g_free(msg_tot);
  g_free(msg_in);
  g_free(msg_out);
  g_free(msg_proto);
  return msg;
}

/* returns a newly allocated string with an xml dump of pkt_stat */
gchar *traffic_stats_xml(const traffic_stats_t *pkt_stat)
{
  gchar *msg;
  gchar *msg_tot, *msg_in, *msg_out;
  gchar *msg_proto;

  if (!pkt_stat)
    return xmltag("traffic_stats","");

  msg_tot = basic_stats_xml(&pkt_stat->stats);
  msg_in = basic_stats_xml(&pkt_stat->stats_in);
  msg_out = basic_stats_xml(&pkt_stat->stats_out);
  msg_proto = protocol_stack_xml(&pkt_stat->stats_protos);
  msg = xmltag("traffic_stats",
               "\n<active_packets>%u</active_packets>\n"
               "<in>\n%s</in>\n"
               "<out>\n%s</out>\n"
               "<tot>\n%s</tot>\n"
               "%s",
               pkt_stat->pkt_list.length, 
               msg_in, msg_out, msg_tot, msg_proto);
  g_free(msg_tot);
  g_free(msg_in);
  g_free(msg_out);
  g_free(msg_proto);
  return msg;
}
