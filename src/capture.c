
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

#include <gnome.h>
#include <ctype.h>
#include <netinet/in.h>
#include <pcap.h>

#include "appdata.h"
#include "capture.h"
#include "names.h"
#include "links.h"
#include "conversations.h"
#include "dns.h"
#include "decode_proto.h"
#include "protocols.h"
#include "preferences.h"
#include "export.h"

#define MAXSIZE 200

#ifdef DISABLE_GDKINPUTADD
#define PCAP_TIMEOUT 10
#else
#define PCAP_TIMEOUT 250
#endif

static pcap_t *pch_struct;		/* pcap structure */
static gint pcap_fd;			/* The file descriptor used by libpcap */
static gint capture_source;		/* It's the input tag or the timeout tag,
				 * in online or offline mode */
static gulong ms_to_next;	/* Used for offline mode to store the amount
				 * of time that we have to wait between
				 * one packet and the next */
static enum status_t capture_status = STOP;

/* Local funtions declarations */
static guint get_offline_packet (void);
static void cap_t_o_destroy(gpointer data);
static guint get_live_packet (void);
static void live_timeout(gpointer data);
static void gdk_input_callback(gpointer dummy, gint source,
			 GdkInputCondition condition);


/* 
 * FUNCTION DEFINITIONS
 */

enum status_t get_capture_status(void)
{
  return capture_status;
}

/* Sets up the pcap device
 * Sets up the mode and related variables
 * Sets up dns if needed
 * Sets up callbacks for pcap and dns
 * Creates nodes and links trees */
gchar *init_capture (void)
{
  gchar *device;
  gchar ebuf[PCAP_ERRBUF_SIZE];
  unsigned int linktype;		/* Type of device we are listening to */
  static gchar errorbuf[300];
  static gboolean data_initialized = FALSE;

  if (!data_initialized)
    {

      if (pref.name_res)
	{
	  if (dns_open ())
            g_warning ("Name resolver not available");
	}

      capture_status = STOP;
      data_initialized = TRUE;
    }

  device = appdata.interface;
  if (!device && !appdata.input_file)
    {
      *ebuf = '\0'; /* reset error buffer before calling pcap functions */
      device = g_strdup (pcap_lookupdev (ebuf));
      if (device == NULL)
	{
	  snprintf (errorbuf, sizeof(errorbuf), 
                    _("No capture device found or insufficient privileges.\n"
                      "Only file replay will be available.\n"
                      "EtherApe must be run with administrative privileges "
                      "(e.g. root) to enable live capture.\n"
                      "Pcap error: %s"), ebuf);
	  return errorbuf;
	}
      /* TODO I should probably tidy this up, I probably don't
       * need the local variable device. But I need to reset 
       * interface since I need to know whether we are in
       * online or offline mode later on */
      appdata.interface = device;
    }


  if (!appdata.input_file)
    {
      *ebuf = '\0'; /* reset error buffer before calling pcap functions */
      if (!
	  (pch_struct = 
	   pcap_open_live (device, MAXSIZE, TRUE, PCAP_TIMEOUT, ebuf)))
	{
	  snprintf (errorbuf, sizeof(errorbuf),
		   _("Error opening %s : %s\n- perhaps you need to be root?"),
		   device, ebuf);
	  return errorbuf;
	}
      pcap_fd = pcap_fileno (pch_struct);
      g_my_info (_("Live device %s opened for capture. pcap_fd: %d"), device,
		 pcap_fd);
    }
  else
    {
      if (device)
	{
	  snprintf (errorbuf, sizeof(errorbuf),
		   _("Can't open both %s and device %s. Please choose one."),
		   appdata.input_file, device);
	  return errorbuf;
	}
      *ebuf = '\0'; /* reset error buffer before calling pcap functions */
      if (!(pch_struct = pcap_open_offline (appdata.input_file, ebuf)))
	{
	  snprintf (errorbuf, sizeof(errorbuf), 
                  _("Error opening %s : %s"), appdata.input_file, ebuf);
	  return errorbuf;
	}
      g_my_info (_("%s opened for offline capture"), appdata.input_file);

    }

  linktype = pcap_datalink(pch_struct);
  if (!setup_link_type(linktype))
    {
      if (appdata.input_file)
        {
          snprintf (errorbuf, sizeof(errorbuf), 
                    _("File %s contains packets with unsupported "
                      "link type %d, cannot replay"), 
                    appdata.input_file,
                    linktype);
        }
      else if (device)
        {
          snprintf (errorbuf, sizeof(errorbuf), 
                    _("Device %s uses unsupported link type %d,"
                      "cannot capture. Please choose another interface."), 
                    device,
                    linktype);
        }
      else
        {
          snprintf (errorbuf, sizeof(errorbuf), _("Unsupported link type %d"), 
                    linktype);
        }
        return errorbuf;
    }
  
  if (appdata.mode == APEMODE_DEFAULT)
    {
      appdata.mode = IP;
      g_free (pref.filter);
      pref.filter = get_default_filter(appdata.mode);
    }
  if (appdata.mode == LINK6 && !has_linklevel())
    {
      snprintf (errorbuf, sizeof(errorbuf), _("This device does not support link-layer mode. "
                                              "Please use IP or TCP modes."));
      return errorbuf;
    }

  if (pref.filter)
    set_filter (pref.filter, device);

  return NULL;
}				/* init_capture */

/* TODO make it return an error value and act accordingly */
/* Installs a filter in the pcap structure */
gint
set_filter (gchar * filter_string, gchar * device)
{
  gchar ebuf[300];
  bpf_u_int32 netnum;
  bpf_u_int32 netmask = 0;
  struct bpf_program fp;

  if (!pch_struct)
    return 1;

  /* A capture filter was specified; set it up. */
  if (device && (pcap_lookupnet (device, &netnum, &netmask, ebuf) < 0))
    {
      g_warning (_
                  ("Couldn't obtain netmask info (%s). Filters involving "
                   "broadcast addresses could behave incorrectly."),
                 ebuf);
      netmask = 0;
    }
  if (pcap_compile (pch_struct, &fp, filter_string, 1, netmask) < 0)
    g_warning (_("Unable to parse filter string (%s). Filter ignored."), 
               pcap_geterr (pch_struct));
  else if (pcap_setfilter (pch_struct, &fp) < 0)
    g_warning (_("Can't install filter (%s). Filter ignored."), 
               pcap_geterr (pch_struct));

  return 0;
}				/* set_filter */

/* returns a string with the default filter to use for each mode */
gchar *get_default_filter (apemode_t mode)
{
  switch (mode)
    {
    case IP:
      return g_strdup ("ip or ip6");
      break;
    case TCP:
      return g_strdup ("tcp");
      break;
    case LINK6:
      return g_strdup ("");
      break;
    default:
      break;
    }
  g_error("Invalid apemode %d", mode);
}



gboolean
start_capture (void)
{
  GnomeCanvas *gc;

  if (capture_status == PLAY)
    return TRUE;

  /* if it's a new capture, we prepare protocol summary and nodes/links catalogs */
  if (STOP == capture_status)
    {
      protocol_summary_open();
      nodes_catalog_open();
      links_catalog_open();
    }

  /*
   * See pause_capture for an explanation of why we don't always
   * add the source
   */
  if (appdata.interface && (capture_status == STOP))
    {
      g_my_debug (_("Starting live capture"));
#ifdef DISABLE_GDKINPUTADD
      /* disabled fd - always use timers */
      capture_source = g_timeout_add_full (G_PRIORITY_DEFAULT,
					   1,
					   (GtkFunction) get_live_packet,
					   NULL,
					   (GDestroyNotify) live_timeout);
      g_my_info(_("Using timers for live capture"));
#else
      /* default: use the fd obtained from pcap to optimize readings */
      capture_source = gdk_input_add (pcap_fd,
				      GDK_INPUT_READ,
				      (GdkInputFunction)gdk_input_callback, 
                                      NULL);
#endif
    }
  else if (!appdata.interface)
    {
      g_my_debug (_("Starting offline capture"));
      capture_source = g_timeout_add_full (G_PRIORITY_DEFAULT,
					   1,
					   (GtkFunction) get_offline_packet,
					   NULL,
					   (GDestroyNotify) cap_t_o_destroy);
    }

  /* set the antialiasing */
  gc = GNOME_CANVAS (glade_xml_get_widget (appdata.xml, "canvas1"));
  if (gc)
    gc->aa = TRUE;

  capture_status = PLAY;
  return TRUE;
}				/* start_capture */

gboolean
pause_capture (void)
{
  if (capture_status != PLAY)
    return TRUE;

  if (!appdata.interface)
    {
      g_my_debug (_("Pausing offline capture"));
      if (!g_source_remove (capture_source))
        {
          g_warning (_("Error while trying to pause capture"));
          return FALSE;
        }
    }

  capture_status = PAUSE;
  return TRUE;

}


gboolean
stop_capture (void)
{
  struct pcap_stat ps;
  if (capture_status == STOP)
      return TRUE;

  if (appdata.interface)
    {
      g_my_debug (_("Stopping live capture"));
      gdk_input_remove (capture_source);	/* gdk_input_remove does not
						 * return an error code */
    }
  else
      g_my_debug (_("Stopping offline capture"));

  capture_status = STOP;

  /* free nodes, protocols, links, conversations and packets */
  protocol_summary_close();
  nodes_catalog_close();
  links_catalog_close();
  delete_conversations ();

  /* Free the list of new_nodes */
  new_nodes_clear();

  /* Clean the buffer */
  if (!appdata.interface)
    get_offline_packet ();

  /* Close the capture */
  pcap_stats (pch_struct, &ps);
  g_my_info("libpcap received %d packets, dropped %d. EtherApe saw %lu",
	      ps.ps_recv, ps.ps_drop, appdata.n_packets);
  pcap_close (pch_struct);
  g_my_info(_("Capture device stopped or file closed"));

  return TRUE;
}				/* stop_capture */

/*
 * Makes sure we don't leave any open device behind, or else we
 * might leave it in promiscous mode
 */
void
cleanup_capture (void)
{
  stop_capture ();
  dns_close(); /* closes the dns resolver, if opened */
}


/* This is a timeout function used when reading from capture files 
 * It forces a waiting time so that it reproduces the rate
 * at which packets were coming */
static guint
get_offline_packet (void)
{
  static struct pcap_pkthdr *pkt_header = NULL;
  static const u_char *pkt_data = NULL;
  static struct timeval last_read_time = { 0, 0 }, this_time;
  double diffms;
  int result;

  if (capture_status == STOP)
    {
      pkt_header = NULL;
      pkt_data = NULL;
      last_read_time.tv_usec = last_read_time.tv_sec = 0;
      return FALSE;
    }

  if (pkt_data)
  {
    gettimeofday (&appdata.now, NULL);
    packet_acquired( (guint8 *)pkt_data, pkt_header->caplen, pkt_header->len);
  }

  result = pcap_next_ex(pch_struct, &pkt_header, &pkt_data);
  switch (result)
    {
    case 1:
      if (last_read_time.tv_sec == 0 && last_read_time.tv_usec == 0)
        {
          last_read_time.tv_sec = pkt_header->ts.tv_sec;
          last_read_time.tv_usec = pkt_header->ts.tv_usec;
        }

      this_time.tv_sec = pkt_header->ts.tv_sec;
      this_time.tv_usec = pkt_header->ts.tv_usec;

      diffms = substract_times_ms(&this_time, &last_read_time);

      /* diff can be negative when listening to multiple interfaces.
       * In that case the delay is zeroed */
      if (diffms < 0)
        ms_to_next = 0; 
      else 
        ms_to_next = diffms;
      if (ms_to_next < appdata.min_delay)
          ms_to_next = appdata.min_delay;
      else if (ms_to_next > appdata.max_delay)
          ms_to_next = appdata.max_delay;

      last_read_time = this_time;
      break;
    case -2:
      capture_status = CAP_EOF;
      /* xml dump if needed */
      if (appdata.export_file_final)
        dump_xml(appdata.export_file_final);
      break;
    default:
      ms_to_next=0; /* error or timeout, ignore packet */
    }
  return FALSE;
}				/* get_offline_packet */

/* cancels the current timer, forcing another packet */
void force_next_packet(void)
{
  if (ms_to_next > 20)
    {
      ms_to_next = 0;
      g_source_remove(capture_source);
    }
}

static void
cap_t_o_destroy (gpointer data)
{

  if (capture_status == PLAY)
    capture_source = g_timeout_add_full (G_PRIORITY_DEFAULT,
					 ms_to_next,
					 (GtkFunction) get_offline_packet,
					 data,
					 (GDestroyNotify) cap_t_o_destroy);

}				/* cap_t_o_destroy */


/* This function is the gdk callback called when the capture socket holds data */
static void
gdk_input_callback(gpointer dummy, gint source, GdkInputCondition condition)
{
  get_live_packet();
}

static guint get_live_packet(void)
{
  struct pcap_pkthdr *pkt_header = NULL;
  const u_char *pkt_data = NULL;

  if (capture_status != PLAY && capture_status != PAUSE )
    return FALSE; /* stop timer */

  /* Get next packet */
  if (pcap_next_ex(pch_struct, &pkt_header, &pkt_data) == 1)
    {
      /* packet read */
      
      /* Redhat's pkt_header.ts is not a timeval, so I can't just copy 
       * the structures */
      appdata.now.tv_sec = pkt_header->ts.tv_sec;
      appdata.now.tv_usec = pkt_header->ts.tv_usec;
     
      if (pkt_data)
        packet_acquired( (guint8 *)pkt_data, pkt_header->caplen, pkt_header->len);
    }
  return FALSE;
}

static void live_timeout(gpointer data)
{
  if (capture_status == PLAY)
    capture_source = g_timeout_add_full (G_PRIORITY_DEFAULT,
                                         1,
                                         (GtkFunction) get_live_packet,
                                         data,
                                         (GDestroyNotify) live_timeout);
}
