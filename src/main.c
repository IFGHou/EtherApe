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

#include <netinet/in.h>
#include "globals.h"
#include "ip-cache.h"
#include "main.h"
#include "diagram.h"
#include "info_windows.h"
#include "capture.h"
#include "datastructs.h"


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
    {"numeric", 'n', POPT_ARG_NONE, &(pref.numeric), 0,
     N_("don't convert addresses to names"), NULL},
    {"diagram-only", 'd', POPT_ARG_NONE, &(pref.diagram_only), 0,
     N_("don't display any node text identification"), NULL},
    {"no-fade", 'F', POPT_ARG_NONE, &(pref.nofade), 0,
     N_("do not fade old links"), NULL},
    {"stationary", 's', POPT_ARG_NONE, &(pref.stationary), 0,
     N_("don't move nodes around"), NULL},
    {"node_limit", 'l', POPT_ARG_INT, &(pref.node_limit), 0,
     N_("limits nodes displayed"), N_("<number of nodes>")},
    {"quiet", 'q', POPT_ARG_NONE, &quiet, 0,
     N_("Don't show warnings"), NULL},
    {"node-color", 'N', POPT_ARG_STRING, &(pref.node_color), 0,
     N_("set the node color"), N_("color")},
    {"text-color", 'T', POPT_ARG_STRING, &(pref.text_color), 0,
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
  gnome_program_init ("EtherApe", VERSION, LIBGNOMEUI_MODULE, argc, argv,
		      GNOME_PARAM_POPT_TABLE, optionsTable, GNOME_PARAM_NONE);


  /* We obtain application parameters 
   * First, absolute defaults
   * Second, values saved in the config file
   * Third, whatever given in the command line */

  /* Absolute defaults */
  pref.numeric = 0;
  pref.mode = IP;
  dns = 1;
  pref.filter = NULL;
  status = STOP;
  pref.refresh_period = 800;	/* ms */

  /* TODO Besides the fact that this probably makes little sense nowadays
   * (at least for node color) it probably leads to a segfault
   * See how it is done for filter, for instance */
  pref.node_color = g_strdup ("brown");
  pref.text_color = g_strdup ("yellow");
  pref.node_limit = -1;


  set_debug_level ();

  /* Config file */
  load_config ("/Etherape/");

  /* Command line */
  poptcon =
    poptGetContext ("Etherape", argc, (const char **) argv, optionsTable, 0);
  while (poptGetNextOpt (poptcon) > 0);

  if (cl_filter)
    {
      if (pref.filter)
	g_free (pref.filter);
      pref.filter = g_strdup (cl_filter);
    }

  if (cl_interface)
    {
      if (pref.interface)
	g_free (pref.interface);
      pref.interface = g_strdup (cl_interface);
    }

  if (cl_input_file)
    {
      if (pref.input_file)
	g_free (pref.input_file);
      pref.input_file = g_strdup (cl_input_file);
    }


  /* dns is used in dns.c as opposite of numeric */
  dns = !pref.numeric;

  /* Find mode of operation */
  if (mode_string)
    {
      if (strstr (mode_string, "ethernet"))
	pref.mode = ETHERNET;
      else if (strstr (mode_string, "fddi"))
	pref.mode = FDDI;
      else if (strstr (mode_string, "ip"))
	pref.mode = IP;
      else if (strstr (mode_string, "tcp"))
	pref.mode = TCP;
      else if (strstr (mode_string, "udp"))
	pref.mode = UDP;
      else
	g_warning (_
		   ("Unrecognized mode. Do etherape --help for a list of modes"));
    }

  /* Glade */

  glade_gnome_init ();

  xml = glade_xml_new (GLADEDIR "/" ETHERAPE_GLADE_FILE, NULL, NULL);
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
  g_signal_connect (G_OBJECT (client), "save_yourself",
		    GTK_SIGNAL_FUNC (save_session), argv[0]);
  g_signal_connect (G_OBJECT (client), "die",
		    GTK_SIGNAL_FUNC (session_die), NULL);
  gtk_widget_show (app1);


  /* 
   * Signal handling
   * Catch SIGINT and SIGTERM and, if we get either of them, clean up
   * and exit.
   * XXX - deal with signal semantics on various platforms.  Or just
   * use "sigaction()" and be done with it?
   */
  signal (SIGTERM, cleanup);
  signal (SIGINT, cleanup);
#if !defined(WIN32)
  if ((oldhandler = signal (SIGHUP, cleanup)) != SIG_DFL)	/* Play nice with nohup */
    signal (SIGHUP, oldhandler);
#endif


  /* With this we force an update of the diagram every x ms 
   * Data in the diagram is updated, and then the canvas redraws itself when
   * the gtk loop is idle. If the CPU can't handle the set refresh_period,
   * then it will just do a best effort */

  widget = glade_xml_get_widget (xml, "canvas1");
  diagram_timeout = g_timeout_add_full (G_PRIORITY_DEFAULT,
					pref.refresh_period,
					(GtkFunction) update_diagram,
					widget,
					(GDestroyNotify) destroying_timeout);

  /* This other timeout makes sure that the info windows are updated */
  g_timeout_add (500, (GtkFunction) update_info_windows, NULL);

  /* another timeout to handle IP-cache timeouts */
  g_timeout_add (10000, (GtkFunction) ipcache_tick, NULL);

  init_menus ();

  gui_start_capture ();


  /* MAIN LOOP */
  gtk_main ();

  protohash_clear();
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
  pref.diagram_only =
    gnome_config_get_bool_with_default ("Diagram/diagram_only=FALSE", &u);
  pref.group_unk =
    gnome_config_get_bool_with_default ("Diagram/group_unk=TRUE", &u);
  pref.stationary
    = gnome_config_get_bool_with_default ("Diagram/stationary=FALSE", &u);
  pref.nofade =
    gnome_config_get_bool_with_default ("Diagram/nofade=FALSE", &u);
  pref.cycle = gnome_config_get_bool_with_default ("Diagram/cycle=TRUE", &u);
  pref.antialias =
    gnome_config_get_bool_with_default ("Diagram/antialias=TRUE", &u);
  pref.node_timeout_time =
    gnome_config_get_float_with_default
    ("Diagram/node_timeout_time=3600000.0", &u);
  pref.gui_node_timeout_time =
    gnome_config_get_float_with_default
    ("Diagram/gui_node_timeout_time=60000.0", &u);
  if (pref.nofade)
    pref.link_timeout_time =
      gnome_config_get_float_with_default
      ("Diagram/link_timeout_time=5000.0", &u);
  else
    pref.link_timeout_time =
      gnome_config_get_float_with_default
      ("Diagram/link_timeout_time=20000.0", &u);
  pref.averaging_time =
    gnome_config_get_float_with_default ("Diagram/averaging_time=3000.0", &u);
  pref.node_radius_multiplier =
    gnome_config_get_float_with_default
    ("Diagram/node_radius_multiplier=0.0005", &u);
  if (u)
    pref.node_radius_multiplier = 0.0005;	/* This is a bug with gnome_config */
  pref.link_width_multiplier =
    gnome_config_get_float_with_default
    ("Diagram/link_width_multiplier=0.0005", &u);
  if (u)
    pref.link_width_multiplier = 0.0005;
  pref.mode = gnome_config_get_int_with_default ("General/mode=-1", &u);	/* DEFAULT */
  if (pref.mode == IP || pref.mode == TCP)
    pref.refresh_period =
      gnome_config_get_int_with_default ("Diagram/refresh_period=3000", &u);
  else
    pref.refresh_period =
      gnome_config_get_int_with_default ("Diagram/refresh_period=800", &u);

  pref.size_mode = gnome_config_get_int_with_default ("Diagram/size_mode=0", &u);	/* LINEAR */
  pref.node_size_variable = gnome_config_get_int_with_default ("Diagram/node_size_variable=2", &u);	/* INST_OUTBOUND */
  pref.stack_level =
    gnome_config_get_int_with_default ("Diagram/stack_level=0", &u);
  if ((pref.stack_level != 0)
      && (version_compare (config_file_version, "0.5.4") < 0))
    g_warning (_("Stack Level is not set to Topmost Recognized Protocol. "
		 "Please check in the preferences dialog that this is what "
		 "you really want"));
  pref.fontname =
    gnome_config_get_string_with_default
    ("Diagram/fontname=-*-*-*-*-*-*-*-140-*-*-*-*-iso8859-1", &u);
  gnome_config_get_vector_with_default
    ("Diagram/colors=#ff0000;WWW #0000ff;DOMAIN #00ff00 #ffff00 #ff00ff #00ffff #ffffff #ff7700 #ff0077 #ffaa77 #7777ff #aaaa33",
     &(pref.n_colors), &(pref.colors), &u);

  if (!pref.n_colors || !pref.colors || !strlen(pref.colors[0]))
  {
     /* color array defined in prefs, but empty */
     pref.n_colors = 1;
     g_free(pref.colors[0]);
     pref.colors[0] = g_strdup("#7f7f7f");
  }

  g_free (config_file_version);
  gnome_config_pop_prefix ();

  protohash_read_prefvect(pref.colors, pref.n_colors);
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
  gnome_config_set_bool ("Diagram/diagram_only", pref.diagram_only);
  gnome_config_set_bool ("Diagram/group_unk", pref.group_unk);
  gnome_config_set_bool ("Diagram/nofade", pref.nofade);
  gnome_config_set_bool ("Diagram/cycle", pref.cycle);
  gnome_config_set_bool ("Diagram/antialias", pref.antialias);
  gnome_config_set_float ("Diagram/node_timeout_time",
			  pref.node_timeout_time);
  gnome_config_set_float ("Diagram/gui_node_timeout_time",
			  pref.gui_node_timeout_time);
  gnome_config_set_float ("Diagram/link_timeout_time",
			  pref.link_timeout_time);
  gnome_config_set_float ("Diagram/averaging_time", pref.averaging_time);
  gnome_config_set_float ("Diagram/node_radius_multiplier",
			  pref.node_radius_multiplier);
  gnome_config_set_float ("Diagram/link_width_multiplier",
			  pref.link_width_multiplier);
#if 0				/* TODO should we save this? */
  gnome_config_set_int ("General/mode", pref.mode);
#endif
  gnome_config_set_int ("Diagram/refresh_period", pref.refresh_period);
  gnome_config_set_int ("Diagram/size_mode", pref.size_mode);
  gnome_config_set_int ("Diagram/node_size_variable",
			pref.node_size_variable);
  gnome_config_set_int ("Diagram/stack_level", pref.stack_level);
  gnome_config_set_string ("Diagram/fontname", pref.fontname);

  gnome_config_set_vector ("Diagram/colors", pref.n_colors,
			   (const gchar * const *) pref.colors);

  gnome_config_set_string ("General/version", VERSION);

  gnome_config_sync ();
  gnome_config_pop_prefix ();

  g_my_info (_("Preferences saved"));

}				/* save_config */


static void
set_debug_level (void)
{
  const gchar *env_debug;
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
static gboolean
save_session (GnomeClient * client, gint phase, GnomeSaveStyle save_style,
	      gboolean is_shutdown, GnomeInteractStyle interact_style,
	      gboolean is_fast, gpointer client_data)
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


/*
 * Quit the program.
 * Makes sure that the capture device is closed, or else we might
 * be leaving it in promiscuous mode
 */
void
cleanup (int signum)
{
  cleanup_capture ();
  gtk_exit (0);
}
