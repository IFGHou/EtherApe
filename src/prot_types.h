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

#include "tcpudp.h"

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
  ETHERTYPE_IP = 0x0800,
  ETHERTYPE_IPv6 = 0x086dd,
  ETHERTYPE_ARP = 0x0806,
  ETHERTYPE_X25L3 = 0x0805,
  ETHERTYPE_REVARP = 0x8035,
  ETHERTYPE_ATALK = 0x809b,
  ETHERTYPE_AARP = 0x80f3,
  ETHERTYPE_IPX = 0x8137,
  ETHERTYPE_VINES = 0xbad,
  ETHERTYPE_TRAIN = 0x1984,
  ETHERTYPE_LOOP = 0x9000,	/* used for layer 2 testing (do i see my own frames on the wire) */
  ETHERTYPE_PPPOED = 0x8863,	/* PPPoE Discovery Protocol */
  ETHERTYPE_PPPOES = 0x8864,	/* PPPoE Session Protocol */
  ETHERTYPE_VLAN = 0x8100,	/* 802.1Q Virtual LAN */
  ETHERTYPE_SNMP = 0x814c	/* SNMP over Ethernet, RFC 1089 */
}
etype_t;
#endif

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
}
iptype_t;
