/* EtherApe
 * Copyright (C) 2001-2009 Juan Toledo, Riccardo Ghetta
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
#include "basic_stats.h"
#include "ui_utils.h"

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

  /* Perform the carry for the later subtraction by updating b. */
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

  /* result.tv_usec is positive. */ 
  result.tv_sec = a.tv_sec - b.tv_sec;
  result.tv_usec = a.tv_usec - b.tv_usec;
  return result;
}				/* substract_times */


/***************************************************************************
 *
 * packet_protos_t implementation
 *
 **************************************************************************/
/* init/delete of a packet_protos_t */
packet_protos_t *packet_protos_init(void)
{
  guint i;
  packet_protos_t *pt = g_malloc(sizeof(packet_protos_t));
  g_assert(pt);
  if (pt)
    {
      for (i = 0; i<=STACK_SIZE ; ++i)
        pt->protonames[i] = NULL;
    }
  return pt;
}

void packet_protos_delete(packet_protos_t *pt)
{
  guint i;
  for (i = 0; i<=STACK_SIZE ; ++i)
    g_free(pt->protonames[i]);
  g_free(pt);
}

/* returns a newly allocated string with a dump of pt */
gchar *packet_protos_dump(const packet_protos_t *pt)
{
  gint i;
  GString *msg;

  /* first position is top proto */
  for (i = STACK_SIZE ; i>=0 ; --i)
    {
      if (pt->protonames[i])
        {
          msg = g_string_new(pt->protonames[i]);
          break;
        }
    }
  if (!msg)
    msg = g_string_new("UNKNOWN");
  
  for (i = 0; i<=STACK_SIZE ; ++i)
    {
      if (pt->protonames[i])
        g_string_append_printf(msg, "/%s", pt->protonames[i]);
      else
        g_string_append(msg, "/UNKNOWN");
    }

  /* returns only the string buffer, freeing the rest */
  return g_string_free(msg, FALSE);
}

/***************************************************************************
 *
 * packet_list_item_t implementation
 *
 **************************************************************************/
packet_list_item_t *
packet_list_item_create(packet_info_t *i, packet_direction d)
{
  packet_list_item_t *newit;

  g_assert(i);

  newit = g_malloc( sizeof(packet_list_item_t) );

  /* increments refcount of packet */
  i->ref_count++;

  /* fills item, adding it to pkt list */
  newit->info = i;
  newit->direction = d;

  return newit;
}

void packet_list_item_delete(packet_list_item_t *pli)
{
  if (pli)
    {
      if (pli->info)
        {
          /* packet exists, decrement ref count */
          pli->info->ref_count--;
    
          if (pli->info->ref_count < 1)
            {
              /* packet now unused, delete it */
              g_free (pli->info->prot_desc);
              g_free (pli->info);
    
              /* global packet stats */
              total_mem_packets--;
            }
        }
    
      g_free(pli);
    }
}

/* removes a packet from a list of packets, destroying it if necessary
 * Returns the PREVIOUS item if any, otherwise the NEXT, thus returning NULL
 * if the list is empty */
GList *
packet_list_remove(GList *item_to_remove)
{
  packet_list_item_t *litem;

  g_assert(item_to_remove);
  
  litem = item_to_remove->data;

  packet_list_item_delete(litem);
  item_to_remove->data = NULL;

  /* TODO I have to come back here and make sure I can't make
   * this any simpler */
  if (item_to_remove->prev)
    {
      /* current packet is not at head */
      GList *item = item_to_remove;
      item_to_remove = item_to_remove->prev; 
      item_to_remove = g_list_delete_link (item_to_remove, item);
    }
  else
    {
      /* packet is head of list */
      item_to_remove=g_list_delete_link(item_to_remove, item_to_remove);
    }

  return item_to_remove;
}

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
  tf_stat->accu_packets = 0;
  tf_stat->last_time = now;
}

void basic_stats_add(basic_stats_t *tf_stat, gdouble val)
{
  g_assert(tf_stat);
  tf_stat->accumulated += val;
  tf_stat->aver_accu += val;
  tf_stat->accu_packets++;
  tf_stat->last_time = now;
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
  if (avg_usecs != 0)
    tf_stat->average = 8000000 * tf_stat->aver_accu / avg_usecs;
}

/* returns a newly allocated string with a dump of the stats */
gchar *basic_stats_dump(const basic_stats_t *tf_stat)
{
  gchar *msg;
  gchar *msg_time;
  if (!tf_stat)
    return g_strdup("basic_stats_t NULL");

  msg_time = timeval_to_str (tf_stat->last_time);
  msg = g_strdup_printf("avg: %f, avg_acc: %f, total: %f, packets: %f, "
                        "last heard: %s",
                        tf_stat->average,
                        tf_stat->aver_accu,
                        tf_stat->accumulated,
                        tf_stat->accu_packets,
                        msg_time);
                        
  g_free(msg_time);
  return msg;
}

