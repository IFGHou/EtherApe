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

gboolean numeric;
gboolean dns;
gboolean diagram_only;
gboolean nofade;
gchar *interface;
guint32 refresh_period;
gint diagram_timeout;
gchar *filter;

GtkWidget *app1;		/* Pointer to the main app window */
GtkWidget *diag_pref;		/* Pointer to the diagram configuration window */

double node_radius_multiplier;		/* used to calculate the radius of the
						 * displayed nodes. So that the user can
						 * select with certain precision this
						 * value, the GUI uses the log10 of the
						 * multiplier*/
double link_width_multiplier;	/* Same explanation as above */

typedef enum
  {
    /* Beware! The value given by the option widget is dependant on
     * the order set in glade! */
    LINEAR = 0,
    LOG = 1,
    SQRT = 2
  }
size_mode_t;


size_mode_t size_mode;	/* Default mode for node size and
				 * link width calculation */

gchar *node_color, *link_color, *text_color;
gchar *fontname;


GTree *canvas_nodes;		/* We don't use the nodes tree directly in order to 
				 * separate data from presentation: that is, we need to
				 * keep a list of CanvasItems, but we do not want to keep
				 * that info on the nodes tree itself */
GTree *canvas_links;		/* See above */

gboolean need_reposition;	/* It is set when a canvas_node has been added 
				 * or deleted */

double averaging_time;	/* Microseconds of time we consider to
					 * calculate traffic averages */
double link_timeout_time;	/* After this time
					 * has passed with no traffic in a 
					 * link, it disappears */
double node_timeout_time;	/* After this time has passed 
					 * with no traffic in/out a 
					 * node, it disappears */
struct timeval now;

/* Global variables */
guint node_id_length;		/* Length of the node_id key. Depends
				 * on the mode of operation */
apemode_t mode;	/* Mode of operation. Can be
				   * ETHERNET, IP, UDP or TCP */
link_type_t linktype;		/* Type of device we are listening to */
guint l3_offset;		/* Offset to the level 3 protocol data
				 * Depends of the linktype */
