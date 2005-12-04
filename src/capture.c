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
#include "protocols.h"

#define MAXSIZE 200
#define PCAP_TIMEOUT 250

#include "globals.h"

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
static GList *new_nodes = NULL;	/* List that contains every new node not yet
				 * acknowledged by the main app with
				 * ape_get_new_node */


/* Local funtions declarations */
static guint get_offline_packet (void);
static void cap_t_o_destroy (gpointer data);
static void read_packet_live(gpointer dummy, gint source,
			 GdkInputCondition condition);
static void packet_acquired(guint8 * packet);

static node_t *create_node (const guint8 * packet, const node_id_t * node_id);

static void add_node_packet (const guint8 * packet,
			     packet_t * packet_info,
			     packet_direction direction);
static void add_link_packet (const link_id_t *link_id, packet_t * packet_info);
static void add_protocol (GList ** protocols, const gchar * stack,
		   guint pkt_size, packet_t * packet_info);

static gint update_node(node_id_t * node_id, node_t * node, gpointer delete_list_ptr);
static gboolean update_protocol (protocol_t * protocol);


static void update_node_names (node_t * node);
static void set_node_name (node_t * node, gchar * preferences);

static gboolean check_packet (packet_t * packet,
			      gpointer parent,
			      enum packet_belongs belongs_to);

static gint prot_freq_compare (gconstpointer a, gconstpointer b);
static GString *print_mem (const node_id_t *node_id);
static void dns_ready (gpointer data, gint fd, GdkInputCondition cond);


/* 
 * FUNCTION DEFINITIONS
 */

/* Returns a pointer to a set of octects that define a link for the
 * current mode in this particular packet */
static node_id_t 
get_node_id (const guint8 * packet, create_node_type_t node_type)
{
  node_id_t node_id;
  memset( &node_id, 0, sizeof(node_id));
  node_id.node_type = pref.mode;

  switch (pref.mode)
    {
    case ETHERNET:
      if (node_type == SRC)
	g_memmove(node_id.addr.eth, packet + 6, sizeof(node_id.addr.eth));
      else
	g_memmove(node_id.addr.eth, packet, sizeof(node_id.addr.eth));
      break;
    case FDDI:
      if (node_type == SRC)
	g_memmove(node_id.addr.fddi, packet + 7, sizeof(node_id.addr.fddi));
      else
	g_memmove(node_id.addr.fddi, packet + 1, sizeof(node_id.addr.fddi));
      break;
    case IEEE802:
      if (node_type == SRC)
	g_memmove(node_id.addr.i802, packet + 8, sizeof(node_id.addr.i802));
      else
	g_memmove(node_id.addr.i802, packet + 2, sizeof(node_id.addr.i802));
      break;
    case IP:
      if (node_type == SRC)
	g_memmove(node_id.addr.ip4, packet + l3_offset + 12, sizeof(node_id.addr.ip4));
      else
	g_memmove(node_id.addr.ip4, packet + l3_offset + 16, sizeof(node_id.addr.ip4));
      break;
    case TCP:
      if (node_type == SRC)
	{
	  guint16 port;
	  g_memmove (node_id.addr.tcp4.host, packet + l3_offset + 12, 4);
	  port = ntohs (*(guint16 *) (packet + l3_offset + 20));
	  g_memmove (node_id.addr.tcp4.port, &port, 2);
	}
      else
	{
	  guint16 port;
	  g_memmove (node_id.addr.tcp4.host, packet + l3_offset + 16, 4);
	  port = ntohs (*(guint16 *) (packet + l3_offset + 22));
	  g_memmove (node_id.addr.tcp4.port, &port, 2);
	}
      break;
    default:
      g_error (_("Reached default in get_node_id"));
    }

  return node_id;
}				/* get_node_id */

/* Returns a set of octects that define a link for the
 * current mode in this particular packet 
 * Is always formed by two node_ids */
static link_id_t
get_link_id (const guint8 * packet)
{
  link_id_t link_id;
  memset( &link_id, 0, sizeof(link_id));

  link_id.src = get_node_id(packet, SRC);
  link_id.dst = get_node_id(packet, DST);

  return link_id;
}				/* get_link_id */


/* Sets up the pcap device
 * Sets up the mode and related variables
 * Sets up dns if needed
 * Sets up callbacks for pcap and dns
 * Creates nodes and links trees */
gchar *
init_capture (void)
{

  guint i = STACK_SIZE;
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

      while (i + 1)
	{
	  all_protocols[i] = NULL;
	  i--;
	}
      if (!pref.numeric)
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
      if (!
	  (pch_struct = 
	   pcap_open_live (device, MAXSIZE, TRUE, PCAP_TIMEOUT, ebuf)))
	{
	  sprintf (errorbuf,
		   _("Error opening %s : %s - perhaps you need to be root?"),
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
  static bpf_u_int32 netnum, netmask;
  static struct bpf_program fp;
  gboolean ok = 1;


  if (!pch_struct)
    return 1;

  /* A capture filter was specified; set it up. */
  if (device && (pcap_lookupnet (device, &netnum, &netmask, ebuf) < 0))
    {
      g_warning (_
		 ("Can't use filter:  Couldn't obtain netmask info (%s)."),
		 ebuf);
      ok = 0;
    }
  if (ok && (pcap_compile (pch_struct, &fp, filter_string, 1, netmask) < 0))
    {
      g_warning (_("Unable to parse filter string (%s)."), pcap_geterr (pch_struct));
      ok = 0;
    }
  if (ok && (pcap_setfilter (pch_struct, &fp) < 0))
    {
      g_warning (_("Can't install filter (%s)."), pcap_geterr (pch_struct));
    }

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
		 ("Can't use filter:  Couldn't obtain netmask info (%s)."),
		 ebuf);
      ok = 0;
    }
  if (ok && (pcap_compile (pch_struct, &fp, filter_string, 1, netmask) < 0))
    {
      g_warning (_("Unable to parse filter string (%s)."), pcap_geterr (pch_struct));
      ok = 0;
    }
  if (ok && (pcap_setfilter (pch_struct, &fp) < 0))
    {
      g_warning (_("Can't install filter (%s)."), pcap_geterr (pch_struct));
    }
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
  update_protocols ();
  update_nodes ();
  links_catalog_update_all();

  /* Free conversations */
  delete_conversations ();

  /* Free the list of new_nodes */
  g_list_free (new_nodes);
  new_nodes = NULL;

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
    packet_acquired(packet);
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
    packet_acquired(packet);

}				/* packet_read */

/* This function is called everytime there is a new packet in
 * the network interface. It then updates traffic information
 * for the appropriate nodes and links */
static void
packet_acquired(guint8 * packet)
{
  packet_t *packet_info = NULL;
  gchar *prot_desc = NULL;
  node_id_t src_node_id;
  node_id_t dst_node_id;
  link_id_t link_id;

  /* Get a string with the protocol tree */
  prot_desc = get_packet_prot (packet, phdr.len);

  /* We create a packet structure to hold data */
  packet_info = g_malloc (sizeof (packet_t));
  packet_info->info.size = phdr.len;
  packet_info->info.timestamp = now;
  packet_info->info.prot_desc = g_strdup (prot_desc);
  packet_info->ref_count = 0;

  /* If there is no node with that id, create it. Otherwise 
   * just use the one available */
  src_node_id = get_node_id (packet, SRC);
  packet_info->src_id = src_node_id;

  dst_node_id = get_node_id (packet, DST);
  packet_info->dst_id = dst_node_id;

  n_packets++;
  n_mem_packets++;

  add_protocol (all_protocols, prot_desc, packet_info->info.size, packet_info);

  /* Add this packet information to the src and dst nodes. If they
   * don't exist, create them */
  add_node_packet (packet, packet_info, OUTBOUND);
  add_node_packet (packet, packet_info, INBOUND);

  link_id = get_link_id (packet);
  /* And now we update link traffic information for this packet */
  add_link_packet (&link_id, packet_info);
}				/* packet_read */



/* We update node information for each new packet that arrives in the
 * network. If the node the packet refers to is unknown, we
 * create it. */
static void
add_node_packet (const guint8 * packet,
		 packet_t * packet_info,
		 packet_direction direction)
{
  node_t *node;
  const node_id_t *node_id;

  if (direction == INBOUND)
    node_id = &packet_info->dst_id;
  else
    node_id = &packet_info->src_id;

  node = nodes_catalog_find(node_id);
  if (node == NULL)
    node = create_node (packet, node_id);

  /* We add a packet to the list of packets to/from that host which we want
   * to account for */
  packet_info->ref_count++;
  node->packets = g_list_prepend (node->packets, packet_info);

  /* We update the node's protocol stack with the protocol
   * information this packet is bearing */
  add_protocol (node->protocols, packet_info->info.prot_desc, packet_info->info.size, NULL);

  /* We update node info */
  node->accumulated += packet_info->info.size;
  if (direction == INBOUND)
    node->accumulated_in += packet_info->info.size;
  else
    node->accumulated_out += packet_info->info.size;
  node->aver_accu += packet_info->info.size;
  if (direction == INBOUND)
    node->aver_accu_in += packet_info->info.size;
  else
    node->aver_accu_out += packet_info->info.size;
  node->last_time = now;
  node->n_packets++;

  /* If this is the first packet we've heard from the node in a while, 
   * we add it to the list of new nodes so that the main app know this 
   * node is active again */
  if (node->n_packets == 1)
    new_nodes = g_list_prepend (new_nodes, node);

  /* Update names list for this node */
  get_packet_names (node->protocols, packet, packet_info->info.size,
		    packet_info->info.prot_desc, direction);

}				/* add_node_packet */


/* Save as above plus we update protocol aggregate information */
static void
add_link_packet (const link_id_t *link_id, packet_t * packet_info)
{
  link_t *link;

  /* retrieves link from catalog, creating a new one if necessary */
  link = links_catalog_find_create(link_id);

  /* We add the packet structure the list of
   * packets that this link has */
  packet_info->ref_count++;
  link->link_packets = g_list_prepend (link->link_packets, packet_info);

  /* We update the link's protocol stack with the protocol
   * information this packet is bearing */
  add_protocol (link->link_protocols, packet_info->info.prot_desc, packet_info->info.size, NULL);

  /* update link info */
  link->accumulated += packet_info->info.size;
  link->last_time = now;
  link->n_packets++;

}				/* add_link_packet */

/* Allocates a new node structure, and adds it to the
 * global nodes binary tree */
static node_t *
create_node (const guint8 * packet, const node_id_t * node_id)
{
  node_t *node = NULL;
  guint i = STACK_SIZE;

  node = g_malloc (sizeof (node_t));

  node->node_id = *node_id;

  node->name = NULL;
  node->numeric_name = NULL;

  node->name = print_mem (node_id);
  node->numeric_name = print_mem (node_id);

  node->average = node->average_in = node->average_out = 0;
  node->n_packets = 0;
  node->accumulated = node->accumulated_in = node->accumulated_out = 0;
  node->aver_accu = node->aver_accu_in = node->aver_accu_out = 0;

  node->packets = NULL;
  while (i + 1)
    {
      node->protocols[i] = NULL;
      node->main_prot[i] = NULL;
      i--;
    }

  nodes_catalog_insert(node);	/* Add it to the catalog of all nodes*/

  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	 _("Creating node: %s. Number of nodes %d"),
	 node->name->str, nodes_catalog_size());

  return node;
}				/* create_node */


/* Callback function everytime a dns_lookup function is finished */
static void
dns_ready (gpointer data, gint fd, GdkInputCondition cond)
{
  dns_ack ();
}


/* For a given protocol stack, it updates both the global 
 * protocols and specific node and link list */
static void
add_protocol (GList ** prot_list, const gchar * stack,
	      guint pkt_size, packet_t * packet)
{
  GList *protocol_item = NULL;
  protocol_t *protocol_info = NULL;
  gchar **tokens = NULL;
  guint i = 0;
  gchar *protocol_name;

  tokens = g_strsplit (stack, "/", 0);

  for (; i <= STACK_SIZE; i++)
    {
      if (pref.group_unk && strstr (tokens[i], "TCP-Port"))
	protocol_name = "TCP-Unknown";
      else if (pref.group_unk && strstr (tokens[i], "UDP-Port"))
	protocol_name = "UDP-Unknown";
      else
	protocol_name = tokens[i];

      /* If there is yet not such protocol, create it */
      if (!(protocol_item = g_list_find_custom (prot_list[i],
						protocol_name,
						protocol_compare)))
	{
	  protocol_info = protocol_t_create(protocol_name);
	  prot_list[i] = g_list_prepend (prot_list[i], protocol_info);
	}
      else
	protocol_info = protocol_item->data;

      protocol_info->last_heard = now;
      protocol_info->accumulated += phdr.len;
      protocol_info->aver_accu += phdr.len;
      protocol_info->n_packets++;

      /* We add a packet to the list of packets that used this protocol */
      if (packet)
	{
	  packet->ref_count++;
	  protocol_info->packets =
	    g_list_prepend (protocol_info->packets, packet);
	}

    }
  g_strfreev (tokens);
}				/* add_protocol */

/* gfunc called by g_list_foreach to remove the node */
static void
gfunc_remove_node(gpointer data, gpointer user_data)
{
  nodes_catalog_remove( (const node_id_t *) data);
}

/* Calls update_node for every node. This is actually a function that
 shouldn't be called often, because it might take a very long time 
 to complete */
void
update_nodes (void)
{
  GList *delete_list = NULL;

  /* we can't delete nodes while traversing the catalog, so while updating we 
   * fill a list with the node_id's to remove */
  nodes_catalog_foreach((GTraverseFunc) update_node, &delete_list);

  /* after, remove all nodes on the list from catalog 
   * WARNING: after this call, the list items are also destroyed */
  g_list_foreach(delete_list, gfunc_remove_node, NULL);
  
  /* free the list - list items are already destroyed */
  g_list_free(delete_list);

  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
         _("Updated nodes. Active nodes %d"), nodes_catalog_size());
}				/* update_nodes */

/* update a node. If expired also add it to the to-remove list */
static gint
update_node(node_id_t * node_id, node_t * node, gpointer delete_list_ptr)
{
  struct timeval diff;

  g_assert(delete_list_ptr);

  if (node->packets)
    update_packet_list (node->packets, node, NODE);

  if (node->n_packets == 0)
    {

      diff = substract_times (now, node->last_time);

      /* Remove node if node is too old or if capture is stopped */
      if ((IS_OLDER (diff, pref.node_timeout_time)
           && pref.node_timeout_time) || (status == STOP))
        {
          GList **delete_list = (GList **)delete_list_ptr;

          g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                 _("Queuing node '%s' for remove"),node->name->str);

          /* First thing we do is delete the node from the list of new_nodes,
           * if it's there */
          new_nodes = g_list_remove (new_nodes, node);

          /* adds current to list of nodes to be delete */
          *delete_list = g_list_append( *delete_list, node_id);
        }
      else
        {
          /* The packet list structure has already been freed in
           * check_packets */
          node->packets = NULL;
          node->average = node->average_in = node->average_out = 0.0;
        }
    }

  return FALSE;
}				/* update_node */

/* Returns a node from the list of new nodes or NULL if there are no more 
 * new nodes */
node_t *
ape_get_new_node (void)
{
  node_t *node = NULL;
  GList *old_item = NULL;

  if (!new_nodes)
    return NULL;

  node = new_nodes->data;
  old_item = new_nodes;

  /* We make sure now that the node hasn't been deleted since */
  /* TODO Sometimes when I get here I have a node, but a null
   * node->node_id. What gives? */
  while (node && !nodes_catalog_find(&node->node_id))
    {
      g_my_debug
	("Already deleted node in list of new nodes, in ape_get_new_node");

      /* Remove this node from the list of new nodes */
      new_nodes = g_list_remove_link (new_nodes, new_nodes);
      g_list_free_1 (old_item);
      if (new_nodes)
	node = new_nodes->data;
      else
	node = NULL;
      old_item = new_nodes;
    }

  if (!new_nodes)
    return NULL;

  /* Remove this node from the list of new nodes */
  new_nodes = g_list_remove_link (new_nodes, new_nodes);
  g_list_free_1 (old_item);

  return node;
}				/* ape_get_new_node */


/* Update the values for the instantaneous traffic values of the global protocols
 * Delete them  if that's the case */
void
update_protocols (void)
{
  GList *item = NULL;
  protocol_t *protocol = NULL;
  guint i = 0;

  for (; i <= STACK_SIZE; i++)
    {
      item = all_protocols[i];
      while (item)
	{
	  protocol = item->data;
	  item = item->next;
	  /* If update_protocol returns false, the protocol has been deleted */
	  if (!update_protocol (protocol))
	    all_protocols[i] = g_list_remove (all_protocols[i], protocol);
	}

      /* This should be unnecesary now, shouldn't it? */
      if (status == STOP)
	{
	  g_list_free (all_protocols[i]);
	  all_protocols[i] = NULL;
	}
    }
}				/* update_protocols */

/* Update the values for the instantaneous traffic values of a particular
 * protocol, which could be either one of the globals or one that belongs
 * to a single node. Returns TRUE for regular updating, and FALSE if the
 * protocol has been deleted */

gboolean
update_protocol (protocol_t * protocol)
{
  update_packet_list (protocol->packets, protocol, PROTOCOL);

  if (protocol->aver_accu == 0)
    protocol->packets = NULL;

  if (status == STOP)
    {
      protocol_t_delete(protocol);
      return (FALSE);
    }

  return TRUE;
}				/* update_protocol */

/* This function is called to discard packets from the list 
 * of packets beloging to a node or a link, and to calculate
 * the average traffic for that node or link */
gboolean
update_packet_list (GList * packets, gpointer parent,
		    enum packet_belongs belongs_to)
{
  GList *packet_l_e = NULL;	/* Packets is a list of packets.
				 * packet_l_e is always the latest (oldest)
				 * list element */
  packet_t *packet = NULL;

  if (!packets || !parent)
    return FALSE; /* no data */

  packet_l_e = g_list_last (packets);

  while (packet_l_e)
  {
    packet_t * packet = (packet_t *)(packet_l_e->data);
    if (check_packet(packet, parent, belongs_to))
      break; /* packet valid, subsequent packets are younger, no need to go further */
    else
      {
        /* packet expired, remove from list - gets the new check position 
         * if this packet is the first of the list, all the previous packets
         * should be already destroyed. We check that remove never returns a
         * NEXT packet */
        GList *next=packet_l_e->next;
        packet_l_e = packet_list_remove(packet_l_e);
        g_assert(packet_l_e == NULL || packet_l_e != next );
      }
  } /* end while */
  
  /* TODO Move all this below to update_node and update_link */
  /* If there still is relevant packets, then calculate average
   * traffic and update names*/

  if (packet_l_e)
    {
      node_t *node = NULL;
      protocol_t *protocol = NULL;
      struct timeval difference;
      guint i = STACK_SIZE;
      gdouble usecs_from_oldest;	/* usecs since the first valid packet */

      packet_l_e = g_list_last (packets);
      packet = (packet_t *) packet_l_e->data;

      node = (node_t *) parent;
      protocol = (protocol_t *) parent;

      difference = substract_times (now, packet->info.timestamp);
      usecs_from_oldest = difference.tv_sec * 1000000 + difference.tv_usec;

      /* average in bps, so we multiply by 8 and 1000000 */
      switch (belongs_to)
	{
	case NODE:

	  node->average = 8000000 * node->aver_accu / usecs_from_oldest;
	  node->average_in = 8000000 * node->aver_accu_in / usecs_from_oldest;
	  node->average_out =
	    8000000 * node->aver_accu_out / usecs_from_oldest;
	  while (i + 1)
	    {
	      if (node->main_prot[i])
		g_free (node->main_prot[i]);
	      node->main_prot[i]
		= get_main_prot (node->protocols, i);
	      i--;
	    }
	  update_node_names (node);
	  break;
	case PROTOCOL:
	  protocol->average =
	    8000000 * protocol->aver_accu / usecs_from_oldest;
	  break;
	default:
	  g_my_critical
	    ("belongs_to not NODE, LINK or PROTOCOL in update_packet_list");
	}

      /* packet(s) remaining */
      return TRUE;
    }
  
  /* no packets remaining */
  return FALSE;
}				/* update_packet_list */

/* removes a packet from a list of packets, destroying it if necessary
 * Returns the PREVIOUS item if any, otherwise the NEXT, thus returning NULL
 * if the list is empty */
GList *
packet_list_remove(GList *item_to_remove)
{
  packet_t *packet;

  g_assert(item_to_remove);
  
  packet = (packet_t *) item_to_remove->data;
  if (packet)
    {
      /* packet exists, decrement ref count */
      packet->ref_count--;

      if (!packet->ref_count)
        {
          /* packet now unused, delete it */
          if (packet->info.prot_desc)
            {
              g_free (packet->info.prot_desc);
              packet->info.prot_desc = NULL;
            }
          g_free (packet);
          item_to_remove->data = NULL;
    
          n_mem_packets--;
        }
    }

  /* TODO I have to come back here and make sure I can't make
   * this any simpler */
  if (item_to_remove->prev)
    {
      /* current packet is not at head */
      GList *item = item_to_remove;
      item_to_remove = item_to_remove->prev; 
      g_list_delete_link (item_to_remove, item);
    }
  else
    {
      /* packet is head of list */
      item_to_remove=g_list_delete_link(item_to_remove, item_to_remove);
    }

  return item_to_remove;
}

/* Sets the node->name and node->numeric_name to the most used of 
 * the default name for the current mode */
static void
update_node_names (node_t * node)
{
  guint i = STACK_SIZE;

  /* TODO Check if it's while i or while i+1. 
   * Then fix it in other places */
  while (i + 1)
    {
      if (node->protocols[i])
	{
          GList *protocol_item = NULL;
          protocol_t *protocol = NULL;
	  protocol_item = all_protocols[i];
	  for (; protocol_item; protocol_item = protocol_item->next)
	    {
	      protocol = (protocol_t *) (protocol_item->data);
	      protocol->node_names
		= g_list_sort (protocol->node_names, node_name_freq_compare);
	    }
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
set_node_name (node_t * node, gchar * preferences)
{
  GList *name_item = NULL, *protocol_item = NULL;
  name_t *name = NULL;
  protocol_t *protocol = NULL;
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
	  protocol_item = g_list_find_custom (node->protocols[j],
					      tokens[0], protocol_compare);
	  if (protocol_item)
	    {
	      protocol = (protocol_t *) (protocol_item->data);
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


/* Finds the most commmon protocol of all the packets in a
 * given node/link */
gchar *
get_main_prot (GList ** protocols, guint level)
{
  protocol_t *protocol;
  /* If we haven't recognized any protocol at that level,
   * we say it's unknown */
  if (!protocols[level])
    return NULL;
  protocols[level] = g_list_sort (protocols[level], prot_freq_compare);
  protocol = (protocol_t *) protocols[level]->data;
  return g_strdup (protocol->name);
}				/* get_main_prot */

/* Make sure this particular packet in a list of packets beloging to 
 * either o link or a node is young enough to be relevant. Else
 * remove it from the list */
/* TODO This whole function is a mess. I must take it to pieces
 * so that it is more readble and maintainable */
static gboolean
check_packet (packet_t *packet,
	      gpointer parent, enum packet_belongs belongs_to)
{

  struct timeval result;
  double time_comparison;

  if (!packet)
    {
      g_warning (_("Null packet in check_packet"));
      return FALSE;
    }

  result = substract_times (now, packet->info.timestamp);

  /* If this packet is older than the averaging time,
   * then it is removed, and the process continues.
   * Else, we are done. 
   * For links, if the timeout time is smaller than the
   * averaging time, we use that instead */

  if ((belongs_to == NODE) || (belongs_to == PROTOCOL))
    if (pref.node_timeout_time)
      time_comparison = (pref.node_timeout_time > pref.averaging_time) ?
	pref.averaging_time : pref.node_timeout_time;
    else
      time_comparison = pref.averaging_time;
  else if (pref.link_timeout_time)
    time_comparison = (pref.link_timeout_time > pref.averaging_time) ?
      pref.averaging_time : pref.link_timeout_time;
  else
    time_comparison = pref.averaging_time;

  /* If this packet is too old, we discard it */
  /* We also delete all packets if capture is stopped */
  if (IS_OLDER (result, time_comparison) || (status == STOP))
    {
      node_t *node = NULL;
      link_t *link = NULL;
      protocol_t *protocol = NULL;
      guint i = 0;
      GList *protocol_item = NULL;
      protocol_t *protocol_info = NULL;
      gchar **tokens = NULL;
      packet_direction direction = INBOUND;
      static packet_direction last_lo_direction = INBOUND;
      gchar *protocol_name = NULL;

      node = (node_t *) parent;
      link = (link_t *) parent;
      protocol = (protocol_t *) parent;
    
      switch (belongs_to)
        {
        case NODE:
          /* Find the direction of the packet */
          if (!memcmp ( &packet->dst_id, &packet->src_id, sizeof(node_id_t)))
            {
              /* This is an evil case that can happen with the loopback
               * device, and I can only think of a hack to try to solve 
               * it */
              if (last_lo_direction == INBOUND)
                direction = OUTBOUND;
              else
                direction = INBOUND;
              last_lo_direction = direction;
            }
          else if (!memcmp (&packet->dst_id, &node->node_id, sizeof(node_id_t)))
            direction = INBOUND;
          else if (!memcmp (&packet->src_id, &node->node_id, sizeof(node_id_t)))
            direction = OUTBOUND;
          else
            g_my_critical ("Packet does not belong to node in check_packet!");
  
          /* Substract this packet's length to the accumulated */
          node->aver_accu -= packet->info.size;
          if (direction == INBOUND)
            node->aver_accu_in -= packet->info.size;
          else
            node->aver_accu_out -= packet->info.size;
          /* Decrease the number of packets */
          g_assert(node->n_packets>0);
          node->n_packets--;
          /* If it was the last packet in the queue, set
           * average to 0. It has to be done here because
           * otherwise average calculation in update_packet list 
           * requires some packets to exist */
          if (!node->aver_accu)
            node->average = 0;
  
          /* We remove protocol aggregate information */
          tokens = g_strsplit (packet->info.prot_desc, "/", 0);
          while ((i <= STACK_SIZE) && tokens[i])
            {
              if (pref.group_unk && strstr (tokens[i], "TCP-Port"))
                protocol_name = "TCP-Unknown";
              else if (pref.group_unk && strstr (tokens[i], "UDP-Port"))
                protocol_name = "UDP-Unknown";
              else
                protocol_name = tokens[i];
  
              protocol_item = g_list_find_custom (node->protocols[i],
                                                  protocol_name,
                                                  protocol_compare);
              if (!protocol_item)
                {
                  g_my_critical
                    ("Protocol not found while removing protocol information from node in check_packet");
                  break;
                }
              protocol_info = protocol_item->data;
              protocol_info->accumulated -= packet->info.size;
              i++;
            }
          g_strfreev (tokens);
          tokens = NULL;
          break;
  
        case LINK:
          /* See above for explanations */
          link->accumulated -= packet->info.size;
          if (!link->accumulated)
            link->average = 0;

          g_assert(link->n_packets>0);
          link->n_packets--;

          /* We remove protocol aggregate information */
          tokens = g_strsplit (packet->info.prot_desc, "/", 0);
          while ((i <= STACK_SIZE) && tokens[i])
            {
              if (pref.group_unk && strstr (tokens[i], "TCP-Port"))
                protocol_name = "TCP-Unknown";
              else if (pref.group_unk && strstr (tokens[i], "UDP-Port"))
                protocol_name = "UDP-Unknown";
              else
                protocol_name = tokens[i];
  
              protocol_item = g_list_find_custom (link->link_protocols[i],
                                                  protocol_name,
                                                  protocol_compare);
              if (!protocol_item)
                {
                  g_my_critical
                    ("Protocol not found while removing protocol information from link in check_packet");
                  break;
                }
              protocol_info = protocol_item->data;
              protocol_info->accumulated -= packet->info.size;
  
              if (!protocol_info->accumulated)
                {
                  g_free (protocol_info->name);
                  protocol_info->name = NULL;
                  g_free (protocol_info);
  
                  link->link_protocols[i] =
                    g_list_delete_link(link->link_protocols[i], protocol_item);
                }
              i++;
            }
          g_strfreev (tokens);
          tokens = NULL;
          break;
        case PROTOCOL:
          protocol->aver_accu -= packet->info.size;
          if (!protocol->aver_accu)
            protocol->average = 0;
          break;
        }

      /* expired packet, remove */
      return FALSE;
    }

  /* packet still valid */
  return TRUE;
}				/* check_packet */

#if 0 // just in case ...
/* Make sure this particular packet in a list of packets beloging to 
 * either o link or a node is young enough to be relevant. Else
 * remove it from the list */
/* TODO This whole function is a mess. I must take it to pieces
 * so that it is more readble and maintainable */
static gboolean
check_packet (GList * packets, GList ** packet_l_e,
	      gpointer parent, enum packet_belongs belongs_to)
{

  struct timeval result;
  double time_comparison;
  guint i = 0;
  node_t *node = NULL;
  link_t *link = NULL;
  protocol_t *protocol = NULL;
  GList *protocol_item = NULL;
  protocol_t *protocol_info = NULL;
  gchar **tokens = NULL;
  packet_t *packet = NULL;
  packet_direction direction = INBOUND;
  static packet_direction last_lo_direction = INBOUND;
  gchar *protocol_name = NULL;

  packet = (packet_t *) packets->data;

  if (!packet)
    {
      g_warning (_("Null packet in check_packet"));
      return FALSE;
    }

  result = substract_times (now, packet->info.timestamp);

  /* If this packet is older than the averaging time,
   * then it is removed, and the process continues.
   * Else, we are done. 
   * For links, if the timeout time is smaller than the
   * averaging time, we use that instead */

  if ((belongs_to == NODE) || (belongs_to == PROTOCOL))
    if (pref.node_timeout_time)
      time_comparison = (pref.node_timeout_time > pref.averaging_time) ?
	pref.averaging_time : pref.node_timeout_time;
    else
      time_comparison = pref.averaging_time;
  else if (pref.link_timeout_time)
    time_comparison = (pref.link_timeout_time > pref.averaging_time) ?
      pref.averaging_time : pref.link_timeout_time;
  else
    time_comparison = pref.averaging_time;

  node = (node_t *) parent;
  link = (link_t *) parent;
  protocol = (protocol_t *) parent;

  /* If this packet is too old, we discard it */
  /* We also delete all packets if capture is stopped */
  if (IS_OLDER (result, time_comparison) || (status == STOP))
    {

      switch (belongs_to)
	{
	case NODE:
	  /* Find the direction of the packet */
	  if (!memcmp ( &packet->dst_id, &packet->src_id, sizeof(node_id_t)))
	    {
	      /* This is an evil case that can happen with the loopback
	       * device, and I can only think of a hack to try to solve 
	       * it */
	      if (last_lo_direction == INBOUND)
		direction = OUTBOUND;
	      else
		direction = INBOUND;
	      last_lo_direction = direction;
	    }
	  else if (!memcmp (&packet->dst_id, &node->node_id, sizeof(node_id_t)))
	    direction = INBOUND;
	  else if (!memcmp (&packet->src_id, &node->node_id, sizeof(node_id_t)))
	    direction = OUTBOUND;
	  else
	    g_my_critical ("Packet does not belong to node in check_packet!");

	  /* Substract this packet's length to the accumulated */
	  node->aver_accu -= packet->info.size;
	  if (direction == INBOUND)
	    node->aver_accu_in -= packet->info.size;
	  else
	    node->aver_accu_out -= packet->info.size;
	  /* Decrease the number of packets */
          g_assert(node->n_packets>0);
	  node->n_packets--;
	  /* If it was the last packet in the queue, set
	   * average to 0. It has to be done here because
	   * otherwise average calculation in update_packet list 
	   * requires some packets to exist */
	  if (!node->aver_accu)
	    node->average = 0;

	  /* We remove protocol aggregate information */
	  tokens = g_strsplit (packet->info.prot_desc, "/", 0);
	  while ((i <= STACK_SIZE) && tokens[i])
	    {
	      if (pref.group_unk && strstr (tokens[i], "TCP-Port"))
		protocol_name = "TCP-Unknown";
	      else if (pref.group_unk && strstr (tokens[i], "UDP-Port"))
		protocol_name = "UDP-Unknown";
	      else
		protocol_name = tokens[i];

	      protocol_item = g_list_find_custom (node->protocols[i],
						  protocol_name,
						  protocol_compare);
	      if (!protocol_item)
		{
		  g_my_critical
		    ("Protocol not found while removing protocol information from node in check_packet");
		  break;
		}
	      protocol_info = protocol_item->data;
	      protocol_info->accumulated -= packet->info.size;
	      i++;
	    }
	  g_strfreev (tokens);
	  tokens = NULL;
	  break;

	case LINK:
	  /* See above for explanations */
	  link->accumulated -= packet->info.size;
          g_assert(link->n_packets>0);
	  link->n_packets--;
	  if (!link->accumulated)
	    link->average = 0;

	  /* We remove protocol aggregate information */
	  tokens = g_strsplit (packet->info.prot_desc, "/", 0);
	  while ((i <= STACK_SIZE) && tokens[i])
	    {
	      if (pref.group_unk && strstr (tokens[i], "TCP-Port"))
		protocol_name = "TCP-Unknown";
	      else if (pref.group_unk && strstr (tokens[i], "UDP-Port"))
		protocol_name = "UDP-Unknown";
	      else
		protocol_name = tokens[i];

	      protocol_item = g_list_find_custom (link->link_protocols[i],
						  protocol_name,
						  protocol_compare);
	      if (!protocol_item)
		{
		  g_my_critical
		    ("Protocol not found while removing protocol information from link in check_packet");
		  break;
		}
	      protocol_info = protocol_item->data;
	      protocol_info->accumulated -= packet->info.size;

	      if (!protocol_info->accumulated)
		{
		  g_free (protocol_info->name);
		  protocol_info->name = NULL;

		  link->link_protocols[i] =
		    g_list_remove_link (link->link_protocols[i], protocol_item);
		  g_free (protocol_info);x  
		}
	      i++;
	    }
	  g_strfreev (tokens);
	  tokens = NULL;
	  break;
	case PROTOCOL:
	  protocol->aver_accu -= packet->info.size;
	  if (!protocol->aver_accu)
	    protocol->average = 0;
	  break;
	}

      packet->ref_count--;

      if (!packet->ref_count)
	{
	  if (packet->info.prot_desc)
	    {
	      g_free (packet->info.prot_desc);
	      packet->info.prot_desc = NULL;

	    }
	  g_free (packet);
	  packets->data = packet = NULL;

	  n_mem_packets--;
	}

      /* TODO I have to come back here and make sure I can't make
       * this any simpler */
      if (packets->prev)
	{
	  GList *item;
	  packets = packets->prev;
	  item = packets->next;
	  g_list_remove_link (packets, item);
	  g_list_free_1 (item);
	  *packet_l_e = packets;
	  return (TRUE);	/* Old packet removed,
				 * keep on searching */
	}
      else
	{
	  packets->data = NULL;
	  *packet_l_e = NULL;
	  g_list_free (packets);
	  /* Last packet removed,
	   * don't search anymore
	   */
	}
    }

  return FALSE;			/* Last packet searched
				 * End search */
}				/* check_packet */
#endif

/* Comparison function used to compare two link protocols */
gint
protocol_compare (gconstpointer a, gconstpointer b)
{
  g_assert (a != NULL);
  g_assert (b != NULL);

  return strcmp (((protocol_t *) a)->name, (gchar *) b);
}

/* Comparison function to sort protocols by their accumulated traffic */
static gint
prot_freq_compare (gconstpointer a, gconstpointer b)
{
  protocol_t *prot_a, *prot_b;

  g_assert (a != NULL);
  g_assert (b != NULL);

  prot_a = (protocol_t *) a;
  prot_b = (protocol_t *) b;

  if (prot_a->accumulated > prot_b->accumulated)
    return -1;
  if (prot_a->accumulated < prot_b->accumulated)
    return 1;
  return 0;
}				/* prot_freq_compare */






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
