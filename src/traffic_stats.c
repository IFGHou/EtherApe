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

/* removes a packet from a list of packets, destroying it if necessary
 * Returns the PREVIOUS item if any, otherwise the NEXT, thus returning NULL
 * if the list is empty */
static GList *
packet_list_remove(GList *item_to_remove);

/***************************************************************************
 *
 * utility functions
 *
 **************************************************************************/

/* Returns a timeval structure with the time difference between to
 * other timevals. result = a - b */
struct timeval
substract_times (struct timeval a, struct timeval b)
{
  struct timeval result;

  /* Perform the carry for the later subtraction by updating Y. */
  if (a.tv_usec < b.tv_usec)
    {
      int nsec = (b.tv_usec - a.tv_usec) / 1000000 + 1;
      b.tv_usec -= 1000000 * nsec;
      b.tv_sec += nsec;
    }
  if (a.tv_usec - b.tv_usec > 1000000)
    {
      int nsec = (a.tv_usec - b.tv_usec) / 1000000;
      b.tv_usec += 1000000 * nsec;
      b.tv_sec -= nsec;
    }

  result.tv_sec = a.tv_sec - b.tv_sec;
  result.tv_usec = a.tv_usec - b.tv_usec;

  return result;
}				/* substract_times */

/***************************************************************************
 *
 * basic_stats_t implementation
 *
 **************************************************************************/
/* resets counters */
void basic_stats_reset(basic_stats_t *tf_stat)
{
  g_assert(tf_stat);
  tf_stat->average = 0;
  tf_stat->accumulated = 0;
  tf_stat->aver_accu = 0;
}

void basic_stats_add(basic_stats_t *tf_stat, gdouble val)
{
  g_assert(tf_stat);
  tf_stat->accumulated += val;
  tf_stat->aver_accu += val;
  /* averages are calculated by basic_stats_avg */
}

void basic_stats_sub(basic_stats_t *tf_stat, gdouble val)
{
  g_assert(tf_stat);
  tf_stat->aver_accu -= val;
  if (!tf_stat->aver_accu)
    tf_stat->average = 0;
  /* averages are calculated by basic_stats_avg */
}

void
basic_stats_avg(basic_stats_t *tf_stat, gdouble avg_usecs)
{
  g_assert(tf_stat);

  /* average in bps, so we multiply by 8 and 1000000 */
  tf_stat->average = 8000000 * tf_stat->aver_accu / avg_usecs;
}

/***************************************************************************
 *
 * traffic_stats_t implementation
 *
 **************************************************************************/

static void traffic_stats_list_item_delete(gpointer data, gpointer dum)
{
  g_free(data); /* TODO: properly release memory */
}

/* initializes counters */
void traffic_stats_init(traffic_stats_t *pkt_stat)
{
  g_assert(pkt_stat);

  pkt_stat->pkt_list = NULL;
  pkt_stat->n_packets = 0;
  pkt_stat->last_time = now;

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
  protocol_stack_reset(&pkt_stat->stats_protos);

  /* resets everything */
  pkt_stat->pkt_list = NULL;
  pkt_stat->n_packets = 0;
  pkt_stat->last_time = now;

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

  newit = g_malloc( sizeof(packet_list_item_t) );

  /* increments refcount of packet */
  new_pkt->ref_count++;

  /* fills item, adding it to pkt list */
  newit->info = new_pkt;
  newit->direction = dir;

  pkt_stat->pkt_list = g_list_prepend (pkt_stat->pkt_list, newit);

  pkt_stat->n_packets++;
  pkt_stat->last_time = now;

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
traffic_stats_purge_expired_packets(traffic_stats_t *pkt_stat, double expire_time, gboolean purge_protos)
{
  struct timeval result;
  GList *packet_l_e = NULL;	/* Packets is a list of packets.
				 * packet_l_e is always the latest (oldest)
				 * list element */

  packet_l_e = g_list_last (pkt_stat->pkt_list);
  while (packet_l_e)
  {
    packet_list_item_t * packet = (packet_list_item_t *)(packet_l_e->data);

    result = substract_times (now, packet->info->timestamp);
    if (!IS_OLDER (result, expire_time) && (status != STOP))
      break; /* packet valid, subsequent packets are younger, no need to go further */

    /* packet expired, remove from stats */
    basic_stats_sub(&pkt_stat->stats, packet->info->size);
    if (packet->direction != OUTBOUND)
      basic_stats_sub(&pkt_stat->stats_in, packet->info->size); /* in or either */
    if (packet->direction != INBOUND)
      basic_stats_sub(&pkt_stat->stats_out, packet->info->size);/* out or either */

    /* and protocol stack */
    protocol_stack_sub_pkt(&pkt_stat->stats_protos, packet->info, purge_protos);

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
      pkt_stat->n_packets = 0;
      pkt_stat->pkt_list=NULL;
      pkt_stat->stats.average = 0;
      pkt_stat->stats_in.average = 0;
      pkt_stat->stats_out.average = 0;
      if (purge_protos)
        protocol_stack_reset(&pkt_stat->stats_protos);
    }
}

/* Update stats, purging expired packets - returns FALSE if there are no 
 * active packets */
gboolean
traffic_stats_update(traffic_stats_t *pkt_stat, double expire_time, gboolean purge_expired_protos)
{
  traffic_stats_purge_expired_packets(pkt_stat, expire_time, purge_expired_protos);

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

/* removes a packet from a list of packets, destroying it if necessary
 * Returns the PREVIOUS item if any, otherwise the NEXT, thus returning NULL
 * if the list is empty */
GList *
packet_list_remove(GList *item_to_remove)
{
  packet_list_item_t *litem;

  g_assert(item_to_remove);
  
  litem = (packet_list_item_t *) item_to_remove->data;
  if (litem)
    {
      packet_info_t *packet = litem->info;

      if (packet)
        {
          /* packet exists, decrement ref count */
          packet->ref_count--;
    
          if (!packet->ref_count)
            {
              /* packet now unused, delete it */
              if (packet->prot_desc)
                {
                  g_free (packet->prot_desc);
                  packet->prot_desc = NULL;
                }
              g_free (packet);

              n_mem_packets--;
            }
          litem->info = NULL;
        }

      g_free(litem);
      item_to_remove->data = NULL;
    }

  /* TODO I have to come back here and make sure I can't make
   * this any simpler */
  if (item_to_remove->prev)
    {
      /* current packet is not at head */
      GList *item = item_to_remove;
      item_to_remove = item_to_remove->prev; 
      g_list_delete_link (item_to_remove, item);
    }
  else
    {
      /* packet is head of list */
      item_to_remove=g_list_delete_link(item_to_remove, item_to_remove);
    }

  return item_to_remove;
}
