/* etypes.h
 * Defines ethernet packet types, similar to tcpdump's ethertype.h
 *
 * $Id$
 *
 * Etherape
 * Copyright 2000 Juan Toledo
 * 
 * Values and ideas copied from Ethereal
 *
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

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
    ETHERTYPE_ETHBRIDGE = 0x6558, /* transparent Ethernet bridging */
    ETHERTYPE_DEC_LB = 0x8038,
    ETHERTYPE_IPv6 = 0x086dd,
    ETHERTYPE_REVARP = 0x8035,
    ETHERTYPE_ATALK = 0x809b,
    ETHERTYPE_AARP = 0x80f3,
    ETHERTYPE_IPX = 0x8137,
    ETHERTYPE_VLAN = 0x8100,  /* 802.1Q Virtual LAN */
    ETHERTYPE_SNMP = 0x814c,  /* SNMP over Ethernet, RFC 1089 */
    ETHERTYPE_WCP = 0x80ff, /* Wellfleet Compression Protocol */
    ETHERTYPE_IPv6 = 0x86dd,
    ETHERTYPE_PPP = 0x880b, /* no, this is not PPPoE */
    ETHERTYPE_MPLS = 0x8847, /* MPLS unicast packet */
    ETHERTYPE_MPLS_MULTI = 0x8848, /* MPLS multicast packet */
    ETHERTYPE_PPPOED = 0x8863,	/* PPPoE Discovery Protocol */
    ETHERTYPE_PPPOES = 0x8864,	/* PPPoE Session Protocol */
    ETHERTYPE_LOOP = 0x9000,	/* used for layer 2 testing (do i see my own frames on the wire) */
  }
etype_t;
#endif /* etypes.h */
