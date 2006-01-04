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
#include "dns.h"
#include "eth_resolv.h"
#include "names_netbios.h"
#include "names.h"
#include "protocols.h"

typedef struct
{
  const guint8 *p;
  guint16 offset;
  guint16 packet_size;
  packet_direction dir;
  node_id_t node_id;    /* topmost node_id: if a decoder can't fill it, it will
                         * use the lower level info */
  struct
  {
    guint8 level;       /* current decoder level */
    gchar **tokens;     /* array of decoder names */
    protostack_t *protos;  /* protocol list */
  } decoder;
}
name_add_t;

typedef void (p_func_t) (name_add_t *);

typedef struct
{
  gchar *prot;
  p_func_t *function;
}
prot_function_t;

static void get_eth_name (name_add_t *nt);
static void get_raw_name (name_add_t *nt);
static void get_null_name (name_add_t *nt);
static void get_linux_sll_name (name_add_t *nt);
static void get_fddi_name (name_add_t *nt);
static void get_ieee802_name (name_add_t *nt);
static void get_llc_name (name_add_t *nt);
static void get_arp_name (name_add_t *nt);
static void get_ip_name (name_add_t *nt);
static void get_ipx_name (name_add_t *nt);
static void get_udp_name (name_add_t *nt);
static void get_tcp_name (name_add_t *nt);
static void get_ipxsap_name (name_add_t *nt);
static void get_nbipx_name (name_add_t *nt);
static void get_nbss_name (name_add_t *nt);
static void get_nbdgm_name (name_add_t *nt);

#define KNOWN_PROTS 18

static prot_function_t prot_functions_table[KNOWN_PROTS + 1] = {
  {"ETH_II", get_eth_name},
  {"802.2", get_eth_name},
  {"802.3", get_eth_name},
  {"ISL", get_eth_name},
  {"RAW", get_raw_name},
  {"NULL", get_null_name},
  {"LINUX-SLL", get_linux_sll_name},
  {"FDDI", get_fddi_name},
  {"Token Ring", get_ieee802_name},
  {"LLC", get_llc_name},
  {"ARP", get_arp_name},
  {"IP", get_ip_name},
  {"IPX", get_ipx_name},
  {"TCP", get_tcp_name},
  {"UDP", get_udp_name},
  {"IPX-SAP", get_ipxsap_name},
  {"IPX-NetBIOS", get_nbipx_name},
  {"NETBIOS-SSN", get_nbss_name},
  {"NETBIOS-DGM", get_nbdgm_name}
};

static void add_name (gchar * numeric, gchar * resolved, gboolean solved,
                      const node_id_t *node_id, const name_add_t *nt);
static void decode_next(name_add_t *nt);

void
get_packet_names (protostack_t *pstk,
		  const guint8 * packet,
		  guint16 size,
		  const gchar * prot_stack, packet_direction direction)
{
  name_add_t nt;

  g_assert (pstk != NULL);
  g_assert (packet != NULL);

  nt.p = packet;
  nt.packet_size = size;
  nt.dir = direction;
  nt.offset = 0;
  
  /* initializes decoders info 
   * Note: Level 0 means topmost - first usable is 1 
   * We initialize as 0 because decode_next() preincrements
   */
  nt.decoder.level = 0;
  nt.decoder.tokens = g_strsplit (prot_stack, "/", 0);
  nt.decoder.protos = pstk;

  /* starts decoding */
  decode_next(&nt);

  g_strfreev (nt.decoder.tokens);
}				/* get_packet_names */

/* increments the level and calls the next name decoding function, if any */
static void decode_next(name_add_t *nt)
{
  static GTree *prot_functions = NULL;
  prot_function_t *next_func = NULL;
  
  if (!prot_functions)
    {
      /* initializes proto table */
      guint i;
      prot_functions = g_tree_new ((GCompareFunc) strcmp);
      for (i = 0; i <= KNOWN_PROTS; i++)
	g_tree_insert (prot_functions,
		       prot_functions_table[i].prot,
		       &(prot_functions_table[i]));
    }

  g_assert(nt);
  g_assert(nt->decoder.tokens[nt->decoder.level]); /* current level must be valid */

  nt->decoder.level++;

  if (!nt->decoder.tokens[nt->decoder.level])
    return; /* no more decoders, exit */

  next_func = g_tree_lookup (prot_functions, nt->decoder.tokens[nt->decoder.level]);
  if (next_func)
    {
      /* before calling the next decoder, we check for size overflow 
       * To continue, we must have at least 4 bytes remaining ... */
      if (nt->packet_size <= nt->offset + 4)
        {
          g_critical(_("detected undersize packet, aborting protocol decode"));
          return;
        }
  
      next_func->function (nt);
    }
}

/* fills a node id with the specified mode and the data taken from nt using current
 * position (offset) + plus a displacement disp.
 * For tcp, it also uses the portdisp displacement as an offset for the port number.
 * Checks also the packet size against overflow
 */
static void fill_node_id(node_id_t *node_id, apemode_t apemode, const name_add_t *nt, 
                          int disp, int portdisp)
{
  guint8 *dt = NULL;
  size_t sz = 0;

  memset(node_id, 0, sizeof(node_id_t));
  node_id->node_type = apemode;
  
  switch (apemode)
  {
  case ETHERNET:
    dt = node_id->addr.eth;
    sz = sizeof(node_id->addr.eth);
    break;
  case FDDI:
    dt = node_id->addr.fddi;
    sz = sizeof(node_id->addr.fddi);
    break;
  case IEEE802:
    dt = node_id->addr.i802;
    sz = sizeof(node_id->addr.i802);
    break;
  case IP:
    dt = node_id->addr.ip4;
    sz = sizeof(node_id->addr.ip4);
    break;
  case TCP:
    dt = node_id->addr.tcp4.host;
    sz = sizeof(node_id->addr.tcp4); /* full size */
    break;
  default:
    g_error (_("Unsopported ape mode in fill_node_id"));
  }

  if (nt->packet_size < nt->offset + disp + sz ||
     nt->packet_size < nt->offset + portdisp + sz)
    g_error (_("Packet overflow"));

  if (TCP == apemode)
  {
    guint16 port;
    g_memmove(dt, nt->p + nt->offset + disp, sz-2);
    port = ntohs (*(guint16 *) (nt->p + nt->offset + portdisp));
    g_memmove(dt+sz-2, &port, 2);
  }
  else
    g_memmove(dt, nt->p + nt->offset + disp, sz);
}

/* Raw is used for ppp and slip. There is actually no information,
 * so we just jump to the next protocol */
static void
get_raw_name (name_add_t *nt)
{
  decode_next(nt);
}				/* get_raw_name */

/* Null is used for loopback. There is actually no information,
 * so we just jump to the next protocol */
/* TODO Are we so sure there is actually no information?
 * Then what are those four bytes? */
static void
get_null_name (name_add_t *nt)
{
  nt->offset += 4;

  decode_next(nt);
}				/* get_null_name */

/* linux-sll is used for ISDN on linux, I believe. 
 * Only one of the MAC addresses involved is shown each time,
 * so by now I will simply not try to decode MAC addresses */
/* TODO Do something useful with the address that shows */
static void
get_linux_sll_name (name_add_t *nt)
{
  /* TODO
   * I'm assuming that the header is always size 16. I don't know
   * weather that's always the case... (I'm making that assumption
   * since ethereal is not decoding a couple of bytes, which then
   * seem to be just padding */
  nt->offset += 16;

  decode_next(nt);
}				/* get_linux_sll_name */

static void
get_eth_name (name_add_t *nt)
{
  gchar *numeric = NULL, *solved = NULL;
  gboolean found_in_ethers = FALSE;

  if (nt->dir == INBOUND)
    fill_node_id(&nt->node_id, ETHERNET, nt, 0, 0);
  else
    fill_node_id(&nt->node_id, ETHERNET, nt, 6, 0);

  numeric = ether_to_str (nt->node_id.addr.eth);
  solved = get_ether_name (nt->node_id.addr.eth);

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
    add_name (numeric, solved, TRUE, &nt->node_id, nt);
  else
    add_name (numeric, solved, FALSE, &nt->node_id, nt);

  nt->offset += 14;

  decode_next(nt);
}				/* get_eth_name */

static void
get_ieee802_name (name_add_t *nt)
{
  gchar *numeric = NULL, *solved = NULL;
  gboolean found_in_ethers = FALSE;

  if (nt->dir == INBOUND)
    fill_node_id(&nt->node_id, IEEE802, nt, 2, 0);
  else
    fill_node_id(&nt->node_id, IEEE802, nt, 8, 0);

  numeric = ether_to_str (nt->node_id.addr.i802);
  solved = get_ether_name (nt->node_id.addr.i802);

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
    add_name (numeric, solved, TRUE, &nt->node_id, nt);
  else
    add_name (numeric, solved, FALSE, &nt->node_id, nt);

  nt->offset += 14;

  decode_next(nt);
}				/* get_ieee802_name */

static void
get_fddi_name (name_add_t *nt)
{
  gchar *numeric = NULL, *solved = NULL;
  gboolean found_in_ethers = FALSE;

  if (nt->dir == INBOUND)
    fill_node_id(&nt->node_id, FDDI, nt, 1, 0);
  else
    fill_node_id(&nt->node_id, FDDI, nt, 7, 0);

  numeric = ether_to_str (nt->node_id.addr.fddi);
  solved = get_ether_name (nt->node_id.addr.fddi);

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
    add_name (numeric, solved, TRUE, &nt->node_id, nt);
  else
    add_name (numeric, solved, FALSE, &nt->node_id, nt);

  nt->offset += 13;

  decode_next(nt);
}				/* get_fddi_name */

/* LLC is the only supported FDDI link layer type */
static void
get_llc_name (name_add_t *nt)
{
  /* TODO IMPORTANT
   * We must decode the llc header to calculate the nt->offset
   * We are just doing assumptions by now */

  if ((linktype == L_FDDI) || (linktype == L_IEEE802))
    nt->offset += 8;
  else if (linktype == L_EN10MB)
    nt->offset += 3;
  else
    return;

  decode_next(nt);
}				/* get_llc_name */

static void
get_arp_name (name_add_t *nt)
{
  guint16 protocol_type;
  guint8 hardware_len, protocol_len;
#define ARPTYPE_IP 0x0800

  /* We can only tell the IP address of the asking node.
   * Most of the times the callee will be the broadcast 
   * address */
  if (nt->dir == INBOUND)
    return;

  /* We only know about IP ARP queries */
  protocol_type = pntohs ((nt->p + nt->offset + 2));
  if (protocol_type != ARPTYPE_IP)
    return;

  if (nt->packet_size <= nt->offset + 7)
    {
      g_critical(_("detected undersize packet, aborting get_arp_name"));
      return;
    }

  hardware_len = *(guint8 *) (nt->p + nt->offset + 4);
  protocol_len = *(guint8 *) (nt->p + nt->offset + 5);

  fill_node_id(&nt->node_id, IP, nt, 8 + hardware_len, 0);

  if (pref.mode == ETHERNET)
    add_name (ip_to_str (nt->node_id.addr.ip4), dns_lookup (pntohl (nt->node_id.addr.ip4), 
              FALSE), TRUE, &nt->node_id, nt);
  else
    add_name (ip_to_str (nt->node_id.addr.ip4), dns_lookup (pntohl (nt->node_id.addr.ip4), 
              TRUE), TRUE, &nt->node_id, nt);

  /* ARP doesn't carry any other protocol on top, so we return 
   * directly */
}				/* get_arp_name */

static void
get_ip_name (name_add_t *nt)
{

  if (nt->dir == INBOUND)
    fill_node_id(&nt->node_id, IP, nt, 16, 0);
  else
    fill_node_id(&nt->node_id, IP, nt, 12, 0);

  if (!pref.name_res)
    add_name (ip_to_str (nt->node_id.addr.ip4), ip_to_str (nt->node_id.addr.ip4), FALSE, &nt->node_id, nt);
  else
    {
      if (pref.mode == ETHERNET)
	add_name (ip_to_str(nt->node_id.addr.ip4), dns_lookup(pntohl (nt->node_id.addr.ip4), FALSE), TRUE, &nt->node_id, nt);
      else
	add_name (ip_to_str(nt->node_id.addr.ip4), dns_lookup(pntohl (nt->node_id.addr.ip4), TRUE), TRUE, &nt->node_id, nt);
    }

  /* TODO I don't like the fact that the gdk_input for dns.c is not
   * called in this function, because it means that it can't be used 
   * as a library */

  nt->offset += 20;

  decode_next(nt);
}				/* get_ip_name */

static void
get_ipx_name (name_add_t *nt)
{
  nt->offset += 30;

  decode_next(nt);
}

static void
get_tcp_name (name_add_t *nt)
{
  guint8 th_off_x2;
  guint8 tcp_len;
  GString *numeric_name, *resolved_name;

  if (nt->dir == OUTBOUND)
      fill_node_id(&nt->node_id, TCP, nt, -8, 0);
  else
      fill_node_id(&nt->node_id, TCP, nt, -4, 2);

  numeric_name = g_string_new("");
  g_string_printf(numeric_name, "%s:%d",ip_to_str(nt->node_id.addr.tcp4.host),
                                *(guint16 *) (nt->node_id.addr.tcp4.port));
  
  resolved_name = g_string_new (dns_lookup (pntohl (nt->node_id.addr.tcp4.host), TRUE));
  resolved_name = g_string_append_c (resolved_name, ':');
  resolved_name = g_string_append (resolved_name,
				   get_tcp_port (*(guint16 *)
						 (nt->node_id.addr.tcp4.port)));

  add_name (numeric_name->str, resolved_name->str, TRUE, &nt->node_id, nt);

  g_string_free (numeric_name, TRUE);
  numeric_name = NULL;
  g_string_free (resolved_name, TRUE);
  resolved_name = NULL;

  if (nt->packet_size <= nt->offset + 14)
    {
      g_critical(_("detected undersize packet, aborting get_tcp_name"));
      return;
    }

  th_off_x2 = *(guint8 *) (nt->p + nt->offset + 12);
  tcp_len = hi_nibble (th_off_x2) * 4;	/* TCP header length, in bytes */
  nt->offset += tcp_len;

  decode_next(nt);
}				/* get_tcp_name */


/* TODO I still have to properly implement this. Right now it's just
 * a placeholder to get to the UDP/NETBIOS-DGM */
static void
get_udp_name (name_add_t *nt)
{
  nt->offset += 8;

  decode_next(nt);
}				/* get_udp_name */


/* TODO SET UP THE id's FOR THIS NETBIOS NAME FUNCTIONS */
static void
get_ipxsap_name (name_add_t *nt)
{
  guint16 sap_type;
  guint16 curpos;
  gchar *name;

  sap_type = pntohs (nt->p + nt->offset);

  /* we want responses */
  if (sap_type != 0x0002)
    return;

  for (curpos = nt->offset + 4; curpos < nt->packet_size ; ++curpos)
    {
      if ( ! *((gchar *) (nt->p + curpos)) )
        break; // found termination
    }
  if (curpos >= nt->packet_size)
    {
      g_critical(_("detected undersize packet, aborting get_sap_name"));
      return;
    }
    
  name = (gchar *) (nt->p + nt->offset + 4);

  g_my_debug ("Sap name %s found", name);

  add_name (name, name, TRUE, &nt->node_id, nt);

}				/* get_ipxsap_name */

static void
get_nbipx_name (name_add_t *nt)
{

}

static void
get_nbss_name (name_add_t *nt)
{
#define SESSION_REQUEST 0x81

  guint i = 0;
  guint8 mesg_type;
  guint16 length;
  gchar *numeric_name = NULL;
  gchar name[(NETBIOS_NAME_LEN - 1) * 4 + MAXDNAME];
  guint name_len;
  int name_type;		/* TODO I hate to use an int here, while I have been
				 * using glib data types all the time. But I just don't
				 * know what it might mean in the ethereal implementation */

  mesg_type = *(guint8 *) (nt->p + nt->offset);
  length = pntohs ((nt->p + nt->offset + 2));

  nt->offset += 4;

  if (mesg_type == SESSION_REQUEST)
    {
      name_len = ethereal_nbns_name ((const gchar *)nt->p, nt->offset, nt->offset, name, &name_type);
      if (nt->dir == OUTBOUND)
	ethereal_nbns_name ((const gchar *)nt->p, nt->offset + name_len, nt->offset + name_len, name,
			    &name_type);


      /* We just want the name, not the space padding behind it */

      for (; i <= (NETBIOS_NAME_LEN - 2) && name[i] != ' '; i++)
        ;
      name[i] = '\0';

      /* Many packages will be straight TCP packages directed to the proper
       * port which first byte happens to be SESSION_REQUEST. In those cases
       * the name will be illegal, and we will not add it */
      if (strcmp (name, "Illegal"))
	{
	  numeric_name =
	    g_strdup_printf ("%s %s (%s)", name, name + NETBIOS_NAME_LEN - 1,
			     get_netbios_host_type (name_type));

	  add_name (numeric_name, name, TRUE, &nt->node_id, nt);
	  g_free (numeric_name);
	}

    }

  nt->offset += length;

  decode_next(nt);
}				/* get_nbss_name */

static void
get_nbdgm_name (name_add_t *nt)
{
  guint8 mesg_type;
  gchar *numeric_name = NULL;
  gchar name[(NETBIOS_NAME_LEN - 1) * 4 + MAXDNAME];
  gboolean name_found = FALSE;
  int name_type;
  int len;
  guint i = 0;

  mesg_type = *(guint8 *) (nt->p + nt->offset);

  nt->offset += 10;

  /* Magic numbers copied from ethereal, as usual
   * They mean Direct (unique|group|broadcast) datagram */
  if (mesg_type == 0x10 || mesg_type == 0x11 || mesg_type == 0x12)
    {
      nt->offset += 4;
      len = ethereal_nbns_name ((const gchar *)nt->p, nt->offset, nt->offset, name, &name_type);

      if (nt->dir == INBOUND)
	ethereal_nbns_name ((const gchar *)nt->p, nt->offset + len, nt->offset + len, name, &name_type);
      name_found = TRUE;

    }
  else if (mesg_type == 0x14 || mesg_type == 0x15 || mesg_type == 0x16)
    {
      if (nt->dir == INBOUND)
	{
	  len = ethereal_nbns_name ((const gchar *)nt->p, nt->offset, nt->offset, name, &name_type);
	  name_found = TRUE;
	}
    }

  /* We just want the name, not the space padding behind it */

  for (; i <= (NETBIOS_NAME_LEN - 2) && name[i] != ' '; i++)
    ;
  name[i] = '\0';

  /* The reasing here might not make sense as in the TCP case, but it
   * doesn't hurt to check that we don't have an illegal name anyhow */
  if (strcmp (name, "Illegal") && name_found)
    {
      numeric_name =
	g_strdup_printf ("%s %s (%s)", name, name + NETBIOS_NAME_LEN - 1,
			 get_netbios_host_type (name_type));

      add_name (numeric_name, name, TRUE, &nt->node_id, nt);
      g_free (numeric_name);
    }


}				/* get_nbdgm_name */



static void
add_name (gchar * numeric_name, gchar * resolved_name, gboolean solved, 
          const node_id_t *node_id, const name_add_t *nt)
{
  protocol_t *protocol = NULL;
  GList *name_item = NULL;
  name_t *name = NULL;
  name_t key;

  /* Find the protocol entry */
  protocol = protocol_stack_find(nt->decoder.protos,
                                 nt->decoder.level,
			         nt->decoder.tokens[nt->decoder.level]);

  key.node_id = *node_id;
  
  /* Have we heard this address before? */
  name_item = g_list_find_custom (protocol->node_names, &key, (GCompareFunc)node_name_id_compare);
  if (name_item)
    name = name_item->data;
  else
    {
      /* First time name */
      name = node_name_create(node_id);
      protocol->node_names = g_list_prepend (protocol->node_names, name);
    }

  if (!pref.name_res)
    node_name_assign(name, numeric_name, numeric_name, FALSE, nt->packet_size);
  else
    node_name_assign(name, resolved_name, numeric_name, solved, nt->packet_size);
}				/* add_name */
