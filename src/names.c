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

void
get_packet_names (GList ** protocols,
		  const guint8 * packet,
		  guint16 size,
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
  packet_size = size;
  dir = direction;
  prot_list = protocols;
  offset = 0;
  level = 1;			/* Level 0 is for the topmost. Not valid */
  tokens = g_strsplit (prot_stack, "/", 0);

  next_func = g_tree_lookup (prot_functions, tokens[level]);
  if (next_func)
    next_func->function ();

  g_strfreev (tokens);
}				/* get_packet_names */


/* Raw is used for ppp and slip. There is actually no information,
 * so we just jump to the next protocol */
static void
get_raw_name (void)
{

  level++;

  next_func = g_tree_lookup (prot_functions, tokens[level]);
  if (next_func)
    next_func->function ();

}				/* get_raw_name */

static void
get_eth_name (void)
{

  if (dir == INBOUND)
    id = p + offset;
  else
    id = p + offset + 6;

  id_length = 6;

  add_name (ether_to_str (id), get_ether_name (id));

  level++;
  offset += 14;

  next_func = g_tree_lookup (prot_functions, tokens[level]);
  if (next_func)
    next_func->function ();

}				/* get_eth_name */


static void
get_ip_name (void)
{

  if (dir == INBOUND)
    id = p + offset + 16;
  else
    id = p + offset + 12;

  id_length = 4;

  if (numeric)
    add_name (ip_to_str (id), ip_to_str (id));
  else
    {
      if (mode == ETHERNET)
	add_name (ip_to_str (id), dns_lookup (pntohl (id), FALSE));
      else
	add_name (ip_to_str (id), dns_lookup (pntohl (id), TRUE));
    }

  /* TODO I don't like the fact that the gdk_input for dns.c is not
   * called in this function, because it means that it can't be used 
   * as a library */

  g_string_assign (name->name, dns_lookup (pntohl (id), TRUE));

  level++;
  offset += 20;

  next_func = g_tree_lookup (prot_functions, tokens[level]);
  if (next_func)
    next_func->function ();

}				/* get_ip_name */

static void
get_tcp_name (void)
{
  guint8 *id_buffer;
  guint16 port;
  GString *numeric_name, *resolved_name;
  gchar *str;


  id_length = 6;

  id_buffer = g_malloc (id_length);

  if (dir == OUTBOUND)
    {
      g_memmove (id_buffer, p + offset - 8, 4);
      port = ntohs (*(guint16 *) (p + offset));
      g_memmove (id_buffer + 4, &port, 2);
    }
  else
    {
      g_memmove (id_buffer, p + offset - 4, 4);
      port = ntohs (*(guint16 *) (p + offset + 2));
      g_memmove (id_buffer + 4, &port, 2);
    }

  numeric_name = g_string_new (ip_to_str (id_buffer));
  numeric_name = g_string_append_c (numeric_name, ':');
  str = g_strdup_printf ("%d", *(guint16 *) (id_buffer + 4));
  numeric_name = g_string_append (numeric_name, str);
  g_free (str);

  resolved_name = g_string_new (dns_lookup (pntohl (id_buffer), TRUE));
  resolved_name = g_string_append_c (resolved_name, ':');
  resolved_name = g_string_append (resolved_name,
				   get_tcp_port (*(guint16 *)
						 (id_buffer + 4)));

  id = id_buffer;
  add_name (numeric_name->str, resolved_name->str);

  g_free (id_buffer);
  g_string_free (numeric_name, TRUE);
  g_string_free (resolved_name, TRUE);

  level++;
  offset += 20;

  next_func = g_tree_lookup (prot_functions, tokens[level]);
  if (next_func)
    next_func->function ();

}				/* get_tcp_name */


static void
add_name (gchar * numeric_name, gchar * resolved_name)
{
  /* Find the protocol entry */
  protocol_item = g_list_find_custom (prot_list[level],
				      tokens[level], protocol_compare);
  protocol = (protocol_t *) (protocol_item->data);
  name_item = protocol->node_names;

  /* Have we heard this address before? */
  name_item = g_list_find_custom (name_item, id, id_compare);
  if (name_item)
    name = name_item->data;
  else
    name = NULL;

  if (!name)			/* First time name */
    {
      name = g_malloc (sizeof (name_t));
      name->node_id = g_memdup (id, id_length);
      name->n_packets = 0;
      name->accumulated = 0;
      name->numeric_name = NULL;
      name->name = NULL;
      protocol->node_names = g_list_prepend (protocol->node_names, name);

    }

  if (!name->numeric_name)
    name->numeric_name = g_string_new (numeric_name);
  else
    g_string_assign (name->numeric_name, numeric_name);

  if (!name->name)
    {
      if (numeric)
	name->name = g_string_new (numeric_name);
      else
	name->name = g_string_new (resolved_name);
    }
  else
    {
      if (numeric)
	g_string_assign (name->name, numeric_name);
      else
	g_string_assign (name->name, resolved_name);
    }

  name->n_packets++;
  name->accumulated += packet_size;

}				/* add_name */

/* Comparison function used to compare two node ids */
static gint
id_compare (gconstpointer a, gconstpointer b)
{
  g_assert (a != NULL);
  g_assert (b != NULL);

  return memcmp (((name_t *) a)->node_id, (guint8 *) b, id_length);
}				/* id_compare */
