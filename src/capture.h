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

/* Information about each packet heard on the network */
typedef struct
{
  packet_info_t info;           /* common informations */
  guint ref_count;		/* How many structures are referencing this 
				 * packet. When the count reaches zero the packet
				 * is deleted */
  node_id_t src_id;		/* Source and destination ids for the packet */
  node_id_t dst_id;		/* Useful to identify direction in node_packets */
}
packet_t;

/* From capture.c */
gchar *init_capture (void);
gboolean start_capture (void);
gboolean pause_capture (void);
gboolean stop_capture (void);
void cleanup_capture (void);
gint set_filter (gchar * filter, gchar * device);

/* removes a packet from a list of packets, destroying it if necessary
 * Returns the PREVIOUS item if any, otherwise the NEXT, thus returning NULL
 * if the list is empty */
GList *
packet_list_remove(GList *item_to_remove);
/* returns FALSE is all packets are expired */
gboolean update_packet_list (GList * packets, gpointer parent,
				enum packet_belongs belongs_to);
gchar *get_main_prot (GList ** protocols, guint level);

void update_nodes (void);
void update_protocols (void);
node_t *ape_get_new_node (void);	/* Returns a new node that hasn't been heard of */
gint protocol_compare (gconstpointer a, gconstpointer b);
