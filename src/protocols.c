/* Etherape
 * Copyright (C) 2000 Juan Toledo
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
#include "etypes.h"
#include "protocols.h"

static GString *prot;

gchar *
get_packet_prot (const guint8 * packet)
{
  if (prot)
    g_string_free (prot, TRUE);
  prot = NULL;

  switch (mode)
    {
    case ETHERNET:
      get_eth_type (packet);
      break;
    case IP:
    case TCP:
    default:
      prot = g_string_new ("UNKNOWN");
      break;
    }

  return prot->str;
}				/* get_packet_prot */


static void
get_eth_type (const guint8 * packet)
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
	  prot = g_string_new ("ISL");
	  return;
	}
    }

  if (ethhdr_type == ETHERNET_802_3)
    {
      prot = g_string_new ("802.3");
      return;
    }

  if (ethhdr_type == ETHERNET_802_2)
    {
      prot = g_string_new ("802.2");
      return;
    }

  /* Else, it's ETHERNET_II */

  prot = g_string_new ("ETH_II");
  get_eth_II (packet, etype);
  return;

}				/* get_eth_type */

static void
get_eth_II (const guint8 * packet, etype_t etype)
{
  /* TODO We are just considering EthernetII here
   * I guess I'll have to do the Right Thing some day. :-) */
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
