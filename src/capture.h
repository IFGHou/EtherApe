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


/* Used on some functions to indicate how to operate on the node info
 * depending on what side of the comm the node was at */
typedef enum
{
  SRC = 0,
  DST = 1
}
create_node_type_t;

static pcap_t *pch;		/* pcap structure */
static gint capture_timeout;	/* timeout function used when 
				 * reading capture data from
				 * a file */
static struct pcap_pkthdr phdr;



/* Local funtions declarations */
guint get_offline_packet (void);
static void packet_read (guint8 * packet, gint source,
			 GdkInputCondition condition);
static guint8 *get_node_id (const guint8 * packet,
			    create_node_type_t node_type);
static guint8 *get_link_id (const guint8 * packet);
static node_t *create_node (const guint8 * packet, const guint8 * node_id);
static link_t *create_link (const guint8 * packet, const guint8 * link_id);
static void dns_ready (gpointer data, gint fd, GdkInputCondition cond);
static void add_node_packet (const guint8 * packet,
			     struct pcap_pkthdr phdr,
			     const guint8 * node_id,
			     const gchar * prot, packet_direction direction);
static void add_link_packet (const guint8 * packet,
			     struct pcap_pkthdr phdr,
			     const guint8 * link_id, const gchar * prot);
void add_protocol (GList ** protocols, const gchar * stack,
		   struct pcap_pkthdr phdr);
static void update_node_names (node_t * node);
static void set_node_name (node_t * node, gchar * preferences);
static gchar *get_main_prot (GList * packets,
			     GList ** protocols, guint level);
static GList *check_packet (GList * packets, enum packet_belongs belongs_to);
static gint prot_freq_compare (gconstpointer a, gconstpointer b);
static gint names_freq_compare (gconstpointer a, gconstpointer b);
gchar *print_mem (const guint8 * ad, guint length);
