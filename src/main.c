

/* Program Name
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

gboolean numeric = 0;
gboolean dns = 0;
gboolean diagram_only = 0;
gchar *interface = NULL;

int
main (int argc, char *argv[])
{
  GtkWidget *app1;
  GtkWidget *hscale;

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
    {"interface", 'i', POPT_ARG_STRING, &interface, 0,
     _ ("set interface to listen to"), _ ("interface name")
    },
    POPT_AUTOHELP
    {NULL, 0, 0, NULL, 0}
  };

#ifdef ENABLE_NLS
  bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
  textdomain (PACKAGE);
#endif


  gnome_init_with_popt_table ("etherape", VERSION, argc, argv, optionsTable, 0, NULL);
  init_capture ();		/* TODO low priority: I'd like to be able to open the 
				 * socket without having initialized gnome, so that 
				 * eventually I'd safely set the effective id to match the
				 * user id and make a safer suid exec. See the source of
				 * mtr for reference */
  init_diagram ();

  app1 = create_app1 ();
  hscale = lookup_widget (app1, "hscale6");
  gtk_signal_connect (GTK_OBJECT (GTK_RANGE (hscale)->adjustment),
	   "value_changed", GTK_SIGNAL_FUNC (on_hscale6_adjustment_changed),
		      NULL);
  gtk_widget_show (app1);

  /* With this we force an update of the diagram every x ms 
   * Data in the diagram is updated, and then the canvas redraws itself when
   * the gtk loop is idle. If the diagram is too complicated, calls to
   * update_diagram will be stacked with no gtk idle time, thus freezing
   * the display */
  /* TODO: Make this time configurable and back up if CPU can't handle it */
  gtk_timeout_add (800 /* ms */ ,
		   (GtkFunction) update_diagram,
		   lookup_widget (GTK_WIDGET (app1), "canvas1"));

  gtk_main ();
  return 0;
}
