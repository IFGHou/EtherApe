/* EtherApe
 * Copyright (C) 2001 Juan Toledo, Riccardo Ghetta
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

#ifndef ETHERAPE_NODE_H
#define ETHERAPE_NODE_H

#define STACK_SIZE 5		/* How many protocol levels to keep
				 * track of (+1) */

struct timeval substract_times (struct timeval a, struct timeval b);


typedef struct
{
  gdouble average;		/* Average bytes in or out in the last x ms */
  gdouble accumulated;		/* Accumulated bytes */
  gdouble aver_accu;		/* Accumulated bytes in the last x ms */
}
traffic_stats_t;
void traffic_stats_reset(traffic_stats_t *tf_stat); /* resets counters */


/* Flag used in node packets to indicate whether this packet was
 * inbound or outbound for the parent node */
typedef enum
{
  INBOUND = 0, OUTBOUND = 1
}
packet_direction;

/* Information about each packet heard on the network */
typedef struct
{
  guint size;			/* Size in bytes of the packet */
  struct timeval timestamp;	/* Time at which the packet was heard */
  gchar *prot_desc;			/* Packet protocol tree */
  packet_direction dir;         /* packet direction - used for nodes */
  guint ref_count;		/* How many structures are referencing this 
				 * packet. When the count reaches zero the packet
				 * is deleted */
}
packet_info_t;



typedef struct
{
  GList *cur_pkt_list;          /* current packets - private */
  gdouble n_packets;		/* Number of total packets received */
  struct timeval last_time;	/* Timestamp of the last packet to be added */
  traffic_stats_t stats;        /* total traffic stats */
}
packet_stats_t;

void packet_stats_initialize(packet_stats_t *pkt_stat); /* initializes counters */
void packet_stats_release(packet_stats_t *pkt_stat); /* releases memory */
void packet_stats_add_packet(packet_stats_t *pkt_stat, packet_info_t *new_pkt); /* adds a packet */


typedef enum
{
  DEFAULT = -1,
  ETHERNET = 0,
  FDDI = 1,
  IEEE802 = 2,
  IP = 3,
  IPX = 4,
  TCP = 5,
  UDP = 6
}
apemode_t;

/* address union */
typedef union __attribute__ ((packed))
{
  guint8 eth[6] ;                 /* ethernet address */
  guint8 fddi[6];                /* ffdi address */
  guint8 i802[6];                /* ieee802 address */
  guint8 ip4[4];                  /* ip address */
  struct __attribute__ ((packed))
  {
      guint8 host[4];            /* tcp/udp address */
      guint8 port[2];            /* port number */
  } tcp4;

} 
node_addr_t;

/* a node identification */
typedef struct
{
  apemode_t node_type;
  node_addr_t addr;
} 
node_id_t;
gint node_id_compare (const node_id_t *a, const node_id_t *b);


typedef struct
{
  node_id_t node_id;		/* node identification */
  GString *name;		/* String with a readable default name of the node */
  GString *numeric_name;	/* String with a numeric representation of the id */
  gdouble average;		/* Average bytes in or out in the last x ms */
  gdouble average_in;		/* Average bytes in in the last x ms */
  gdouble average_out;		/* Average bytes out in the last x ms */
  gdouble accumulated;		/* Accumulated bytes */
  gdouble accumulated_in;	/* Accumulated incoming bytes */
  gdouble accumulated_out;	/* Accumulated outcoming bytes */
  gdouble aver_accu;		/* Accumulated bytes in the last x ms */
  gdouble aver_accu_in;		/* Accumulated incoming bytes in the last x ms */
  gdouble aver_accu_out;	/* Accumulated outcoming bytes in the last x ms */
  gdouble n_packets;		/* Number of total packets received */
  struct timeval last_time;	/* Timestamp of the last packet to be added */
  GList *packets;		/* List of packets sizes in or out and
				 * its sizes. Used to calculate average
				 * traffic */
  gchar *main_prot[STACK_SIZE + 1];	/* Most common protocol for the node */
  GList *protocols[STACK_SIZE + 1];	/* It's a stack. Each level is a list of
					 * all protocols heard at that level */
}
node_t;

void node_dump(node_t * node);
void node_delete(node_t *node); /* destroys a node, releasing memory */

/* nodes catalog methods */
void nodes_catalog_open(void); /* initializes the catalog */
void nodes_catalog_close(void); /* closes the catalog, releasing all nodes */
node_t *nodes_catalog_find(const node_id_t *key); /* finds a node */
void nodes_catalog_insert(node_t *new_node); /* inserts a new node */
void nodes_catalog_remove(const node_id_t *key); /* removes AND DESTROYS the named node from catalog */
gint nodes_catalog_size(void); /* returns the current number of nodes in catalog */
void nodes_catalog_foreach(GTraverseFunc func, gpointer data); /* calls the func for every node */


typedef struct
{
  node_id_t node_id;
  GString *name;
  GString *numeric_name;
  gboolean solved;
  gdouble accumulated;
  gdouble n_packets;
}
name_t;
name_t * node_name_create(const node_id_t *node_id);
void node_name_delete(name_t * name);
void node_name_assign(name_t * name, const gchar *nm, const gchar *num_nm, 
                 gboolean slv, gdouble sz);
gint node_name_id_compare(const name_t *a, const name_t *b);
gint node_name_freq_compare (gconstpointer a, gconstpointer b);


/* Information about each protocol heard on a link */
typedef struct
{
  gchar *name;			/* Name of the protocol */
  gdouble average;		/* Average bytes in or out in the last x ms */
  gdouble aver_accu;		/* Accumulated bytes in the last x ms */
  gdouble accumulated;		/* Accumulated traffic in bytes for this protocol */
  guint n_packets;		/* Number of packets containing this protocol */
  GList *packets;		/* List of packets that used this protocol */
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

GList *all_protocols[STACK_SIZE + 1];	/* It's a stack. Each level is a list of 
					 * all protocols heards at that level */


#endif
