
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

#include "basic_stats.h"
#include "node_id.h"

/* Information about each protocol heard on a link */
typedef struct
{
  gchar *name;			/* Name of the protocol */
  basic_stats_t stats;
  GList *node_names;		/* A list of all node names (name_t) used with
				 * this protocol (used in node protocols) */
} protocol_t;

protocol_t *protocol_t_create(const gchar *protocol_name);
void protocol_t_delete(protocol_t *prot);
/* returns a new string with a dump of prot */
gchar *protocol_t_dump(const protocol_t *prot);

typedef struct
{
  GList *protostack[STACK_SIZE + 1];	/* It's a stack. Each level is a list of 
                                         * all protocol_t heard at that level */
} protostack_t;

/* protocol stack methods */
void protocol_stack_open(protostack_t *pstk);
void protocol_stack_reset(protostack_t *pstk);
/* adds packet data to the stack */
void protocol_stack_add_pkt(protostack_t *pstk, const packet_info_t * packet);
/* subtracts packet data from stack */
void protocol_stack_sub_pkt(protostack_t *pstk, const packet_info_t * packet);
/* calculates averages */
void protocol_stack_avg(protostack_t *pstk, gdouble avg_usecs);
/* checks for protocol expiration ... */
void protocol_stack_purge_expired(protostack_t *pstk, double expire_time);
/* finds named protocol in the requested level of protostack*/
const protocol_t *protocol_stack_find(const protostack_t *pstk, size_t level, const gchar *protoname);
/* sorts on the most used protocol in the requested level and returns it */
gchar *protocol_stack_sort_most_used(protostack_t *pstk, size_t level);
/* returns a newly allocated string with a dump of pstk */
gchar *protocol_stack_dump(const protostack_t *pstk);


/* protocol summary method */
void protocol_summary_open(void); /* initializes the summary */
void protocol_summary_close(void); /* frees summary, releasing resources */
void protocol_summary_add_packet(packet_info_t *packet); /* adds a new packet to summary */
void protocol_summary_update_all(void); /* update stats on protocol summary */
long protocol_summary_size(void); /* total number of protos */
void protocol_summary_foreach(size_t level, GFunc func, gpointer data); /* calls func for every proto at level */
const protocol_t *protocol_summary_find(size_t level, const gchar *protoname); /* finds named protocol */
const protostack_t *protocol_summary_stack(void); /* access directly the stack (only for proto windows) */

#endif
