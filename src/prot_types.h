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

#if 0
#include "tcpudp.h"
#endif

/* ETHERNET */

#ifndef __ETYPES_H__
#define __ETYPES_H__

/*
 * Maximum length of an IEEE 802.3 frame; Ethernet type/length values
 * greater than it are types, Ethernet type/length values less than or
 * equal to it are lengths.
 */
#define IEEE_802_3_MAX_LEN 1500

typedef enum
{
  ETHERNET_II = 0,
  ETHERNET_802_2 = 1,
  ETHERNET_802_3 = 2,
  ETHERNET_SNAP = 3
}
ethhdrtype_t;

typedef enum
{
  ETHERTYPE_UNK = 0x0000,
  ETHERTYPE_VINES = 0xbad,
  ETHERTYPE_TRAIN = 0x1984,
  ETHERTYPE_3C_NBP_DGRAM = 0x3c07,
  ETHERTYPE_IP = 0x0800,
  ETHERTYPE_X25L3 = 0x0805,
  ETHERTYPE_ARP = 0x0806,
  ETHERTYPE_DEC = 0x6000,
  ETHERTYPE_DNA_DL = 0x6001,
  ETHERTYPE_DNA_RC = 0x6002,
  ETHERTYPE_DNA_RT = 0x6003,
  ETHERTYPE_LAT = 0x6004,
  ETHERTYPE_DEC_DIAG = 0x6005,
  ETHERTYPE_DEC_CUST = 0x6006,
  ETHERTYPE_DEC_SCA = 0x6007,
  ETHERTYPE_ETHBRIDGE = 0x6558,	/* transparent Ethernet bridging */
  ETHERTYPE_DEC_LB = 0x8038,
  ETHERTYPE_REVARP = 0x8035,
  ETHERTYPE_ATALK = 0x809b,
  ETHERTYPE_AARP = 0x80f3,
  ETHERTYPE_IPX = 0x8137,
  ETHERTYPE_VLAN = 0x8100,	/* 802.1Q Virtual LAN */
  ETHERTYPE_SNMP = 0x814c,	/* SNMP over Ethernet, RFC 1089 */
  ETHERTYPE_WCP = 0x80ff,	/* Wellfleet Compression Protocol */
  ETHERTYPE_IPv6 = 0x86dd,
  ETHERTYPE_PPP = 0x880b,	/* no, this is not PPPoE */
  ETHERTYPE_MPLS = 0x8847,	/* MPLS unicast packet */
  ETHERTYPE_MPLS_MULTI = 0x8848,	/* MPLS multicast packet */
  ETHERTYPE_PPPOED = 0x8863,	/* PPPoE Discovery Protocol */
  ETHERTYPE_PPPOES = 0x8864,	/* PPPoE Session Protocol */
  ETHERTYPE_LOOP = 0x9000,	/* used for layer 2 testing (do i see my own frames on the wire) */
}
etype_t;

typedef enum
{
  SAP_NULL = 0x00,
  SAP_LLC_SLMGMT = 0x02,
  SAP_SNA_PATHCTRL = 0x04,
  SAP_IP = 0x06,
  SAP_SNA1 = 0x08,
  SAP_SNA2 = 0x0C,
  SAP_PROWAY_NM_INIT = 0x0E,
  SAP_TI = 0x18,
  SAP_BPDU = 0x42,
  SAP_RS511 = 0x4E,
  SAP_X25 = 0x7E,
  SAP_XNS = 0x80,
  SAP_NESTAR = 0x86,
  SAP_PROWAY_ASLM = 0x8E,
  SAP_SNAP = 0xAA,
  SAP_ARP = 0x98,
  SAP_VINES1 = 0xBA,
  SAP_VINES2 = 0xBC,
  SAP_NETWARE = 0xE0,
  SAP_NETBIOS = 0xF0,
  SAP_IBMNM = 0xF4,
  SAP_RPL1 = 0xF8,
  SAP_UB = 0xFA,
  SAP_RPL2 = 0xFC,
  SAP_OSINL = 0xFE,
  SAP_GLOBAL = 0xFF
}
sap_type_t;

typedef enum
{
  IPX_PACKET_TYPE_IPX = 0,
  IPX_PACKET_TYPE_RIP = 1,
  IPX_PACKET_TYPE_ECHO = 2,
  IPX_PACKET_TYPE_ERROR = 3,
  IPX_PACKET_TYPE_PEP = 4,
  IPX_PACKET_TYPE_SPX = 5,
  IPX_PACKET_TYPE_NCP = 17,
  IPX_PACKET_TYPE_WANBCAST = 20	/* propagated NetBIOS packet? */
}
ipx_type_t;

typedef enum
{
  IPX_SOCKET_PING_CISCO = 0x0002,	/* In cisco this is set with: ipx ping-default cisco */
  IPX_SOCKET_NCP = 0x0451,
  IPX_SOCKET_SAP = 0x0452,
  IPX_SOCKET_IPXRIP = 0x0453,
  IPX_SOCKET_NETBIOS = 0x0455,
  IPX_SOCKET_DIAGNOSTIC = 0x0456,
  IPX_SOCKET_SERIALIZATION = 0x0457,
  IPX_SOCKET_NWLINK_SMB_SERVER = 0x0550,
  IPX_SOCKET_NWLINK_SMB_NAMEQUERY = 0x0551,
  IPX_SOCKET_NWLINK_SMB_REDIR = 0x0552,
  IPX_SOCKET_NWLINK_SMB_MAILSLOT = 0x0553,
  IPX_SOCKET_NWLINK_SMB_MESSENGER = 0x0554,
  IPX_SOCKET_NWLINK_SMB_BROWSE = 0x0555,	/* ? not sure on this */
  IPX_SOCKET_ATTACHMATE_GW = 0x055d,
  IPX_SOCKET_IPX_MESSAGE = 0x4001,
  IPX_SOCKET_ADSM = 0x8522,	/* www.tivoli.com */
  IPX_SOCKET_EIGRP = 0x85be,	/* cisco ipx eigrp */
  IPX_SOCKET_WIDE_AREA_ROUTER = 0x9001,
  IPX_SOCKET_SNMP_AGENT = 0x900F,	/* RFC 1906 */
  IPX_SOCKET_SNMP_SINK = 0x9010,	/* RFC 1906 */
  IPX_SOCKET_PING_NOVELL = 0x9086,	/* In cisco this is set with: ipx ping-default novell */
  IPX_SOCKET_TCP_TUNNEL = 0x9091,	/* RFC 1791 */
  IPX_SOCKET_UDP_TUNNEL = 0x9092	/* RFC 1791 */
}
ipx_socket_t;

/* IP */

typedef enum
{
  IP_PROTO_IP = 0,		/* dummy for IP */
  IP_PROTO_HOPOPTS = 0,		/* IP6 hop-by-hop options */
  IP_PROTO_ICMP = 1,		/* control message protocol */
  IP_PROTO_IGMP = 2,		/* group mgmt protocol */
  IP_PROTO_GGP = 3,		/* gateway^2 (deprecated) */
  IP_PROTO_IPIP = 4,		/* IP inside IP */
  IP_PROTO_IPV4 = 4,		/* IP header */
  IP_PROTO_TCP = 6,		/* tcp */
  IP_PROTO_EGP = 8,		/* exterior gateway protocol */
  IP_PROTO_PUP = 12,		/* pup */
  IP_PROTO_UDP = 17,		/* user datagram protocol */
  IP_PROTO_IDP = 22,		/* xns idp */
  IP_PROTO_TP = 29,		/* tp-4 w/ class negotiation */
  IP_PROTO_IPV6 = 41,		/* IP6 header */
  IP_PROTO_ROUTING = 43,	/* IP6 routing header */
  IP_PROTO_FRAGMENT = 44,	/* IP6 fragmentation header */
  IP_PROTO_RSVP = 46,		/* Resource ReSerVation protocol */
  IP_PROTO_GRE = 47,		/* GRE */
  IP_PROTO_ESP = 50,		/* ESP */
  IP_PROTO_AH = 51,		/* AH */
  IP_PROTO_ICMPV6 = 58,		/* ICMP6 */
  IP_PROTO_NONE = 59,		/* IP6 no next header */
  IP_PROTO_DSTOPTS = 60,	/* IP6 no next header */
  IP_PROTO_EON = 80,		/* ISO cnlp */
  IP_PROTO_VINES = 83,		/* Vines over raw IP */
  IP_PROTO_EIGRP = 88,
  IP_PROTO_OSPF = 89,
  IP_PROTO_ENCAP = 98,		/* encapsulation header */
  IP_PROTO_PIM = 103,		/* Protocol Independent Mcast */
  IP_PROTO_IPCOMP = 108,	/* IP payload compression */
  IP_PROTO_VRRP = 112		/* Virtual Router Redundancy Protocol */
} iptype_t;

typedef guint16 port_type_t;

#endif  /* __ETYPES_H__ */
