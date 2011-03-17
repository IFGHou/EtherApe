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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "appdata.h"
#include <ctype.h>
#include <string.h>
#include "protocols.h"
#include "node.h"
#include "preferences.h"
#include "util.h"

static gint 
protocol_compare (gconstpointer a, gconstpointer b);


/***************************************************************************
 *
 * protocol_stack implementation
 *
 **************************************************************************/

void protocol_stack_open(protostack_t *pstk)
{
  g_assert(pstk);
  guint i;
  for (i = 0 ; i <= STACK_SIZE ; ++i)
    pstk->protostack[i] = NULL;
}

void protocol_stack_reset(protostack_t *pstk)
{
  guint i;
  protocol_t *protocol_info;

  g_assert(pstk);
  for (i = 0 ; i <= STACK_SIZE ; ++i)
    {
      while (pstk->protostack[i])
        {
          protocol_info = pstk->protostack[i]->data;

          protocol_t_delete(protocol_info);
          pstk->protostack[i] = g_list_delete_link (pstk->protostack[i], pstk->protostack[i]);
        }
    }
}

/* adds the given packet to the stack */
void
protocol_stack_add_pkt(protostack_t *pstk, const packet_info_t * packet)
{
  GList *protocol_item;
  protocol_t *protocol_info;
  guint i;

  g_assert(packet);
  g_assert(pstk);

  for (i = 0; i <= STACK_SIZE; i++)
    {
      if (!(packet->prot_desc->protonames[i]))
        continue;
      
      protocol_item = g_list_find_custom (pstk->protostack[i],
                                          packet->prot_desc->protonames[i],
                                          protocol_compare);
      if (protocol_item)
        protocol_info = protocol_item->data;
      else
	{
          /* If there is yet not such protocol, create it */
	  protocol_info = protocol_t_create(packet->prot_desc->protonames[i]);
	  pstk->protostack[i] = g_list_prepend (pstk->protostack[i], protocol_info);
	}

      g_assert( !strcmp(protocol_info->name, packet->prot_desc->protonames[i]));
      basic_stats_add(&protocol_info->stats, packet->size);
    }
}				/* add_protocol */


void protocol_stack_sub_pkt(protostack_t *pstk, const packet_info_t * packet)
{
  guint i = 0;
  GList *item = NULL;
  protocol_t *protocol = NULL;

  g_assert(pstk);

  if (!packet)
    return;

  /* We remove protocol aggregate information */
  while ((i <= STACK_SIZE) && packet->prot_desc->protonames[i])
    {
      item = g_list_find_custom (pstk->protostack[i],
                                 packet->prot_desc->protonames[i],
                                 protocol_compare);
      if (!item)
        {
          g_my_critical
            ("Protocol not found while subtracting packet in protocol_stack_sub_pkt");
          break;
        }
      protocol = item->data;

      g_assert( !strcmp(protocol->name, packet->prot_desc->protonames[i]));
      basic_stats_sub(&protocol->stats, packet->size);
      i++;
    }
}

/* calculates averages on protocol stack items */
void
protocol_stack_avg(protostack_t *pstk, gdouble avgtime)
{
  GList *item;
  protocol_t *protocol;
  guint i;

  g_assert(pstk);

  for (i = 0; i <= STACK_SIZE; i++)
    {
      item = pstk->protostack[i];
      while (item)
        {
          protocol = (protocol_t *)item->data;
          basic_stats_avg(&protocol->stats, avgtime);
          item = item->next;
        }
    }
}

/* checks for protocol expiration ... */
void
protocol_stack_purge_expired(protostack_t *pstk, double expire_time)
{
  g_assert(pstk);

  if (expire_time>0)
    {
      GList *item;
      GList *next_item;
      protocol_t *protocol;
      double diffms;
      guint i;
      for (i = 0; i <= STACK_SIZE; i++)
        {
          item = pstk->protostack[i];
          while (item)
            {
              protocol = (protocol_t *)item->data;
              next_item = item->next;
              if (protocol->stats.aver_accu<=0)
                {
                  /* no traffic active on this proto, check purging */
                  diffms = substract_times_ms(&appdata.now, &protocol->stats.last_time);
                  if (diffms >= expire_time)
                    {
                      protocol_t_delete(protocol);
                      pstk->protostack[i] = g_list_delete_link(pstk->protostack[i], item);
                    }
                }
              item = next_item;
            }
        }
    }
}


/* finds named protocol in the level protocols of protostack*/
const protocol_t *protocol_stack_find(const protostack_t *pstk, size_t level, const gchar *protoname)
{
  GList *item;

  g_assert(pstk);

  if (level>STACK_SIZE || !protoname)
    return NULL;
  
  item = g_list_find_custom (pstk->protostack[level], protoname, protocol_compare);
  if (item && item->data)
    return item->data;

  return NULL;
}

/* Comparison function to sort protocols by their accumulated traffic */
static gint
prot_freq_compare (gconstpointer a, gconstpointer b)
{
  const protocol_t *prot_a, *prot_b;

  g_assert (a != NULL);
  g_assert (b != NULL);

  prot_a = (const protocol_t *) a;
  prot_b = (const protocol_t *) b;

  if (prot_a->stats.accumulated > prot_b->stats.accumulated)
    return -1;
  if (prot_a->stats.accumulated < prot_b->stats.accumulated)
    return 1;
  return 0;
}				/* prot_freq_compare */


/* sorts on the most used protocol in the requested level and returns it */
gchar *
protocol_stack_sort_most_used(protostack_t *pstk, size_t level)
{
  protocol_t *protocol;

  /* If we haven't recognized any protocol at that level,
   * we say it's unknown */
  if (level>STACK_SIZE || !pstk || !pstk->protostack[level])
    return NULL;
  pstk->protostack[level] = g_list_sort (pstk->protostack[level], prot_freq_compare);
  protocol = (protocol_t *) pstk->protostack[level]->data;
  return g_strdup (protocol->name);
}				/* get_main_prot */

/* returns a newly allocated string with a dump of pstk */
gchar *protocol_stack_dump(const protostack_t *pstk)
{
  guint i;
  gchar *msg;

  if (!pstk)
    return g_strdup("protostack_t NULL");

  msg = g_strdup("");
  for (i = 0 ; i <= STACK_SIZE ; ++i)
    {
      gchar *msg_level;
      gchar *tmp;
      if (!pstk->protostack[i])
        msg_level = g_strdup("-none-");
      else
        {
          const GList *cur_el = pstk->protostack[i];
          msg_level = NULL;
          while (cur_el)
            {
              gchar *msg_proto;
              const protocol_t *p = (const protocol_t *)(cur_el->data);
              g_assert(p);

              msg_proto = protocol_t_dump(p);
              if (!msg_level)
                msg_level = msg_proto;
              else
                {
                  tmp = msg_level;
                  msg_level = g_strdup_printf("%s,[%s]", tmp, msg_proto);
                  g_free(tmp);
                  g_free(msg_proto);
                }
              cur_el = cur_el->next;
            }
        }
      tmp = msg;
      msg = g_strdup_printf("%slevel %d: [%s]\n", tmp, i, msg_level);
      g_free(tmp);
      g_free(msg_level);
    }
  return msg;
}

/* returns a newly allocated string with an xml dump of pstk */
gchar *protocol_stack_xml(const protostack_t *pstk)
{
  guint i;
  gchar *msg;
  gchar *xml;

  if (!pstk)
    return xmltag("protocols", "");

  msg = g_strdup("");
  for (i = 1 ; i <= STACK_SIZE ; ++i)
    {
      gchar *msg_level;
      gchar *tmp;
      if (!pstk->protostack[i])
        continue;

      const GList *cur_el = pstk->protostack[i];
      msg_level = NULL;
      while (cur_el)
        {
          gchar *msg_proto;
          const protocol_t *p = (const protocol_t *)(cur_el->data);
          g_assert(p);

          msg_proto = protocol_t_xml(p, i);
          if (!msg_level)
            msg_level = msg_proto;
          else
            {
              tmp = msg_level;
              msg_level = g_strdup_printf("%s%s", tmp, msg_proto);
              g_free(tmp);
              g_free(msg_proto);
            }
          cur_el = cur_el->next;
        }
      tmp = msg;
      msg = g_strdup_printf("%s%s", tmp, msg_level);
      g_free(tmp);
      g_free(msg_level);
    }
  xml = xmltag("protocols", "%s", msg);
  g_free(msg);
  return xml;
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
  g_assert(pr);
  pr->name = g_strdup (protocol_name);
  basic_stats_reset(&pr->stats);
  pr->node_names = NULL;

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
      prot->node_names = g_list_delete_link (prot->node_names, name_item);
    }

  g_free (prot);
}

/* returns a new string with a dump of prot */
gchar *protocol_t_dump(const protocol_t *prot)
{
  gchar *msg;
  gchar *msg_stats;
  gchar *msg_names;

  if (!prot)
    return g_strdup("protocol_t NULL");

  msg_stats = basic_stats_dump(&prot->stats);

  if (!prot->node_names)
    msg_names = g_strdup("-- no names --");
  else
    {
      const GList *cur_el;
      msg_names = NULL;
      cur_el = prot->node_names;
      while (cur_el)
        {
          gchar *str_name;
          const name_t *cur_name;

          cur_name = (const name_t *)(cur_el->data);
          str_name = node_name_dump(cur_name);
          if (!msg_names)
            msg_names = str_name;
          else
            {
              gchar *tmp = msg_names;
              msg_names = g_strjoin(",", tmp, str_name, NULL);
              g_free(tmp);
              g_free(str_name);
            }
          cur_el = cur_el->next;
        }
    }
  
  msg = g_strdup_printf("protocol name: %s, stats [%s], "
                         "node_names [%s]",
                         prot->name, msg_stats, msg_names);
                         
  g_free(msg_stats);
  g_free(msg_names);
  return msg;
}

/* returns a new string with an xml dump of prot */
gchar *protocol_t_xml(const protocol_t *prot, guint level)
{
  gchar *msg;
  gchar *msg_stats;
  gchar *msg_names;

  if (!prot)
    return xmltag("protocol","");

  msg_stats = basic_stats_xml(&prot->stats);

  if (!prot->node_names)
    msg_names = g_strdup("");
  else
    {
      const GList *cur_el;
      msg_names = NULL;
      cur_el = prot->node_names;
      while (cur_el)
        {
          gchar *str_name;
          const name_t *cur_name;

          cur_name = (const name_t *)(cur_el->data);
          str_name = node_name_xml(cur_name);
          if (!msg_names)
            msg_names = str_name;
          else
            {
              gchar *tmp = msg_names;
              msg_names = g_strjoin(",", tmp, str_name, NULL);
              g_free(tmp);
              g_free(str_name);
            }
          cur_el = cur_el->next;
        }
    }
  
  msg = xmltag("protocol", 
               "\n<level>%u</level>\n<key>%s</key>\n%s%s",
               level,
               prot->name, 
               msg_stats, msg_names);
                         
  g_free(msg_stats);
  g_free(msg_names);
  return msg;
}


/* Comparison function used to compare two link protocols */
static gint
protocol_compare (gconstpointer a, gconstpointer b)
{
  g_assert (a != NULL);
  g_assert (b != NULL);

  return strcmp (((const protocol_t *) a)->name, b);
}

/***************************************************************************
 *
 * protocol_summary_t implementation
 *
 **************************************************************************/
static traffic_stats_t *protosummary_stats = NULL;

/* initializes the summary */
void protocol_summary_open(void)
{
  if (protosummary_stats)
    protocol_summary_close();

  protosummary_stats = g_malloc( sizeof(traffic_stats_t) );
  g_assert(protosummary_stats);
  traffic_stats_init(protosummary_stats);
}

/* frees summary, releasing resources */
void protocol_summary_close(void)
{
  if (protosummary_stats)
    {
      traffic_stats_reset(protosummary_stats);
      g_free(protosummary_stats);
      protosummary_stats = NULL;
    }
}

/* adds a new packet to summary */
void protocol_summary_add_packet(packet_info_t *packet)
{
  if (!protosummary_stats)
    protocol_summary_open();

  traffic_stats_add_packet( protosummary_stats, packet, EITHERBOUND); 
}

/* update stats on protocol summary */
void protocol_summary_update_all(void)
{
  if (protosummary_stats)
    traffic_stats_update(protosummary_stats, pref.averaging_time, 
                          pref.proto_timeout_time);
}

/* number of protos at specified level */
long protocol_summary_size(void)
{
  long totproto = 0;
  gint i;
  if (!protosummary_stats)
    return 0;
  for (i = 0; i <= STACK_SIZE ; ++i)
    {
      if (protosummary_stats->stats_protos.protostack[i])
        {
          totproto += 
              g_list_length(protosummary_stats->stats_protos.protostack[i]);
        }
    }
  return totproto;
}


/* calls func for every protocol at the specified level */
void protocol_summary_foreach(size_t level, GFunc func, gpointer data)
{
  if (!protosummary_stats || level > STACK_SIZE)
    return;
  g_list_foreach (protosummary_stats->stats_protos.protostack[level], func, data);
}


/* finds named protocol in the level protocols of protostack*/
const protocol_t *protocol_summary_find(size_t level, const gchar *protoname)
{
  if (!protosummary_stats)
    return NULL;
  return protocol_stack_find(&protosummary_stats->stats_protos, level, protoname);
}

 /* access directly the stack (only for proto windows) */
const protostack_t *protocol_summary_stack(void)
{
  if (!protosummary_stats)
    return NULL;
  return &protosummary_stats->stats_protos;
}
