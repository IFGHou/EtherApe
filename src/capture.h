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

#define MAXSIZE 200
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
static struct pcap_pkthdr phdr;

static guint32 ms_to_next;	/* Used for offline mode to store the amount
				 * of time that we have to wait between
				 * one packet and the next */
gint pcap_fd;			/* The file descriptor used by libpcap */
gint capture_source;		/* It's the input tag or the timeout tag,
				 * in online or offline mode */
static gint dns_fd = 0;		/* The file descriptor used by dns.c */
static GList *new_nodes = NULL;	/* List that contains every new node not yet
				 * acknowledged by the main app with
				 * ape_get_new_node */

/* Local funtions declarations */
static guint get_offline_packet (void);
static void cap_t_o_destroy (gpointer data);
static void packet_read (guint8 * packet, gint source,
			 GdkInputCondition condition);

static guint8 *get_node_id (const guint8 * packet,
			    create_node_type_t node_type);
static guint8 *get_link_id (const guint8 * packet);

static node_t *create_node (const guint8 * packet, const guint8 * node_id);
static link_t *create_link (const guint8 * packet, const guint8 * link_id);

static void add_node_packet (const guint8 * packet,
			     packet_t * packet_info,
			     const guint8 * node_id,
			     packet_direction direction);
static void add_link_packet (const guint8 * packet,
			     packet_t * packet_info, const guint8 * link_id);
void add_protocol (GList ** protocols, const gchar * stack,
		   struct pcap_pkthdr phdr, packet_t * packet_info);

static gint update_node_packets (guint8 * node_id, node_t * node,
				 gpointer pointer);
static gint update_node (guint8 * node_id, node_t * node, gpointer pointer);
static gint update_link (guint8 * link_id, link_t * link, gpointer pointer);
static gboolean update_protocol (protocol_t * protocol);


static void update_node_names (node_t * node);
static void set_node_name (node_t * node, gchar * preferences);


static void update_packet_list (GList * packets, gpointer parent,
				enum packet_belongs belongs_to);
static gboolean check_packet (GList * packets, GList ** packet_l_e,
			      gpointer parent,
			      enum packet_belongs belongs_to);

static void forget_node_from_protocols (guint8 * node_id);
static gchar *get_main_prot (GList * packets,
			     GList ** protocols, guint level);
static gint prot_freq_compare (gconstpointer a, gconstpointer b);
static gint names_freq_compare (gconstpointer a, gconstpointer b);
gchar *print_mem (const guint8 * ad, guint length);
static void dns_ready (gpointer data, gint fd, GdkInputCondition cond);
