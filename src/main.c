/* Etherape
 * Copyright (C) 2000 Juan Toledo
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>

#include "interface.h"
#include "support.h"
#include "diagram.h"
#include "callbacks.h"

/* TODO Organize global variables in a sensible way */
gboolean numeric = 0;
gboolean dns = 0;
gboolean diagram_only = 0;
gboolean fix_overlap = 0;
gchar *interface;
guint32 refresh_period = 800;
gint diagram_timeout;
gchar *filter = "";

extern gchar *node_color, *link_color, *text_color;
extern double node_timeout_time, link_timeout_time, averaging_time, node_radius_multiplier,
  link_width_multiplier;
extern apemode_t mode;


int
main (int argc, char *argv[])
{
  GtkWidget *app1;
  GtkWidget *hscale;
  gchar *mode_string = NULL;

  struct poptOption optionsTable[] =
  {
    {"numeric", 'n', POPT_ARG_NONE, &numeric, 0,
     _ ("don't convert addresses to names"), NULL
    },
    {"with-dns-resolving", 'r', POPT_ARG_NONE, &dns, 0,
     _ ("use IP name resolving. Caution! Long timeouts!"), NULL
    },
    {"diagram-only", 'd', POPT_ARG_NONE, &diagram_only, 0,
     _ ("don't display any node text identification"), NULL
    },
    {"mode", 'm', POPT_ARG_STRING, &mode_string, 0,
     _ ("mode of operation"), _ ("<ethernet|ip|tcp|udp>")
    },
    {"interface", 'i', POPT_ARG_STRING, &interface, 0,
     _ ("set interface to listen to"), _ ("<interface name>")
    },
    {"filter", 'f', POPT_ARG_STRING, &filter, 0,
     _ ("set capture filter"), _ ("<capture filter>")
    },
    {"no-overlap", 'o', POPT_ARG_NONE, &fix_overlap, 0,
     _ ("makes diagram more readable when it is crowded"), NULL
    },
    {"node-color", 'N', POPT_ARG_STRING, &node_color, 0,
     _ ("set the node color"), _ ("color")
    },
    {"link-color", 'L', POPT_ARG_STRING, &link_color, 0,
     _ ("set the link color"), _ ("color")
    },
    {"text-color", 'T', POPT_ARG_STRING, &text_color, 0,
     _ ("set the text color"), _ ("color")
    },

    POPT_AUTOHELP
    {NULL, 0, 0, NULL, 0}
  };

#ifdef ENABLE_NLS
  bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
  textdomain (PACKAGE);
#endif


  gnome_init_with_popt_table ("etherape", VERSION, argc, argv, optionsTable, 0, NULL);

  /* If called as interape, act as interape */
  if (strstr (argv[0], "interape"))
    mode = IP;


  /* Find mode of operation */
  if (mode_string)
    {
      if (strstr (mode_string, "ethernet"))
	mode = ETHERNET;
      else if (strstr (mode_string, "ip"))
	mode = IP;
      else if (strstr (mode_string, "tcp"))
	mode = TCP;
      else if (strstr (mode_string, "udp"))
	mode = UDP;
      else
	g_warning (_ ("Unrecognized mode. Do etherape --help for a list of modes"));
    }

  /* Only ip traffic makes sense when used as interape */
  switch (mode)
    {
    case IP:
      filter = g_strconcat ("ip ", filter, NULL);
      break;
    case TCP:
      filter = g_strconcat ("tcp ", filter, NULL);
      break;
    case DEFAULT:
    case UDP:
    case ETHERNET:
    }

  init_capture ();		/* TODO low priority: I'd like to be able to open the 
				 * socket without having initialized gnome, so that 
				 * eventually I'd safely set the effective id to match the
				 * user id and make a safer suid exec. See the source of
				 * mtr for reference */

  app1 = create_app1 ();
  if (mode == IP)
    gtk_window_set_title (GTK_WINDOW (app1), "Interape");

  /* Sets controls to the values of variables */
  init_diagram (app1);

  hscale = lookup_widget (app1, "node_radius_slider");
  gtk_signal_connect (GTK_OBJECT (GTK_RANGE (hscale)->adjustment),
		      "value_changed", GTK_SIGNAL_FUNC (on_node_radius_slider_adjustment_changed),
		      NULL);
  hscale = lookup_widget (app1, "link_width_slider");
  gtk_signal_connect (GTK_OBJECT (GTK_RANGE (hscale)->adjustment),
  "value_changed", GTK_SIGNAL_FUNC (on_link_width_slider_adjustment_changed),
		      NULL);
  hscale = lookup_widget (app1, "averaging_spin");
  gtk_signal_connect (GTK_OBJECT (GTK_SPIN_BUTTON (hscale)->adjustment),
    "value_changed", GTK_SIGNAL_FUNC (on_averaging_spin_adjustment_changed),
		      NULL);
  hscale = lookup_widget (app1, "refresh_spin");
  gtk_signal_connect (GTK_OBJECT (GTK_SPIN_BUTTON (hscale)->adjustment),
      "value_changed", GTK_SIGNAL_FUNC (on_refresh_spin_adjustment_changed),
		      lookup_widget (GTK_WIDGET (app1), "canvas1"));
  hscale = lookup_widget (app1, "node_to_spin");
  gtk_signal_connect (GTK_OBJECT (GTK_SPIN_BUTTON (hscale)->adjustment),
      "value_changed", GTK_SIGNAL_FUNC (on_node_to_spin_adjustment_changed),
		      NULL);
  hscale = lookup_widget (app1, "link_to_spin");
  gtk_signal_connect (GTK_OBJECT (GTK_SPIN_BUTTON (hscale)->adjustment),
      "value_changed", GTK_SIGNAL_FUNC (on_link_to_spin_adjustment_changed),
		      NULL);

  gtk_widget_show (app1);

  /* With this we force an update of the diagram every x ms 
   * Data in the diagram is updated, and then the canvas redraws itself when
   * the gtk loop is idle. If the diagram is too complicated, calls to
   * update_diagram will be stacked with no gtk idle time, thus freezing
   * the display */
  /* TODO: Back up if CPU can't handle it */
  diagram_timeout = gtk_timeout_add (refresh_period /* ms */ ,
				     (GtkFunction) update_diagram,
			      lookup_widget (GTK_WIDGET (app1), "canvas1"));

  /* MAIN LOOP */
  gtk_main ();
  return 0;
}
