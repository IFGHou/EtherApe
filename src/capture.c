
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

#include <gnome.h>
#include <ctype.h>
#include <netinet/in.h>
#include <pcap.h>

#include "globals.h"
#include "capture.h"
#include "names.h"
#include "node.h"
#include "conversations.h"
#include "dns.h"
#include "decode_proto.h"
#include "protocols.h"

#define MAXSIZE 200
#define PCAP_TIMEOUT 250

/* Used on some functions to indicate how to operate on the node info
 * depending on what side of the comm the node was at */
typedef enum
{
  SRC = 0,
  DST = 1
}
create_node_type_t;

static pcap_t *pch_struct;		/* pcap structure */
static gint pcap_fd;			/* The file descriptor used by libpcap */
static gint capture_source;		/* It's the input tag or the timeout tag,
				 * in online or offline mode */
static guint ms_to_next;	/* Used for offline mode to store the amount
				 * of time that we have to wait between
				 * one packet and the next */
static enum status_t capture_status = STOP;

/* Local funtions declarations */
static guint get_offline_packet (void);
static void cap_t_o_destroy (gpointer data);
static void read_packet_live(gpointer dummy, gint source,
			 GdkInputCondition condition);
static void packet_acquired(guint8 * packet, guint raw_size, guint pkt_size);

static void add_node_packet (const guint8 * packet,
                             guint raw_size,
			     packet_info_t * packet_info,
                             const node_id_t *node_id,
			     packet_direction direction);


static void dns_ready (gpointer data, gint fd, GdkInputCondition cond);

/* etherape has to handle several data link layer (OSI L2) packet types.
 * The following table tries to make easier adding new types.
 * End of table is signaled by an entry with lt_desc NULL */
typedef struct
{
  const gchar *lt_desc; /* linktype description */
  int dlt_linktype; /* pcap link type (DLT_xxxx defines) */
  apemode_t l2_idtype;   /* link level address slot in node_id_t */
  short l2_dst_ofs; /* offset of level 2 dst addr. -1 if no l2 data */
  short l2_src_ofs; /* offset of level 2 src addr. -1 if no l2 data */
  short l3_ofs;  /* offset from start of packet to level 3 layer */
} linktype_data_t;

static linktype_data_t linktypes[] = {
 {"Ethernet",     DLT_EN10MB,  LINK6, 0,  6, 14 },
 {"RAW",          DLT_RAW,     IP,   -1, -1,  0 }, /* raw IP, like PPP o SLIP */
#ifdef DLT_LINUX_SLL
 {"LINUX_SLL",    DLT_LINUX_SLL, IP, -1, -1, 16 }, /* Linux cooked */
#endif  
 {"BSD Loopback", DLT_NULL,    IP,   -1, -1,  4 }, /* we ignore l2 data */
 {"FDDI",         DLT_FDDI,    LINK6, 1,  7, 21 },
 {"IEEE802",      DLT_IEEE802, LINK6, 2,  8, 22 }, /* Token Ring */
  
};

const linktype_data_t *lkentry = NULL;


/* 
 * FUNCTION DEFINITIONS
 */

/* Sets the correct linktype entry. Returns false if not found */
static gboolean setup_linktype(int linktype)
{
  int i;

  lkentry = NULL;
  for (i = 0; linktypes[i].lt_desc != NULL ; ++i)
    {
      if (linktypes[i].dlt_linktype == linktype)
        {
          lkentry = linktypes + i;
          return TRUE;
        }
    }
  
  return FALSE; /* link type not supported */
}

/* true if current device captures l2 data */ 
gboolean has_linklevel(void)
{
  if (lkentry)
    return lkentry->l2_idtype == LINK6;
  else
    return FALSE;
}


/* Returns the node id for the current mode in this particular packet */
static node_id_t 
get_node_id (const guint8 * raw_packet, size_t raw_size, create_node_type_t node_type)
{
  node_id_t node_id;

  memset( &node_id, 0, sizeof(node_id));
  node_id.node_type = pref.mode;

  if (!lkentry)
    {
      g_error (_("Unsupported link type"));
      return;  /* g_error() aborts, but just to be sure ... */
    }
  
  switch (pref.mode)
    {
    case LINK6:
      if (lkentry->l2_idtype == LINK6)
        {
          int minlen = lkentry->l2_dst_ofs;
          if (minlen < lkentry->l2_src_ofs)
            minlen = lkentry->l2_src_ofs;
          
          if (raw_size < minlen + sizeof(node_id.addr.eth))
            {
              g_critical(_("Received subsize %s packet! Forged ?"), 
                         lkentry->lt_desc);
            }
          else
            {
              if (node_type == SRC)
                g_memmove(node_id.addr.eth, raw_packet + lkentry->l2_src_ofs, 
                          sizeof(node_id.addr.eth));
              else
                g_memmove(node_id.addr.eth, raw_packet+lkentry->l2_dst_ofs, 
                          sizeof(node_id.addr.eth));
            }
        }
      else
        {
          g_critical(_("Can't handle linktype %d (%s) at link layer level. "
                       "Please select IP or above"), lkentry->l2_idtype, 
                                                     lkentry->lt_desc);
        }
      break;
    case IP:
      if (raw_size >= lkentry->l3_ofs + 16 + sizeof(node_id.addr.ip4))
        {
          if (node_type == SRC)
            g_memmove(node_id.addr.ip4, raw_packet + lkentry->l3_ofs + 12, 
                      sizeof(node_id.addr.ip4));
          else
            g_memmove(node_id.addr.ip4, raw_packet + lkentry->l3_ofs + 16, 
                      sizeof(node_id.addr.ip4));
        }
        else
          g_critical(_("Received subsize IP packet! Forged ?"));
      break;
    case TCP:
      if (raw_size >= lkentry->l3_ofs + 24)
        {
          if (node_type == SRC)
            {
              guint16 port;
              g_memmove (node_id.addr.tcp4.host, raw_packet + 
                         lkentry->l3_ofs + 12, 4);
              port = ntohs (*(guint16 *) (raw_packet + lkentry->l3_ofs + 20));
              g_memmove (node_id.addr.tcp4.port, &port, 2);
            }
          else
            {
              guint16 port;
              g_memmove (node_id.addr.tcp4.host, 
                         raw_packet + lkentry->l3_ofs + 16, 4);
              port = ntohs (*(guint16 *) (raw_packet + lkentry->l3_ofs + 22));
              g_memmove (node_id.addr.tcp4.port, &port, 2);
            }
        }
        else
          g_critical(_("Received subsize TCP/UDP packet! Forged ?"));
      break;
    default:
      g_error (_("Reached default in get_node_id"));
    }

  return node_id;
}				/* get_node_id */

enum status_t get_capture_status(void)
{
  return capture_status;
}

/* Sets up the pcap device
 * Sets up the mode and related variables
 * Sets up dns if needed
 * Sets up callbacks for pcap and dns
 * Creates nodes and links trees */
gchar *
init_capture (void)
{
  gchar *device;
  gchar ebuf[300];
  gchar *str = NULL;
//  gboolean error = FALSE;
  int linktype;		/* Type of device we are listening to */
  static gchar errorbuf[300];
  static gboolean data_initialized = FALSE;

  if (!data_initialized)
    {

      if (pref.name_res)
	{
	  dns_open ();
          if (dns_hasfd())
             {
                g_my_debug ("File descriptor for DNS is %d", dns_waitfd ());
                gdk_input_add (dns_waitfd (),
                               GDK_INPUT_READ, (GdkInputFunction) dns_ready, NULL);
             }
	}

      capture_status = STOP;
      data_initialized = TRUE;
    }

  device = pref.interface;
  if (!device && !pref.input_file)
    {
      *ebuf = '\0'; /* reset error buffer before calling pcap functions */
      device = g_strdup (pcap_lookupdev (ebuf));
      if (device == NULL)
	{
	  snprintf (errorbuf, sizeof(errorbuf), _("Error getting device: %s"), ebuf);
	  return errorbuf;
	}
      /* TODO I should probably tidy this up, I probably don't
       * need the local variable device. But I need to reset 
       * interface since I need to know whether we are in
       * online or offline mode later on */
      pref.interface = device;
    }


  if (!pref.input_file)
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
		   pref.input_file, device);
	  return errorbuf;
	}
      *ebuf = '\0'; /* reset error buffer before calling pcap functions */
      if (!(pch_struct = pcap_open_offline (pref.input_file, ebuf)))
	{
	  snprintf (errorbuf, sizeof(errorbuf), 
                  _("Error opening %s : %s"), pref.input_file, ebuf);
	  return errorbuf;
	}
      g_my_info (_("%s opened for offline capture"), pref.input_file);

    }

  linktype = pcap_datalink(pch_struct);
  if (!setup_linktype(linktype))
    {
        snprintf (errorbuf, sizeof(errorbuf), _("Link type %d not supported"), 
                  linktype);
        return errorbuf;
    }
  
  g_my_info (_("Link type is %s"), lkentry->lt_desc);

  if (pref.mode == DEFAULT)
    pref.mode = IP;
  if (pref.mode == LINK6 && lkentry->l2_idtype != pref.mode)
    {
      snprintf (errorbuf, sizeof(errorbuf), _("Mode not available in this device"));
      return errorbuf;
    }
  
  /* TODO Shouldn't we free memory somwhere because of the strconcat? */
  switch (pref.mode)
    {
    case IP:
      if (pref.filter)
	str = g_strconcat ("ip and ", pref.filter, NULL);
      else
	{
	  g_free (pref.filter);
	  pref.filter = NULL;
	  str = g_strdup ("ip");
	}
      break;
    case TCP:
      if (pref.filter && *pref.filter)
	str = g_strconcat ("tcp and ", pref.filter, NULL);
      else
	{
	  g_free (pref.filter);
	  pref.filter = NULL;
	  str = g_strdup ("tcp");
	}
      break;
    case DEFAULT:
    case LINK6:
      if (pref.filter)
	str = g_strdup (pref.filter);
      break;
    }
  if (pref.filter)
    g_free (pref.filter);
  pref.filter = str;
  str = NULL;

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
  bpf_u_int32 netmask;
  struct bpf_program fp;

  if (!pch_struct)
    return 1;

  /* A capture filter was specified; set it up. */
  if (device && (pcap_lookupnet (device, &netnum, &netmask, ebuf) < 0))
    {
      g_warning (_
                  ("Couldn't obtain netmask info (%s). Filters involving broadcast addresses could behave incorrectly."),
                 ebuf);
      netmask = 0;
    }
  if (pcap_compile (pch_struct, &fp, filter_string, 1, netmask) < 0)
    g_warning (_("Unable to parse filter string (%s)."), pcap_geterr (pch_struct));
  else if (pcap_setfilter (pch_struct, &fp) < 0)
    g_warning (_("Can't install filter (%s)."), pcap_geterr (pch_struct));

  return 0;
}				/* set_filter */

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
  if (pref.interface && (capture_status == STOP))
    {
      g_my_debug (_("Starting live capture"));
      capture_source = gdk_input_add (pcap_fd,
				      GDK_INPUT_READ,
				      (GdkInputFunction) read_packet_live, NULL);
    }
  else if (!pref.interface)
    {
      g_my_debug (_("Starting offline capture"));
      capture_source = g_timeout_add_full (G_PRIORITY_DEFAULT,
					   1,
					   (GtkFunction) get_offline_packet,
					   NULL,
					   (GDestroyNotify) cap_t_o_destroy);
    }

  /* set the antialiasing */
  gc = GNOME_CANVAS (glade_xml_get_widget (xml, "canvas1"));
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

  if (!pref.interface)
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

  if (pref.interface)
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

  if (pref.filter)
    {
      g_free (pref.filter);
      pref.filter = NULL;
    }

  /* Clean the buffer */
  if (!pref.interface)
    get_offline_packet ();

  /* Close the capture */
  pcap_stats (pch_struct, &ps);
  g_my_debug ("libpcap received %d packets, dropped %d. EtherApe saw %g",
	      ps.ps_recv, ps.ps_drop, n_packets);
  pcap_close (pch_struct);
  g_my_info (_("Capture device stopped or file closed"));

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
  static struct timeval last_time = { 0, 0 }, this_time, diff;
  int result;

  if (capture_status == STOP)
    {
      pkt_header = NULL;
      pkt_data = NULL;
      last_time.tv_usec = last_time.tv_sec = 0;
      return FALSE;
    }

  if (pkt_data)
  {
    gettimeofday (&now, NULL);
    packet_acquired( (guint8 *)pkt_data, pkt_header->caplen, pkt_header->len);
  }

  result = pcap_next_ex(pch_struct, &pkt_header, &pkt_data);
  switch (result)
    {
    case 1:
      if (last_time.tv_sec == 0 && last_time.tv_usec == 0)
        {
          last_time.tv_sec = pkt_header->ts.tv_sec;
          last_time.tv_usec = pkt_header->ts.tv_usec;
        }

      this_time.tv_sec = pkt_header->ts.tv_sec;
      this_time.tv_usec = pkt_header->ts.tv_usec;

      diff = substract_times (this_time, last_time);

      /* diff can be negative when listening to multiple interfaces.
       * In that case the delay is zeroed */
      if (diff.tv_sec < 0)
        ms_to_next = 0; 
      else 
        ms_to_next = diff.tv_sec * 1000 + diff.tv_usec / 1000;
      if (ms_to_next > pref.max_delay)
          ms_to_next = pref.max_delay;

      last_time = this_time;
      break;
    case -2:
      capture_status = CAP_EOF;
      break;
    default:
      ms_to_next=0; /* error or timeout, ignore packet */
    }
  return FALSE;
}				/* get_offline_packet */


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
read_packet_live(gpointer dummy, gint source, GdkInputCondition condition)
{
  struct pcap_pkthdr *pkt_header = NULL;
  const u_char *pkt_data = NULL;

  /* Get next packet */
  if (pcap_next_ex(pch_struct, &pkt_header, &pkt_data) != 1)
    return; /* read failed */

  /* Redhat's pkt_header.ts is not a timeval, so I can't
   * just copy the structures */
  now.tv_sec = pkt_header->ts.tv_sec;
  now.tv_usec = pkt_header->ts.tv_usec;
 
  if (pkt_data)
    packet_acquired( (guint8 *)pkt_data, pkt_header->caplen, pkt_header->len);
}

/* This function is called everytime there is a new packet in
 * the network interface. It then updates traffic information
 * for the appropriate nodes and links 
 * Receives both the captured (raw) size and the real packet size */
static void
packet_acquired(guint8 * raw_packet, guint raw_size, guint pkt_size)
{
  packet_info_t *packet;
  node_id_t src_node_id;
  node_id_t dst_node_id;
  link_id_t link_id;

  if (!lkentry)
    {
      g_error(_("Data link entry not initialized"));
      return;  /* g_error() should abort, but just to be sure ... */
    }
  
  /* We create a packet structure to hold data */
  packet = g_malloc (sizeof (packet_info_t));
  g_assert(packet);
  
  packet->size = pkt_size;
  packet->timestamp = now;
  packet->ref_count = 0;

  /* Get a string with the protocol tree */
  packet->prot_desc = get_packet_prot (raw_packet, raw_size, 
                                       lkentry->dlt_linktype, lkentry->l3_ofs);

  src_node_id = get_node_id (raw_packet, raw_size, SRC);
  dst_node_id = get_node_id (raw_packet, raw_size, DST);

  n_packets++;
  total_mem_packets++;

  /* Add this packet information to the src and dst nodes. If they
   * don't exist, create them */
  add_node_packet (raw_packet, raw_size, packet, &src_node_id, OUTBOUND);
  add_node_packet (raw_packet, raw_size, packet, &dst_node_id, INBOUND);

  /* And now we update link traffic information for this packet */
  link_id.src = src_node_id;
  link_id.dst = dst_node_id;
  links_catalog_add_packet(&link_id, packet);

  /* finally, update global protocol stats */
  protocol_summary_add_packet(packet);
}


/* We update node information for each new packet that arrives in the
 * network. If the node the packet refers to is unknown, we
 * create it. */
static void
add_node_packet (const guint8 * raw_packet,
                 guint raw_size,
		 packet_info_t * packet,
                 const node_id_t *node_id,
		 packet_direction direction)
{
  node_t *node;

  node = nodes_catalog_find(node_id);
  if (node == NULL)
    {
      /* creates the new node, adding it to the catalog */
      node = nodes_catalog_new(node_id);
      g_assert(node);
    }

  traffic_stats_add_packet(&node->node_stats, packet, direction);

  /* If this is the first packet we've heard from the node in a while, 
   * we add it to the list of new nodes so that the main app know this 
   * node is active again */
  if (node->node_stats.n_packets == 1)
    new_nodes_add(node);

  /* Update names list for this node */
  get_packet_names (&node->node_stats.stats_protos, raw_packet, raw_size,
		    packet->prot_desc, direction, lkentry->dlt_linktype);

}				/* add_node_packet */

/* Callback function everytime a dns_lookup function is finished */
static void
dns_ready (gpointer data, gint fd, GdkInputCondition cond)
{
  dns_ack ();
}

