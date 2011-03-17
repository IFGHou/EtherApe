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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <netinet/in.h>
#include <signal.h>
#include <gnome.h>
#include <libgnomeui/gnome-client.h>
#include "appdata.h"
#include "ip-cache.h"
#include "main.h"
#include "diagram.h"
#include "preferences.h"
#include "info_windows.h"
#include "menus.h"
#include "capture.h"
#include "datastructs.h"

/***************************************************************************
 *
 * local variables
 *
 **************************************************************************/
static gboolean quiet = FALSE;
static void (*old_sighup_handler) (int);

/***************************************************************************
 *
 * internal functions
 *
 **************************************************************************/
static void free_static_data(void);
static void set_debug_level (void);
static void session_die (GnomeClient * client, gpointer client_data);
static gint save_session (GnomeClient * client, gint phase, 
                          GnomeSaveStyle save_style, gint is_shutdown, 
                          GnomeInteractStyle interact_style, gint is_fast, 
                          gpointer client_data);
static void log_handler (gchar * log_domain, GLogLevelFlags mask, 
                         const gchar * message, gpointer user_data);

/* signal handling */
static void install_handlers(void);
static void signal_export(int signum);

/***************************************************************************
 *
 * implementation
 *
 **************************************************************************/
int
main (int argc, char *argv[])
{
  GtkWidget *widget;
  GnomeClient *client;
  gchar *mode_string = NULL;
  gchar *cl_filter = NULL;
  gchar *cl_interface = NULL;
  gchar *cl_input_file = NULL;
  gchar *export_file_final = NULL;
  gchar *export_file_signal = NULL;
  gboolean cl_numeric = FALSE;
  glong midelay = 0;
  glong madelay = G_MAXLONG;
  gchar *version;
  gchar *cl_glade_file = NULL;
  poptContext poptcon;

  struct poptOption optionsTable[] = {
    {"diagram-only", 'd', POPT_ARG_NONE, &(pref.diagram_only), 0,
     N_("don't display any node text identification"), NULL},
    {"replay-file", 'r', POPT_ARG_STRING, &cl_input_file, 0,
     N_("replay packets from file"), N_("<file to replay>")},
    {"filter", 'f', POPT_ARG_STRING, &cl_filter, 0,
     N_("set capture filter"), N_("<capture filter>")},
    {"interface", 'i', POPT_ARG_STRING, &cl_interface, 0,
     N_("set interface to listen to"), N_("<interface name>")},
    {"final-export", 0, POPT_ARG_STRING, &export_file_final, 0,
     N_("export to named file at end of replay"), N_("<file to export to>")},
    {"signal-export", 0, POPT_ARG_STRING, &export_file_signal, 0,
     N_("export to named file on receiving USR1"), N_("<file to export to>")},
    {"stationary", 's', POPT_ARG_NONE, &(pref.stationary), 0,  
     N_("don't move nodes around (deprecated)"), NULL}, 
    {"node-limit", 'l', POPT_ARG_INT, &(appdata.node_limit), 0,
     N_("limits nodes displayed"), N_("<number of nodes>")},
    {"mode", 'm', POPT_ARG_STRING, &mode_string, 0,
     N_("mode of operation"), N_("<link|ip|tcp>")},
    {"numeric", 'n', POPT_ARG_NONE, &cl_numeric, 0,
     N_("don't convert addresses to names"), NULL},
    {"quiet", 'q', POPT_ARG_NONE, &quiet, 0,
     N_("Disable informational messages"), NULL},
    {"min-delay", 0, POPT_ARG_LONG, &midelay,  0,
     N_("minimum packet delay in ms for reading capture files [cli only]"),
      N_("<delay>")},
    {"max-delay", 0, POPT_ARG_LONG, &madelay,  0,
     N_("maximum packet delay in ms for reading capture files [cli only]"),
      N_("<delay>")},
    {"glade-file", 0, POPT_ARG_STRING, &(cl_glade_file), 0,
     N_("uses the named libglade file for widgets"), N_("<glade file>")},

    POPT_AUTOHELP {NULL, 0, 0, NULL, 0, NULL, NULL}
  };

#ifdef ENABLE_NLS
  bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
  bind_textdomain_codeset(PACKAGE, "UTF-8"); /* force UTF-8 conversion */
  textdomain (PACKAGE);
#endif

  /* We set the window icon to use */
  if (!getenv ("GNOME_DESKTOP_ICON"))
    putenv ("GNOME_DESKTOP_ICON=" PIXMAPS_DIR "/etherape.png");

#ifdef PACKAGE_SCM_REV
  /* We initiate the application and read command line options */
  version = g_strdup_printf("%s (hg id %s)", VERSION, 
                            (*PACKAGE_SCM_REV) ? PACKAGE_SCM_REV : 
                              _("-unknown-"));
#else
  version = g_strdup(VERSION);
#endif
  gnome_program_init ("EtherApe", version, 
                      LIBGNOMEUI_MODULE, argc, argv,
		      GNOME_PARAM_POPT_TABLE, optionsTable, GNOME_PARAM_NONE);
  g_free(version);

  appdata_init(&appdata);

  /* We obtain application parameters 
   * First, absolute defaults
   * Second, values saved in the config file
   * Third, whatever given in the command line */
  init_config(&pref);

  set_debug_level();

  /* Config file */
  load_config();

  /* Command line */
  cl_numeric = !pref.name_res;
  poptcon =
    poptGetContext ("Etherape", argc, (const char **) argv, optionsTable, 0);
  while (poptGetNextOpt (poptcon) > 0);

  if (cl_interface)
    {
      if (appdata.interface)
	g_free (appdata.interface);
      appdata.interface = g_strdup (cl_interface);
    }

  if (export_file_final)
    {
      if (appdata.export_file_final)
	g_free (appdata.export_file_final);
      appdata.export_file_final = g_strdup (export_file_final);
    }
  if (export_file_signal)
    {
      if (appdata.export_file_signal)
	g_free (appdata.export_file_signal);
      appdata.export_file_signal = g_strdup (export_file_signal);
    }

  pref.name_res = !cl_numeric;

  if (cl_input_file)
    {
      if (appdata.input_file)
	g_free (appdata.input_file);
      appdata.input_file = g_strdup (cl_input_file);
    }

  /* Find mode of operation */
  if (mode_string)
    {
      if (strstr (mode_string, "link"))
	appdata.mode = LINK6;
      else if (strstr (mode_string, "ip"))
	appdata.mode = IP;
      else if (strstr (mode_string, "tcp"))
	appdata.mode = TCP;
      else
	g_warning (_
		   ("Unrecognized mode. Do etherape --help for a list of modes"));
      g_free(pref.filter);
      pref.filter = get_default_filter(appdata.mode);
    }

  if (cl_filter)
    {
      if (pref.filter)
	g_free (pref.filter);
      pref.filter = g_strdup (cl_filter);
    }

  if (midelay >= 0 && midelay <= G_MAXLONG)
    {
       appdata.min_delay = midelay;
       if (appdata.min_delay != 0)
         g_message("Minimum delay set to %lu ms", appdata.min_delay);
    }
  else
      g_message("Invalid minimum delay %ld, ignored", midelay);
  
  if (madelay >= 0 && madelay <= G_MAXLONG)
    {
      if (madelay < appdata.min_delay)
        {
          g_message("Maximum delay must be less of minimum delay");
          appdata.max_delay = appdata.min_delay;
        }
      else
        appdata.max_delay = madelay;
      if (appdata.max_delay != G_MAXLONG)
        g_message("Maximum delay set to %lu ms", appdata.max_delay);
    }
  else
      g_message("Invalid maximum delay %ld, ignored", madelay);
  
  /* Glade */
  glade_gnome_init ();
  glade_require("gnome");
  glade_require("canvas");
  if (!appdata_init_glade(cl_glade_file))
    return 1;

  /* prepare decoders */
  services_init();
  
  /* Sets controls to the values of variables and connects signals */
  init_diagram (appdata.xml);

  /* Session handling */
  client = gnome_master_client ();
  g_signal_connect (G_OBJECT (client), "save_yourself",
		    GTK_SIGNAL_FUNC (save_session), argv[0]);
  g_signal_connect (G_OBJECT (client), "die",
		    GTK_SIGNAL_FUNC (session_die), NULL);
  gtk_widget_show (appdata.app1);

  install_handlers();

  /* With this we force an update of the diagram every x ms 
   * Data in the diagram is updated, and then the canvas redraws itself when
   * the gtk loop is idle. If the CPU can't handle the set refresh_period,
   * then it will just do a best effort */

  widget = glade_xml_get_widget (appdata.xml, "canvas1");
  destroying_idle (widget);

  /* This other timeout makes sure that the info windows are updated */
  g_timeout_add (500, (GtkFunction) update_info_windows, NULL);

  /* another timeout to handle IP-cache timeouts */
  g_timeout_add (10000, (GtkFunction) ipcache_tick, NULL);

  init_menus ();
  
  gui_start_capture ();

  /* MAIN LOOP */
  gtk_main ();

  free_static_data();
  return 0;
}				/* main */

/* releases all static and cached data. Called just before exiting. Obviously 
 * it's not stricly needed, since the memory will be returned to the OS anyway,
 * but makes finding memory leaks much easier. */
static void free_static_data(void)
{
  protohash_clear();
  ipcache_clear();
  services_clear();
}

static void
set_debug_level (void)
{
  const gchar *env_debug;
  env_debug = g_getenv("APE_DEBUG");

  appdata.debug_mask = (G_LOG_LEVEL_MASK & ~(G_LOG_LEVEL_DEBUG | G_LOG_LEVEL_INFO));

  if (env_debug)
    {
      if (!g_ascii_strcasecmp(env_debug, "INFO"))
	appdata.debug_mask = (G_LOG_LEVEL_MASK & ~G_LOG_LEVEL_DEBUG);
      else if (!g_ascii_strcasecmp(env_debug, "DEBUG"))
	appdata.debug_mask = G_LOG_LEVEL_MASK;
    }

  if (quiet)
    appdata.debug_mask = 0;

  g_log_set_handler (NULL, G_LOG_LEVEL_MASK, (GLogFunc) log_handler, NULL);
  g_my_debug ("debug_mask %d", appdata.debug_mask);
}

static void
log_handler (gchar * log_domain,
	     GLogLevelFlags mask, const gchar * message, gpointer user_data)
{
  if (mask & appdata.debug_mask)
    g_log_default_handler ("EtherApe", mask, message, user_data);
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
  g_assert(argv);

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


/***************************************************************************
 *
 * signal handling
 *
 **************************************************************************/

/* installs signal handlers */
static void install_handlers(void)
{
  /* 
   * Signal handling
   * Catch SIGINT and SIGTERM and, if we get either of them, clean up
   * and exit.
   * XXX - deal with signal semantics on various platforms.  Or just
   * use "sigaction()" and be done with it?
   */
  if (signal(SIGTERM, cleanup) == SIG_IGN)
     signal(SIGTERM, SIG_IGN);
  if (signal(SIGINT, cleanup) == SIG_IGN)
     signal(SIGINT, SIG_IGN);
#if !defined(WIN32)
  if ((old_sighup_handler = signal (SIGHUP, cleanup)) != SIG_DFL)	/* Play nice with nohup */
    signal (SIGHUP, old_sighup_handler);
#endif
  if (signal(SIGUSR1, signal_export) == SIG_IGN)
     signal(SIGUSR1, SIG_IGN);
}

/*
 * Quit the program.
 * Makes sure that the capture device is closed, or else we might
 * be leaving it in promiscuous mode
 */
void cleanup(int signum)
{
  cleanup_capture ();
  free_static_data();
  gtk_exit (0);
}

/* activates a flag requesting an xml dump */
static void signal_export(int signum)
{
  appdata.request_dump = TRUE;
}
