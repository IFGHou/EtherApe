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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "globals.h"
#include "main.h"


int
main (int argc, char *argv[])
{
  gchar *mode_string = NULL;
  GtkWidget *widget;
  GnomeClient *client;
  gchar *cl_filter = NULL, *cl_interface = NULL, *cl_input_file = NULL;
  poptContext poptcon;

  struct poptOption optionsTable[] = {
    {"mode", 'm', POPT_ARG_STRING, &mode_string, 0,
     N_("mode of operation"), N_("<ethernet|fddi|ip|tcp>")},
    {"interface", 'i', POPT_ARG_STRING, &cl_interface, 0,
     N_("set interface to listen to"), N_("<interface name>")},
    {"filter", 'f', POPT_ARG_STRING, &cl_filter, 0,
     N_("set capture filter"), N_("<capture filter>")},
    {"infile", 'r', POPT_ARG_STRING, &cl_input_file, 0,
     N_("set input file"), N_("<file name>")},
    {"numeric", 'n', POPT_ARG_NONE, &numeric, 0,
     N_("don't convert addresses to names"), NULL},
    {"diagram-only", 'd', POPT_ARG_NONE, &diagram_only, 0,
     N_("don't display any node text identification"), NULL},
    {"no-fade", 'F', POPT_ARG_NONE, &nofade, 0,
     N_("do not fade old links"), NULL},
    {"stationary", 's', POPT_ARG_NONE, &stationary, 0,
     N_("don't move nodes around"), NULL},
    {"node_limit", 'l', POPT_ARG_INT, &node_limit, 0,
     N_("limits nodes displayed"), N_("<number of nodes>")},
    {"quiet", 'q', POPT_ARG_NONE, &quiet, 0,
     N_("Don't show warnings"), NULL},
    {"node-color", 'N', POPT_ARG_STRING, &node_color, 0,
     N_("set the node color"), N_("color")},
    {"link-color", 'L', POPT_ARG_STRING, &link_color, 0,
     N_("set the link color"), N_("color")},
    {"text-color", 'T', POPT_ARG_STRING, &text_color, 0,
     N_("set the text color"), N_("color")},

    POPT_AUTOHELP {NULL, 0, 0, NULL, 0}
  };


#ifdef ENABLE_NLS
  bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
  textdomain (PACKAGE);
#endif

  /* We set the window icon to use */
  if (!getenv ("GNOME_DESKTOP_ICON"))
    putenv ("GNOME_DESKTOP_ICON=" PIXMAPS_DIR "/etherape.png");


  /* We initiate the application and read command line options */
  gnome_init_with_popt_table ("EtherApe", VERSION, argc, argv, optionsTable,
			      0, NULL);


  /* We obtain application parameters 
   * First, absolute defaults
   * Second, values saved in the config file
   * Third, whatever given in the command line */

  /* Absolute defaults */
  numeric = 0;
  mode = IP;
  dns = 1;
  filter = NULL;
  status = STOP;
  refresh_period = 800;		/* ms */
  node_color = g_strdup ("brown");
  link_color = g_strdup ("tan");	/* TODO I think link_color is
					 * actually never used anymore,
					 * is it? */
  text_color = g_strdup ("yellow");
  node_limit = -1;


  set_debug_level ();

  /* Config file */
  load_config ("/Etherape/");

  /* Command line */
  poptcon = poptGetContext ("Etherape", argc, argv, optionsTable, 0);
  while (poptGetNextOpt (poptcon) > 0);

  if (cl_filter)
    {
      if (filter)
	g_free (filter);
      filter = g_strdup (cl_filter);
    }

  if (cl_interface)
    {
      if (interface)
	g_free (interface);
      interface = g_strdup (cl_interface);
    }

  if (cl_input_file)
    {
      if (input_file)
	g_free (input_file);
      input_file = g_strdup (cl_input_file);
    }


  /* dns is used in dns.c as opposite of numeric */
  dns = !numeric;

  /* Find mode of operation */
  if (mode_string)
    {
      if (strstr (mode_string, "ethernet"))
	mode = ETHERNET;
      else if (strstr (mode_string, "fddi"))
	mode = FDDI;
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

  /* Glade */

  glade_gnome_init ();
  xml = glade_xml_new (GLADEDIR "/" ETHERAPE_GLADE_FILE, NULL);
  if (!xml)
    {
      g_error (_("We could not load the interface! (%s)"),
	       GLADEDIR "/" ETHERAPE_GLADE_FILE);
      return 1;
    }
  glade_xml_signal_autoconnect (xml);


  app1 = glade_xml_get_widget (xml, "app1");
  diag_pref = glade_xml_get_widget (xml, "diag_pref");

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
   * the gtk loop is idle. If the CPU can't handle the set refresh_period,
   * then it will just do a best effort */

  widget = glade_xml_get_widget (xml, "canvas1");
  diagram_timeout = g_timeout_add_full (G_PRIORITY_DEFAULT,
					refresh_period,
					(GtkFunction) update_diagram,
					widget,
					(GDestroyNotify) destroying_timeout);

  /* This other timeout makes sure that the info windows are updated */
  g_timeout_add (500, (GtkFunction) update_info_windows, NULL);

  init_menus ();

  gui_start_capture ();


  /* MAIN LOOP */
  gtk_main ();
  return 0;
}				/* main */



/* loads configuration from .gnome/Etherape */
static void
load_config (char *prefix)
{
  gboolean u;
  gchar *config_file_version;

  gnome_config_push_prefix (prefix);

  config_file_version =
    gnome_config_get_string_with_default ("General/version=0.5.4", &u);
  diagram_only =
    gnome_config_get_bool_with_default ("Diagram/diagram_only=FALSE", &u);
  stationary
    = gnome_config_get_bool_with_default ("Diagram/stationary=FALSE", &u);
  /* Not yet, since we can't force fading
     nofade = gnome_config_get_bool_with_default ("Diagram/nofade=FALSE", &u);
   */
  node_timeout_time =
    gnome_config_get_float_with_default
    ("Diagram/node_timeout_time=3600000.0", &u);
  gui_node_timeout_time =
    gnome_config_get_float_with_default
    ("Diagram/gui_node_timeout_time=60000.0", &u);
  if (nofade)
    link_timeout_time =
      gnome_config_get_float_with_default
      ("Diagram/link_timeout_time=5000.0", &u);
  else
    link_timeout_time =
      gnome_config_get_float_with_default
      ("Diagram/link_timeout_time=20000.0", &u);
  averaging_time =
    gnome_config_get_float_with_default ("Diagram/averaging_time=3000.0", &u);
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
  node_size_variable = gnome_config_get_int_with_default ("Diagram/node_size_variable=2", &u);	/* INST_OUTBOUND */
  stack_level =
    gnome_config_get_int_with_default ("Diagram/stack_level=0", &u);
  if ((stack_level != 0)
      && (version_compare (config_file_version, "0.5.4") < 0))
    g_warning (_("Stack Level is not set to Topmost Recognized Protocol. "
		 "Please check in the preferences dialog that this is what "
		 "you really want"));
  fontname =
    gnome_config_get_string_with_default
    ("Diagram/fontname=-*-*-*-*-*-*-*-140-*-*-*-*-iso8859-1", &u);

  g_free (config_file_version);
  gnome_config_pop_prefix ();
}				/* load_config */

static gint
version_compare (const gchar * a, const gchar * b)
{
  guint a_mj, a_mi, a_pl, b_mj, b_mi, b_pl;

  g_assert (a != NULL);
  g_assert (b != NULL);

  /* TODO What should we return if there was a problem? */
  g_return_val_if_fail ((get_version_levels (a, &a_mj, &a_mi, &a_pl)
			 && get_version_levels (b, &b_mj, &b_mi, &b_pl)), 0);
  if (a_mj < b_mj)
    return -1;
  else if (a_mj > b_mj)
    return 1;
  else if (a_mi < b_mi)
    return -1;
  else if (a_mi > b_mi)
    return 1;
  else if (a_pl < b_pl)
    return -1;
  else if (a_pl > b_pl)
    return 1;
  else
    return 0;
}

static gboolean
get_version_levels (const gchar * version_string,
		    guint * major, guint * minor, guint * patch)
{
  gchar **tokens;

  g_assert (version_string != NULL);

  tokens = g_strsplit (version_string, ".", 0);
  g_return_val_if_fail ((tokens
			 && tokens[0] && tokens[1] && tokens[2]
			 && sscanf (tokens[0], "%d", major)
			 && sscanf (tokens[1], "%d", minor)
			 && sscanf (tokens[2], "%d", patch)), FALSE);
  g_strfreev (tokens);
  return TRUE;
}



/* saves configuration to .gnome/Etherape */
/* It's not static since it will be called from the GUI */
void
save_config (char *prefix)
{
  gnome_config_push_prefix (prefix);
  gnome_config_set_bool ("Diagram/diagram_only", diagram_only);
  gnome_config_set_bool ("Diagram/nofade", nofade);
  gnome_config_set_float ("Diagram/node_timeout_time", node_timeout_time);
  gnome_config_set_float ("Diagram/gui_node_timeout_time",
			  gui_node_timeout_time);
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
  gnome_config_set_int ("Diagram/node_size_variable", node_size_variable);
  gnome_config_set_int ("Diagram/stack_level", stack_level);
  gnome_config_set_string ("Diagram/fontname", fontname);
  gnome_config_set_string ("General/version", VERSION);

  gnome_config_sync ();
  gnome_config_pop_prefix ();

  g_my_info (_("Preferences saved"));

}				/* save_config */


static void
set_debug_level (void)
{
  gchar *env_debug;
  env_debug = g_getenv ("DEBUG");

  debug_mask = (G_LOG_LEVEL_MASK & ~(G_LOG_LEVEL_DEBUG | G_LOG_LEVEL_INFO));

  if (env_debug)
    {
      if (!strcmp (env_debug, "INFO"))
	debug_mask = (G_LOG_LEVEL_MASK & ~G_LOG_LEVEL_DEBUG);
      else if (!strcmp (env_debug, "DEBUG"))
	debug_mask = G_LOG_LEVEL_MASK;
    }

  if (quiet)
    debug_mask = 0;

  g_log_set_handler (NULL, G_LOG_LEVEL_MASK, (GLogFunc) log_handler, NULL);
  g_my_debug ("debug_mask %d", debug_mask);
}

static void
log_handler (gchar * log_domain,
	     GLogLevelFlags mask, const gchar * message, gpointer user_data)
{
  if (mask & debug_mask)
    g_log_default_handler (NULL, mask, message, user_data);
}

/* the gnome session manager may call this function */
static void
session_die (GnomeClient * client, gpointer client_data)
{
  g_message ("in die");
  gtk_main_quit ();
}				/* session_die */

/* the gnome session manager may call this function */
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
}				/* save_session */
