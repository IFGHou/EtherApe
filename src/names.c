/* EtherApe
 * Copyright (C) 2001 Juan Toledo
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
  tokens = NULL;
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

/* Null is used for loopback. There is actually no information,
 * so we just jump to the next protocol */
/* TODO Are we so sure there is actually no information?
 * Then what are those four bytes? */
static void
get_null_name (void)
{

  level++;
  offset += 4;

  next_func = g_tree_lookup (prot_functions, tokens[level]);
  if (next_func)
    next_func->function ();

}				/* get_null_name */

/* linux-sll is used for ISDN on linux, I believe. 
 * Only one of the MAC addresses involved is shown each time,
 * so by now I will simply not try to decode MAC addresses */
/* TODO Do something useful with the address that shows */
static void
get_linux_sll_name (void)
{

  level++;
  /* TODO
   * I'm assuming that the header is always size 16. I don't know
   * weather that's always the case... (I'm making that assumption
   * since ethereal is not decoding a couple of bytes, which then
   * seem to be just padding */
  offset += 16;

  next_func = g_tree_lookup (prot_functions, tokens[level]);
  if (next_func)
    next_func->function ();

}				/* get_linux_sll_name */

static void
get_eth_name (void)
{
  gchar *numeric = NULL, *solved = NULL;
  gboolean found_in_ethers = FALSE;

  if (dir == INBOUND)
    id = p + offset;
  else
    id = p + offset + 6;

  id_length = 6;

  numeric = ether_to_str (id);
  solved = get_ether_name (id);

  /* get_ether_name will return an ethernet address with
   * the first three numbers substituted with the manufacter
   * if it cannot find an /etc/ethers entry. If it is so,
   * then the last 8 characters (for example ab:cd:ef) will
   * be the same, and we will note that the name hasn't
   * been solved */

  if (numeric && solved)
    found_in_ethers = strcmp (numeric + strlen (numeric) - 8,
			      solved + strlen (solved) - 8);

  if (found_in_ethers)
    add_name (numeric, solved, TRUE);
  else
    add_name (numeric, solved, FALSE);

  level++;
  offset += 14;

  next_func = g_tree_lookup (prot_functions, tokens[level]);
  if (next_func)
    next_func->function ();

}				/* get_eth_name */

static void
get_ieee802_name (void)
{
  gchar *numeric = NULL, *solved = NULL;
  gboolean found_in_ethers = FALSE;

  if (dir == INBOUND)
    id = p + offset + 2;
  else
    id = p + offset + 8;

  id_length = 6;

  numeric = ether_to_str (id);
  solved = get_ether_name (id);

  /* get_ether_name will return an ethernet address with
   * the first three numbers substituted with the manufacter
   * if it cannot find an /etc/ethers entry. If it is so,
   * then the last 8 characters (for example ab:cd:ef) will
   * be the same, and we will note that the name hasn't
   * been solved */

  if (numeric && solved)
    found_in_ethers = strcmp (numeric + strlen (numeric) - 8,
			      solved + strlen (solved) - 8);

  if (found_in_ethers)
    add_name (numeric, solved, TRUE);
  else
    add_name (numeric, solved, FALSE);

  level++;
  offset += 14;

  next_func = g_tree_lookup (prot_functions, tokens[level]);
  if (next_func)
    next_func->function ();

}				/* get_ieee802_name */

static void
get_fddi_name (void)
{
  gchar *numeric = NULL, *solved = NULL;
  gboolean found_in_ethers = FALSE;

  if (dir == INBOUND)
    id = p + offset + 1;
  else
    id = p + offset + 7;

  id_length = 6;

  numeric = ether_to_str (id);
  solved = get_ether_name (id);

  /* get_ether_name will return an ethernet address with
   * the first three numbers substituted with the manufacter
   * if it cannot find an /etc/ethers entry. If it is so,
   * then the last 8 characters (for example ab:cd:ef) will
   * be the same, and we will note that the name hasn't
   * been solved */

  if (numeric && solved)
    found_in_ethers = strcmp (numeric + strlen (numeric) - 8,
			      solved + strlen (solved) - 8);

  if (found_in_ethers)
    add_name (numeric, solved, TRUE);
  else
    add_name (numeric, solved, FALSE);

  level++;
  offset += 13;

  next_func = g_tree_lookup (prot_functions, tokens[level]);
  if (next_func)
    next_func->function ();

}				/* get_fddi_name */

/* LLC is the only supported FDDI link layer type */
static void
get_llc_name (void)
{

  level++;
  /* TODO IMPORTANT
   * We must decode the llc header to calculate the offset
   * We are just doing assumptions by now */

  if ((linktype == L_FDDI) || (linktype == L_IEEE802))
    offset += 8;
  else if (linktype == L_EN10MB)
    offset += 3;
  else
    return;

  next_func = g_tree_lookup (prot_functions, tokens[level]);
  if (next_func)
    next_func->function ();

}				/* get_llc_name */

static void
get_arp_name (void)
{
  guint16 protocol_type;
  guint8 hardware_len, protocol_len;
  const guint8 *id;
#define ARPTYPE_IP 0x0800

  /* We can only tell the IP address of the asking node.
   * Most of the times the callee will be the broadcast 
   * address */
  if (dir == INBOUND)
    return;

  /* We only know about IP ARP queries */
  protocol_type = pntohs ((p + offset + 2));
  if (protocol_type != ARPTYPE_IP)
    return;

  hardware_len = *(guint8 *) (p + offset + 4);
  protocol_len = *(guint8 *) (p + offset + 5);

  id = p + offset + 8 + hardware_len;

  if (pref.mode == ETHERNET)
    add_name (ip_to_str (id), dns_lookup (pntohl (id), FALSE), TRUE);
  else
    add_name (ip_to_str (id), dns_lookup (pntohl (id), TRUE), TRUE);


  /* ARP doesn't carry any other protocol on top, so we return 
   * directly */
}				/* get_arp_name */

static void
get_ip_name (void)
{

  if (dir == INBOUND)
    id = p + offset + 16;
  else
    id = p + offset + 12;

  id_length = 4;

  if (pref.numeric)
    add_name (ip_to_str (id), ip_to_str (id), FALSE);
  else
    {
      if (pref.mode == ETHERNET)
	add_name (ip_to_str (id), dns_lookup (pntohl (id), FALSE), TRUE);
      else
	add_name (ip_to_str (id), dns_lookup (pntohl (id), TRUE), TRUE);
    }

  /* TODO I don't like the fact that the gdk_input for dns.c is not
   * called in this function, because it means that it can't be used 
   * as a library */

  level++;
  offset += 20;

  next_func = g_tree_lookup (prot_functions, tokens[level]);

  if (next_func)
    next_func->function ();

}				/* get_ip_name */

static void
get_ipx_name (void)
{
  level++;
  offset += 30;

  next_func = g_tree_lookup (prot_functions, tokens[level]);
  if (next_func)
    next_func->function ();
}

static void
get_tcp_name (void)
{
  guint8 *id_buffer;
  guint8 th_off_x2;
  guint8 tcp_len;
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

  /* TODO I should substitute these for a g_snprintf
   * and save myself so many allocations */

  numeric_name = g_string_new (ip_to_str (id_buffer));
  numeric_name = g_string_append_c (numeric_name, ':');
  str = g_strdup_printf ("%d", *(guint16 *) (id_buffer + 4));
  numeric_name = g_string_append (numeric_name, str);
  g_free (str);
  str = NULL;

  resolved_name = g_string_new (dns_lookup (pntohl (id_buffer), TRUE));
  resolved_name = g_string_append_c (resolved_name, ':');
  resolved_name = g_string_append (resolved_name,
				   get_tcp_port (*(guint16 *)
						 (id_buffer + 4)));

  id = id_buffer;
  add_name (numeric_name->str, resolved_name->str, TRUE);

  g_free (id_buffer);
  id_buffer = NULL;
  g_string_free (numeric_name, TRUE);
  numeric_name = NULL;
  g_string_free (resolved_name, TRUE);
  resolved_name = NULL;

  level++;

  th_off_x2 = *(guint8 *) (p + offset + 12);
  tcp_len = hi_nibble (th_off_x2) * 4;	/* TCP header length, in bytes */
  offset += tcp_len;

  next_func = g_tree_lookup (prot_functions, tokens[level]);
  if (next_func)
    next_func->function ();

}				/* get_tcp_name */


/* TODO I still have to properly implement this. Right now it's just
 * a placeholder to get to the UDP/NETBIOS-DGM */
static void
get_udp_name (void)
{

  level++;
  offset += 8;

  next_func = g_tree_lookup (prot_functions, tokens[level]);
  if (next_func)
    next_func->function ();

}				/* get_udp_name */


/* TODO SET UP THE id's FOR THIS NETBIOS NAME FUNCTIONS */
static void
get_ipxsap_name (void)
{
  guint16 sap_type;
  gchar *name;

  sap_type = pntohs (p + offset);

  /* we want responses */
  if (sap_type != 0x0002)
    return;

  name = (gchar *) (p + offset + 4);

  g_my_debug ("Sap name %s found", name);

  add_name (name, name, TRUE);

}				/* get_ipxsap_name */

static void
get_nbipx_name (void)
{

}				/* get_ipxsap_name */


static void
get_nbss_name (void)
{
  /* TODO this really shouldn't be definened both here and in
   * names_netbios.h */
#define SESSION_REQUEST 0x81
#define MAXDNAME        1025	/* maximum domain name length */
#define NETBIOS_NAME_LEN  16

  guint i = 0;
  guint8 mesg_type;
  guint16 length;
  gchar *numeric_name = NULL;
  gchar name[(NETBIOS_NAME_LEN - 1) * 4 + MAXDNAME];
  guint name_len;
  int name_type;		/* TODO I hate to use an int here, while I have been
				   * using glib data types all the time. But I just don't
				   * know what it might mean in the ethereal implementation */


  mesg_type = *(guint8 *) (p + offset);
  length = pntohs ((p + offset + 2));

  offset += 4;

  if (mesg_type == SESSION_REQUEST)
    {
      name_len = ethereal_nbns_name (p, offset, offset, name, &name_type);
      if (dir == OUTBOUND)
	ethereal_nbns_name (p, offset + name_len, offset + name_len, name,
			    &name_type);


      /* We just want the name, not the space padding behind it */

      for (; i <= (NETBIOS_NAME_LEN - 2) && name[i] != ' '; i++);
      name[i] = '\0';

      /* Many packages will be straight TCP packages directed to the proper
       * port which first byte happens to be SESSION_REQUEST. In those cases
       * the name will be illegal, and we will not add it */
      if (strcmp (name, "Illegal"))
	{
	  numeric_name =
	    g_strdup_printf ("%s %s (%s)", name, name + NETBIOS_NAME_LEN - 1,
			     get_netbios_host_type (name_type));

	  add_name (numeric_name, name, TRUE);
	  g_free (numeric_name);
	}

    }


  level++;
  offset += length;

  next_func = g_tree_lookup (prot_functions, tokens[level]);
  if (next_func)
    next_func->function ();
}				/* get_nbss_name */

static void
get_nbdgm_name (void)
{
  guint8 mesg_type;
  gchar *numeric_name = NULL;
  gchar name[(NETBIOS_NAME_LEN - 1) * 4 + MAXDNAME];
  gboolean name_found = FALSE;
  int name_type;
  int len;
  guint i = 0;

  mesg_type = *(guint8 *) (p + offset);

  offset += 10;

  /* Magic numbers copied from ethereal, as usual
   * They mean Direct (unique|group|broadcast) datagram */
  if (mesg_type == 0x10 || mesg_type == 0x11 || mesg_type == 0x12)
    {
      offset += 4;
      len = ethereal_nbns_name (p, offset, offset, name, &name_type);

      if (dir == INBOUND)
	ethereal_nbns_name (p, offset + len, offset + len, name, &name_type);
      name_found = TRUE;

    }
  else if (mesg_type == 0x14 || mesg_type == 0x15 || mesg_type == 0x16)
    {
      if (dir == INBOUND)
	{
	  len = ethereal_nbns_name (p, offset, offset, name, &name_type);
	  name_found = TRUE;
	}
    }

  /* We just want the name, not the space padding behind it */

  for (; i <= (NETBIOS_NAME_LEN - 2) && name[i] != ' '; i++);
  name[i] = '\0';

  /* The reasing here might not make sense as in the TCP case, but it
   * doesn't hurt to check that we don't have an illegal name anyhow */
  if (strcmp (name, "Illegal") && name_found)
    {
      numeric_name =
	g_strdup_printf ("%s %s (%s)", name, name + NETBIOS_NAME_LEN - 1,
			 get_netbios_host_type (name_type));

      add_name (numeric_name, name, TRUE);
      g_free (numeric_name);
    }


}				/* get_nbdgm_name */



static void
add_name (gchar * numeric_name, gchar * resolved_name, gboolean solved)
{
  GList *protocol_item = NULL;
  protocol_t *protocol = NULL;
  GList *name_item = NULL;

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
      if (pref.numeric)
	name->name = g_string_new (numeric_name);
      else
	name->name = g_string_new (resolved_name);
    }
  else
    {
      if (pref.numeric)
	g_string_assign (name->name, numeric_name);
      else
	g_string_assign (name->name, resolved_name);
    }

  if (pref.numeric)
    name->solved = FALSE;
  else
    name->solved = solved;

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
