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

#include <pcap.h>
#include "globals.h"

#define MAXSIZE 60
#define PCAP_TIMEOUT 250

/* 
 * LOCAL ENUMERATIONS
 */

/* Since gdb does understand enums and not defines, and as 
 * way to make an easier transition to a non-pcap etherape,
 * I define my own enum for link type codes */
typedef enum
  {
    L_NULL = DLT_NULL,		/* no link-layer encapsulation */
    L_EN10MB = DLT_EN10MB,	/* Ethernet (10Mb) */
    L_EN3MB = DLT_EN3MB,	/* Experimental Ethernet (3Mb) */
    L_AX25 = DLT_AX25,		/* Amateur Radio AX.25 */
    L_PRONET = DLT_PRONET,	/* Proteon ProNET Token Ring */
    L_CHAOS = DLT_CHAOS,	/* Chaos */
    L_IEEE802 = DLT_IEEE802,	/* IEEE 802 Networks */
    L_ARCNET = DLT_ARCNET,	/* ARCNET */
    L_SLIP = DLT_SLIP,		/* Serial Line IP */
    L_PPP = DLT_PPP,		/* Point-to-point Protocol */
    L_FDDI = DLT_FDDI,		/* FDDI */
    L_ATM_RFC1483 = DLT_ATM_RFC1483,	/* LLC/SNAP encapsulated atm */
    L_RAW = DLT_RAW,		/* raw IP */
    L_SLIP_BSDOS = DLT_SLIP_BSDOS,	/* BSD/OS Serial Line IP */
    L_PPP_BSDOS = DLT_PPP_BSDOS	/* BSD/OS Point-to-point Protocol */
  }
link_type_t;

/* Used on some functions to indicate how to operate on the node info
 * depending on what side of the comm the node was at */
typedef enum
  {
    SRC = 0,
    DST = 1
  }
create_node_type_t;

pcap_t *pch;			/* pcap structure */


/* Local funtions declarations */
static void packet_read (pcap_t * pch, gint source,
			 GdkInputCondition condition);
static guint8 *get_node_id (const guint8 * packet,
			    create_node_type_t node_type);
static guint8 *get_link_id (const guint8 * packet);
static node_t *create_node (const guint8 * packet,
			    const guint8 * node_id);
static link_t *create_link (const guint8 * packet,
			    const guint8 * link_id);
static void fill_names (node_t * node, const guint8 * node_id,
			const guint8 * packet);
static void dns_ready (gpointer data, gint fd,
		       GdkInputCondition cond);
static void add_node_packet (const guint8 * packet,
			     struct pcap_pkthdr phdr,
			     const guint8 * node_id);
static void add_link_packet (const guint8 * packet,
			     struct pcap_pkthdr phdr,
			     const guint8 * link_id);
void add_protocol (GList ** protocols, gchar * stack,
		   struct pcap_pkthdr phdr,
		   gboolean is_link);
static gchar *get_main_prot (GList * packets, link_t * link, guint level);
static GList *check_packet (GList * packets,
			    enum packet_belongs belongs_to);
static gint prot_freq_compare (gconstpointer a, gconstpointer b);
