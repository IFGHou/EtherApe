/* EtherApe
 * Copyright (C) 2001 Juan Toledo
 * $Id$
 *
 * This file is mostly a rehash of algorithms found in
 * packet-*. of ethereal
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
#include "protocols.h"
#include <ctype.h>
#include <string.h>

static GString *prot;
static const guint8 *packet;
guint capture_len = 0;

gchar *
get_packet_prot (const guint8 * p, guint len)
{
  gchar **tokens = NULL;
  gchar *top_prot = NULL;
  gchar *str = NULL;

  guint i = 0;

  g_assert (p != NULL);

  if (prot)
    {
      g_string_free (prot, TRUE);
      prot = NULL;
    }
  prot = g_string_new ("");

  packet = p;
  capture_len = len;

  switch (linktype)
    {
    case L_EN10MB:
      get_eth_type ();
      break;
    case L_FDDI:
      prot = g_string_new ("FDDI");
      get_fddi_type ();
      break;
    case L_IEEE802:
      prot = g_string_append (prot, "Token Ring");
      get_ieee802_type ();
      break;
    case L_RAW:		/* Both for PPP and SLIP */
      prot = g_string_append (prot, "RAW/IP");
      get_ip ();
      break;
    case L_NULL:
      prot = g_string_append (prot, "NULL/IP");
      get_ip ();
      break;
    default:
      break;
    }

  /* TODO I think this is a serious waste of CPU and memory.
     * I think I should look at other ways of dealing with not recognized
     * protocols other than just creating entries saying UNKNOWN */
  /* I think I'll do this by changing the global protocols into
   * a GArray */

  if (prot)
    {
      tokens = g_strsplit (prot->str, "/", 0);
      while ((tokens[i] != NULL) && i <= STACK_SIZE)
	i++;
      g_strfreev (tokens);
      tokens = NULL;
    }
  for (; i <= STACK_SIZE; i++)
    prot = g_string_append (prot, "/UNKNOWN");

  tokens = g_strsplit (prot->str, "/", 0);
  for (i = 0; (i <= STACK_SIZE) && strcmp (tokens[i], "UNKNOWN"); i++)
    top_prot = tokens[i];

  g_assert (top_prot != NULL);
  str = g_strdup_printf ("%s/", top_prot);
  prot = g_string_prepend (prot, str);
  g_free (str);
  str = NULL;
  g_strfreev (tokens);
  tokens = NULL;

  /* g_message ("Protocol stack is %s", prot->str); */
  return prot->str;
}				/* get_packet_prot */


static void
get_eth_type (void)
{
  etype_t etype;
  ethhdrtype_t ethhdr_type = ETHERNET_II;	/* Default */

  etype = pntohs (&packet[12]);


  if (etype <= IEEE_802_3_MAX_LEN)
    {

      /* Is there an 802.2 layer? I can tell by looking at the first 2
       *        bytes after the 802.3 header. If they are 0xffff, then what
       *        follows the 802.3 header is an IPX payload, meaning no 802.2.
       *        (IPX/SPX is they only thing that can be contained inside a
       *        straight 802.3 packet). A non-0xffff value means that there's an
       *        802.2 layer inside the 802.3 layer */
      if (packet[14] == 0xff && packet[15] == 0xff)
	{
	  ethhdr_type = ETHERNET_802_3;
	}
      else
	{
	  ethhdr_type = ETHERNET_802_2;
	}

      /* Oh, yuck.  Cisco ISL frames require special interpretation of the
       *        destination address field; fortunately, they can be recognized by
       *        checking the first 5 octets of the destination address, which are
       *        01-00-0C-00-00 for ISL frames. */
      if (packet[0] == 0x01 && packet[1] == 0x00 && packet[2] == 0x0C
	  && packet[3] == 0x00 && packet[4] == 0x00)
	{
	  /* TODO Analyze ISL frames */
	  prot = g_string_append (prot, "ISL");
	  return;
	}
    }

  if (ethhdr_type == ETHERNET_802_3)
    {
      prot = g_string_append (prot, "802.3");
      return;
    }

  if (ethhdr_type == ETHERNET_802_2)
    {
      prot = g_string_append (prot, "802.2");
      return;
    }

  /* Else, it's ETHERNET_II */

  prot = g_string_append (prot, "ETH_II");
  get_eth_II (etype);
  return;

}				/* get_eth_type */

static void
get_fddi_type (void)
{
  prot = g_string_append (prot, "/LLC");
  /* Ok, this is only temporary while I truly dissect LLC 
   * and fddi */
  if ((packet[19] == 0x08) && (packet[20] == 0x00))
    {
      prot = g_string_append (prot, "/IP");
      get_ip ();
    }

}				/* get_fddi_type */

static void
get_ieee802_type (void)
{
  /* As with FDDI, we only support LLC by now */
  prot = g_string_append (prot, "/LLC");

  if ((packet[20] == 0x08) && (packet[21] == 0x00))
    {
      prot = g_string_append (prot, "/IP");
      get_ip ();
    }

}

static void
get_eth_II (etype_t etype)
{
  switch (etype)
    {
    case ETHERTYPE_IP:
      prot = g_string_append (prot, "/IP");
      get_ip ();
      break;
    case ETHERTYPE_ARP:
      prot = g_string_append (prot, "/ARP");
      break;
    case ETHERTYPE_IPv6:
      prot = g_string_append (prot, "/IPv6");
      break;
    case ETHERTYPE_X25L3:
      prot = g_string_append (prot, "/X25L3");
      break;
    case ETHERTYPE_REVARP:
      prot = g_string_append (prot, "/REVARP");
      break;
    case ETHERTYPE_ATALK:
      prot = g_string_append (prot, "/ATALK");
      break;
    case ETHERTYPE_AARP:
      prot = g_string_append (prot, "/AARP");
      break;
    case ETHERTYPE_IPX:
      prot = g_string_append (prot, "/IPX");
      break;
    case ETHERTYPE_VINES:
      prot = g_string_append (prot, "/VINES");
      break;
    case ETHERTYPE_TRAIN:
      prot = g_string_append (prot, "/TRAIN");
      break;
    case ETHERTYPE_LOOP:
      prot = g_string_append (prot, "/LOOP");
      break;
    case ETHERTYPE_PPPOED:
      prot = g_string_append (prot, "/PPPOED");
      break;
    case ETHERTYPE_PPPOES:
      prot = g_string_append (prot, "/PPPOES");
      break;
    case ETHERTYPE_VLAN:
      prot = g_string_append (prot, "/VLAN");
      break;
    case ETHERTYPE_SNMP:
      prot = g_string_append (prot, "/SNMP");
      break;
    case ETHERTYPE_UNK:
    default:
      prot = g_string_append (prot, "/ETH_UNKNOWN");
    }

  return;
}				/* get_eth_II */

static void
get_ip (void)
{
  guint16 fragment_offset;
  iptype_t ip_type;
  ip_type = packet[l3_offset + 9];
  fragment_offset = pntohs (packet + l3_offset + 6);
  fragment_offset &= 0x0fff;
  offset = l3_offset + 20;

  switch (ip_type)
    {
    case IP_PROTO_ICMP:
      prot = g_string_append (prot, "/ICMP");
      break;
    case IP_PROTO_TCP:
      if (fragment_offset)
	prot = g_string_append (prot, "/TCP_FRAGMENT");
      else
	{
	  prot = g_string_append (prot, "/TCP");
	  get_tcp ();
	}
      break;
    case IP_PROTO_UDP:
      if (fragment_offset)
	prot = g_string_append (prot, "/UDP_FRAGMENT");
      else
	{
	  prot = g_string_append (prot, "/UDP");
	  get_udp ();
	}
      break;
    case IP_PROTO_IGMP:
      prot = g_string_append (prot, "/IGMP");
      break;
    case IP_PROTO_GGP:
      prot = g_string_append (prot, "/GGP");
      break;
    case IP_PROTO_IPIP:
      prot = g_string_append (prot, "/IPIP");
      break;
#if 0				/* TODO How come IPIP and IPV4 share the same number? */
    case IP_PROTO_IPV4:
      prot = g_string_append (prot, "/IPV4");
      break;
#endif
    case IP_PROTO_EGP:
      prot = g_string_append (prot, "/EGP");
      break;
    case IP_PROTO_PUP:
      prot = g_string_append (prot, "/PUP");
      break;
    case IP_PROTO_IDP:
      prot = g_string_append (prot, "/IDP");
      break;
    case IP_PROTO_TP:
      prot = g_string_append (prot, "/TP");
      break;
    case IP_PROTO_IPV6:
      prot = g_string_append (prot, "/IPV6");
      break;
    case IP_PROTO_ROUTING:
      prot = g_string_append (prot, "/ROUTING");
      break;
    case IP_PROTO_FRAGMENT:
      prot = g_string_append (prot, "/FRAGMENT");
      break;
    case IP_PROTO_RSVP:
      prot = g_string_append (prot, "/RSVP");
      break;
    case IP_PROTO_GRE:
      prot = g_string_append (prot, "/GRE");
      break;
    case IP_PROTO_ESP:
      prot = g_string_append (prot, "/ESP");
      break;
    case IP_PROTO_AH:
      prot = g_string_append (prot, "/AH");
      break;
    case IP_PROTO_ICMPV6:
      prot = g_string_append (prot, "/ICPMPV6");
      break;
    case IP_PROTO_NONE:
      prot = g_string_append (prot, "/NONE");
      break;
    case IP_PROTO_DSTOPTS:
      prot = g_string_append (prot, "/DSTOPTS");
      break;
    case IP_PROTO_VINES:
      prot = g_string_append (prot, "/VINES");
      break;
    case IP_PROTO_EIGRP:
      prot = g_string_append (prot, "/EIGRP");
      break;
    case IP_PROTO_OSPF:
      prot = g_string_append (prot, "/OSPF");
      break;
    case IP_PROTO_ENCAP:
      prot = g_string_append (prot, "/ENCAP");
      break;
    case IP_PROTO_PIM:
      prot = g_string_append (prot, "/PIM");
      break;
    case IP_PROTO_IPCOMP:
      prot = g_string_append (prot, "/IPCOMP");
      break;
    case IP_PROTO_VRRP:
      prot = g_string_append (prot, "/VRRP");
      break;
    default:
      prot = g_string_append (prot, "/IP_UNKNOWN");
    }

  return;
}

static void
get_tcp (void)
{

  tcp_service_t *src_service, *dst_service, *chosen_service;
  tcp_type_t src_port, dst_port;
  guint8 th_off_x2;
  guint8 tcp_len;
  gchar *str;

  if (!tcp_services)
    load_services ();

  src_port = pntohs (packet + offset);
  dst_port = pntohs (packet + offset + 2);
  th_off_x2 = *(guint8 *) (packet + offset + 12);
  tcp_len = hi_nibble (th_off_x2) * 4;	/* TCP header length, in bytes */

  offset += tcp_len;

  if (offset >= capture_len)
    return;			/* Continue only if there is room
				 * for data in the packet */

  src_service = g_tree_lookup (tcp_services, &src_port);
  dst_service = g_tree_lookup (tcp_services, &dst_port);

  if (IS_PORT (TCP_NETBIOS_SSN))
    {
      get_netbios_ssn ();
      return;
    }

  if (!src_service && !dst_service)
    {
      prot = g_string_append (prot, "/TCP_UNKNOWN");
      return;
    }

  /* Give priority to registered ports */
  if ((src_port < 1024) && (dst_port >= 1024))
    chosen_service = src_service;
  else if ((src_port >= 1024) && (dst_port < 1024))
    chosen_service = dst_service;
  else if (!dst_service)	/* Give priority to dst_service just because */
    chosen_service = src_service;
  else
    chosen_service = dst_service;

  str = g_strdup_printf ("/%s", chosen_service->name);
  prot = g_string_append (prot, str);
  g_free (str);
  str = NULL;
  return;
}				/* get_tcp */

static void
get_udp (void)
{
  udp_service_t *src_service, *dst_service, *chosen_service;
  udp_type_t src_port, dst_port;
  gchar *str;

  if (!udp_services)
    load_services ();

  src_port = pntohs (packet + offset);
  dst_port = pntohs (packet + offset + 2);

  offset += 8;

  /* TODO We should check up the size of the packet the same
   * way it is done in TCP */

  src_service = g_tree_lookup (udp_services, &src_port);
  dst_service = g_tree_lookup (udp_services, &dst_port);

  /* It's not possible to know in advance whether an UDP
   * packet is an RPC packet. We'll try */
  if (get_rpc ())
    return;

  if (IS_PORT (UDP_NETBIOS_NS))
    {
      get_netbios_dgm ();
      return;
    }

  if (!dst_service && !src_service)
    {
      prot = g_string_append (prot, "/UDP_UNKNOWN");
      return;
    }

  /* Give priority to registered ports */
  if ((src_port < 1024) && (dst_port >= 1024))
    chosen_service = src_service;
  else if ((src_port >= 1024) && (dst_port < 1024))
    chosen_service = dst_service;
  else if (!dst_service)	/* Give priority to dst_service just because */
    chosen_service = src_service;
  else
    chosen_service = dst_service;

  str = g_strdup_printf ("/%s", chosen_service->name);
  prot = g_string_append (prot, str);
  g_free (str);
  str = NULL;
  return;
}				/* get_udp */

static gboolean
get_rpc (void)
{
  enum rpc_type msg_type;
  enum rpc_program msg_program;

  /* Determine whether this is an RPC packet */

  if ((offset + 24) > capture_len)
    return FALSE;		/* not big enough */

  msg_type = pntohl (packet + offset + 4);
  if (msg_type != RPC_REPLY && msg_type != RPC_CALL)
    return FALSE;

  prot = g_string_append (prot, "/RPC");

  switch (msg_type)
    {
    case RPC_REPLY:
      /* RPC_REPLYs don't carry an rpc program tag */
      /* TODO In order to be able to dissect what is it's 
       * protocol I'd have to keep track of who sent
       * which call */
      return TRUE;
    case RPC_CALL:
      msg_program = pntohl (packet + offset + 12);
      switch (msg_program)
	{
	case BOOTPARAMS_PROGRAM:
	  prot = g_string_append (prot, "/BOOTPARAMS");
	  break;
	case MOUNT_PROGRAM:
	  prot = g_string_append (prot, "/MOUNT");
	  break;
	case NLM_PROGRAM:
	  prot = g_string_append (prot, "/NLM");
	  break;
	case PORTMAP_PROGRAM:
	  prot = g_string_append (prot, "/PORTMAP");
	  break;
	case STAT_PROGRAM:
	  prot = g_string_append (prot, "/STAT");
	  break;
	case NFS_PROGRAM:
	  prot = g_string_append (prot, "/NFS");
	  break;
	case YPBIND_PROGRAM:
	  prot = g_string_append (prot, "/YPBIND");
	  break;
	case YPSERV_PROGRAM:
	  prot = g_string_append (prot, "/YPSERV");
	  break;
	case YPXFR_PROGRAM:
	  prot = g_string_append (prot, "/YPXFR");
	  break;
	default:
	  return FALSE;
	}
      return TRUE;
    default:
      return FALSE;
    }
  return FALSE;
}				/* get_rpc */

void
get_netbios_ssn (void)
{
#define SESSION_MESSAGE 0
  guint8 mesg_type;

  prot = g_string_append (prot, "/NETBIOS-SSN");

  mesg_type = *(guint8 *) (packet + offset);

  if (mesg_type == SESSION_MESSAGE)
    prot = g_string_append (prot, "/SMB");

  /* TODO Calculate new offset whenever we have
   * a "dissector" for a higher protocol */
  return;
}				/* get_netbions_ssn */

void
get_netbios_dgm (void)
{
  guint8 mesg_type;

  prot = g_string_append (prot, "/NETBIOS-DGM");

  mesg_type = *(guint8 *) (packet + offset);

  /* Magic numbers copied from ethereal, as usual
   * They mean Direct (unique|group|broadcast) datagram */
  if (mesg_type == 0x10 || mesg_type == 0x11 || mesg_type == 0x12)
    prot = g_string_append (prot, "/SMB");

  /* TODO Calculate new offset whenever we have
   * a "dissector" for a higher protocol */
  return;
}

/* Comparison function to sort tcp services port number */
static gint
tcp_compare (gconstpointer a, gconstpointer b)
{
  tcp_type_t port_a, port_b;

  port_a = *(tcp_type_t *) a;
  port_b = *(tcp_type_t *) b;

  if (port_a > port_b)
    return 1;
  if (port_a < port_b)
    return -1;
  return 0;
}				/* tcp_compare */

/* Comparison function to sort udp services port number */
static gint
udp_compare (gconstpointer a, gconstpointer b)
{
  udp_type_t port_a, port_b;

  port_a = *(tcp_type_t *) a;
  port_b = *(tcp_type_t *) b;

  if (port_a > port_b)
    return 1;
  if (port_a < port_b)
    return -1;
  return 0;
}				/* udp_compare */


/* TODO this is probably this single piece of code I am most ashamed of.
 * I should learn how to use flex or yacc and do this The Right Way (TM)*/
static void
load_services (void)
{
  FILE *services = NULL;
  gchar *line;
  gchar **t1 = NULL, **t2 = NULL;
  gchar *str;
  tcp_service_t *tcp_service;
  udp_service_t *udp_service;
  guint i;
  char filename[PATH_MAX];

  tcp_type_t port_number;	/* udp and tcp are the same */

  strcpy (filename, CONFDIR "/services");
  if (!(services = fopen (filename, "r")))
    {
      strcpy (filename, "/etc/services");
      if (!(services = fopen (filename, "r")))
	{
	  g_my_critical (_
			 ("Failed to open %s. No TCP or UDP services will be recognized"),
			 filename);
	  return;
	}
    }

  g_my_info (_("Reading TCP and UDP services from %s"), filename);

  tcp_services = g_tree_new ((GCompareFunc) tcp_compare);
  udp_services = g_tree_new ((GCompareFunc) udp_compare);

  line = g_malloc (LINESIZE);

  while (fgets (line, LINESIZE, services))
    {
      if (line[0] != '#' && line[0] != ' ' && line[0] != '\n'
	  && line[0] != '\t')
	{
	  gboolean error = FALSE;

	  if (!g_strdelimit (line, " \t\n", ' '))
	    error = TRUE;

	  if (error || !(t1 = g_strsplit (line, " ", 0)))
	    error = TRUE;
	  if (!error && t1[0])
	    g_strup (t1[0]);

	  for (i = 1; t1[i] && !strcmp ("", t1[i]); i++);


	  if (!error && (str = t1[i]))
	    if (!(t2 = g_strsplit (str, "/", 0)))
	      error = TRUE;

	  if (error || !t2[0])
	    error = TRUE;

	  /* TODO The h here is not portable */
	  if (!sscanf (t2[0], "%hd", &port_number) || (port_number < 1))
	    error = TRUE;

	  if (error || !t2[1])
	    error = TRUE;

	  if (error
	      || (g_strcasecmp ("udp", t2[1]) && g_strcasecmp ("tcp", t2[1])
		  && g_strcasecmp ("ddp", t2[1])))
	    error = TRUE;

	  if (error)
	    g_warning ("Unable to  parse line %s", line);
	  else
	    {
	      g_my_debug ("Loading service %s %s %d", t2[1], t1[0],
			  port_number);
	      if (!g_strcasecmp ("tcp", t2[1]))
		{
		  tcp_service = g_malloc (sizeof (tcp_service_t));
		  tcp_service->number = port_number;
		  tcp_service->name = g_strdup (t1[0]);
		  g_tree_insert (tcp_services,
				 &(tcp_service->number), tcp_service);
		}
	      else if (!g_strcasecmp ("udp", t2[1]))
		{
		  udp_service = g_malloc (sizeof (udp_service_t));
		  udp_service->number = port_number;
		  udp_service->name = g_strdup (t1[0]);
		  g_tree_insert (udp_services,
				 &(udp_service->number), udp_service);
		}
	      else
		g_my_info ("DDP protocols not supported in %s", line);

	    }

	  g_strfreev (t2);
	  t2 = NULL;
	  g_strfreev (t1);
	  t1 = NULL;

	}
    }

  g_free (line);
  line = NULL;
  fclose (services);
}				/* load_services */
