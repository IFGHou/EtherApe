
/* Etherape
 * Copyright (C) 2005 Juan Toledo, R.Ghetta
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

#ifndef PROTOCOLS_H
#define PROTOCOLS_H


/* Information about each protocol heard on a link */
typedef struct
{
  gchar *name;			/* Name of the protocol */
  gdouble average;		/* Average bytes in or out in the last x ms */
  gdouble aver_accu;		/* Accumulated bytes in the last x ms */
  gdouble accumulated;		/* Accumulated traffic in bytes for this protocol */
  guint proto_packets;		/* Number of packets containing this protocol */
  GdkColor color;		/* The color associated with this protocol. It's here
				 * so that I can use the same structure and lookup functions
				 * in capture.c and diagram.c */
  GList *node_names;		/* Has a list of all node names used with this
				 * protocol (used in node protocols) */
  struct timeval last_heard;	/* The last at which this protocol carried traffic */
}
protocol_t;

protocol_t *protocol_t_create(const gchar *protocol_name);
void protocol_t_delete(protocol_t *prot);

gint protocol_compare (gconstpointer a, gconstpointer b);




/* protocol stack methods */
void protocol_stack_init(GList *protostack[]);
void protocol_stack_free(GList *protostack[]);
/* adds packet data to the stack */
void protocol_stack_add_pkt(GList *protostack[], const packet_info_t * packet);
/* subtracts packet data from stack */
void protocol_stack_sub_pkt(GList *protostack[], const packet_info_t * packet, gboolean purge_entry);
/* finds named protocol in the level protocols of protostack*/
const protocol_t *protocol_stack_find(GList *protostack[], size_t level, const gchar *protoname);

gchar *get_main_prot (GList ** protocols, guint level);
void update_node_names (node_t * node);


typedef struct 
{
  guint n_packets;		/* Number of active packets linked to some proto*/
  GList *packets;		/* List of active packets */
  
  GList *protostack[STACK_SIZE + 1];	/* It's a stack. Each level is a list of 
                                         * all protocols heard at that level */
} protocol_summary_t;

/* protocol summary method */
void protocol_summary_open(void); /* initializes the summary */
void protocol_summary_close(void); /* frees summary, releasing resources */
void protocol_summary_add_packet(packet_info_t *packet); /* adds a new packet to summary */
void protocol_summary_update_all(void); /* update stats on protocol summary */
guint protocol_summary_size(size_t level); /* number of protos at specified level */
void protocol_summary_foreach(size_t level, GFunc func, gpointer data); /* calls func for every proto at level */
const protocol_t *protocol_summary_find(size_t level, const gchar *protoname); /* finds named protocol */

#endif
