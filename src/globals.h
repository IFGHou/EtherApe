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

#include "common.h"
#include <glade/glade.h>
#include <gtk/gtk.h>

/* Variables */

GladeXML *xml;
GtkWidget *app1;		/* Pointer to the main app window */
GtkStatusbar *statusbar;        /* Main window statusbar */
struct timeval now;		/* Set both at each packet capture and 
				 * in each redraw of the diagram */
gdouble n_packets;		/* Number of total packets received */
gdouble total_mem_packets;		/* Number of packets currently in memory */

link_type_t linktype;		/* Type of device we are listening to */
gboolean need_reposition;	/* Force a diagram relayout */
gint diagram_timeout;		/* Descriptor of the diagram timeout function
				 * (Used to change the refresh_period in the callback */

struct pref_struct
{

/* General settings */

  gchar *input_file;		/* Capture file to read from */
  gboolean name_res;		/* Whether dns lookups are performed */
  apemode_t mode;		/* Mode of operation. Can be
				 * ETHERNET, IP, UDP or TCP */

/* Diagram settings */

  gboolean diagram_only;	/* Do not use text on the diagram */
  gboolean group_unk;		/* Whether to display as one every unkown port protocol */
  gboolean cycle;		/* Weather to reuse colors that are assigned to certain
				 * protocols */
  gboolean stationary;		/* Use alternative algorith for node placing */
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
  gchar *text_color;		/* Default text color */
  gchar *fontname;		/* Font to be used for text display */
  guint stack_level;		/* Which level of the protocol stack 
				 * we will concentrate on */
  gint node_limit;		/* Max number of nodes to show. If <0 it's not
				 * limited */

  /* after this time has passed without traffic on a protocol, it's removed
   * from the global protocol stats */
  gdouble proto_timeout_time;

  /* After this time has passed with no traffic for a node, it 
   * disappears from the diagram */
  gdouble gui_node_timeout_time;

  /* After this time has passed with no traffic for a node, it 
  * is deleted from memory */
  gdouble node_timeout_time;

  /* after this time has passed without traffic on a protocol, it's removed
   * from the node protocol stats */
  gdouble proto_node_timeout_time;

  gint n_colors;		/* Numbers of colors to be used on the diagram */
  gchar **colors;		/* list of colors to be used on the diagram. Format is
				 * color[;protocol] [color[;protocol] ...
				 * where color is represented by sis hex digits (RGB) */

  /* After this time has passed with no traffic for a link, it 
   * disappears from the diagram */
  gdouble gui_link_timeout_time;
  
  /* After this time has passed with no traffic for a link, it 
  * is deleted from memory */
  gdouble link_timeout_time;

  /* after this time has passed without traffic on a protocol, it's removed
   * from the link protocol stats */
  gdouble proto_link_timeout_time;

  guint32 refresh_period;	/* Time between diagram refreshes */
  gdouble averaging_time;	/* Microseconds of time we consider to
				 * calculate traffic averages */

  gchar *interface;		/* Network interface to listen to */
  gchar *filter;		/* Pcap filter to be used */

  gboolean is_debug;    /* TRUE if DEBUG specified on environment */

  gboolean zero_delay;  /* TRUE if delay processing tcpdump files is disabled */

  gchar *glade_file;    /* fullspec of xml glade file */
}
pref;

#endif
