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
#ifdef L_LINUX_SLL
    case L_LINUX_SLL:
      prot = g_string_append (prot, "LINUX-SLL");
      get_linux_sll_type ();
      break;
#endif
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
      prot = g_string_append (prot, "802.3-RAW");
      return;
    }

  if (ethhdr_type == ETHERNET_802_2)
    {
      prot = g_string_append (prot, "802.3");
      get_eth_802_3 (ethhdr_type);
      return;
    }

  /* Else, it's ETHERNET_II */

  prot = g_string_append (prot, "ETH_II");
  get_eth_II (etype);
  return;

}				/* get_eth_type */

static void
get_eth_802_3 (ethhdrtype_t ethhdr_type)
{

  offset = 14;

  switch (ethhdr_type)
    {
    case ETHERNET_802_2:
      prot = g_string_append (prot, "/LLC");
      get_llc ();
      break;
    case ETHERNET_802_3:
      get_ipx ();
      break;
    default:
    }
}				/* get_eth_802_3 */

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
  append_etype_prot (etype);

  if (etype == ETHERTYPE_IP)
    get_ip ();
  if (etype == ETHERTYPE_IPX)
    get_ipx ();

  return;
}				/* get_eth_II */

/* Gets the protocol type out of the linux-sll header.
 * I have no real idea of what can be there, but since IP
 * is 0x800 I guess it follows ethernet specifications */
static void
get_linux_sll_type (void)
{
  etype_t etype;

  etype = pntohs (&packet[14]);
  append_etype_prot (etype);

  if (etype == ETHERTYPE_IP)
    get_ip ();
  if (etype == ETHERTYPE_IPX)
    get_ipx ();

  return;
}				/* get_linux_sll_type */

static void
get_llc (void)
{
#define SAP_SNAP 0xAA
#define XDLC_I          0x00	/* Information frames */
#define XDLC_U          0x03	/* Unnumbered frames */
#define XDLC_UI         0x00	/* Unnumbered Information */
#define XDLC_IS_INFORMATION(control) \
     (((control) & 0x1) == XDLC_I || (control) == (XDLC_UI|XDLC_U))

  sap_type_t dsap, ssap;
  gboolean is_snap;
  guint16 control;

  dsap = *(guint8 *) (packet + offset);
  ssap = *(guint8 *) (packet + offset + 1);

  is_snap = (dsap == SAP_SNAP) && (ssap == SAP_SNAP);

  /* SNAP not yet supported */
  if (is_snap)
    return;

  /* To get this control value is actually a lot more
   * complicated than this, see xdlc.c in ethereal,
   * but I'll try like this, it seems it works for my pourposes at
   * least most of the time */
  control = *(guint8 *) (packet + offset + 2);

  if (!XDLC_IS_INFORMATION (control))
    return;

  offset += 3;

  switch (dsap)
    {
    case SAP_NULL:
      prot = g_string_append (prot, "/LLC-NULL");
      break;
    case SAP_LLC_SLMGMT:
      prot = g_string_append (prot, "/LLC-SLMGMT");
      break;
    case SAP_SNA_PATHCTRL:
      prot = g_string_append (prot, "/PATHCTRL");
      break;
    case SAP_IP:
      prot = g_string_append (prot, "/IP");
      break;
    case SAP_SNA1:
      prot = g_string_append (prot, "/SNA1");
      break;
    case SAP_SNA2:
      prot = g_string_append (prot, "/SNA2");
      break;
    case SAP_PROWAY_NM_INIT:
      prot = g_string_append (prot, "/PROWAY-NM-INIT");
      break;
    case SAP_TI:
      prot = g_string_append (prot, "/TI");
      break;
    case SAP_BPDU:
      prot = g_string_append (prot, "/BPDU");
      break;
    case SAP_RS511:
      prot = g_string_append (prot, "/RS511");
      break;
    case SAP_X25:
      prot = g_string_append (prot, "/X25");
      break;
    case SAP_XNS:
      prot = g_string_append (prot, "/XNS");
      break;
    case SAP_NESTAR:
      prot = g_string_append (prot, "/NESTAR");
      break;
    case SAP_PROWAY_ASLM:
      prot = g_string_append (prot, "/PROWAY-ASLM");
      break;
    case SAP_ARP:
      prot = g_string_append (prot, "/ARP");
      break;
    case SAP_SNAP:
      /* We are not supposed to reach this point */
      g_warning ("Reached SNAP while checking for DSAP in get_llc");
      prot = g_string_append (prot, "/LLC-SNAP");
      break;
    case SAP_VINES1:
      prot = g_string_append (prot, "/VINES1");
      break;
    case SAP_VINES2:
      prot = g_string_append (prot, "/VINES2");
      break;
    case SAP_NETWARE:
      get_ipx ();
      break;
    case SAP_NETBIOS:
      prot = g_string_append (prot, "/NETBIOS");
      get_netbios ();
      break;
    case SAP_IBMNM:
      prot = g_string_append (prot, "/IBMNM");
      break;
    case SAP_RPL1:
      prot = g_string_append (prot, "/RPL1");
      break;
    case SAP_UB:
      prot = g_string_append (prot, "/UB");
      break;
    case SAP_RPL2:
      prot = g_string_append (prot, "/RPL2");
      break;
    case SAP_OSINL:
      prot = g_string_append (prot, "/OSINL");
      break;
    case SAP_GLOBAL:
      prot = g_string_append (prot, "/LLC-GLOBAL");
      break;
    }

}				/* get_llc */

static void
get_ip (void)
{
  guint16 fragment_offset;
  iptype_t ip_type;
  ip_type = packet[l3_offset + 9];
  fragment_offset = pntohs (packet + l3_offset + 6);
  fragment_offset &= 0x0fff;

  /*This is used for conversations */
  global_src_address = pntohl (packet + l3_offset + 12);
  global_dst_address = pntohl (packet + l3_offset + 16);

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
get_ipx ()
{
  ipx_socket_t ipx_dsocket, ipx_ssocket;
  guint16 ipx_length;
  ipx_type_t ipx_type;

  /* Make sure this is an IPX packet */
  if ((offset + 30 > capture_len) || *(guint16 *) (packet + offset) != 0xffff)
    return;

  prot = g_string_append (prot, "/IPX");

  ipx_dsocket = pntohs (packet + offset + 16);
  ipx_ssocket = pntohs (packet + offset + 28);
  ipx_type = *(guint8 *) (packet + offset + 5);
  ipx_length = pntohs (packet + offset + 2);

  switch (ipx_type)
    {
      /* Look at the socket with these two types */
    case IPX_PACKET_TYPE_PEP:
    case IPX_PACKET_TYPE_IPX:
      break;
    case IPX_PACKET_TYPE_RIP:
      prot = g_string_append (prot, "/IPX-RIP");
      break;
    case IPX_PACKET_TYPE_ECHO:
      prot = g_string_append (prot, "/IPX-ECHO");
      break;
    case IPX_PACKET_TYPE_ERROR:
      prot = g_string_append (prot, "/IPX-ERROR");
      break;
    case IPX_PACKET_TYPE_SPX:
      prot = g_string_append (prot, "/IPX-SPX");
      break;
    case IPX_PACKET_TYPE_NCP:
      prot = g_string_append (prot, "/IPX-NCP");
      break;
    case IPX_PACKET_TYPE_WANBCAST:
      prot = g_string_append (prot, "/IPX-NetBIOS");
      break;
    }

  if ((ipx_type != IPX_PACKET_TYPE_IPX) && (ipx_type != IPX_PACKET_TYPE_PEP)
      && (ipx_type != IPX_PACKET_TYPE_WANBCAST))
    return;

  if ((ipx_dsocket == IPX_SOCKET_SAP) || (ipx_ssocket == IPX_SOCKET_SAP))
    prot = g_string_append (prot, "/IPX-SAP");
  else if ((ipx_dsocket == IPX_SOCKET_ATTACHMATE_GW)
	   || (ipx_ssocket == IPX_SOCKET_ATTACHMATE_GW))
    prot = g_string_append (prot, "/ATTACHMATE-GW");
  else if ((ipx_dsocket == IPX_SOCKET_PING_NOVELL)
	   || (ipx_ssocket == IPX_SOCKET_PING_NOVELL))
    prot = g_string_append (prot, "/PING-NOVELL");
  else if ((ipx_dsocket == IPX_SOCKET_NCP) || (ipx_ssocket == IPX_SOCKET_NCP))
    prot = g_string_append (prot, "/IPX-NCP");
  else if ((ipx_dsocket == IPX_SOCKET_IPXRIP)
	   || (ipx_ssocket == IPX_SOCKET_IPXRIP))
    prot = g_string_append (prot, "/IPX-RIP");
  else if ((ipx_dsocket == IPX_SOCKET_NETBIOS)
	   || (ipx_ssocket == IPX_SOCKET_NETBIOS))
    prot = g_string_append (prot, "/IPX-NetBIOS");
  else if ((ipx_dsocket == IPX_SOCKET_DIAGNOSTIC)
	   || (ipx_ssocket == IPX_SOCKET_DIAGNOSTIC))
    prot = g_string_append (prot, "/IPX-DIAG");
  else if ((ipx_dsocket == IPX_SOCKET_SERIALIZATION)
	   || (ipx_ssocket == IPX_SOCKET_SERIALIZATION))
    prot = g_string_append (prot, "/IPX-SERIAL.");
  else if ((ipx_dsocket == IPX_SOCKET_ADSM)
	   || (ipx_ssocket == IPX_SOCKET_ADSM))
    prot = g_string_append (prot, "/IPX-ADSM");
  else if ((ipx_dsocket == IPX_SOCKET_EIGRP)
	   || (ipx_ssocket == IPX_SOCKET_EIGRP))
    prot = g_string_append (prot, "/EIGRP");
  else if ((ipx_dsocket == IPX_SOCKET_WIDE_AREA_ROUTER)
	   || (ipx_ssocket == IPX_SOCKET_WIDE_AREA_ROUTER))
    prot = g_string_append (prot, "/IPX W.A. ROUTER");
  else if ((ipx_dsocket == IPX_SOCKET_TCP_TUNNEL)
	   || (ipx_ssocket == IPX_SOCKET_TCP_TUNNEL))
    prot = g_string_append (prot, "/IPX-TCP-TUNNEL");
  else if ((ipx_dsocket == IPX_SOCKET_UDP_TUNNEL)
	   || (ipx_ssocket == IPX_SOCKET_UDP_TUNNEL))
    prot = g_string_append (prot, "/IPX-UDP-TUNNEL");
  else if ((ipx_dsocket == IPX_SOCKET_NWLINK_SMB_SERVER)
	   || (ipx_ssocket == IPX_SOCKET_NWLINK_SMB_SERVER)
	   || (ipx_dsocket == IPX_SOCKET_NWLINK_SMB_NAMEQUERY)
	   || (ipx_ssocket == IPX_SOCKET_NWLINK_SMB_NAMEQUERY)
	   || (ipx_dsocket == IPX_SOCKET_NWLINK_SMB_REDIR)
	   || (ipx_ssocket == IPX_SOCKET_NWLINK_SMB_REDIR)
	   || (ipx_dsocket == IPX_SOCKET_NWLINK_SMB_MAILSLOT)
	   || (ipx_ssocket == IPX_SOCKET_NWLINK_SMB_MAILSLOT)
	   || (ipx_dsocket == IPX_SOCKET_NWLINK_SMB_MESSENGER)
	   || (ipx_ssocket == IPX_SOCKET_NWLINK_SMB_MESSENGER)
	   || (ipx_dsocket == IPX_SOCKET_NWLINK_SMB_BROWSE)
	   || (ipx_ssocket == IPX_SOCKET_NWLINK_SMB_BROWSE))
    prot = g_string_append (prot, "/IPX-SMB");
  else if ((ipx_dsocket == IPX_SOCKET_SNMP_AGENT)
	   || (ipx_ssocket == IPX_SOCKET_SNMP_AGENT)
	   || (ipx_dsocket == IPX_SOCKET_SNMP_SINK)
	   || (ipx_ssocket == IPX_SOCKET_SNMP_SINK))
    prot = g_string_append (prot, "/SNMP");
  else
    g_my_debug ("Unknown IPX ports %d, %d", ipx_dsocket, ipx_ssocket);

}				/* get_ipx */

static void
get_tcp (void)
{

  tcp_service_t *src_service, *dst_service, *chosen_service;
  tcp_type_t src_port, dst_port, chosen_port;
  guint8 th_off_x2;
  guint8 tcp_len;
  gchar *str;

  if (!tcp_services)
    load_services ();

  global_src_port = src_port = pntohs (packet + offset);
  global_dst_port = dst_port = pntohs (packet + offset + 2);
  th_off_x2 = *(guint8 *) (packet + offset + 12);
  tcp_len = hi_nibble (th_off_x2) * 4;	/* TCP header length, in bytes */

  offset += tcp_len;

  if (offset >= capture_len)
    return;			/* Continue only if there is room
				 * for data in the packet */

  src_service = g_tree_lookup (tcp_services, &src_port);
  dst_service = g_tree_lookup (tcp_services, &dst_port);

  /* Check whether this packet belongs to a registered conversation */

  if ((str = find_conversation (global_src_address, global_dst_address,
				src_port, dst_port)))
    {
      prot = g_string_append (prot, str);
      return;
    }

  /* It's not possible to know in advance whether an UDP
   * packet is an RPC packet. We'll try */
  /* False means we are calling rpc from a TCP packet */
  if (get_rpc (FALSE))
    return;

  if (IS_PORT (TCP_NETBIOS_SSN))
    {
      get_netbios_ssn ();
      return;
    }
  else if (IS_PORT (TCP_FTP))
    {
      get_ftp ();
      return;
    }

  chosen_port = choose_port (src_port, dst_port);

  if (!src_service && !dst_service)
    {
      prot = g_string_append (prot, "/TCP-Port ");
      str = g_strdup_printf ("%d", chosen_port);
      prot = g_string_append (prot, str);
      g_free (str);
      return;
    }

  if (!dst_service || ((src_port == chosen_port) && src_service))
    chosen_service = src_service;
  else
    chosen_service = dst_service;

#if 0
  /* Give priority to registered ports */
  if ((src_port < 1024) && (dst_port >= 1024))
    chosen_service = src_service;
  else if ((src_port >= 1024) && (dst_port < 1024))
    chosen_service = dst_service;
  else if (!dst_service)	/* Give priority to dst_service just because */
    chosen_service = src_service;
  else
    chosen_service = dst_service;
#endif

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
  udp_type_t src_port, dst_port, chosen_port;
  gchar *str;

  if (!udp_services)
    load_services ();

  global_src_port = src_port = pntohs (packet + offset);
  global_dst_port = dst_port = pntohs (packet + offset + 2);

  offset += 8;

  /* TODO We should check up the size of the packet the same
   * way it is done in TCP */

  src_service = g_tree_lookup (udp_services, &src_port);
  dst_service = g_tree_lookup (udp_services, &dst_port);

  /* It's not possible to know in advance whether an UDP
   * packet is an RPC packet. We'll try */
  if (get_rpc (TRUE))
    return;

  if (IS_PORT (UDP_NETBIOS_NS))
    {
      get_netbios_dgm ();
      return;
    }

  chosen_port = choose_port (src_port, dst_port);

  if (!dst_service && !src_service)
    {
      prot = g_string_append (prot, "/UDP-Port ");
      str = g_strdup_printf ("%d", chosen_port);
      prot = g_string_append (prot, str);
      g_free (str);
      return;
    }

  if (!dst_service || ((src_port == chosen_port) && src_service))
    chosen_service = src_service;
  else
    chosen_service = dst_service;

#if 0
  /* Give priority to registered ports */
  if ((src_port < 1024) && (dst_port >= 1024))
    chosen_service = src_service;
  else if ((src_port >= 1024) && (dst_port < 1024))
    chosen_service = dst_service;
  else if (!dst_service)	/* Give priority to dst_service just because */
    chosen_service = src_service;
  else
    chosen_service = dst_service;
#endif

  str = g_strdup_printf ("/%s", chosen_service->name);
  prot = g_string_append (prot, str);
  g_free (str);
  str = NULL;
  return;
}				/* get_udp */

static gboolean
get_rpc (gboolean is_udp)
{
  enum rpc_type msg_type;
  enum rpc_program msg_program;
  gchar *rpc_prot = NULL;

  /* Determine whether this is an RPC packet */

  if ((offset + 24) > capture_len)
    return FALSE;		/* not big enough */

  if (is_udp)
    {
      msg_type = pntohl (packet + offset + 4);
      msg_program = pntohl (packet + offset + 12);
    }
  else
    {
      msg_type = pntohl (packet + offset + 8);
      msg_program = pntohl (packet + offset + 16);
    }

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
      if (!(rpc_prot = find_conversation (global_dst_address, 0,
					  global_dst_port, 0)))
	return FALSE;
      prot = g_string_append (prot, rpc_prot);
      return TRUE;
    case RPC_CALL:
      switch (msg_program)
	{
	case BOOTPARAMS_PROGRAM:
	  rpc_prot = g_strdup ("/BOOTPARAMS");
	  break;
	case MOUNT_PROGRAM:
	  rpc_prot = g_strdup ("/MOUNT");
	  break;
	case NLM_PROGRAM:
	  rpc_prot = g_strdup ("/NLM");
	  break;
	case PORTMAP_PROGRAM:
	  rpc_prot = g_strdup ("/PORTMAP");
	  break;
	case STAT_PROGRAM:
	  rpc_prot = g_strdup ("/STAT");
	  break;
	case NFS_PROGRAM:
	  rpc_prot = g_strdup ("/NFS");
	  break;
	case YPBIND_PROGRAM:
	  rpc_prot = g_strdup ("/YPBIND");
	  break;
	case YPSERV_PROGRAM:
	  rpc_prot = g_strdup ("/YPSERV");
	  break;
	case YPXFR_PROGRAM:
	  rpc_prot = g_strdup ("/YPXFR");
	  break;
	default:
	  return FALSE;
	}
      prot = g_string_append (prot, rpc_prot);

      /* Search for an already existing conversation, if not, create one */
      if (!find_conversation (global_src_address, 0, global_src_port, 0))
	add_conversation (global_src_address, 0,
			  global_src_port, 0, rpc_prot);

      g_free (rpc_prot);
      return TRUE;
    default:
      return FALSE;
    }
  return FALSE;
}				/* get_rpc */

/* This function is only called from a straight llc packet,
 * never from an IP packet */
void
get_netbios (void)
{
  guint16 hdr_len;

  /* Check that there is room for the minimum header */
  if (offset + 5 > capture_len)
    return;

  hdr_len = pletohs (packet + offset);

  /* If there is any data at all, it is SMB (or so I understand
   * from Ethereal's packet-netbios.c */

  if (offset + hdr_len < capture_len)
    prot = g_string_append (prot, "/SMB");

}				/* get_netbios */

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
}				/* get_netbios_dgm */

void
get_ftp (void)
{
  gchar *mesg = NULL;
  guint size = capture_len - offset;
  gchar *hi_str = NULL, *low_str = NULL;
  guint hi_byte, low_byte;
  guint16 server_port;
  guint i = 0;


  prot = g_string_append (prot, "/FTP");


  if ((offset + 3) > capture_len)
    return;			/* not big enough */

  if ((gchar) packet[offset] != '2'
      || (gchar) packet[offset + 1] != '2'
      || (gchar) packet[offset + 2] != '7')
    return;

  /* We have a passive message. Get the port */
  mesg = g_malloc (size + 1);
  memcpy (mesg, packet + offset, size);
  mesg[size] = '\0';

  g_my_debug ("Found FTP passive command: %s", mesg);

  g_my_debug ("FTP Token: %s", strtok (mesg, "(,)"));
  for (i = 1; i <= 4; i++)
    g_my_debug ("FTP Token: %s", strtok (NULL, "(,)"));

  g_my_debug ("FTP Token: %s", hi_str = strtok (NULL, "(,)"));
  if (!hi_str || !sscanf (hi_str, "%d", &hi_byte))
    return;
  g_my_debug ("FTP Token: %s", low_str = strtok (NULL, "(,)"));
  if (!low_str || !sscanf (low_str, "%d", &low_byte))
    return;

  server_port = hi_byte * 256 + low_byte;

  g_my_debug ("FTP Hi: %d. Low: %d. Passive port is %d", hi_byte, low_byte,
	      server_port);

  /* A port number zero means any port */
  add_conversation (global_src_address, global_dst_address,
		    server_port, 0, "/FTP-PASSIVE");

  g_free (mesg);

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
	    g_warning (_("Unable to  parse line %s"), line);
	  else
	    {
#if DEBUG
	      g_my_debug ("Loading service %s %s %d", t2[1], t1[0],
			  port_number);
#endif
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
		g_my_info (_("DDP protocols not supported in %s"), line);

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

/* Given two port numbers, it returns the 
 * one that is a privileged port if the other
 * is not. If not, just returns the lower numbered */
static guint16
choose_port (guint16 a, guint16 b)
{
  guint ret;

  if ((a < 1024) && (b >= 1024))
    ret = a;
  else if ((a >= 1024) && (b < 1024))
    ret = b;
  else if (a <= b)
    ret = a;
  else
    ret = b;

  return ret;

}				/* choose_port */

static void
append_etype_prot (etype_t etype)
{
  switch (etype)
    {
    case ETHERTYPE_IP:
      prot = g_string_append (prot, "/IP");
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
    case ETHERTYPE_DNA_DL:
      prot = g_string_append (prot, "/DNA-DL");
      break;
    case ETHERTYPE_DNA_RC:
      prot = g_string_append (prot, "/DNA-RC");
      break;
    case ETHERTYPE_DNA_RT:
      prot = g_string_append (prot, "/DNA-RT");
      break;
    case ETHERTYPE_DEC:
      prot = g_string_append (prot, "/DEC");
      break;
    case ETHERTYPE_DEC_DIAG:
      prot = g_string_append (prot, "/DEC-DIAG");
      break;
    case ETHERTYPE_DEC_CUST:
      prot = g_string_append (prot, "/DEC-CUST");
      break;
    case ETHERTYPE_DEC_SCA:
      prot = g_string_append (prot, "/DEC-SCA");
      break;
    case ETHERTYPE_DEC_LB:
      prot = g_string_append (prot, "/DEC-LB");
      break;
    case ETHERTYPE_MPLS:
      prot = g_string_append (prot, "/MPLS");
      break;
    case ETHERTYPE_MPLS_MULTI:
      prot = g_string_append (prot, "/MPLS-MULTI");
      break;
    case ETHERTYPE_LAT:
      prot = g_string_append (prot, "/LAT");
      break;
    case ETHERTYPE_PPP:
      prot = g_string_append (prot, "/PPP");
      break;
    case ETHERTYPE_WCP:
      prot = g_string_append (prot, "/WCP");
      break;
    case ETHERTYPE_3C_NBP_DGRAM:
      prot = g_string_append (prot, "/3C-NBP-DGRAM");
      break;
    case ETHERTYPE_ETHBRIDGE:
      prot = g_string_append (prot, "/ETHBRIDGE");
      break;
    case ETHERTYPE_UNK:
      prot = g_string_append (prot, "/ETH_UNKNOWN");
    }

  return;
}				/* append_etype_prot */
