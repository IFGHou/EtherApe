

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

int
main (int argc, char *argv[])
{
  GtkWidget *app1;

#ifdef ENABLE_NLS
  bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
  textdomain (PACKAGE);
#endif

  init_capture ();
  init_diagram ();

  gnome_init ("etherape", VERSION, argc, argv);

  app1 = create_app1 ();
  gtk_widget_show (app1);

  /* With this we force an update of the diagram every x ms 
   * Data in the diagram is updated, and then the canvas redraws itself when
   * the gtk loop is idle. If the diagram is too complicated, calls to
   * update_diagram will be stacked with no gtk idle time, thus freezing
   * the display */
  /* TODO: Make this time configurable and back up if CPU can't handle it */
  gtk_timeout_add (800 /* ms */ ,
		   update_diagram,
		   lookup_widget (GTK_WIDGET (app1), "canvas1"));

  gtk_main ();
  return 0;
}
