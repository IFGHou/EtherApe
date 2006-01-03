/* EtherApe
 * Copyright (C) 2001 Juan Toledo
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

#ifndef ETHERAPE_GLOBALS_H
#define ETHERAPE_GLOBALS_H

#include <gnome.h>
#include <glib.h>
#include <libgnomecanvas/gnome-canvas.h>
#include <libgnomeui/gnome-entry.h>
#include <sys/time.h>
#include <pcap.h>
#include <glade/glade.h>
#include <string.h>
#include <arpa/nameser.h>
#include "links.h"

#define ETHERAPE_GLADE_FILE "etherape.glade2"	// glade 2 file

#ifndef MAXDNAME
#define MAXDNAME        1025	/* maximum domain name length */
#endif


/* Enumerations */

/* Since gdb does understand enums and not defines, and as 
 * way to make an easier transition to a non-pcap etherape,
 * I define my own enum for link type codes */
/* See /usr/include/net/bpf.h for a longer description of
 * some of the options */
typedef enum
{
  L_NULL = DLT_NULL,		/* no link-layer encapsulation */
  L_EN10MB = DLT_EN10MB,	/* Ethernet (10Mb) */
  L_EN3MB = DLT_EN3MB,		/* Experimental Ethernet (3Mb) */
  L_AX25 = DLT_AX25,		/* Amateur Radio AX.25 */
  L_PRONET = DLT_PRONET,	/* Proteon ProNET Token Ring */
  L_CHAOS = DLT_CHAOS,		/* Chaos */
  L_IEEE802 = DLT_IEEE802,	/* IEEE 802 Networks */
  L_ARCNET = DLT_ARCNET,	/* ARCNET */
  L_SLIP = DLT_SLIP,		/* Serial Line IP */
  L_PPP = DLT_PPP,		/* Point-to-point Protocol */
  L_FDDI = DLT_FDDI,		/* FDDI */
  L_ATM_RFC1483 = DLT_ATM_RFC1483,	/* LLC/SNAP encapsulated atm */
  L_RAW = DLT_RAW,		/* raw IP */
  L_SLIP_BSDOS = DLT_SLIP_BSDOS,	/* BSD/OS Serial Line IP */
  L_PPP_BSDOS = DLT_PPP_BSDOS,	/* BSD/OS Point-to-point Protocol */
#ifdef DLT_ATM_CLIP
  L_ATM_CLIP = DLT_ATM_CLIP,	/* Linux Classical-IP over ATM */
#endif
#ifdef DLT_PPP_SERIAL
  L_PPP_SERIAL = DLT_PPP_SERIAL,	/* PPP over serial with HDLC encapsulation */
#endif
#ifdef DLT_C_HDLC
  L_C_HDLC = DLT_C_HDLC,	/* Cisco HDLC */
#endif
#ifdef DLT_IEEE802_11
  L_IEEE802_11 = DLT_IEEE802_11,	/* IEEE 802.11 wireless */
#endif
#ifdef DLT_LOOP
  L_LOOP = DLT_LOOP,		/* OpenBSD loopback */
#endif
#ifdef DLT_LINUX_SLL
  L_LINUX_SLL = DLT_LINUX_SLL	/* Linux cooked sockets */
#endif
}
link_type_t;

typedef enum
{
  /* Beware! The value given by the option widget is dependant on
   * the order set in glade! */
  LINEAR = 0,
  LOG = 1,
  SQRT = 2
}
size_mode_t;

typedef enum
{
  INST_TOTAL = 0,
  INST_INBOUND = 1,
  INST_OUTBOUND = 2,
  ACCU_TOTAL = 3,
  ACCU_INBOUND = 4,
  ACCU_OUTBOUND = 5
}
node_size_variable_t;

/* Possible states of capture status */
enum status_t
{
  STOP = 0, PLAY = 1, PAUSE = 2
};


/* Variables */

GladeXML *xml;
GtkWidget *app1;		/* Pointer to the main app window */
struct timeval now;		/* Set both at each packet capture and 
				 * in each redraw of the diagram */
gdouble n_packets;		/* Number of total packets received */
gdouble n_mem_packets;		/* Number of packets currently in memory */

link_type_t linktype;		/* Type of device we are listening to */
guint l3_offset;		/* Offset to the level 3 protocol data
				 * Depends of the linktype */
enum status_t status;		/* Keeps capture status (playing, stopped, paused) */
gboolean end_of_file;		/* Marks that the end of the offline file
				 * has been reached */
gboolean need_reposition;	/* Force a diagram relayout */
gint diagram_timeout;		/* Descriptor of the diagram timeout function
				 * (Used to change the refresh_period in the callback */
GList *legend_protocols;

struct
{

/* General settings */

  gchar *input_file;		/* Capture file to read from */
  gboolean name_res;		/* Whether dns lookups are performed */
  apemode_t mode;		/* Mode of operation. Can be
				 * ETHERNET, IP, UDP or TCP */

/* Diagram settings */

  gboolean diagram_only;	/* Do not use text on the diagram */
  gboolean group_unk;		/* Whether to display as one every unkown port protocol */
  gboolean nofade;		/* Do not fade unused links */
  gboolean antialias;		/* activate node antialiasing */
  gboolean cycle;		/* Weather to reuse colors that are assigned to certain
				 * protocols */
  gboolean stationary;		/* Use alternative algorith for node placing */
  guint32 refresh_period;	/* Time between diagram refreshes */
  gdouble node_radius_multiplier;	/* used to calculate the radius of the
					 * displayed nodes. So that the user can
					 * select with certain precision this
					 * value, the GUI uses the log10 of the
					 * multiplier */
  gdouble link_width_multiplier;	/* Same explanation as above */
  size_mode_t size_mode;	/* Default mode for node size and
				 * link width calculation */
  node_size_variable_t node_size_variable;	/* Default variable that sets the node
						 * size */
  gchar *node_color, *text_color;	/* Default colors */
  gchar *fontname;		/* Font to be used for text display */
  guint stack_level;		/* Which level of the protocol stack 
				 * we will concentrate on */
  gint node_limit;		/* Max number of nodes to show. If <0 it's not
				 * limited */
  gdouble gui_node_timeout_time;	/* After this time has passed with no traffic
					 * for a node, it disappears from the diagram */
  gint n_colors;		/* Numbers of colors to be used on the diagram */
  gchar **colors;		/* list of colors to be used on the diagram. Format is
				 * color[;protocol] [color[;protocol] ...
				 * where color is represented by sis hex digits (RGB) */

/* Capture settings */

  gdouble averaging_time;	/* Microseconds of time we consider to
				 * calculate traffic averages */
  gdouble link_timeout_time;	/* After this time
				 * has passed with no traffic in a 
				 * link, it disappears */
  gdouble node_timeout_time;	/* After this time has passed 
				 * with no traffic in/out a
				 * node, it is deleted from memory */
  gchar *interface;		/* Network interface to listen to */
  gchar *filter;		/* Pcap filter to be used */

}
pref;


/* Global functions declarations */

/* From menus.c */
void init_menus (void);
void fatal_error_dialog (const gchar * message);
void gui_start_capture (void);
void gui_pause_capture (void);
gboolean gui_stop_capture (void);	/* gui_stop_capture might fail. For instance,
					 * it can't run if diagram_update is running */

/* Macros */

#define g_my_debug(format, args...)      g_log (G_LOG_DOMAIN, \
						  G_LOG_LEVEL_DEBUG, \
						  format, ##args)
#define g_my_info(format, args...)      g_log (G_LOG_DOMAIN, \
						  G_LOG_LEVEL_INFO, \
						  format, ##args)
#define g_my_critical(format, args...)      g_log (G_LOG_DOMAIN, \
						      G_LOG_LEVEL_CRITICAL, \
						      format, ##args)

#define IS_OLDER(diff, timeout)        ((diff.tv_sec > timeout/1000) | \
					    (diff.tv_usec/1000 >= timeout))



/* Pointer versions of ntohs and ntohl.  Given a pointer to a member of a
 * byte array, returns the value of the two or four bytes at the pointer.
 */
#define pntohs(p)  ((guint16)                       \
		       ((guint16)*((guint8 *)p+0)<<8|  \
			   (guint16)*((guint8 *)p+1)<<0))

#define pntohl(p)  ((guint32)*((guint8 *)p+0)<<24|  \
		       (guint32)*((guint8 *)p+1)<<16|  \
		       (guint32)*((guint8 *)p+2)<<8|   \
		       (guint32)*((guint8 *)p+3)<<0)

#define pletohs(p)  ((guint16)                       \
			((guint16)*((guint8 *)(p)+1)<<8|  \
			    (guint16)*((guint8 *)(p)+0)<<0))


/* Takes the hi_nibble value from a byte */
#define hi_nibble(b) ((b & 0xf0) >> 4)

#endif
