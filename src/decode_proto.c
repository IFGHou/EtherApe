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

/* internal types */
typedef struct 
{
  const guint8 *packet;
  guint capture_len;
  packet_protos_t *pr;

  guint offset;
  guint cur_level;

  /* These are used for conversations */
  guint32 global_src_address;
  guint32 global_dst_address;
  guint16 global_src_port;
  guint16 global_dst_port;

} decode_proto_t;

/* starts a new decode, allocating a new packet_protos_t */
void decode_proto_start(decode_proto_t *dp, const guint8 *pkt, guint caplen);

/* sets protoname at current level, and passes at next level */
void decode_proto_add(decode_proto_t *dp, const gchar *fmt, ...);

/* internal functions declarations */

static void get_eth_type (decode_proto_t *dp, guint l3_offset);
static void get_fddi_type (decode_proto_t *dp, guint l3_offset);
static void get_ieee802_type (decode_proto_t *dp, guint l3_offset);
static void get_eth_II (decode_proto_t *dp, etype_t etype, guint l3_offset);
static void get_eth_802_3 (decode_proto_t *dp, ethhdrtype_t ethhdr_type);
static void get_linux_sll_type (decode_proto_t *dp, guint l3_offset);

static void get_llc (decode_proto_t *dp);
static void get_ip (decode_proto_t *dp, guint l3_offset);
static void get_ipx (decode_proto_t *dp);
static void get_tcp (decode_proto_t *dp);
static void get_udp (decode_proto_t *dp);

static void get_netbios (decode_proto_t *dp);
static void get_netbios_ssn (decode_proto_t *dp);
static void get_netbios_dgm (decode_proto_t *dp);

static void get_ftp (decode_proto_t *dp);
static gboolean get_rpc (decode_proto_t *dp, gboolean is_udp);
static guint16 choose_port (guint16 a, guint16 b);
static void append_etype_prot (decode_proto_t *dp, etype_t etype);

/* ------------------------------------------------------------
 * Implementation
 * ------------------------------------------------------------*/
/* starts a new decode, allocating a new packet_protos_t */
void decode_proto_start(decode_proto_t *dp, const guint8 *pkt, guint caplen)
{
  dp->packet = pkt;
  dp->capture_len = caplen;
  dp->pr = packet_protos_init();
  dp->offset = 0;
  dp->cur_level = 1;
  dp->global_src_address = 0;
  dp->global_dst_address = 0;
  dp->global_src_port = 0;
  dp->global_dst_port = 0;
}
void decode_proto_add(decode_proto_t *dp, const gchar *fmt, ...)
{
  va_list ap;
  if (dp->cur_level <= STACK_SIZE)
    {
      va_start(ap, fmt);
      dp->pr->protonames[dp->cur_level] = g_strdup_vprintf(fmt, ap);
      va_end(ap);
      dp->cur_level++;
    }
  else
    g_warning("protocol too deep, higher levels ignored");
}


packet_protos_t *get_packet_prot (const guint8 * p, guint raw_size, 
                                  int link_type, guint l3_offset)
{
  decode_proto_t decp;
  guint i;
  gchar *prot;

  g_assert (p != NULL);

  decode_proto_start(&decp, p, raw_size);

  switch (link_type)
    {
    case DLT_EN10MB:
      get_eth_type (&decp, l3_offset);
      break;
    case DLT_FDDI:
      decode_proto_add(&decp, "FDDI");
      get_fddi_type (&decp, l3_offset);
      break;
    case DLT_IEEE802:
      decode_proto_add(&decp, "Token Ring");
      get_ieee802_type (&decp, l3_offset);
      break;
    case DLT_RAW:		/* Both for PPP and SLIP */
      decode_proto_add(&decp, "RAW/IP");
      get_ip (&decp, l3_offset);
      break;
    case DLT_NULL:
      decode_proto_add(&decp, "NULL/IP");
      get_ip (&decp, l3_offset);
      break;
#ifdef DLT_LINUX_SLL
    case DLT_LINUX_SLL:
      decode_proto_add(&decp, "LINUX-SLL");
      get_linux_sll_type (&decp, l3_offset);
      break;
#endif
    default:
      break;
    }

  /* first position is top proto */
  for (i = STACK_SIZE ; i>0 ; --i)
    {
      if (decp.pr->protonames[i])
        {
          decp.pr->protonames[0] = g_strdup(decp.pr->protonames[i]);
          break;
        }
    }

  return decp.pr;
}				/* get_packet_prot */

/* ------------------------------------------------------------
 * Private functions
 * ------------------------------------------------------------*/

static void
get_eth_type (decode_proto_t *dp, guint l3_offset)
{
  etype_t etype;
  ethhdrtype_t ethhdr_type = ETHERNET_II;	/* Default */

  etype = pntohs (&dp->packet[12]);


  if (etype <= IEEE_802_3_MAX_LEN)
    {

      /* Is there an 802.2 layer? I can tell by looking at the first 2
       *        bytes after the 802.3 header. If they are 0xffff, then what
       *        follows the 802.3 header is an IPX payload, meaning no 802.2.
       *        (IPX/SPX is they only thing that can be contained inside a
       *        straight 802.3 packet). A non-0xffff value means that there's an
       *        802.2 layer inside the 802.3 layer */
      if (dp->packet[14] == 0xff && dp->packet[15] == 0xff)
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
      if (dp->packet[0] == 0x01 && dp->packet[1] == 0x00 && dp->packet[2] == 0x0C
	  && dp->packet[3] == 0x00 && dp->packet[4] == 0x00)
	{
	  /* TODO Analyze ISL frames */
	  decode_proto_add(dp, "ISL");
	  return;
	}
    }

  if (ethhdr_type == ETHERNET_802_3)
    {
      decode_proto_add(dp, "802.3-RAW");
      return;
    }

  if (ethhdr_type == ETHERNET_802_2)
    {
      decode_proto_add(dp, "802.3");
      get_eth_802_3 (dp, ethhdr_type);
      return;
    }

  /* Else, it's ETHERNET_II */
  decode_proto_add(dp, "ETH_II");
  get_eth_II (dp, etype, l3_offset);
}				/* get_eth_type */

static void
get_eth_802_3 (decode_proto_t *dp, ethhdrtype_t ethhdr_type)
{
  dp->offset = 14;

  switch (ethhdr_type)
    {
    case ETHERNET_802_2:
      decode_proto_add(dp, "LLC");
      get_llc (dp);
      break;
    case ETHERNET_802_3:
      get_ipx (dp);
      break;
    default:
      break;
    }
}				/* get_eth_802_3 */

static void
get_fddi_type (decode_proto_t *dp, guint l3_offset)
{
  decode_proto_add(dp, "LLC");
  /* Ok, this is only temporary while I truly dissect LLC 
   * and fddi */
  if ((dp->packet[19] == 0x08) && (dp->packet[20] == 0x00))
    {
      decode_proto_add(dp, "IP");
      get_ip (dp, l3_offset);
    }

}				/* get_fddi_type */

static void
get_ieee802_type (decode_proto_t *dp, guint l3_offset)
{
  /* As with FDDI, we only support LLC by now */
  decode_proto_add(dp, "LLC");

  if ((dp->packet[20] == 0x08) && (dp->packet[21] == 0x00))
    {
      decode_proto_add(dp, "IP");
      get_ip (dp, l3_offset);
    }

}

static void
get_eth_II (decode_proto_t *dp, etype_t etype, guint l3_offset)
{
  append_etype_prot (dp, etype);

  if (etype == ETHERTYPE_IP)
    get_ip (dp, l3_offset);
  if (etype == ETHERTYPE_IPX)
    get_ipx (dp);
}				/* get_eth_II */

/* Gets the protocol type out of the linux-sll header.
 * I have no real idea of what can be there, but since IP
 * is 0x800 I guess it follows ethernet specifications */
static void
get_linux_sll_type (decode_proto_t *dp, guint l3_offset)
{
  etype_t etype;

  etype = pntohs (&dp->packet[14]);
  append_etype_prot (dp, etype);

  if (etype == ETHERTYPE_IP)
    get_ip (dp, l3_offset);
  if (etype == ETHERTYPE_IPX)
    get_ipx (dp);
}				/* get_linux_sll_type */

static void
get_llc (decode_proto_t *dp)
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

  dsap = *(guint8 *) (dp->packet + dp->offset);
  ssap = *(guint8 *) (dp->packet + dp->offset + 1);

  is_snap = (dsap == SAP_SNAP) && (ssap == SAP_SNAP);

  /* SNAP not yet supported */
  if (is_snap)
    return;

  /* To get this control value is actually a lot more
   * complicated than this, see xdlc.c in ethereal,
   * but I'll try like this, it seems it works for my pourposes at
   * least most of the time */
  control = *(guint8 *) (dp->packet + dp->offset + 2);

  if (!XDLC_IS_INFORMATION (control))
    return;

  dp->offset += 3;

  switch (dsap)
    {
    case SAP_NULL:
      decode_proto_add(dp, "LLC-NULL");
      break;
    case SAP_LLC_SLMGMT:
      decode_proto_add(dp, "LLC-SLMGMT");
      break;
    case SAP_SNA_PATHCTRL:
      decode_proto_add(dp, "PATHCTRL");
      break;
    case SAP_IP:
      decode_proto_add(dp, "IP");
      break;
    case SAP_SNA1:
      decode_proto_add(dp, "SNA1");
      break;
    case SAP_SNA2:
      decode_proto_add(dp, "SNA2");
      break;
    case SAP_PROWAY_NM_INIT:
      decode_proto_add(dp, "PROWAY-NM-INIT");
      break;
    case SAP_TI:
      decode_proto_add(dp, "TI");
      break;
    case SAP_BPDU:
      decode_proto_add(dp, "BPDU");
      break;
    case SAP_RS511:
      decode_proto_add(dp, "RS511");
      break;
    case SAP_X25:
      decode_proto_add(dp, "X25");
      break;
    case SAP_XNS:
      decode_proto_add(dp, "XNS");
      break;
    case SAP_NESTAR:
      decode_proto_add(dp, "NESTAR");
      break;
    case SAP_PROWAY_ASLM:
      decode_proto_add(dp, "PROWAY-ASLM");
      break;
    case SAP_ARP:
      decode_proto_add(dp, "ARP");
      break;
    case SAP_SNAP:
      /* We are not supposed to reach this point */
      g_warning ("Reached SNAP while checking for DSAP in get_llc");
      decode_proto_add(dp, "LLC-SNAP");
      break;
    case SAP_VINES1:
      decode_proto_add(dp, "VINES1");
      break;
    case SAP_VINES2:
      decode_proto_add(dp, "VINES2");
      break;
    case SAP_NETWARE:
      get_ipx (dp);
      break;
    case SAP_NETBIOS:
      decode_proto_add(dp, "NETBIOS");
      get_netbios (dp);
      break;
    case SAP_IBMNM:
      decode_proto_add(dp, "IBMNM");
      break;
    case SAP_RPL1:
      decode_proto_add(dp, "RPL1");
      break;
    case SAP_UB:
      decode_proto_add(dp, "UB");
      break;
    case SAP_RPL2:
      decode_proto_add(dp, "RPL2");
      break;
    case SAP_OSINL:
      decode_proto_add(dp, "OSINL");
      break;
    case SAP_GLOBAL:
      decode_proto_add(dp, "LLC-GLOBAL");
      break;
    }

}				/* get_llc */

static void
get_ip (decode_proto_t *dp, guint l3_offset)
{
  guint16 fragment_offset;
  iptype_t ip_type;
  ip_type = dp->packet[l3_offset + 9];
  fragment_offset = pntohs (dp->packet + l3_offset + 6);
  fragment_offset &= 0x0fff;

  /*This is used for conversations */
  dp->global_src_address = pntohl (dp->packet + l3_offset + 12);
  dp->global_dst_address = pntohl (dp->packet + l3_offset + 16);

  dp->offset = l3_offset + 20;

  switch (ip_type)
    {
    case IP_PROTO_ICMP:
      decode_proto_add(dp, "ICMP");
      break;
    case IP_PROTO_TCP:
      if (fragment_offset)
	decode_proto_add(dp, "TCP_FRAGMENT");
      else
	{
	  decode_proto_add(dp, "TCP");
	  get_tcp (dp);
	}
      break;
    case IP_PROTO_UDP:
      if (fragment_offset)
	decode_proto_add(dp, "UDP_FRAGMENT");
      else
	{
	  decode_proto_add(dp, "UDP");
	  get_udp (dp);
	}
      break;
    case IP_PROTO_IGMP:
      decode_proto_add(dp, "IGMP");
      break;
    case IP_PROTO_GGP:
      decode_proto_add(dp, "GGP");
      break;
    case IP_PROTO_IPIP:
      decode_proto_add(dp, "IPIP");
      break;
#if 0				/* TODO How come IPIP and IPV4 share the same number? */
    case IP_PROTO_IPV4:
      decode_proto_add(dp, "IPV4");
      break;
#endif
    case IP_PROTO_EGP:
      decode_proto_add(dp, "EGP");
      break;
    case IP_PROTO_PUP:
      decode_proto_add(dp, "PUP");
      break;
    case IP_PROTO_IDP:
      decode_proto_add(dp, "IDP");
      break;
    case IP_PROTO_TP:
      decode_proto_add(dp, "TP");
      break;
    case IP_PROTO_IPV6:
      decode_proto_add(dp, "IPV6");
      break;
    case IP_PROTO_ROUTING:
      decode_proto_add(dp, "ROUTING");
      break;
    case IP_PROTO_FRAGMENT:
      decode_proto_add(dp, "FRAGMENT");
      break;
    case IP_PROTO_RSVP:
      decode_proto_add(dp, "RSVP");
      break;
    case IP_PROTO_GRE:
      decode_proto_add(dp, "GRE");
      break;
    case IP_PROTO_ESP:
      decode_proto_add(dp, "ESP");
      break;
    case IP_PROTO_AH:
      decode_proto_add(dp, "AH");
      break;
    case IP_PROTO_ICMPV6:
      decode_proto_add(dp, "ICPMPV6");
      break;
    case IP_PROTO_NONE:
      decode_proto_add(dp, "NONE");
      break;
    case IP_PROTO_DSTOPTS:
      decode_proto_add(dp, "DSTOPTS");
      break;
    case IP_PROTO_VINES:
      decode_proto_add(dp, "VINES");
      break;
    case IP_PROTO_EIGRP:
      decode_proto_add(dp, "EIGRP");
      break;
    case IP_PROTO_OSPF:
      decode_proto_add(dp, "OSPF");
      break;
    case IP_PROTO_ENCAP:
      decode_proto_add(dp, "ENCAP");
      break;
    case IP_PROTO_PIM:
      decode_proto_add(dp, "PIM");
      break;
    case IP_PROTO_IPCOMP:
      decode_proto_add(dp, "IPCOMP");
      break;
    case IP_PROTO_VRRP:
      decode_proto_add(dp, "VRRP");
      break;
    default:
      decode_proto_add(dp, "IP_UNKNOWN");
    }
}

static void
get_ipx (decode_proto_t *dp)
{
  ipx_socket_t ipx_dsocket, ipx_ssocket;
  guint16 ipx_length;
  ipx_type_t ipx_type;

  /* Make sure this is an IPX packet */
  if ((dp->offset + 30 > dp->capture_len) || *(guint16 *) (dp->packet + dp->offset) != 0xffff)
    return;

  decode_proto_add(dp, "IPX");

  ipx_dsocket = pntohs (dp->packet + dp->offset + 16);
  ipx_ssocket = pntohs (dp->packet + dp->offset + 28);
  ipx_type = *(guint8 *) (dp->packet + dp->offset + 5);
  ipx_length = pntohs (dp->packet + dp->offset + 2);

  switch (ipx_type)
    {
      /* Look at the socket with these two types */
    case IPX_PACKET_TYPE_PEP:
    case IPX_PACKET_TYPE_IPX:
      break;
    case IPX_PACKET_TYPE_RIP:
      decode_proto_add(dp, "IPX-RIP");
      break;
    case IPX_PACKET_TYPE_ECHO:
      decode_proto_add(dp, "IPX-ECHO");
      break;
    case IPX_PACKET_TYPE_ERROR:
      decode_proto_add(dp, "IPX-ERROR");
      break;
    case IPX_PACKET_TYPE_SPX:
      decode_proto_add(dp, "IPX-SPX");
      break;
    case IPX_PACKET_TYPE_NCP:
      decode_proto_add(dp, "IPX-NCP");
      break;
    case IPX_PACKET_TYPE_WANBCAST:
      decode_proto_add(dp, "IPX-NetBIOS");
      break;
    }

  if ((ipx_type != IPX_PACKET_TYPE_IPX) && (ipx_type != IPX_PACKET_TYPE_PEP)
      && (ipx_type != IPX_PACKET_TYPE_WANBCAST))
    return;

  if ((ipx_dsocket == IPX_SOCKET_SAP) || (ipx_ssocket == IPX_SOCKET_SAP))
    decode_proto_add(dp, "IPX-SAP");
  else if ((ipx_dsocket == IPX_SOCKET_ATTACHMATE_GW)
	   || (ipx_ssocket == IPX_SOCKET_ATTACHMATE_GW))
    decode_proto_add(dp, "ATTACHMATE-GW");
  else if ((ipx_dsocket == IPX_SOCKET_PING_NOVELL)
	   || (ipx_ssocket == IPX_SOCKET_PING_NOVELL))
    decode_proto_add(dp, "PING-NOVELL");
  else if ((ipx_dsocket == IPX_SOCKET_NCP) || (ipx_ssocket == IPX_SOCKET_NCP))
    decode_proto_add(dp, "IPX-NCP");
  else if ((ipx_dsocket == IPX_SOCKET_IPXRIP)
	   || (ipx_ssocket == IPX_SOCKET_IPXRIP))
    decode_proto_add(dp, "IPX-RIP");
  else if ((ipx_dsocket == IPX_SOCKET_NETBIOS)
	   || (ipx_ssocket == IPX_SOCKET_NETBIOS))
    decode_proto_add(dp, "IPX-NetBIOS");
  else if ((ipx_dsocket == IPX_SOCKET_DIAGNOSTIC)
	   || (ipx_ssocket == IPX_SOCKET_DIAGNOSTIC))
    decode_proto_add(dp, "IPX-DIAG");
  else if ((ipx_dsocket == IPX_SOCKET_SERIALIZATION)
	   || (ipx_ssocket == IPX_SOCKET_SERIALIZATION))
    decode_proto_add(dp, "IPX-SERIAL.");
  else if ((ipx_dsocket == IPX_SOCKET_ADSM)
	   || (ipx_ssocket == IPX_SOCKET_ADSM))
    decode_proto_add(dp, "IPX-ADSM");
  else if ((ipx_dsocket == IPX_SOCKET_EIGRP)
	   || (ipx_ssocket == IPX_SOCKET_EIGRP))
    decode_proto_add(dp, "EIGRP");
  else if ((ipx_dsocket == IPX_SOCKET_WIDE_AREA_ROUTER)
	   || (ipx_ssocket == IPX_SOCKET_WIDE_AREA_ROUTER))
    decode_proto_add(dp, "IPX W.A. ROUTER");
  else if ((ipx_dsocket == IPX_SOCKET_TCP_TUNNEL)
	   || (ipx_ssocket == IPX_SOCKET_TCP_TUNNEL))
    decode_proto_add(dp, "IPX-TCP-TUNNEL");
  else if ((ipx_dsocket == IPX_SOCKET_UDP_TUNNEL)
	   || (ipx_ssocket == IPX_SOCKET_UDP_TUNNEL))
    decode_proto_add(dp, "IPX-UDP-TUNNEL");
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
    decode_proto_add(dp, "IPX-SMB");
  else if ((ipx_dsocket == IPX_SOCKET_SNMP_AGENT)
	   || (ipx_ssocket == IPX_SOCKET_SNMP_AGENT)
	   || (ipx_dsocket == IPX_SOCKET_SNMP_SINK)
	   || (ipx_ssocket == IPX_SOCKET_SNMP_SINK))
    decode_proto_add(dp, "SNMP");
  else
    g_my_debug ("Unknown IPX ports %d, %d", ipx_dsocket, ipx_ssocket);

}				/* get_ipx */

static void
get_tcp (decode_proto_t *dp)
{

  const port_service_t *src_service, *dst_service, *chosen_service;
  port_type_t src_port, dst_port, chosen_port;
  guint8 th_off_x2;
  guint8 tcp_len;
  const gchar *str;
  gboolean src_pref = FALSE;
  gboolean dst_pref = FALSE;

  dp->global_src_port = src_port = pntohs (dp->packet + dp->offset);
  dp->global_dst_port = dst_port = pntohs (dp->packet + dp->offset + 2);
  th_off_x2 = *(guint8 *) (dp->packet + dp->offset + 12);
  tcp_len = hi_nibble (th_off_x2) * 4;	/* TCP header length, in bytes */

  dp->offset += tcp_len;


  /* Check whether this packet belongs to a registered conversation */
  if ((str = find_conversation (dp->global_src_address, dp->global_dst_address,
				src_port, dst_port)))
    {
      decode_proto_add(dp, str);
      return;
    }

  /* It's not possible to know in advance whether an UDP
   * packet is an RPC packet. We'll try */
  /* False means we are calling rpc from a TCP packet */
  if (get_rpc (dp, FALSE))
    return;

  if (src_port==TCP_NETBIOS_SSN || dst_port==TCP_NETBIOS_SSN)
    {
      get_netbios_ssn (dp);
      return;
    }
  else if (src_port==TCP_FTP || dst_port == TCP_FTP)
    {
      get_ftp (dp);
      return;
    }

  src_service = services_tcp_find(src_port);
  dst_service = services_tcp_find(dst_port);
  chosen_port = choose_port (src_port, dst_port);

  if (!src_service && !dst_service)
    {
      if (pref.group_unk)
        decode_proto_add(dp, "TCP-UNKNOwN");
      else
        {
          if (chosen_port == src_port)
            decode_proto_add(dp, "TCP:%d-%d", chosen_port, dst_port);
          else
            decode_proto_add(dp, "TCP:%d-%d", chosen_port, src_port);
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

  decode_proto_add(dp, "%s", chosen_service->name);
}				/* get_tcp */

static void
get_udp (decode_proto_t *dp)
{
  const port_service_t *src_service, *dst_service, *chosen_service;
  port_type_t src_port, dst_port, chosen_port;
  gboolean src_pref = FALSE;
  gboolean dst_pref = FALSE;

  dp->global_src_port = src_port = pntohs (dp->packet + dp->offset);
  dp->global_dst_port = dst_port = pntohs (dp->packet + dp->offset + 2);

  dp->offset += 8;

  /* It's not possible to know in advance whether an UDP
   * packet is an RPC packet. We'll try */
  if (get_rpc (dp, TRUE))
    return;

  if (src_port==UDP_NETBIOS_NS || dst_port==UDP_NETBIOS_NS)
    {
      get_netbios_dgm (dp);
      return;
    }

  src_service = services_udp_find(src_port);
  dst_service = services_udp_find(dst_port);

  chosen_port = choose_port (src_port, dst_port);

  if (!dst_service && !src_service)
    {
      if (pref.group_unk)
        decode_proto_add(dp, "UDP-UNKNOWN");
      else
        {
          if (chosen_port == src_port)
            decode_proto_add(dp, "UDP:%d-%d", chosen_port, dst_port);
          else
            decode_proto_add(dp, "UDP:%d-%d", chosen_port, src_port);
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
  decode_proto_add(dp, "%s", chosen_service->name);
}				/* get_udp */

static gboolean
get_rpc (decode_proto_t *dp, gboolean is_udp)
{
  enum rpc_type msg_type;
  enum rpc_program msg_program;
  const gchar *rpc_prot = NULL;

  /* Determine whether this is an RPC packet */

  if ((dp->offset + 24) > dp->capture_len)
    return FALSE;		/* not big enough */

  if (is_udp)
    {
      msg_type = pntohl (dp->packet + dp->offset + 4);
      msg_program = pntohl (dp->packet + dp->offset + 12);
    }
  else
    {
      msg_type = pntohl (dp->packet + dp->offset + 8);
      msg_program = pntohl (dp->packet + dp->offset + 16);
    }

  if (msg_type != RPC_REPLY && msg_type != RPC_CALL)
    return FALSE;

  decode_proto_add(dp, "RPC");

  switch (msg_type)
    {
    case RPC_REPLY:
      /* RPC_REPLYs don't carry an rpc program tag */
      /* TODO In order to be able to dissect what is it's 
       * protocol I'd have to keep track of who sent
       * which call */
      if (!(rpc_prot = find_conversation (dp->global_dst_address, 0,
					  dp->global_dst_port, 0)))
	return FALSE;
      decode_proto_add(dp, rpc_prot);
      return TRUE;
    case RPC_CALL:
      switch (msg_program)
	{
	case BOOTPARAMS_PROGRAM:
	  decode_proto_add(dp, "BOOTPARAMS");
	  break;
	case MOUNT_PROGRAM:
	  decode_proto_add(dp, "MOUNT");
	  break;
	case NLM_PROGRAM:
	  decode_proto_add(dp, "NLM");
	  break;
	case PORTMAP_PROGRAM:
	  decode_proto_add(dp, "PORTMAP");
	  break;
	case STAT_PROGRAM:
	  decode_proto_add(dp, "STAT");
	  break;
	case NFS_PROGRAM:
	  decode_proto_add(dp, "NFS");
	  break;
	case YPBIND_PROGRAM:
	  decode_proto_add(dp, "YPBIND");
	  break;
	case YPSERV_PROGRAM:
	  decode_proto_add(dp, "YPSERV");
	  break;
	case YPXFR_PROGRAM:
	  decode_proto_add(dp, "YPXFR");
	  break;
	default:
	  return FALSE;
	}

      /* Search for an already existing conversation, if not, create one */
      if (!find_conversation (dp->global_src_address, 0, dp->global_src_port, 0))
	add_conversation (dp->global_src_address, 0,
			  dp->global_src_port, 0, rpc_prot);

      return TRUE;
    default:
      return FALSE;
    }
  return FALSE;
}				/* get_rpc */

/* This function is only called from a straight llc packet,
 * never from an IP packet */
void
get_netbios (decode_proto_t *dp)
{
  guint16 hdr_len;

  /* Check that there is room for the minimum header */
  if (dp->offset + 5 > dp->capture_len)
    return;

  hdr_len = pletohs (dp->packet + dp->offset);

  /* If there is any data at all, it is SMB (or so I understand
   * from Ethereal's packet-netbios.c */

  if (dp->offset + hdr_len < dp->capture_len)
    decode_proto_add(dp, "SMB");

}				/* get_netbios */

void
get_netbios_ssn (decode_proto_t *dp)
{
#define SESSION_MESSAGE 0
  guint8 mesg_type;

  decode_proto_add(dp, "NETBIOS-SSN");

  mesg_type = *(guint8 *) (dp->packet + dp->offset);

  if (mesg_type == SESSION_MESSAGE)
    decode_proto_add(dp, "SMB");

  /* TODO Calculate new dp->offset whenever we have
   * a "dissector" for a higher protocol */
}				/* get_netbions_ssn */

void
get_netbios_dgm (decode_proto_t *dp)
{
  guint8 mesg_type;

  decode_proto_add(dp, "NETBIOS-DGM");

  mesg_type = *(guint8 *) (dp->packet + dp->offset);

  /* Magic numbers copied from ethereal, as usual
   * They mean Direct (unique|group|broadcast) datagram */
  if (mesg_type == 0x10 || mesg_type == 0x11 || mesg_type == 0x12)
    decode_proto_add(dp, "SMB");

  /* TODO Calculate new dp->offset whenever we have
   * a "dissector" for a higher protocol */
}				/* get_netbios_dgm */

void
get_ftp (decode_proto_t *dp)
{
  gchar *mesg = NULL;
  guint size = dp->capture_len - dp->offset;
  gchar *str;
  guint hi_byte, low_byte;
  guint16 server_port;
  guint i = 0;

  decode_proto_add(dp, "FTP");
  if ((dp->offset + 3) > dp->capture_len)
    return;			/* not big enough */

  if ((gchar) dp->packet[dp->offset] != '2'
      || (gchar) dp->packet[dp->offset + 1] != '2'
      || (gchar) dp->packet[dp->offset + 2] != '7')
    return;

  /* We have a passive message. Get the port */
  mesg = g_malloc (size + 1);
  g_assert(mesg);

  memcpy (mesg, dp->packet + dp->offset, size);
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
  add_conversation (dp->global_src_address, dp->global_dst_address,
		    server_port, 0, "FTP-PASSIVE");

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
append_etype_prot (decode_proto_t *dp, etype_t etype)
{
  switch (etype)
    {
    case ETHERTYPE_IP:
      decode_proto_add(dp, "IP");
      break;
    case ETHERTYPE_ARP:
      decode_proto_add(dp, "ARP");
      break;
    case ETHERTYPE_IPv6:
      decode_proto_add(dp, "IPv6");
      break;
    case ETHERTYPE_X25L3:
      decode_proto_add(dp, "X25L3");
      break;
    case ETHERTYPE_REVARP:
      decode_proto_add(dp, "REVARP");
      break;
    case ETHERTYPE_ATALK:
      decode_proto_add(dp, "ATALK");
      break;
    case ETHERTYPE_AARP:
      decode_proto_add(dp, "AARP");
      break;
    case ETHERTYPE_IPX:
      break;
    case ETHERTYPE_VINES:
      decode_proto_add(dp, "VINES");
      break;
    case ETHERTYPE_TRAIN:
      decode_proto_add(dp, "TRAIN");
      break;
    case ETHERTYPE_LOOP:
      decode_proto_add(dp, "LOOP");
      break;
    case ETHERTYPE_PPPOED:
      decode_proto_add(dp, "PPPOED");
      break;
    case ETHERTYPE_PPPOES:
      decode_proto_add(dp, "PPPOES");
      break;
    case ETHERTYPE_VLAN:
      decode_proto_add(dp, "VLAN");
      break;
    case ETHERTYPE_SNMP:
      decode_proto_add(dp, "SNMP");
      break;
    case ETHERTYPE_DNA_DL:
      decode_proto_add(dp, "DNA-DL");
      break;
    case ETHERTYPE_DNA_RC:
      decode_proto_add(dp, "DNA-RC");
      break;
    case ETHERTYPE_DNA_RT:
      decode_proto_add(dp, "DNA-RT");
      break;
    case ETHERTYPE_DEC:
      decode_proto_add(dp, "DEC");
      break;
    case ETHERTYPE_DEC_DIAG:
      decode_proto_add(dp, "DEC-DIAG");
      break;
    case ETHERTYPE_DEC_CUST:
      decode_proto_add(dp, "DEC-CUST");
      break;
    case ETHERTYPE_DEC_SCA:
      decode_proto_add(dp, "DEC-SCA");
      break;
    case ETHERTYPE_DEC_LB:
      decode_proto_add(dp, "DEC-LB");
      break;
    case ETHERTYPE_MPLS:
      decode_proto_add(dp, "MPLS");
      break;
    case ETHERTYPE_MPLS_MULTI:
      decode_proto_add(dp, "MPLS-MULTI");
      break;
    case ETHERTYPE_LAT:
      decode_proto_add(dp, "LAT");
      break;
    case ETHERTYPE_PPP:
      decode_proto_add(dp, "PPP");
      break;
    case ETHERTYPE_WCP:
      decode_proto_add(dp, "WCP");
      break;
    case ETHERTYPE_3C_NBP_DGRAM:
      decode_proto_add(dp, "3C-NBP-DGRAM");
      break;
    case ETHERTYPE_ETHBRIDGE:
      decode_proto_add(dp, "ETHBRIDGE");
      break;
    case ETHERTYPE_UNK:
      decode_proto_add(dp, "ETH_UNKNOWN");
    }
}				/* append_etype_prot */

