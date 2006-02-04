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
static struct pcap_pkthdr phdr;
static gint pcap_fd;			/* The file descriptor used by libpcap */
static gint capture_source;		/* It's the input tag or the timeout tag,
				 * in online or offline mode */
static guint32 ms_to_next;	/* Used for offline mode to store the amount
				 * of time that we have to wait between
				 * one packet and the next */
static guint node_id_length;		/* Length of the node_id key. Depends
				 * on the mode of operation */


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

static void set_node_name (node_t * node, const gchar * preferences);


static GString *print_mem (const node_id_t *node_id);
static void dns_ready (gpointer data, gint fd, GdkInputCondition cond);


/* 
 * FUNCTION DEFINITIONS
 */

/* Returns a pointer to a set of octects that define a link for the
 * current mode in this particular packet */
static node_id_t 
get_node_id (const guint8 * raw_packet, size_t raw_size, create_node_type_t node_type)
{
  node_id_t node_id;
  memset( &node_id, 0, sizeof(node_id));
  node_id.node_type = pref.mode;

  switch (pref.mode)
    {
    case ETHERNET:
      if (raw_size >= 6+sizeof(node_id.addr.eth))
        {
          if (node_type == SRC)
            g_memmove(node_id.addr.eth, raw_packet + 6, sizeof(node_id.addr.eth));
          else
            g_memmove(node_id.addr.eth, raw_packet, sizeof(node_id.addr.eth));
        }
        else
          g_critical(_("Received subsize ethernet packet! Forged ?"));
      break;
    case FDDI:
      if (raw_size >= 7+sizeof(node_id.addr.fddi))
        {
          if (node_type == SRC)
            g_memmove(node_id.addr.fddi, raw_packet + 7, sizeof(node_id.addr.fddi));
          else
            g_memmove(node_id.addr.fddi, raw_packet + 1, sizeof(node_id.addr.fddi));
        }
        else
          g_critical(_("Received subsize FDDI packet! Forged ?"));
      break;
    case IEEE802:
      if (raw_size >= 8+sizeof(node_id.addr.i802))
        {
          if (node_type == SRC)
            g_memmove(node_id.addr.i802, raw_packet + 8, sizeof(node_id.addr.i802));
          else
            g_memmove(node_id.addr.i802, raw_packet + 2, sizeof(node_id.addr.i802));
        }
        else
          g_critical(_("Received subsize IEEE802 packet! Forged ?"));
      break;
    case IP:
      if (raw_size >= 16+sizeof(node_id.addr.ip4))
        {
          if (node_type == SRC)
            g_memmove(node_id.addr.ip4, raw_packet + l3_offset + 12, sizeof(node_id.addr.ip4));
          else
            g_memmove(node_id.addr.ip4, raw_packet + l3_offset + 16, sizeof(node_id.addr.ip4));
        }
        else
          g_critical(_("Received subsize IP packet! Forged ?"));
      break;
    case TCP:
      if (raw_size >= l3_offset+24)
        {
          if (node_type == SRC)
            {
              guint16 port;
              g_memmove (node_id.addr.tcp4.host, raw_packet + l3_offset + 12, 4);
              port = ntohs (*(guint16 *) (raw_packet + l3_offset + 20));
              g_memmove (node_id.addr.tcp4.port, &port, 2);
            }
          else
            {
              guint16 port;
              g_memmove (node_id.addr.tcp4.host, raw_packet + l3_offset + 16, 4);
              port = ntohs (*(guint16 *) (raw_packet + l3_offset + 22));
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
  gboolean error = FALSE;
  static gchar errorbuf[300];
  static gboolean data_initialized = FALSE;


  if (!data_initialized)
    {
      nodes_catalog_open();
      links_catalog_open();

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

      status = STOP;
      data_initialized = TRUE;

      n_packets = n_mem_packets = 0;
    }

  device = pref.interface;
  if (!device && !pref.input_file)
    {
      *ebuf = '\0'; /* reset error buffer before calling pcap functions */
      device = g_strdup (pcap_lookupdev (ebuf));
      if (device == NULL)
	{
	  sprintf (errorbuf, _("Error getting device: %s"), ebuf);
	  return errorbuf;
	}
      /* TODO I should probably tidy this up, I probably don't
       * need the local variable device. But I need to reset 
       * interface since I need to know whether we are in
       * online or offline mode later on */
      pref.interface = device;
    }


  end_of_file = FALSE;
  if (!pref.input_file)
    {
      *ebuf = '\0'; /* reset error buffer before calling pcap functions */
      if (!
	  (pch_struct = 
	   pcap_open_live (device, MAXSIZE, TRUE, PCAP_TIMEOUT, ebuf)))
	{
	  sprintf (errorbuf,
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
	  sprintf (errorbuf,
		   _("Can't open both %s and device %s. Please choose one."),
		   pref.input_file, device);
	  return errorbuf;
	}
      *ebuf = '\0'; /* reset error buffer before calling pcap functions */
      if (!(pch_struct = pcap_open_offline (pref.input_file, ebuf)))
	{
	  sprintf (errorbuf, _("Error opening %s : %s"), pref.input_file,
		   ebuf);
	  return errorbuf;
	}
      g_my_info (_("%s opened for offline capture"), pref.input_file);

    }


  linktype = pcap_datalink (pch_struct);

  /* l3_offset is equal to the size of the link layer header */

  switch (linktype)
    {
    case L_EN10MB:
      g_my_info (_("Link type is Ethernet"));
      if (pref.mode == DEFAULT)
	pref.mode = IP;
      if (pref.mode == FDDI)
	error = TRUE;
      l3_offset = 14;
      break;
    case L_RAW:		/* The case for PPP or SLIP, for instance */
      g_my_info (_("Link type is RAW"));
      if (pref.mode == DEFAULT)
	pref.mode = IP;
      if ((pref.mode == ETHERNET) || (pref.mode == FDDI)
	  || (pref.mode == IEEE802))
	error = TRUE;
      l3_offset = 0;
      break;
    case L_FDDI:		/* We are assuming LLC async frames only */
      g_my_info (_("Link type is FDDI"));
      if (pref.mode == DEFAULT)
	pref.mode = IP;
      if ((pref.mode == ETHERNET) || (pref.mode == IEEE802))
	error = TRUE;
      l3_offset = 21;
      break;
    case L_IEEE802:
      /* As far as I know IEEE802 is Token Ring */
      g_my_info (_("Link type is Token Ring"));
      if (pref.mode == DEFAULT)
	pref.mode = IP;
      if ((pref.mode == ETHERNET) || (pref.mode == FDDI))
	error = TRUE;
      l3_offset = 22;
      break;
    case L_NULL:		/* Loopback */
      g_my_info (_("Link type is NULL"));
      if (pref.mode == DEFAULT)
	pref.mode = IP;
      if ((pref.mode == ETHERNET) || (pref.mode == FDDI)
	  || (pref.mode == IEEE802))
	error = TRUE;
      l3_offset = 4;
      break;
#ifdef DLT_LINUX_SLL
    case L_LINUX_SLL:		/* Linux cooked sockets (I believe this
				 * is used for ISDN on linux) */
      g_my_info (_("Link type is Linux cooked sockets"));
      if (pref.mode == DEFAULT)
	pref.mode = IP;
      if ((pref.mode == ETHERNET) || (pref.mode == FDDI)
	  || (pref.mode == IEEE802))
	error = TRUE;
      l3_offset = 16;
      break;
#endif
    default:
      sprintf (errorbuf, _("Link type not yet supported"));
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
      if (pref.filter)
	str = g_strconcat ("tcp and ", pref.filter, NULL);
      else
	{
	  g_free (pref.filter);
	  pref.filter = NULL;
	  str = g_strdup ("tcp");
	}
      break;
    case UDP:
      if (pref.filter)
	str = g_strconcat ("udp and ", pref.filter, NULL);
      else
	{
	  g_free (pref.filter);
	  pref.filter = NULL;
	  str = g_strdup ("udp");
	}
      break;
    case DEFAULT:
    case ETHERNET:
    case IPX:
    case FDDI:
    case IEEE802:
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


  if (error)
    {
      sprintf (errorbuf, _("Mode not available in this device"));
      return errorbuf;
    }

  switch (pref.mode)
    {
    case ETHERNET:
      node_id_length = 6;
      break;
    case FDDI:
      node_id_length = 6;
      break;
    case IEEE802:
      node_id_length = 6;
      break;
    case IP:
      node_id_length = 4;
      break;
    case TCP:
      node_id_length = 6;
      break;
    default:
      sprintf (errorbuf, _("Ape mode not yet supported"));
      return errorbuf;
    }


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
#if 0
  /* TODO pending to be more general, since we want to be able 
   * to change the capturing device in runtime. */
  if (!current_device)
    current_device = g_strdup (device);

  /* A capture filter was specified; set it up. */
  if (current_device
      && (pcap_lookupnet (current_device, &netnum, &netmask, ebuf) < 0))
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
#endif
}				/* set_filter */

gboolean
start_capture (void)
{
  GnomeCanvas *gc;

  if ((status != PAUSE) && (status != STOP))
    {
      g_warning (_("Status not PAUSE or STOP at start_capture"));
      return FALSE;
    }

  /* preparing protocol summary */
  protocol_summary_open();

  /*
   * See pause_capture for an explanation of why we don't always
   * add the source
   */
  if (pref.interface && (status == STOP))
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
    gc->aa = pref.antialias;

  status = PLAY;
  return TRUE;
}				/* start_capture */

gboolean
pause_capture (void)
{
  if (status != PLAY)
    g_warning (_("Status not PLAY at pause_capture"));

  if (pref.interface)
    {
      /* Why would we want to miss packets while pausing to 
       * better analyze a moment in time? If we wanted
       * to do this it should be optional at least.
       * In order for this to work, start_capture should only
       * add the source if the pause was for an offline capture */
#if 0
      g_my_debug (_("Pausing live capture"));
      gdk_input_remove (capture_source);	/* gdk_input_remove does not
						 * return an error code */
#endif
    }
  else
    {
      g_my_debug (_("Pausing offline capture"));
      if (!end_of_file)
	{
	  if (!g_source_remove (capture_source))
	    {
	      g_warning (_("Error while trying to pause capture"));
	      return FALSE;
	    }
	}
    }

  status = PAUSE;
  return TRUE;

}


gboolean
stop_capture (void)
{
  struct pcap_stat ps;

  if ((status != PLAY) && (status != PAUSE))
    {
      g_warning (_("Status not PLAY or PAUSE at stop_capture"));
      return FALSE;
    }

  if (pref.interface)
    {
      g_my_debug (_("Stopping live capture"));
      gdk_input_remove (capture_source);	/* gdk_input_remove does not
						 * return an error code */
    }
  else
    {
      g_my_debug (_("Stopping offline capture"));
      if (!end_of_file)
	{
	  if (!g_source_remove (capture_source))
	    {
	      g_warning (_
			 ("Error while removing capture source in stop_capture"));
	      return FALSE;
	    }
	}
    }

  status = STOP;

  /* With status in STOP, all protocols, nodes and links will be deleted */
  protocol_summary_close();
  nodes_catalog_update_all();
  links_catalog_update_all();

  /* Free conversations */
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
  n_packets = 0;
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
  if (status != STOP)
    stop_capture ();
  dns_close(); /* closes the dns resolver, if opened */
}


/* This is a timeout function used when reading from capture files 
 * It forces a waiting time so that it reproduces the rate
 * at which packets were coming */
static guint
get_offline_packet (void)
{
  static guint8 *packet = NULL;
  static struct timeval last_time = { 0, 0 }, this_time, diff;

  if (status == STOP)
    {
      packet = NULL;
      last_time.tv_usec = last_time.tv_sec = 0;
      return FALSE;
    }

  if (packet)
  {
    gettimeofday (&now, NULL);
    packet_acquired(packet, phdr.caplen, phdr.len);
  }

  packet = (guint8 *) pcap_next (pch_struct, &phdr);
  if (!packet)
    end_of_file = TRUE;

  if (last_time.tv_sec == 0 && last_time.tv_usec == 0)
    {
      last_time.tv_sec = phdr.ts.tv_sec;
      last_time.tv_usec = phdr.ts.tv_usec;
    }

  this_time.tv_sec = phdr.ts.tv_sec;
  this_time.tv_usec = phdr.ts.tv_usec;

  diff = substract_times (this_time, last_time);
  ms_to_next = diff.tv_sec * 1000 + diff.tv_usec / 1000;

  last_time = this_time;

  return FALSE;
}				/* get_offline_packet */


static void
cap_t_o_destroy (gpointer data)
{

  if ((status == PLAY) && !end_of_file)
    capture_source = g_timeout_add_full (G_PRIORITY_DEFAULT,
					 ms_to_next,
					 (GtkFunction) get_offline_packet,
					 data,
					 (GDestroyNotify) cap_t_o_destroy);

}				/* capture_t_o_destroy */


/* This function is the gdk callback called when the capture socket holds data */
static void
read_packet_live(gpointer dummy, gint source, GdkInputCondition condition)
{
  guint8 * packet = NULL;

  /* Get next packet */
  packet = (guint8 *) pcap_next (pch_struct, &phdr);

  /* Redhat's phdr.ts is not a timeval, so I can't
   * just copy the structures */
  now.tv_sec = phdr.ts.tv_sec;
  now.tv_usec = phdr.ts.tv_usec;

  if (packet)
    packet_acquired(packet, phdr.caplen, phdr.len);

}				/* packet_read */

/* This function is called everytime there is a new packet in
 * the network interface. It then updates traffic information
 * for the appropriate nodes and links 
 * Receives both the captured (raw) size and the real packet size */
static void
packet_acquired(guint8 * raw_packet, guint raw_size, guint pkt_size)
{
  packet_info_t *packet;
  const gchar *prot_desc = NULL;
  node_id_t src_node_id;
  node_id_t dst_node_id;
  link_id_t link_id;

  /* Get a string with the protocol tree */
  prot_desc = get_packet_prot (raw_packet, raw_size);

  /* We create a packet structure to hold data */
  packet = g_malloc (sizeof (packet_info_t));
  packet->size = pkt_size;
  packet->timestamp = now;
  packet->prot_desc = g_strdup (prot_desc);
  packet->ref_count = 0;

  /* If there is no node with that id, create it. Otherwise 
   * just use the one available */
  src_node_id = get_node_id (raw_packet, raw_size, SRC);
  dst_node_id = get_node_id (raw_packet, raw_size, DST);

  n_packets++;
  n_mem_packets++;

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
      GString *node_id_str = print_mem (node_id);
      node = node_create(node_id, node_id_str->str);
      nodes_catalog_insert(node);
    
      g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
             _("Creating node: %s. Number of nodes %d"),
             node_id_str->str, nodes_catalog_size());
      g_string_free(node_id_str, TRUE);
    }

  traffic_stats_add_packet(&node->node_stats, packet, direction);

  /* If this is the first packet we've heard from the node in a while, 
   * we add it to the list of new nodes so that the main app know this 
   * node is active again */
  if (node->node_stats.n_packets == 1)
    new_nodes_add(node);

  /* Update names list for this node */
  get_packet_names (&node->node_stats.stats_protos, raw_packet, raw_size,
		    packet->prot_desc, direction);

}				/* add_node_packet */

/* Callback function everytime a dns_lookup function is finished */
static void
dns_ready (gpointer data, gint fd, GdkInputCondition cond)
{
  dns_ack ();
}

/* Sets the node->name and node->numeric_name to the most used of 
 * the default name for the current mode */
void
update_node_names (node_t * node)
{
  GList *protocol_item;
  protocol_t *protocol;
  guint i = STACK_SIZE;

  /* TODO Check if it's while i or while i+1. 
   * Then fix it in other places */
  while (i + 1)
    {
      for ( protocol_item = node->node_stats.stats_protos.protostack[i]; 
            protocol_item; 
            protocol_item = protocol_item->next)
        {
          protocol = (protocol_t *) (protocol_item->data);
          protocol->node_names
            = g_list_sort (protocol->node_names, node_name_freq_compare);
        }

      i--;
    }

  switch (pref.mode)
    {
    case ETHERNET:
      set_node_name (node,
		     "ETH_II,SOLVED;802.2,SOLVED;803.3,SOLVED;"
		     "NETBIOS-DGM,n;NETBIOS-SSN,n;IP,n;"
		     "IPX-SAP,n;ARP,n;" "ETH_II,n;802.2,n;802.3,n");
      break;
    case FDDI:
      set_node_name (node,
		     "FDDI,SOLVED;NETBIOS-DGM,n;NETBIOS-SSN,n;IP,n;ARP,n;FDDI,n");
      break;
    case IEEE802:
      set_node_name (node,
		     "IEEE802,SOLVED;NETBIOS-DGM,n;NETBIOS-SSN,n;IP,n;ARP,n;IEEE802,n");
      break;
    case IP:
      set_node_name (node, "NETBIOS-DGM,n;NETBIOS-SSN,n;IP,n");
      break;
    case TCP:
      set_node_name (node, "TCP,n");
      break;
    default:
      break;
    }
}				/* update_node_names */


static void
set_node_name (node_t * node, const gchar * preferences)
{
  GList *name_item = NULL;
  name_t *name = NULL;
  const protocol_t *protocol = NULL;
  gchar **prots, **tokens;
  guint i = 0;
  guint j = STACK_SIZE;
  gboolean cont = TRUE;

  prots = g_strsplit (preferences, ";", 0);
  for (; prots[i] && cont; i++)
    {
      tokens = g_strsplit (prots[i], ",", 0);

      /* We don't do level 0, which has the topmost prot */
      for (j = STACK_SIZE; j && cont; j--)
	{
	  protocol = protocol_stack_find(&node->node_stats.stats_protos, j, tokens[0]);
	  if (protocol)
	    {
	      name_item = protocol->node_names;
	      if (!strcmp (protocol->name, tokens[0]) && name_item)
		{
		  name = (name_t *) (name_item->data);
		  /* If we require this protocol to be solved and it's not,
		   * the we have to go on */
		  if (strcmp (tokens[1], "SOLVED") || name->solved)
		    {
		      if (node->name && strcmp (node->name->str, name->name->str))
			  g_my_debug ("Switching node name from %s to %s",
				      node->name->str, name->name->str);

		      g_string_assign (node->name, name->name->str);
		      g_string_assign (node->numeric_name,
				       name->numeric_name->str);
		      cont = FALSE;
		    }
		}
	    }

	}
      g_strfreev (tokens);
      tokens = NULL;
    }
  g_strfreev (prots);
  prots = NULL;
}				/* set_node_name */


/* creates a new string from the given address */
static GString *
print_mem (const node_id_t *node_id)
{
  static const gchar hex_digits[16] = "0123456789abcdef";
  gchar cur[50];
  char punct = ':';
  gchar *p;
  int i;
  guint32 octet;
  const guint8 *ad = NULL;
  guint length = 0;

  switch (node_id->node_type)
    {
    case ETHERNET:
      ad = node_id->addr.eth;
      length = sizeof(node_id->addr.eth);
      break;
    case FDDI:
      ad = node_id->addr.fddi;
      length = sizeof(node_id->addr.fddi);
      break;
    case IEEE802:
      ad = node_id->addr.i802;
      length = sizeof(node_id->addr.i802);
      break;
    case IP:
      ad = node_id->addr.ip4;
      length = sizeof(node_id->addr.ip4);
      break;
    case TCP:
      ad = node_id->addr.tcp4.host;
      length = sizeof(node_id->addr.tcp4);
      break;
    default:
      g_error (_("Unsopported ape mode in print_mem"));
    }

  if (length * 3 >= 50)
     g_error (_("Invalid length in print_mem"));
    
  p = &cur[length * 3];
  *--p = '\0';
  i = length - 1;
  for (;;)
    {
      octet = ad[i];
      *--p = hex_digits[octet & 0xF];
      octet >>= 4;
      *--p = hex_digits[octet & 0xF];
      if (i == 0)
	break;
      if (punct)
	*--p = punct;
      i--;
    }
  return g_string_new(p);
}				/* print_mem */
