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

#include <gnome.h>
#include <netinet/in.h>
#include "names.h"

typedef void (p_func_t) (void);

typedef struct
{
  gchar *prot;
  p_func_t *function;
}
prot_function_t;

#define KNOWN_PROTS 4

static prot_function_t prot_functions_table[KNOWN_PROTS + 1] = {
  {"ETH_II", get_eth_name},
  {"802.2", get_eth_name},
  {"802.3", get_eth_name},
  {"ISL", get_eth_name},
  {"IP", get_ip_name}
};

static const guint8 *p;
static guint16 offset;
static guint8 level;
static guint id_length;
static packet_direction dir;
static GList **prot_list;
static GList *protocol_item;
static protocol_t *protocol;
static GList *name_item;
static name_t *name;
static gchar **tokens = NULL;
static GTree *prot_functions = NULL;
static prot_function_t *next_func = NULL;



void
get_packet_names (GList ** protocols,
		  const guint8 * packet,
		  const gchar * prot_stack, packet_direction direction)
{
  g_assert (protocols != NULL);
  g_assert (packet != NULL);

  if (!prot_functions)
    {
      guint i = 0;
      prot_functions = g_tree_new ((GCompareFunc) strcmp);
      for (; i <= KNOWN_PROTS; i++)
	g_tree_insert (prot_functions,
		       prot_functions_table[i].prot,
		       &(prot_functions_table[i]));
    }

  p = packet;
  dir = direction;
  prot_list = protocols;
  offset = 0;
  level = 0;
  tokens = g_strsplit (prot_stack, "/", 0);

  switch (linktype)
    {
    case L_EN10MB:
      get_eth_name ();
      break;
    case L_FDDI:
      break;
    case L_PPP:
      break;
    case L_SLIP:
      break;
    default:
      break;
    }

  g_strfreev (tokens);
}				/* get_packet_names */

void
get_eth_name (void)
{
  const guint8 *eth_address;
  static gchar *prot = NULL;

  prot = tokens[level];

  if (dir == INBOUND)
    eth_address = p + offset;
  else
    eth_address = p + offset + 6;

  id_length = 6;

  /* Find the protocol entry */
  protocol_item = g_list_find_custom (prot_list[level],
				      prot, protocol_compare);
  protocol = (protocol_t *) (protocol_item->data);
  name_item = protocol->node_names;

  /* Have we heard this address before? */
  name = g_list_find_custom (name_item, eth_address, id_compare);

  if (!name)			/* First time name */
    {
      name = g_malloc (sizeof (name_t));
      name->node_id = g_memdup (eth_address, 6);
      name->numeric_name = g_string_new (ether_to_str (eth_address));
      if (numeric)
	name->name = g_string_new (ether_to_str (eth_address));
      else
	name->name = g_string_new (get_ether_name (eth_address));
      protocol->node_names = g_list_prepend (protocol->node_names, name);

    }

  level++;
  offset += 14;

  next_func = g_tree_lookup (prot_functions, tokens[level]);
  if (next_func)
    next_func->function ();

}				/* get_eth_name */


void
get_ip_name (void)
{
}				/* get_ip_name */


/* Comparison function used to compare two node ids */
gint id_compare (gconstpointer a, gconstpointer b)
{
  g_assert (a != NULL);
  g_assert (b != NULL);

  return memcmp (((name_t *) a)->node_id, (guint8 *) b, id_length);
}				/* id_compare */
