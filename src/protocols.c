/* EtherApe
 * Copyright (C) 2005 Juan Toledo, R.Ghetta
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
#include <ctype.h>
#include <string.h>
#include "protocols.h"


/***************************************************************************
 *
 * protocol_stack implementation
 *
 **************************************************************************/

void protocol_stack_init(GList *protostack[])
{
  guint i;
  for (i = 0 ; i <= STACK_SIZE ; ++i)
    protostack[i] = NULL;
}

void protocol_stack_free(GList *protostack[])
{
  guint i;
  protocol_t *protocol_info;

  for (i = 0 ; i <= STACK_SIZE ; ++i)
    {
      while (protostack[i])
        {
          protocol_info = protostack[i]->data;

          protocol_t_delete(protocol_info);
          protostack[i] = g_list_delete_link (protostack[i], protostack[i]);
        }
    }
}

/* adds the given packet to the stack */
void
protocol_stack_add_pkt(GList *protostack[], const packet_info_t * packet)
{
  GList *protocol_item;
  protocol_t *protocol_info;
  gchar **tokens = NULL;
  guint i;
  gchar *protocol_name;

  g_assert(packet);
  
  tokens = g_strsplit (packet->prot_desc, "/", 0);

  for (i = 0; i <= STACK_SIZE; i++)
    {
      if (pref.group_unk && strstr (tokens[i], "TCP-Port"))
	protocol_name = "TCP-Unknown";
      else if (pref.group_unk && strstr (tokens[i], "UDP-Port"))
	protocol_name = "UDP-Unknown";
      else
	protocol_name = tokens[i];

      /* If there is yet not such protocol, create it */
      if (!(protocol_item = g_list_find_custom (protostack[i],
						protocol_name,
						protocol_compare)))
	{
	  protocol_info = protocol_t_create(protocol_name);
	  protostack[i] = g_list_prepend (protostack[i], protocol_info);
	}
      else
	protocol_info = protocol_item->data;

      protocol_info->last_heard = now;
      protocol_info->accumulated += packet->size;
      protocol_info->aver_accu += packet->size;
      protocol_info->proto_packets++;

    }
  g_strfreev (tokens);
}				/* add_protocol */


void protocol_stack_sub_pkt(GList *protostack[], const packet_info_t * packet, gboolean purge_entry)
{
  guint i = 0;
  gchar **tokens = NULL;
  GList *item = NULL;
  protocol_t *protocol = NULL;
  gchar *protocol_name = NULL;

  if (!packet)
    return;

  /* We remove protocol aggregate information */
  tokens = g_strsplit (packet->prot_desc, "/", 0);
  while ((i <= STACK_SIZE) && tokens[i])
    {
      if (pref.group_unk && strstr (tokens[i], "TCP-Port"))
        protocol_name = "TCP-Unknown";
      else if (pref.group_unk && strstr (tokens[i], "UDP-Port"))
        protocol_name = "UDP-Unknown";
      else
        protocol_name = tokens[i];

      item = g_list_find_custom (protostack[i],
                                 protocol_name,
                                 protocol_compare);
      if (!item)
        {
          g_my_critical
            ("Protocol not found while subtracting packet in protocol_stack_sub_pkt");
          break;
        }
      protocol = item->data;
      protocol->aver_accu -= packet->size;

      if (protocol->aver_accu<=0)
        {
          /* no traffic active */
          protocol->aver_accu = 0;
          protocol->average = 0;

          if (purge_entry)
            {
              /* purge entry requested */
              g_free (protocol->name);
              g_free (protocol);
    
              protostack[i] = g_list_delete_link(protostack[i], item);
            }
        }
      i++;
    }
  g_strfreev (tokens);
}

/* finds named protocol in the level protocols of protostack*/
const protocol_t *protocol_stack_find(GList *protostack[], size_t level, const gchar *protoname)
{
  GList *item;
  
  if (level>STACK_SIZE || !protoname)
    return NULL;
  
  item = g_list_find_custom (protostack[level], protoname, protocol_compare);
  if (item && item->data)
    return (const protocol_t *)item->data;

  return NULL;
}

/***************************************************************************
 *
 * protocol_summary_t implementation
 *
 **************************************************************************/
static protocol_summary_t *protosummary = NULL;

/* initializes the summary */
void protocol_summary_open(void)
{
  if (protosummary)
    protocol_summary_close();

  protosummary = g_malloc( sizeof(protocol_summary_t) );
  protosummary->n_packets = 0;
  protosummary->packets = NULL;
  protocol_stack_init(protosummary->protostack);
}

/* frees summary, releasing resources */
void protocol_summary_close(void)
{
  if (protosummary)
    {
      while (protosummary->packets)
        protosummary->packets = packet_list_remove(protosummary->packets);
      protosummary->packets = NULL;
      protosummary->n_packets = 0;
      protocol_stack_free(protosummary->protostack);
      g_free(protosummary);
      protosummary = NULL;
    }
}

/* adds a new packet to summary */
void protocol_summary_add_packet(packet_info_t *packet)
{
  packet_list_item_t *newit;
  
  if (!protosummary)
    protocol_summary_open();

  newit = g_malloc( sizeof(packet_list_item_t) );
  packet->ref_count++;
  newit->info = packet;
  newit->direction = EITHERBOUND;
  protosummary->packets = g_list_prepend (protosummary->packets, newit);
  ++protosummary->n_packets;

  protocol_stack_add_pkt(protosummary->protostack, packet);
}

static void
protocol_summary_purge_expired_packets()
{
  struct timeval result;
  double time_comparison;
  GList *packet_l_e = NULL;	/* Packets is a list of packets.
				 * packet_l_e is always the latest (oldest)
				 * list element */

  /* If this packet is older than the averaging time,
   * then it is removed, and the process continues.
   * Else, we are done. */
  if (pref.node_timeout_time)
    time_comparison = (pref.node_timeout_time > pref.averaging_time) ?
      pref.averaging_time : pref.node_timeout_time;
  else
    time_comparison = pref.averaging_time;

  packet_l_e = g_list_last (protosummary->packets);
  while (packet_l_e)
  {
    packet_list_item_t * packet = (packet_list_item_t *)(packet_l_e->data);

    result = substract_times (now, packet->info->timestamp);
    if (!IS_OLDER (result, time_comparison) && (status != STOP))
      break; /* packet valid, subsequent packets are younger, no need to go further */

    /* TODO: insert proto expiration. Right a proto is never removed */
    /* packet expired, remove WITHOUT removing the proto entry, because the 
     * proto legend/window can't cope */
    protocol_stack_sub_pkt(protosummary->protostack, packet->info, FALSE);

    /* packet expired, remove from list - gets the new check position 
     * if this packet is the first of the list, all the previous packets
     * should be already destroyed. We check that remove never returns a
     * NEXT packet */
    GList *next=packet_l_e->next;
    packet_l_e = packet_list_remove(packet_l_e);
    g_assert(packet_l_e == NULL || packet_l_e != next );
    protosummary->n_packets--;
  }

  if (!packet_l_e)
    {
      /* removed all packets */
      protosummary->n_packets = 0;
      protosummary->packets=NULL;
    }
}

/* update stats on protocol summary */
void protocol_summary_update_all(void)
{
  if (!protosummary)
    return;

  protocol_summary_purge_expired_packets();

  if (protosummary->packets)
    {
      /* ok, we have active packets, update stats */
      GList *item = NULL;
      protocol_t *protocol = NULL;
      guint i = 0;
      struct timeval difference;
      packet_list_item_t *packet;
      gdouble usecs_from_oldest;	/* usecs since the oldest valid packet */

      item = g_list_last (protosummary->packets);
      packet = (packet_list_item_t *) item->data;

      difference = substract_times (now, packet->info->timestamp);
      usecs_from_oldest = difference.tv_sec * 1000000 + difference.tv_usec;

      for (i = 0; i <= STACK_SIZE; i++)
        {
          item = protosummary->protostack[i];
          while (item)
            {
              protocol = (protocol_t *)item->data;

              protocol->average =
                8000000 * protocol->aver_accu / usecs_from_oldest;

              item = item->next;
            }
        }
    }
}

/* number of protos at specified level */
guint protocol_summary_size(size_t level)
{
  if (!protosummary || level > STACK_SIZE)
    return 0;
  return g_list_length(protosummary->protostack[level]);
}


/* calls func for every protocol at the specified level */
void protocol_summary_foreach(size_t level, GFunc func, gpointer data)
{
  if (!protosummary || level > STACK_SIZE)
    return;
  g_list_foreach (protosummary->protostack[level], func, data);
}


/* finds named protocol in the level protocols of protostack*/
const protocol_t *protocol_summary_find(size_t level, const gchar *protoname)
{
  if (!protosummary)
    return NULL;
  return protocol_stack_find(protosummary->protostack, level, protoname);
}

/***************************************************************************
 *
 * protocol_t implementation
 *
 **************************************************************************/
protocol_t *protocol_t_create(const gchar *protocol_name)
{
  protocol_t *pr = NULL;

  pr = g_malloc (sizeof (protocol_t));
  pr->name = g_strdup (protocol_name);
  pr->accumulated = 0;
  pr->aver_accu = 0;
  pr->average = 0;
  pr->proto_packets = 0;
  pr->node_names = NULL;
  pr->color.pixel = 0;
  pr->color.red = 0;
  pr->color.green = 0;
  pr->color.blue = 0;
  pr->last_heard.tv_sec = 0;
  pr->last_heard.tv_usec = 0;

  return pr;
}

void protocol_t_delete(protocol_t *prot)
{
  g_assert(prot);

  g_free (prot->name);
  prot->name = NULL;

  while (prot->node_names)
    {
      GList *name_item = prot->node_names;
      name_t *name = name_item->data;
      node_name_delete(name);
      prot->node_names =
        g_list_delete_link (prot->node_names,
                            name_item);
    }

  g_free (prot);
}

/* Comparison function used to compare two link protocols */
gint
protocol_compare (gconstpointer a, gconstpointer b)
{
  g_assert (a != NULL);
  g_assert (b != NULL);

  return strcmp (((protocol_t *) a)->name, (gchar *) b);
}
