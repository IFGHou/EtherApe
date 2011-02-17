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

#include <netinet/in.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include "globals.h"
#include "names.h"
#include "dns.h"
#include "eth_resolv.h"
#include "names_netbios.h"
#include "protocols.h"
#include "datastructs.h"
#include "util.h"

typedef struct
{
  const guint8 *p;
  guint16 offset;
  guint16 packet_size;
  packet_direction dir;
  int link_type;
  node_id_t node_id;    /* topmost node_id: if a decoder can't fill it, it will
                         * use the lower level info */
  struct
  {
    guint8 level;       /* current decoder level */
    const packet_protos_t *tokens;     /* array of decoder names */
    protostack_t *protos;  /* protocol list */
  } decoder;
}
name_add_t;

typedef gboolean (p_func_t) (name_add_t *);

typedef struct
{
  gchar *prot;
  p_func_t *function;
}
prot_function_t;

static gboolean get_null_name (name_add_t *nt);
static gboolean get_linux_sll_name (name_add_t *nt);
static gboolean get_link6_name (name_add_t *nt);
static gboolean get_llc_name (name_add_t *nt);
static gboolean get_arp_name (name_add_t *nt);
static gboolean get_ip_name (name_add_t *nt);
static gboolean get_ipv6_name (name_add_t *nt);
static gboolean get_ipx_name (name_add_t *nt);
static gboolean get_udp_name (name_add_t *nt);
static gboolean get_tcp_name (name_add_t *nt);
static gboolean get_ipxsap_name (name_add_t *nt);
static gboolean get_nbss_name (name_add_t *nt);
static gboolean get_nbdgm_name (name_add_t *nt);

/* not all protocol types can be useful to get a name */
static prot_function_t prot_functions_table[] = {
  {"ETH_II", get_link6_name},
  {"802.2", get_link6_name},
  {"802.3", get_link6_name},
  {"ISL", get_link6_name},
  {"NULL", get_null_name},
  {"LINUX-SLL", get_linux_sll_name},
  {"FDDI", get_link6_name},
  {"Token Ring", get_link6_name},
  {"LLC", get_llc_name},
  {"ARP", get_arp_name},
  {"IP", get_ip_name},
  {"IPV6", get_ipv6_name},
  {"IPX", get_ipx_name},
  {"TCP", get_tcp_name},
  {"UDP", get_udp_name},
  {"IPX-SAP", get_ipxsap_name},
  {"NETBIOS-SSN", get_nbss_name},
  {"NETBIOS-DGM", get_nbdgm_name},

  /* terminator entry, must be last */
  {NULL, NULL}
};

static void missing_data_msg(const name_add_t *nt, const char *pr)
{
  if (pr)
    g_warning
    (_("not enough captured data, terminating protocol decode for '%s' (level %d)"),
    pr, nt->decoder.level);
  else
    g_warning
    (_("not enough captured data, terminating protocol decode at level %d"),
    nt->decoder.level);
    
}

static void add_name (const gchar * numeric, const gchar * resolved, 
                      const node_id_t *node_id, 
                      const name_add_t *nt);
static void decode_next(name_add_t *nt);

void
get_packet_names (protostack_t *pstk,
		  const guint8 * packet,
		  guint16 size,
		  const packet_protos_t * prot_stack, 
                  packet_direction direction,
                  int link_type)
{
  name_add_t nt;

  g_assert (pstk != NULL);
  g_assert (packet != NULL);

  nt.p = packet;
  nt.packet_size = size;
  nt.dir = direction;
  nt.offset = 0;
  nt.link_type = link_type;
  
  /* initializes decoders info 
   * Note: Level 0 means topmost - first usable is 1 
   */
  nt.decoder.level = 1;
  nt.decoder.tokens = prot_stack;
  nt.decoder.protos = pstk;

  /* starts decoding */
  decode_next(&nt);
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
      for (i = 0; prot_functions_table[i].prot != NULL ; ++i)
	g_tree_insert (prot_functions,
		       prot_functions_table[i].prot,
		       &(prot_functions_table[i]));
    }

  g_assert(nt);

  /* whe decode al most STACK_SIZE levels */
  while (nt->decoder.level <= STACK_SIZE && 
         nt->decoder.tokens->protonames[nt->decoder.level])
    {
      next_func = g_tree_lookup (prot_functions, nt->decoder.tokens->protonames[nt->decoder.level]);
      if (next_func)
        {
          /* before calling the next decoder, we check for size overflow */
          if (nt->packet_size <= nt->offset)
              return;
      
          if (! next_func->function (nt))
            break; /* can't advance further */
        }

      nt->decoder.level++;
    }
}

/* fills a node id with the specified mode and the data taken from nt using current
 * position (offset) + plus a displacement disp.
 * For tcp, it also uses the portdisp displacement as an offset for the port number.
 * Checks also the packet size against overflow
 */
static void fill_node_id(node_id_t *node_id, apemode_t apemode, const name_add_t *nt, 
                          int disp, int portdisp, int type)
{
  guint8 *dt = NULL;
  size_t sz = 0;

  memset(node_id, 0, sizeof(node_id_t));
  node_id->node_type = apemode; 
  
  switch (apemode)
  {
  case LINK6:
    dt = node_id->addr.eth;
    sz = sizeof(node_id->addr.eth);
    break;
  case IP:
    dt = node_id->addr.ip.addr8;
    sz = address_len(type);
    break;
  case TCP:
    dt = node_id->addr.tcp4.host.addr8;
    sz = address_len(type);
    break;
  default:
    g_error (_("Unsupported ape mode in fill_node_id"));
  }

  if (nt->packet_size < nt->offset + disp + sz ||
     nt->packet_size < nt->offset + portdisp + 2)
    {
      missing_data_msg(nt, NULL);
      return;
    }

  if (TCP == apemode)
  {
    node_id->addr.tcp4.host.type = type;
    g_memmove(dt, nt->p + nt->offset + disp, sz);
    node_id->addr.tcp4.port = ntohs (*(guint16 *) (nt->p + nt->offset + portdisp));
  }
  else
  {
    if (IP == apemode)
      node_id->addr.ip.type = type;
    g_memmove(dt, nt->p + nt->offset + disp, sz);
  }
}

/* Null is used for loopback. There is actually no information,
 * so we just jump to the next protocol */
/* TODO Are we so sure there is actually no information?
 * Then what are those four bytes? */
static gboolean get_null_name (name_add_t *nt)
{
  nt->offset += 4;
  return TRUE;
}				/* get_null_name */

/* linux-sll is used for ISDN on linux, I believe. 
 * Only one of the MAC addresses involved is shown each time,
 * so by now I will simply not try to decode MAC addresses */
/* TODO Do something useful with the address that shows */
static gboolean get_linux_sll_name (name_add_t *nt)
{
  /* TODO
   * I'm assuming that the header is always size 16. I don't know
   * weather that's always the case... (I'm making that assumption
   * since ethereal is not decoding a couple of bytes, which then
   * seem to be just padding */
  nt->offset += 16;
  return TRUE;
}				/* get_linux_sll_name */

/* common handling for ethernet-like data */
static gboolean eth_name_common(apemode_t ethmode, name_add_t *nt)
{
  const gchar *numeric, *solved;

  if (nt->dir == INBOUND)
    fill_node_id(&nt->node_id, ethmode, nt, ethmode, 0, 0);
  else
    fill_node_id(&nt->node_id, ethmode, nt, ethmode+6, 0, 0);

  numeric = ether_to_str (nt->node_id.addr.eth);

  /* solved is not NULL only if the address is in ethers file */
  if (pref.name_res)
    solved = get_ether_name (nt->node_id.addr.eth, TRUE);
  else
    solved = NULL;

  add_name (numeric, solved, &nt->node_id, nt);

  nt->offset += 14;
  return TRUE;
}

static gboolean get_link6_name(name_add_t *nt)
{
  return eth_name_common(LINK6, nt);
}

/* LLC is the only supported FDDI link layer type */
static gboolean get_llc_name (name_add_t *nt)
{
  /* TODO IMPORTANT
   * We must decode the llc header to calculate the nt->offset
   * We are just doing assumptions by now */

  if (nt->link_type == DLT_FDDI || nt->link_type == DLT_IEEE802)
    nt->offset += 8;
  else if (nt->link_type == DLT_EN10MB)
    nt->offset += 3;
  else
    return FALSE; /* no further info */
  return TRUE;
}				/* get_llc_name */

static gboolean get_arp_name (name_add_t *nt)
{
  guint16 protocol_type;
  guint8 hardware_len, protocol_len;
#define ARPTYPE_IP 0x0800

  /* We can only tell the IP address of the asking node.
   * Most of the times the callee will be the broadcast 
   * address */
  if (nt->dir == INBOUND)
    return FALSE;

  if (nt->packet_size <= nt->offset + 4)
    {
      missing_data_msg(nt, "ARP");
      return FALSE;
    }

  /* We only know about IP ARP queries */
  protocol_type = pntohs ((nt->p + nt->offset + 2));
  if (protocol_type != ARPTYPE_IP)
    return FALSE;

  if (nt->packet_size <= nt->offset + 7)
    {
      missing_data_msg(nt, "ARP");
      return FALSE;
    }

  hardware_len = *(guint8 *) (nt->p + nt->offset + 4);
  protocol_len = *(guint8 *) (nt->p + nt->offset + 5);

  fill_node_id(&nt->node_id, IP, nt, 8 + hardware_len, 0, AF_INET);

  add_name (ipv4_to_str (nt->node_id.addr.ip.addr_v4), 
            dns_lookup (&nt->node_id.addr.ip), 
            &nt->node_id, nt);

  /* ARP doesn't carry any other protocol on top, so we return 
   * directly */
  return FALSE;
}				/* get_arp_name */

static gboolean get_ip_name (name_add_t *nt)
{

  if (nt->dir == INBOUND)
    fill_node_id(&nt->node_id, IP, nt, 16, 0, AF_INET);
  else
    fill_node_id(&nt->node_id, IP, nt, 12, 0, AF_INET);

  if (!pref.name_res)
    add_name (ipv4_to_str (nt->node_id.addr.ip.addr_v4), 
              NULL, &nt->node_id, nt);
  else
    add_name (ipv4_to_str (nt->node_id.addr.ip.addr_v4), 
              dns_lookup (&nt->node_id.addr.ip), 
              &nt->node_id, nt);

  /* IPv4 header length can be variable */
  nt->offset += nt->offset < nt->packet_size ?
            (nt->p[nt->offset] & 15) << 2 : 0;
  return TRUE;
}				/* get_ip_name */

static gboolean get_ipv6_name (name_add_t *nt)
{

  if (nt->dir == INBOUND)
    fill_node_id(&nt->node_id, IP, nt, 24, 0, AF_INET6);
  else
    fill_node_id(&nt->node_id, IP, nt, 8, 0, AF_INET6);

  if (!pref.name_res)
    add_name (ipv6_to_str (nt->node_id.addr.ip.addr_v6), 
              NULL, &nt->node_id, nt);
  else
    add_name (ipv6_to_str (nt->node_id.addr.ip.addr_v6), 
              dns_lookup (&nt->node_id.addr.ip), 
              &nt->node_id, nt);

  /* IPv6 header length is always constant */
  nt->offset += 40;
  return TRUE;
}				/* get_ipv6_name */

static gboolean get_ipx_name (name_add_t *nt)
{
  nt->offset += 30;
  return TRUE;
}

static gboolean get_tcp_name (name_add_t *nt)
{
  guint8 th_off_x2;
  guint8 tcp_len;

  /* tcp names are useful only if someone uses them ... */
  if (pref.mode == TCP)
    {
      gchar *numeric_name, *resolved_name;
      int type = nt->node_id.addr.tcp4.host.type;
      int shift = type == AF_INET6 ? 2 : 0;
      
      if (nt->dir == OUTBOUND)
          fill_node_id(&nt->node_id, TCP, nt, -8<<shift, 0, type);
      else
          fill_node_id(&nt->node_id, TCP, nt, -4<<shift, 2, type);

      numeric_name = g_strdup_printf("%s:%d",
                      address_to_str(&nt->node_id.addr.tcp4.host),
                      nt->node_id.addr.tcp4.port);

      if (pref.name_res)
        {
          const gchar *dnsname;
          const port_service_t *port;
          dnsname = dns_lookup (&nt->node_id.addr.tcp4.host);
          port = services_tcp_find(nt->node_id.addr.tcp4.port);
          if (port)
            resolved_name = g_strdup_printf("%s:%s", dnsname, port->name);
          else
            resolved_name = g_strdup_printf("%s:%d", dnsname, 
                                            nt->node_id.addr.tcp4.port);
        }
      else
        resolved_name = NULL;

      add_name (numeric_name, resolved_name, &nt->node_id, nt);

      g_free (numeric_name);
      g_free (resolved_name);
    }
  
  if (nt->packet_size <= nt->offset + 14)
    {
      missing_data_msg(nt, "TCP");
      return FALSE;
    }

  th_off_x2 = *(guint8 *) (nt->p + nt->offset + 12);
  tcp_len = hi_nibble (th_off_x2) * 4;	/* TCP header length, in bytes */
  nt->offset += tcp_len;
  return TRUE;
}				/* get_tcp_name */


/* TODO I still have to properly implement this. Right now it's just
 * a placeholder to get to the UDP/NETBIOS-DGM */
static gboolean get_udp_name (name_add_t *nt)
{
  nt->offset += 8;
  return TRUE;
}				/* get_udp_name */


/* TODO SET UP THE id's FOR THIS NETBIOS NAME FUNCTIONS */
static gboolean get_ipxsap_name (name_add_t *nt)
{
  guint16 sap_type;
  guint16 curpos;
  gchar *name;

  if (nt->packet_size <= nt->offset + 2)
      return FALSE; /* not a real ipxsap packet */

  sap_type = pntohs (nt->p + nt->offset);

  /* we want responses */
  if (sap_type != 0x0002)
    return FALSE;

  for (curpos = nt->offset + 4; curpos < nt->packet_size ; ++curpos)
    {
      if ( ! *((gchar *) (nt->p + curpos)) )
        break; /* found termination */
    }
  if (curpos >= nt->packet_size)
    {
      missing_data_msg(nt, "IPXSAP");
      return FALSE;
    }
    
  name = (gchar *) (nt->p + nt->offset + 4);

  g_my_debug ("Sap name %s found", name);

  add_name (name, name, &nt->node_id, nt);

  return FALSE; /* no other names */
}				/* get_ipxsap_name */

static gboolean get_nbss_name (name_add_t *nt)
{
#define SESSION_REQUEST 0x81

  guint8 mesg_type;

  if (nt->packet_size < nt->offset + 1)
      return FALSE; /* not a netbios packet */

  mesg_type = *(guint8 *) (nt->p + nt->offset);
  nt->offset += 2;

  if (mesg_type == SESSION_REQUEST)
    {
      guint i = 0;
      guint16 length;
      gchar *numeric_name = NULL;
      gchar name[(NETBIOS_NAME_LEN - 1) * 4 + MAXDNAME];
      guint name_len;
      int name_type;		/* TODO I hate to use an int here, while I have been
                                     * using glib data types all the time. But I just don't
                                     * know what it might mean in the ethereal implementation */
      if (nt->packet_size <= nt->offset + 2)
        {
          missing_data_msg(nt, "NBSS");
          return FALSE;
        }
      length = pntohs ((nt->p + nt->offset + 2));

      nt->offset += 2;
      if (nt->packet_size <= nt->offset + length)
        {
          missing_data_msg(nt, "NBSS");
          return FALSE;
        }

      name_len = ethereal_nbns_name ((const gchar *)nt->p, nt->offset, nt->packet_size, name, sizeof(name), &name_type);
      if (nt->dir == OUTBOUND)
	ethereal_nbns_name ((const gchar *)nt->p, nt->offset + name_len, 
                            nt->packet_size, name, sizeof(name), &name_type);

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

	  add_name (numeric_name, name, &nt->node_id, nt);
	  g_free (numeric_name);
	}

      nt->offset += length;
    }

  return TRUE;
}				/* get_nbss_name */

static gboolean get_nbdgm_name (name_add_t *nt)
{
  guint8 mesg_type;
  gchar *numeric_name = NULL;
  gchar name[(NETBIOS_NAME_LEN - 1) * 4 + MAXDNAME];
  gboolean name_found = FALSE;
  int name_type;
  int len;
  guint i = 0;

  if (nt->packet_size < nt->offset + 1)
    return FALSE; /* not a real nbgdm packet */

  mesg_type = *(guint8 *) (nt->p + nt->offset);

  nt->offset += 10;

  /* Magic numbers copied from ethereal, as usual
   * They mean Direct (unique|group|broadcast) datagram */
  if (mesg_type == 0x10 || mesg_type == 0x11 || mesg_type == 0x12)
    {
      nt->offset += 4;
      len = ethereal_nbns_name ((const gchar *)nt->p, nt->offset, nt->packet_size, name, sizeof(name), &name_type);

      if (nt->dir == INBOUND)
	ethereal_nbns_name ((const gchar *)nt->p, nt->offset + len, nt->packet_size, name, sizeof(name), &name_type);
      name_found = TRUE;

    }
  else if (mesg_type == 0x14 || mesg_type == 0x15 || mesg_type == 0x16)
    {
      if (nt->dir == INBOUND)
	{
	  len = ethereal_nbns_name ((const gchar *)nt->p, nt->offset, nt->packet_size, name, sizeof(name), &name_type);
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

      add_name (numeric_name, name, &nt->node_id, nt);
      g_free (numeric_name);
    }
  return FALSE; /* no other names */
}				/* get_nbdgm_name */


static void
add_name (const gchar * numeric_name, const gchar * resolved_name, 
          const node_id_t *node_id, const name_add_t *nt)
{
  protocol_t *protocol = NULL;
  GList *name_item = NULL;
  name_t *name = NULL;
  name_t key;

  /* Find the protocol entry
     note: protocol_stack_find returns a const ptr, but this function
     modifies it. At this point, is safe
   */
  protocol = (protocol_t *)protocol_stack_find(nt->decoder.protos,
                                 nt->decoder.level,
			         nt->decoder.tokens->protonames[nt->decoder.level]);

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
    node_name_assign(name, NULL, numeric_name, nt->packet_size);
  else
    node_name_assign(name, resolved_name, numeric_name, nt->packet_size);
}				/* add_name */
