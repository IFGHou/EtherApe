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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>

#include "globals.h"
#include "diagram.h"
#include "interface.h"
#include "support.h"

static void
session_die (GnomeClient * client, gpointer client_data)
{
  g_message ("in die");
  gtk_main_quit ();
}

static gint
save_session (GnomeClient * client, gint phase, GnomeSaveStyle save_style,
	      gint is_shutdown, GnomeInteractStyle interact_style,
	      gint is_fast, gpointer client_data)
{
  gchar **argv;
  guint argc;

  /* allocate 0-filled, so it will be NULL-terminated */
  argv = g_malloc0 (sizeof (gchar *) * 4);
  argc = 1;

  argv[0] = client_data;

  g_message ("In save_session");
#if 0
  if (message)
    {
      argv[1] = "--message";
      argv[2] = message;
      argc = 3;
    }
#endif

  gnome_client_set_clone_command (client, argc, argv);
  gnome_client_set_restart_command (client, argc, argv);

  return TRUE;
}

void
load_config (char *prefix)
{
  gboolean u;

  gnome_config_push_prefix (prefix);
  diagram_only =
    gnome_config_get_bool_with_default ("Diagram/diagram_only=FALSE", &u);
  nofade = gnome_config_get_bool_with_default ("Diagram/nofade=FALSE", &u);
  node_timeout_time =
    gnome_config_get_float_with_default
    ("Diagram/node_timeout_time=60000000.0", &u);
  if (nofade)
    link_timeout_time =
      gnome_config_get_float_with_default
      ("Diagram/link_timeout_time=5000000.0", &u);
  else
    link_timeout_time =
      gnome_config_get_float_with_default
      ("Diagram/link_timeout_time=20000000.0", &u);
  averaging_time =
    gnome_config_get_float_with_default ("Diagram/averaging_time=3000000.0",
					 &u);
  node_radius_multiplier =
    gnome_config_get_float_with_default
    ("Diagram/node_radius_multiplier=0.0005", &u);
  if (u)
    node_radius_multiplier = 0.0005;	/* This is a bug with gnome_config */
  link_width_multiplier =
    gnome_config_get_float_with_default
    ("Diagram/link_width_multiplier=0.0005", &u);
  if (u)
    link_width_multiplier = 0.0005;
  mode = gnome_config_get_int_with_default ("General/mode=-1", &u);	/* DEFAULT */
  if (mode == IP || mode == TCP)
    refresh_period =
      gnome_config_get_int_with_default ("Diagram/refresh_period=3000", &u);
  else
    refresh_period =
      gnome_config_get_int_with_default ("Diagram/refresh_period=800", &u);

  size_mode = gnome_config_get_int_with_default ("Diagram/size_mode=0", &u);	/* LINEAR */
  stack_level =
    gnome_config_get_int_with_default ("Diagram/stack_level=1", &u);
  fontname =
    gnome_config_get_string_with_default
    ("Diagram/fontname=-misc-fixed-medium-r-*-*-*-140-*-*-*-*-*-*", &u);

  gnome_config_pop_prefix ();
}				/* load_config */

void
save_config (char *prefix)
{
  gnome_config_push_prefix (prefix);
  gnome_config_set_bool ("Diagram/diagram_only", diagram_only);
  gnome_config_set_bool ("Diagram/nofade", nofade);
  gnome_config_set_float ("Diagram/node_timeout_time", node_timeout_time);
  gnome_config_set_float ("Diagram/link_timeout_time", link_timeout_time);
  gnome_config_set_float ("Diagram/averaging_time", averaging_time);
  gnome_config_set_float ("Diagram/node_radius_multiplier",
			  node_radius_multiplier);
  gnome_config_set_float ("Diagram/link_width_multiplier",
			  link_width_multiplier);
#if 0				/* TODO should we save this? */
  gnome_config_set_int ("General/mode", mode);
#endif
  gnome_config_set_int ("Diagram/refresh_period", refresh_period);
  gnome_config_set_int ("Diagram/size_mode", size_mode);
  gnome_config_set_int ("Diagram/stack_level", stack_level);
  gnome_config_set_string ("Diagram/fontname", fontname);

  gnome_config_sync ();
  gnome_config_pop_prefix ();

  g_message (_("Preferences saved"));

}				/* save_config */

int
main (int argc, char *argv[])
{
  gchar *mode_string = NULL;
  GtkWidget *widget;
  GnomeClient *client;
  poptContext poptcon;

  struct poptOption optionsTable[] = {
    {"numeric", 'n', POPT_ARG_NONE, &numeric, 0,
     _("don't convert addresses to names"), NULL},
    {"diagram-only", 'd', POPT_ARG_NONE, &diagram_only, 0,
     _("don't display any node text identification"), NULL},
    {"mode", 'm', POPT_ARG_STRING, &mode_string, 0,
     _("mode of operation"), _("<ethernet|ip|tcp|udp>")},
    {"interface", 'i', POPT_ARG_STRING, &interface, 0,
     _("set interface to listen to"), _("<interface name>")},
    {"filter", 'f', POPT_ARG_STRING, &filter, 0,
     _("set capture filter"), _("<capture filter>")},
    {"no-fade", 'F', POPT_ARG_NONE, &nofade, 0,
     _("do not fade old links"), NULL},
    {"node-color", 'N', POPT_ARG_STRING, &node_color, 0,
     _("set the node color"), _("color")},
    {"link-color", 'L', POPT_ARG_STRING, &link_color, 0,
     _("set the link color"), _("color")},
    {"text-color", 'T', POPT_ARG_STRING, &text_color, 0,
     _("set the text color"), _("color")},

    POPT_AUTOHELP {NULL, 0, 0, NULL, 0}
  };

#ifdef ENABLE_NLS
  bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
  textdomain (PACKAGE);
#endif


  /* We initiate the application and read command line options */
  gnome_init_with_popt_table ("Etherape", VERSION, argc, argv, optionsTable,
			      0, NULL);

  /* We obtain application parameters 
   * First, absolute defaults
   * Second, values saved in the config file
   * Third, whatever given in the command line */

  /* Absolute defaults */
  numeric = 0;
  mode = ETHERNET;
  dns = 1;
  filter = g_strdup ("");
  refresh_period = 800;		/* ms */
  node_color = g_strdup ("brown");
  link_color = g_strdup ("tan");	/* TODO I think link_color is
					 * actually never used anymore,
					 * is it? */
  text_color = g_strdup ("yellow");

  /* Config file */
  load_config ("/Etherape/");

  /* Command line */
  poptcon = poptGetContext ("Etherape", argc, argv, optionsTable, 0);
  while (poptGetNextOpt (poptcon) > 0);

  if (!fontname)
    fontname = g_strdup ("-misc-fixed-medium-r-*-*-*-140-*-*-*-*-*-*");


  /* dns is used in dns.c as opposite of numeric */
  dns = !numeric;

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
	g_warning (_
		   ("Unrecognized mode. Do etherape --help for a list of modes"));
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
      break;
    }

  init_capture ();		/* TODO low priority: I'd like to be able to open the 
				 * socket without having initialized gnome, so that 
				 * eventually I'd safely set the effective id to match the
				 * user id and make a safer suid exec. See the source of
				 * mtr for reference */

  /* We create main windows */
  app1 = create_app1 ();
  diag_pref = create_diag_pref ();

  /* Sets controls to the values of variables and connects signals */
  init_diagram ();

  /* Session handling */
  client = gnome_master_client ();
  gtk_signal_connect (GTK_OBJECT (client), "save_yourself",
		      GTK_SIGNAL_FUNC (save_session), argv[0]);
  gtk_signal_connect (GTK_OBJECT (client), "die",
		      GTK_SIGNAL_FUNC (session_die), NULL);


  gtk_widget_show (app1);


  /* With this we force an update of the diagram every x ms 
   * Data in the diagram is updated, and then the canvas redraws itself when
   * the gtk loop is idle. If the diagram is too complicated, calls to
   * update_diagram will be stacked with no gtk idle time, thus freezing
   * the display */
  /* TODO: Back up if CPU can't handle it */
  widget = lookup_widget (GTK_WIDGET (app1), "canvas1");
  diagram_timeout = gtk_timeout_add (refresh_period /* ms */ ,
				     (GtkFunction) update_diagram, widget);

  /* MAIN LOOP */
  gtk_main ();
  return 0;
}				/* main */
