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
#include <ctype.h>
#include <string.h>
#include "prot_types.h"
#include "util.h"
#include "decode_proto.h"
#include "protocols.h"
#include "conversations.h"
#include "datastructs.h"

#define TCP_FTP 21
#define TCP_NETBIOS_SSN 139
#define UDP_NETBIOS_NS 138

/* Enums */
enum rpc_type
{
  RPC_CALL = 0,
  RPC_REPLY = 1
};

enum rpc_program
{
  BOOTPARAMS_PROGRAM = 1,
  MOUNT_PROGRAM = 100005,
  NFS_PROGRAM = 100003,
  NLM_PROGRAM = 100021,
  PORTMAP_PROGRAM = 100000,
  STAT_PROGRAM = 100024,
  YPBIND_PROGRAM = 100007,
  YPSERV_PROGRAM = 100004,
  YPXFR_PROGRAM = 100069
};

/* Functions declarations */

static void get_eth_type (GString *prot, guint l3_offset);
static void get_fddi_type (GString *prot, guint l3_offset);
static void get_ieee802_type (GString *prot, guint l3_offset);
static void get_eth_II (GString *prot, etype_t etype, guint l3_offset);
static void get_eth_802_3 (GString *prot, ethhdrtype_t ethhdr_type);
static void get_linux_sll_type (GString *prot, guint l3_offset);

static void get_llc (GString *prot);
static void get_ip (GString *prot, guint l3_offset);
static void get_ipx (GString *prot);
static void get_tcp (GString *prot);
static void get_udp (GString *prot);

static void get_netbios (GString *prot);
static void get_netbios_ssn (GString *prot);
static void get_netbios_dgm (GString *prot);

static void get_ftp (GString *prot);
static gboolean get_rpc (GString *prot, gboolean is_udp);
static guint16 choose_port (guint16 a, guint16 b);
static void append_etype_prot (GString *prot, etype_t etype);

/* Variables */
static guint offset = 0;
//static GString *prot = NULL;
static const guint8 *packet;
static guint capture_len = 0;

/* These are used for conversations */
static guint32 global_src_address;
static guint32 global_dst_address;
static guint16 global_src_port;
static guint16 global_dst_port;

/* ------------------------------------------------------------
 * Implementation
 * ------------------------------------------------------------*/
gchar *
get_packet_prot (const guint8 * p, guint raw_size, link_type_t link_type, 
                 guint l3_offset)
{
  GString *prot;
  guint i = 0;

  g_assert (p != NULL);

  prot = g_string_new ("");

  packet = p;
  capture_len = raw_size;

  switch (link_type)
    {
    case L_EN10MB:
      get_eth_type (prot, l3_offset);
      break;
    case L_FDDI:
      g_string_append(prot, "FDDI");
      get_fddi_type (prot, l3_offset);
      break;
    case L_IEEE802:
      g_string_append (prot, "Token Ring");
      get_ieee802_type (prot, l3_offset);
      break;
    case L_RAW:		/* Both for PPP and SLIP */
      g_string_append (prot, "RAW/IP");
      get_ip (prot, l3_offset);
      break;
    case L_NULL:
      g_string_append (prot, "NULL/IP");
      get_ip (prot, l3_offset);
      break;
#ifdef DLT_LINUX_SLL
    case L_LINUX_SLL:
      g_string_append (prot, "LINUX-SLL");
      get_linux_sll_type (prot, l3_offset);
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
  i = 0;
  if (prot)
    {
      gchar **tokens;
      gchar *top_prot = NULL;

      tokens = g_strsplit (prot->str, "/", 0);
      while ((tokens[i] != NULL) && i <= STACK_SIZE)
          top_prot = tokens[i++];
      g_assert (top_prot != NULL);
      g_string_prepend (prot, "/");
      g_string_prepend (prot, top_prot);
      g_strfreev (tokens);
    }
  for (; i <= STACK_SIZE; i++)
    g_string_append (prot, "/UNKNOWN");

  /* returns only the string buffer */
  return g_string_free (prot, FALSE);
}				/* get_packet_prot */

/* ------------------------------------------------------------
 * Private functions
 * ------------------------------------------------------------*/

static void
get_eth_type (GString *prot, guint l3_offset)
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
	  g_string_append (prot, "ISL");
	  return;
	}
    }

  if (ethhdr_type == ETHERNET_802_3)
    {
      g_string_append (prot, "802.3-RAW");
      return;
    }

  if (ethhdr_type == ETHERNET_802_2)
    {
      g_string_append (prot, "802.3");
      get_eth_802_3 (prot, ethhdr_type);
      return;
    }

  /* Else, it's ETHERNET_II */
  g_string_append (prot, "ETH_II");
  get_eth_II (prot, etype, l3_offset);
}				/* get_eth_type */

static void
get_eth_802_3 (GString *prot, ethhdrtype_t ethhdr_type)
{
  offset = 14;

  switch (ethhdr_type)
    {
    case ETHERNET_802_2:
      g_string_append (prot, "/LLC");
      get_llc (prot);
      break;
    case ETHERNET_802_3:
      get_ipx (prot);
      break;
    default:
      break;
    }
}				/* get_eth_802_3 */

static void
get_fddi_type (GString *prot, guint l3_offset)
{
  g_string_append (prot, "/LLC");
  /* Ok, this is only temporary while I truly dissect LLC 
   * and fddi */
  if ((packet[19] == 0x08) && (packet[20] == 0x00))
    {
      g_string_append (prot, "/IP");
      get_ip (prot, l3_offset);
    }

}				/* get_fddi_type */

static void
get_ieee802_type (GString *prot, guint l3_offset)
{
  /* As with FDDI, we only support LLC by now */
  g_string_append (prot, "/LLC");

  if ((packet[20] == 0x08) && (packet[21] == 0x00))
    {
      g_string_append (prot, "/IP");
      get_ip (prot, l3_offset);
    }

}

static void
get_eth_II (GString *prot, etype_t etype, guint l3_offset)
{
  append_etype_prot (prot, etype);

  if (etype == ETHERTYPE_IP)
    get_ip (prot, l3_offset);
  if (etype == ETHERTYPE_IPX)
    get_ipx (prot);
}				/* get_eth_II */

/* Gets the protocol type out of the linux-sll header.
 * I have no real idea of what can be there, but since IP
 * is 0x800 I guess it follows ethernet specifications */
static void
get_linux_sll_type (GString *prot, guint l3_offset)
{
  etype_t etype;

  etype = pntohs (&packet[14]);
  append_etype_prot (prot, etype);

  if (etype == ETHERTYPE_IP)
    get_ip (prot, l3_offset);
  if (etype == ETHERTYPE_IPX)
    get_ipx (prot);
}				/* get_linux_sll_type */

static void
get_llc (GString *prot)
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
      g_string_append (prot, "/LLC-SLMGMT");
      break;
    case SAP_SNA_PATHCTRL:
      g_string_append (prot, "/PATHCTRL");
      break;
    case SAP_IP:
      g_string_append (prot, "/IP");
      break;
    case SAP_SNA1:
      g_string_append (prot, "/SNA1");
      break;
    case SAP_SNA2:
      g_string_append (prot, "/SNA2");
      break;
    case SAP_PROWAY_NM_INIT:
      g_string_append (prot, "/PROWAY-NM-INIT");
      break;
    case SAP_TI:
      g_string_append (prot, "/TI");
      break;
    case SAP_BPDU:
      g_string_append (prot, "/BPDU");
      break;
    case SAP_RS511:
      g_string_append (prot, "/RS511");
      break;
    case SAP_X25:
      g_string_append (prot, "/X25");
      break;
    case SAP_XNS:
      g_string_append (prot, "/XNS");
      break;
    case SAP_NESTAR:
      g_string_append (prot, "/NESTAR");
      break;
    case SAP_PROWAY_ASLM:
      g_string_append (prot, "/PROWAY-ASLM");
      break;
    case SAP_ARP:
      g_string_append (prot, "/ARP");
      break;
    case SAP_SNAP:
      /* We are not supposed to reach this point */
      g_warning ("Reached SNAP while checking for DSAP in get_llc");
      g_string_append (prot, "/LLC-SNAP");
      break;
    case SAP_VINES1:
      g_string_append (prot, "/VINES1");
      break;
    case SAP_VINES2:
      g_string_append (prot, "/VINES2");
      break;
    case SAP_NETWARE:
      get_ipx (prot);
      break;
    case SAP_NETBIOS:
      g_string_append (prot, "/NETBIOS");
      get_netbios (prot);
      break;
    case SAP_IBMNM:
      g_string_append (prot, "/IBMNM");
      break;
    case SAP_RPL1:
      g_string_append (prot, "/RPL1");
      break;
    case SAP_UB:
      g_string_append (prot, "/UB");
      break;
    case SAP_RPL2:
      g_string_append (prot, "/RPL2");
      break;
    case SAP_OSINL:
      g_string_append (prot, "/OSINL");
      break;
    case SAP_GLOBAL:
      g_string_append (prot, "/LLC-GLOBAL");
      break;
    }

}				/* get_llc */

static void
get_ip (GString *prot, guint l3_offset)
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
      g_string_append (prot, "/ICMP");
      break;
    case IP_PROTO_TCP:
      if (fragment_offset)
	g_string_append (prot, "/TCP_FRAGMENT");
      else
	{
	  g_string_append (prot, "/TCP");
	  get_tcp (prot);
	}
      break;
    case IP_PROTO_UDP:
      if (fragment_offset)
	g_string_append (prot, "/UDP_FRAGMENT");
      else
	{
	  g_string_append (prot, "/UDP");
	  get_udp (prot);
	}
      break;
    case IP_PROTO_IGMP:
      g_string_append (prot, "/IGMP");
      break;
    case IP_PROTO_GGP:
      g_string_append (prot, "/GGP");
      break;
    case IP_PROTO_IPIP:
      g_string_append (prot, "/IPIP");
      break;
#if 0				/* TODO How come IPIP and IPV4 share the same number? */
    case IP_PROTO_IPV4:
      g_string_append (prot, "/IPV4");
      break;
#endif
    case IP_PROTO_EGP:
      g_string_append (prot, "/EGP");
      break;
    case IP_PROTO_PUP:
      g_string_append (prot, "/PUP");
      break;
    case IP_PROTO_IDP:
      g_string_append (prot, "/IDP");
      break;
    case IP_PROTO_TP:
      g_string_append (prot, "/TP");
      break;
    case IP_PROTO_IPV6:
      g_string_append (prot, "/IPV6");
      break;
    case IP_PROTO_ROUTING:
      g_string_append (prot, "/ROUTING");
      break;
    case IP_PROTO_FRAGMENT:
      g_string_append (prot, "/FRAGMENT");
      break;
    case IP_PROTO_RSVP:
      g_string_append (prot, "/RSVP");
      break;
    case IP_PROTO_GRE:
      g_string_append (prot, "/GRE");
      break;
    case IP_PROTO_ESP:
      g_string_append (prot, "/ESP");
      break;
    case IP_PROTO_AH:
      g_string_append (prot, "/AH");
      break;
    case IP_PROTO_ICMPV6:
      g_string_append (prot, "/ICPMPV6");
      break;
    case IP_PROTO_NONE:
      g_string_append (prot, "/NONE");
      break;
    case IP_PROTO_DSTOPTS:
      g_string_append (prot, "/DSTOPTS");
      break;
    case IP_PROTO_VINES:
      g_string_append (prot, "/VINES");
      break;
    case IP_PROTO_EIGRP:
      g_string_append (prot, "/EIGRP");
      break;
    case IP_PROTO_OSPF:
      g_string_append (prot, "/OSPF");
      break;
    case IP_PROTO_ENCAP:
      g_string_append (prot, "/ENCAP");
      break;
    case IP_PROTO_PIM:
      g_string_append (prot, "/PIM");
      break;
    case IP_PROTO_IPCOMP:
      g_string_append (prot, "/IPCOMP");
      break;
    case IP_PROTO_VRRP:
      g_string_append (prot, "/VRRP");
      break;
    default:
      g_string_append (prot, "/IP_UNKNOWN");
    }
}

static void
get_ipx (GString *prot)
{
  ipx_socket_t ipx_dsocket, ipx_ssocket;
  guint16 ipx_length;
  ipx_type_t ipx_type;

  /* Make sure this is an IPX packet */
  if ((offset + 30 > capture_len) || *(guint16 *) (packet + offset) != 0xffff)
    return;

  g_string_append (prot, "/IPX");

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
      g_string_append (prot, "/IPX-RIP");
      break;
    case IPX_PACKET_TYPE_ECHO:
      g_string_append (prot, "/IPX-ECHO");
      break;
    case IPX_PACKET_TYPE_ERROR:
      g_string_append (prot, "/IPX-ERROR");
      break;
    case IPX_PACKET_TYPE_SPX:
      g_string_append (prot, "/IPX-SPX");
      break;
    case IPX_PACKET_TYPE_NCP:
      g_string_append (prot, "/IPX-NCP");
      break;
    case IPX_PACKET_TYPE_WANBCAST:
      g_string_append (prot, "/IPX-NetBIOS");
      break;
    }

  if ((ipx_type != IPX_PACKET_TYPE_IPX) && (ipx_type != IPX_PACKET_TYPE_PEP)
      && (ipx_type != IPX_PACKET_TYPE_WANBCAST))
    return;

  if ((ipx_dsocket == IPX_SOCKET_SAP) || (ipx_ssocket == IPX_SOCKET_SAP))
    g_string_append (prot, "/IPX-SAP");
  else if ((ipx_dsocket == IPX_SOCKET_ATTACHMATE_GW)
	   || (ipx_ssocket == IPX_SOCKET_ATTACHMATE_GW))
    g_string_append (prot, "/ATTACHMATE-GW");
  else if ((ipx_dsocket == IPX_SOCKET_PING_NOVELL)
	   || (ipx_ssocket == IPX_SOCKET_PING_NOVELL))
    g_string_append (prot, "/PING-NOVELL");
  else if ((ipx_dsocket == IPX_SOCKET_NCP) || (ipx_ssocket == IPX_SOCKET_NCP))
    g_string_append (prot, "/IPX-NCP");
  else if ((ipx_dsocket == IPX_SOCKET_IPXRIP)
	   || (ipx_ssocket == IPX_SOCKET_IPXRIP))
    g_string_append (prot, "/IPX-RIP");
  else if ((ipx_dsocket == IPX_SOCKET_NETBIOS)
	   || (ipx_ssocket == IPX_SOCKET_NETBIOS))
    g_string_append (prot, "/IPX-NetBIOS");
  else if ((ipx_dsocket == IPX_SOCKET_DIAGNOSTIC)
	   || (ipx_ssocket == IPX_SOCKET_DIAGNOSTIC))
    g_string_append (prot, "/IPX-DIAG");
  else if ((ipx_dsocket == IPX_SOCKET_SERIALIZATION)
	   || (ipx_ssocket == IPX_SOCKET_SERIALIZATION))
    g_string_append (prot, "/IPX-SERIAL.");
  else if ((ipx_dsocket == IPX_SOCKET_ADSM)
	   || (ipx_ssocket == IPX_SOCKET_ADSM))
    g_string_append (prot, "/IPX-ADSM");
  else if ((ipx_dsocket == IPX_SOCKET_EIGRP)
	   || (ipx_ssocket == IPX_SOCKET_EIGRP))
    g_string_append (prot, "/EIGRP");
  else if ((ipx_dsocket == IPX_SOCKET_WIDE_AREA_ROUTER)
	   || (ipx_ssocket == IPX_SOCKET_WIDE_AREA_ROUTER))
    g_string_append (prot, "/IPX W.A. ROUTER");
  else if ((ipx_dsocket == IPX_SOCKET_TCP_TUNNEL)
	   || (ipx_ssocket == IPX_SOCKET_TCP_TUNNEL))
    g_string_append (prot, "/IPX-TCP-TUNNEL");
  else if ((ipx_dsocket == IPX_SOCKET_UDP_TUNNEL)
	   || (ipx_ssocket == IPX_SOCKET_UDP_TUNNEL))
    g_string_append (prot, "/IPX-UDP-TUNNEL");
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
    g_string_append (prot, "/IPX-SMB");
  else if ((ipx_dsocket == IPX_SOCKET_SNMP_AGENT)
	   || (ipx_ssocket == IPX_SOCKET_SNMP_AGENT)
	   || (ipx_dsocket == IPX_SOCKET_SNMP_SINK)
	   || (ipx_ssocket == IPX_SOCKET_SNMP_SINK))
    g_string_append (prot, "/SNMP");
  else
    g_my_debug ("Unknown IPX ports %d, %d", ipx_dsocket, ipx_ssocket);

}				/* get_ipx */

static void
get_tcp (GString *prot)
{

  const port_service_t *src_service, *dst_service, *chosen_service;
  port_type_t src_port, dst_port, chosen_port;
  guint8 th_off_x2;
  guint8 tcp_len;
  const gchar *str;
  gboolean src_pref = FALSE;
  gboolean dst_pref = FALSE;

  global_src_port = src_port = pntohs (packet + offset);
  global_dst_port = dst_port = pntohs (packet + offset + 2);
  th_off_x2 = *(guint8 *) (packet + offset + 12);
  tcp_len = hi_nibble (th_off_x2) * 4;	/* TCP header length, in bytes */

  offset += tcp_len;


  /* Check whether this packet belongs to a registered conversation */
  if ((str = find_conversation (global_src_address, global_dst_address,
				src_port, dst_port)))
    {
      g_string_append (prot, str);
      return;
    }

  /* It's not possible to know in advance whether an UDP
   * packet is an RPC packet. We'll try */
  /* False means we are calling rpc from a TCP packet */
  if (get_rpc (prot, FALSE))
    return;

  if (src_port==TCP_NETBIOS_SSN || dst_port==TCP_NETBIOS_SSN)
    {
      get_netbios_ssn (prot);
      return;
    }
  else if (src_port==TCP_FTP || dst_port == TCP_FTP)
    {
      get_ftp (prot);
      return;
    }

  src_service = services_tcp_find(src_port);
  dst_service = services_tcp_find(dst_port);
  chosen_port = choose_port (src_port, dst_port);

  if (!src_service && !dst_service)
    {
      if (pref.group_unk)
        g_string_append(prot, "/TCP-unknown");
      else
        {
          if (chosen_port == src_port)
            g_string_append_printf(prot, "/TCP:%d-%d", chosen_port, dst_port);
          else
            g_string_append_printf(prot, "/TCP:%d-%d", chosen_port, src_port);
        }
      return;
    }

  /* if one of the services has user-defined color, then we favor it,
   * otherwise chosen_port wins */
  if (src_service)
    src_pref = src_service->preferred;
  if (dst_service)
    dst_pref = dst_service->preferred;
  
  if (src_pref && !dst_pref)
    chosen_service = src_service;
  else if (!src_pref && dst_pref)
    chosen_service = dst_service;
  else if (!dst_service || ((src_port == chosen_port) && src_service))
    chosen_service = src_service;
  else
    chosen_service = dst_service;

  g_string_append_printf(prot, "/%s", chosen_service->name);
}				/* get_tcp */

static void
get_udp (GString *prot)
{
  const port_service_t *src_service, *dst_service, *chosen_service;
  port_type_t src_port, dst_port, chosen_port;
  gboolean src_pref = FALSE;
  gboolean dst_pref = FALSE;

  global_src_port = src_port = pntohs (packet + offset);
  global_dst_port = dst_port = pntohs (packet + offset + 2);

  offset += 8;

  /* It's not possible to know in advance whether an UDP
   * packet is an RPC packet. We'll try */
  if (get_rpc (prot, TRUE))
    return;

  if (src_port==UDP_NETBIOS_NS || dst_port==UDP_NETBIOS_NS)
    {
      get_netbios_dgm (prot);
      return;
    }

  src_service = services_udp_find(src_port);
  dst_service = services_udp_find(dst_port);

  chosen_port = choose_port (src_port, dst_port);

  if (!dst_service && !src_service)
    {
      if (pref.group_unk)
        g_string_append(prot, "/UDP-unknown");
      else
        {
          if (chosen_port == src_port)
            g_string_append_printf(prot, "/UDP:%d-%d", chosen_port, dst_port);
          else
            g_string_append_printf(prot, "/UDP:%d-%d", chosen_port, src_port);
        }
      return;
    }

  /* if one of the services has user-defined color, then we favor it,
   * otherwise chosen_port wins */
  if (src_service)
    src_pref = src_service->preferred;
  if (dst_service)
    dst_pref = dst_service->preferred;
  
  if (src_pref && !dst_pref)
    chosen_service = src_service;
  else if (!src_pref && dst_pref)
    chosen_service = dst_service;
  else if (!dst_service || ((src_port == chosen_port) && src_service))
    chosen_service = src_service;
  else
    chosen_service = dst_service;
  g_string_append_printf(prot, "/%s", chosen_service->name);
}				/* get_udp */

static gboolean
get_rpc (GString *prot, gboolean is_udp)
{
  enum rpc_type msg_type;
  enum rpc_program msg_program;
  const gchar *rpc_prot = NULL;

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

  g_string_append (prot, "/RPC");

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
      g_string_append (prot, rpc_prot);
      return TRUE;
    case RPC_CALL:
      switch (msg_program)
	{
	case BOOTPARAMS_PROGRAM:
	  prot = g_string_append(prot, "/BOOTPARAMS");
	  break;
	case MOUNT_PROGRAM:
	  prot = g_string_append(prot, "/MOUNT");
	  break;
	case NLM_PROGRAM:
	  prot = g_string_append(prot, "/NLM");
	  break;
	case PORTMAP_PROGRAM:
	  prot = g_string_append(prot, "/PORTMAP");
	  break;
	case STAT_PROGRAM:
	  prot = g_string_append(prot, "/STAT");
	  break;
	case NFS_PROGRAM:
	  prot = g_string_append(prot, "/NFS");
	  break;
	case YPBIND_PROGRAM:
	  prot = g_string_append(prot, "/YPBIND");
	  break;
	case YPSERV_PROGRAM:
	  prot = g_string_append(prot, "/YPSERV");
	  break;
	case YPXFR_PROGRAM:
	  prot = g_string_append(prot, "/YPXFR");
	  break;
	default:
	  return FALSE;
	}

      /* Search for an already existing conversation, if not, create one */
      if (!find_conversation (global_src_address, 0, global_src_port, 0))
	add_conversation (global_src_address, 0,
			  global_src_port, 0, rpc_prot);

      return TRUE;
    default:
      return FALSE;
    }
  return FALSE;
}				/* get_rpc */

/* This function is only called from a straight llc packet,
 * never from an IP packet */
void
get_netbios (GString *prot)
{
  guint16 hdr_len;

  /* Check that there is room for the minimum header */
  if (offset + 5 > capture_len)
    return;

  hdr_len = pletohs (packet + offset);

  /* If there is any data at all, it is SMB (or so I understand
   * from Ethereal's packet-netbios.c */

  if (offset + hdr_len < capture_len)
    g_string_append (prot, "/SMB");

}				/* get_netbios */

void
get_netbios_ssn (GString *prot)
{
#define SESSION_MESSAGE 0
  guint8 mesg_type;

  g_string_append (prot, "/NETBIOS-SSN");

  mesg_type = *(guint8 *) (packet + offset);

  if (mesg_type == SESSION_MESSAGE)
    g_string_append (prot, "/SMB");

  /* TODO Calculate new offset whenever we have
   * a "dissector" for a higher protocol */
}				/* get_netbions_ssn */

void
get_netbios_dgm (GString *prot)
{
  guint8 mesg_type;

  g_string_append (prot, "/NETBIOS-DGM");

  mesg_type = *(guint8 *) (packet + offset);

  /* Magic numbers copied from ethereal, as usual
   * They mean Direct (unique|group|broadcast) datagram */
  if (mesg_type == 0x10 || mesg_type == 0x11 || mesg_type == 0x12)
    g_string_append (prot, "/SMB");

  /* TODO Calculate new offset whenever we have
   * a "dissector" for a higher protocol */
}				/* get_netbios_dgm */

void
get_ftp (GString *prot)
{
  gchar *mesg = NULL;
  guint size = capture_len - offset;
  gchar *str;
  guint hi_byte, low_byte;
  guint16 server_port;
  guint i = 0;

  g_string_append (prot, "/FTP");
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

  str = strtok (mesg, "(,)");
  g_my_debug ("FTP Token: %s", str ? str : "NULL");
  for (i = 0; i < 4 && str; i++)
    {
      str = strtok (NULL, "(,)");
      g_my_debug ("FTP Token: %s", str ? str : "NULL");
    }

  str = strtok (NULL, "(,)");
  g_my_debug ("FTP Token: %s", str ? str : "NULL");
  if (!str || !sscanf (str, "%d", &hi_byte))
    {
      g_free(mesg);
      return;
    }
  str = strtok (NULL, "(,)");
  g_my_debug ("FTP Token: %s", str ? str : "NULL");
  if (!str || !sscanf (str, "%d", &low_byte))
    {
      g_free(mesg);
      return;
    }

  server_port = hi_byte * 256 + low_byte;
  g_my_debug ("FTP Hi: %d. Low: %d. Passive port is %d", hi_byte, low_byte,
	      server_port);

  /* A port number zero means any port */
  add_conversation (global_src_address, global_dst_address,
		    server_port, 0, "/FTP-PASSIVE");

  g_free (mesg);
}

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
append_etype_prot (GString *prot, etype_t etype)
{
  switch (etype)
    {
    case ETHERTYPE_IP:
      g_string_append (prot, "/IP");
      break;
    case ETHERTYPE_ARP:
      g_string_append (prot, "/ARP");
      break;
    case ETHERTYPE_IPv6:
      g_string_append (prot, "/IPv6");
      break;
    case ETHERTYPE_X25L3:
      g_string_append (prot, "/X25L3");
      break;
    case ETHERTYPE_REVARP:
      g_string_append (prot, "/REVARP");
      break;
    case ETHERTYPE_ATALK:
      g_string_append (prot, "/ATALK");
      break;
    case ETHERTYPE_AARP:
      g_string_append (prot, "/AARP");
      break;
    case ETHERTYPE_IPX:
      break;
    case ETHERTYPE_VINES:
      g_string_append (prot, "/VINES");
      break;
    case ETHERTYPE_TRAIN:
      g_string_append (prot, "/TRAIN");
      break;
    case ETHERTYPE_LOOP:
      g_string_append (prot, "/LOOP");
      break;
    case ETHERTYPE_PPPOED:
      g_string_append (prot, "/PPPOED");
      break;
    case ETHERTYPE_PPPOES:
      g_string_append (prot, "/PPPOES");
      break;
    case ETHERTYPE_VLAN:
      g_string_append (prot, "/VLAN");
      break;
    case ETHERTYPE_SNMP:
      g_string_append (prot, "/SNMP");
      break;
    case ETHERTYPE_DNA_DL:
      g_string_append (prot, "/DNA-DL");
      break;
    case ETHERTYPE_DNA_RC:
      g_string_append (prot, "/DNA-RC");
      break;
    case ETHERTYPE_DNA_RT:
      g_string_append (prot, "/DNA-RT");
      break;
    case ETHERTYPE_DEC:
      g_string_append (prot, "/DEC");
      break;
    case ETHERTYPE_DEC_DIAG:
      g_string_append (prot, "/DEC-DIAG");
      break;
    case ETHERTYPE_DEC_CUST:
      g_string_append (prot, "/DEC-CUST");
      break;
    case ETHERTYPE_DEC_SCA:
      g_string_append (prot, "/DEC-SCA");
      break;
    case ETHERTYPE_DEC_LB:
      g_string_append (prot, "/DEC-LB");
      break;
    case ETHERTYPE_MPLS:
      g_string_append (prot, "/MPLS");
      break;
    case ETHERTYPE_MPLS_MULTI:
      g_string_append (prot, "/MPLS-MULTI");
      break;
    case ETHERTYPE_LAT:
      g_string_append (prot, "/LAT");
      break;
    case ETHERTYPE_PPP:
      g_string_append (prot, "/PPP");
      break;
    case ETHERTYPE_WCP:
      g_string_append (prot, "/WCP");
      break;
    case ETHERTYPE_3C_NBP_DGRAM:
      g_string_append (prot, "/3C-NBP-DGRAM");
      break;
    case ETHERTYPE_ETHBRIDGE:
      g_string_append (prot, "/ETHBRIDGE");
      break;
    case ETHERTYPE_UNK:
      g_string_append (prot, "/ETH_UNKNOWN");
    }
}				/* append_etype_prot */

