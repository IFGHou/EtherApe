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
#include "dns.h"
#include "eth_resolv.h"

/* 
 * FUNCTION DEFINITIONS
 */

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
      nodes = g_tree_new (node_id_compare);
      links = g_tree_new (link_id_compare);
      while (i + 1)
	{
	  protocols[i] = NULL;
	  i--;
	}
      if (!numeric)
	{
	  dns_open ();
	  dns_fd = dns_waitfd ();
	  g_my_debug ("File descriptor for DNS is %d", dns_fd);
	  gdk_input_add (dns_fd,
			 GDK_INPUT_READ, (GdkInputFunction) dns_ready, NULL);
	}
      else
	dns_fd = 0;

      status = STOP;
      data_initialized = TRUE;
    }

  device = interface;
  if (!device && !input_file)
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
      interface = device;
    }


  end_of_file = FALSE;
  if (!input_file)
    {
      if (!
	  ((pcap_t *) pch =
	   pcap_open_live (device, MAXSIZE, TRUE, PCAP_TIMEOUT, ebuf)))
	{
	  sprintf (errorbuf,
		   _("Error opening %s : %s - perhaps you need to be root?"),
		   device, ebuf);
	  return errorbuf;
	}
      pcap_fd = pcap_fileno (pch);
      g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "pcap_fd: %d", pcap_fd);
    }
  else
    {
      if (device)
	{
	  sprintf (errorbuf,
		   _("Can't open both %s and device %s. Please choose one."),
		   input_file, device);
	  return errorbuf;
	}
      if (!((pcap_t *) pch = pcap_open_offline (input_file, ebuf)))
	{
	  sprintf (errorbuf, _("Error opening %s : %s"), input_file, ebuf);
	  return errorbuf;
	}
      g_my_info (_("%s opened for offline capture"), input_file);

    }


  linktype = pcap_datalink (pch);

  /* l3_offset is equal to the size of the link layer header */

  switch (linktype)
    {
    case L_EN10MB:
      g_my_info (_("Link type is Ethernet"));
      if (mode == DEFAULT)
	mode = IP;
      if (mode == FDDI)
	error = TRUE;
      l3_offset = 14;
      break;
    case L_RAW:		/* The case for PPP or SLIP, for instance */
      g_my_info (_("Link type is RAW"));
      if (mode == DEFAULT)
	mode = IP;
      if ((mode == ETHERNET) || (mode == FDDI) || (mode == IEEE802))
	error = TRUE;
      l3_offset = 0;
      break;
    case L_FDDI:		/* We are assuming LLC async frames only */
      g_my_info (_("Link type is FDDI"));
      if (mode == DEFAULT)
	mode = IP;
      if ((mode == ETHERNET) || (mode == IEEE802))
	error = TRUE;
      l3_offset = 21;
      break;
    case L_IEEE802:
      /* As far as I know IEEE802 is Token Ring */
      g_my_info (_("Link type is Token Ring"));
      if (mode == DEFAULT)
	mode = IP;
      if ((mode == ETHERNET) || (mode == FDDI))
	error = TRUE;
      l3_offset = 22;
      break;
    case L_NULL:		/* Loopback */
      g_my_info (_("Link type is NULL"));
      if (mode == DEFAULT)
	mode = IP;
      if ((mode == ETHERNET) || (mode == FDDI) || (mode == IEEE802))
	error = TRUE;
      l3_offset = 4;
      break;
    default:
      sprintf (errorbuf, _("Link type not yet supported"));
      return errorbuf;
    }

  /* TODO Shouldn't we free memory somwhere because of the strconcat? */
  switch (mode)
    {
    case IP:
      if (filter)
	str = g_strconcat ("ip and ", filter, NULL);
      else
	{
	  g_free (filter);
	  str = g_strdup ("ip");
	}
      break;
    case TCP:
      if (filter)
	str = g_strconcat ("tcp and ", filter, NULL);
      else
	{
	  g_free (filter);
	  str = g_strdup ("tcp");
	}
      break;
    case UDP:
      if (filter)
	str = g_strconcat ("udp and ", filter, NULL);
      else
	{
	  g_free (filter);
	  str = g_strdup ("udp");
	}
      break;
    case DEFAULT:
    case ETHERNET:
    case IPX:
    case FDDI:
    case IEEE802:
      if (filter)
	str = g_strdup (filter);
      break;
    }
  g_free (filter);
  filter = str;
  str = NULL;

  if (filter)
    set_filter (filter, device);


  if (error)
    {
      sprintf (errorbuf, _("Mode not available in this device"));
      return errorbuf;
    }

  switch (mode)
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
set_filter (gchar * filter, gchar * device)
{
  gchar ebuf[300];
  static bpf_u_int32 netnum, netmask;
  static struct bpf_program fp;
  gboolean ok = 1;


  if (!pch)
    return 1;

  /* A capture filter was specified; set it up. */
  if (device && (pcap_lookupnet (device, &netnum, &netmask, ebuf) < 0))
    {
      g_warning (_
		 ("Can't use filter:  Couldn't obtain netmask info (%s)."),
		 ebuf);
      ok = 0;
    }
  if (ok && (pcap_compile (pch, &fp, filter, 1, netmask) < 0))
    {
      g_warning (_("Unable to parse filter string (%s)."), pcap_geterr (pch));
      ok = 0;
    }
  if (ok && (pcap_setfilter (pch, &fp) < 0))
    {
      g_warning (_("Can't install filter (%s)."), pcap_geterr (pch));
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
  if (ok && (pcap_compile (pch, &fp, filter, 1, netmask) < 0))
    {
      g_warning (_("Unable to parse filter string (%s)."), pcap_geterr (pch));
      ok = 0;
    }
  if (ok && (pcap_setfilter (pch, &fp) < 0))
    {
      g_warning (_("Can't install filter (%s)."), pcap_geterr (pch));
    }
#endif
}

gboolean
start_capture (void)
{

  if ((status != PAUSE) && (status != STOP))
    {
      g_warning (_("Status not PAUSE or STOP at start_capture"));
      return FALSE;
    }

  if (interface)
    {
      g_my_debug (_("Starting live capture"));
      capture_source = gdk_input_add (pcap_fd,
				      GDK_INPUT_READ,
				      (GdkInputFunction) packet_read, NULL);
    }
  else
    {
      g_my_debug (_("Starting offline capture"));
      end_of_file = FALSE;
      capture_source = g_timeout_add_full (G_PRIORITY_DEFAULT,
					   1,
					   (GtkFunction) get_offline_packet,
					   NULL,
					   (GDestroyNotify) cap_t_o_destroy);
    }

  status = PLAY;
  return TRUE;
}				/* start_capture */

gboolean
pause_capture (void)
{
  if (status != PLAY)
    g_warning (_("Status not PLAY at pause_capture"));

  if (interface)
    {
      g_my_debug (_("Pausing live capture"));
      gdk_input_remove (capture_source);	/* gdk_input_remove does not
						 * return an error code */
    }
  else
    {
      g_my_debug (_("Pausing offline capture"));
      if (!end_of_file)
	{
	  end_of_file = TRUE;	/* Otherwise a new timeout would be
				 * created automatically */
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

  if ((status != PLAY) && (status != PAUSE))
    {
      g_warning (_("Status not PLAY or PAUSE at stop_capture"));
      return FALSE;
    }

  if (interface)
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
	  end_of_file = TRUE;	/* Otherwise a new timeout would be
				 * created automatically */
	  if (!g_source_remove (capture_source))
	    {
	      g_warning (_
			 ("Error while removing capture source in stop_capture"));
	      return FALSE;
	    }
	}
    }

  /* Next time update_node and update_link are called,
     all node and link information will be deleted */
  /* TODO Perhaps we should make sure we delete all that data here instead of relying
   * in the GUI to do it (by calling update_node and update_link) */
  status = STOP;

  /* Free the list of new_nodes */
  g_list_free (new_nodes);
  new_nodes = NULL;

  if (filter)
    {
      g_free (filter);
      filter = NULL;
    }

  /* Clean the buffer */
  if (!interface)
    get_offline_packet ();
  /* Close the capture */
  pcap_close (pch);

  return TRUE;
}				/* stop_capture */


/* This is a timeout function used when reading from capture files 
 * It forces a waiting time so that it reproduces the rate
 * at which packets where coming */
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
    packet_read (packet, 0, GDK_INPUT_READ);

  packet = (guint8 *) pcap_next (pch, &phdr);
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

  if (!end_of_file)
    capture_source = g_timeout_add_full (G_PRIORITY_DEFAULT,
					 ms_to_next,
					 (GtkFunction) get_offline_packet,
					 data,
					 (GDestroyNotify) cap_t_o_destroy);

}				/* capture_t_o_destroy */


/* This function is called everytime there is a new packet in
 * the network interface. It then updates traffic information
 * for the appropriate nodes and links */
static void
packet_read (guint8 * packet, gint source, GdkInputCondition condition)
{
  guint8 *link_id = NULL;
  packet_t *packet_info = NULL;
  gchar *prot = NULL;

  /* Redhat's phdr.ts is not a timeval, so I can't
   * just copy the structures */


  /* Get next packet only if in live mode */
  if (source)
    {
      packet = (guint8 *) pcap_next (pch, &phdr);
      now.tv_sec = phdr.ts.tv_sec;
      now.tv_usec = phdr.ts.tv_usec;
    }
  else
    gettimeofday (&now, NULL);

  if (!packet)
    return;

  /* Get a string with the protocol tree */
  prot = get_packet_prot (packet, phdr.len);

  /* We create a packet structure to hold data */
  packet_info = g_malloc (sizeof (packet_t));
  packet_info->size = phdr.len;
  packet_info->timestamp = now;
  packet_info->prot = g_strdup (prot);
  packet_info->ref_count = 3;
  packet_info->src_id = g_memdup (get_node_id (packet, SRC), node_id_length);
  packet_info->dst_id = g_memdup (get_node_id (packet, DST), node_id_length);

  add_protocol (protocols, prot, phdr, packet_info->src_id, packet_info->dst_id);

  /* Add this packet information to the src and dst nodes. If they
   * don't exist, create them */
  add_node_packet (packet, packet_info, packet_info->src_id, OUTBOUND);
  add_node_packet (packet, packet_info, packet_info->dst_id, INBOUND);

  link_id = get_link_id (packet);
  /* And now we update link traffic information for this packet */
  add_link_packet (packet, packet_info, link_id);
#ifndef LEAK
  return;
#endif


}				/* packet_read */


/* Returns a pointer to a set of octects that define a link for the
 * current mode in this particular packet */
static guint8 *
get_node_id (const guint8 * packet, create_node_type_t node_type)
{
  static guint8 *node_id = NULL;

  if (node_id)
    {
      g_free (node_id);
      node_id = NULL;
    }

  switch (mode)
    {
    case ETHERNET:
      if (node_type == SRC)
	node_id = g_memdup (packet + 6, node_id_length);
      else
	node_id = g_memdup (packet, node_id_length);
      break;
    case FDDI:
      if (node_type == SRC)
	node_id = g_memdup (packet + 7, node_id_length);
      else
	node_id = g_memdup (packet + 1, node_id_length);
      break;
    case IEEE802:
      if (node_type == SRC)
	node_id = g_memdup (packet + 8, node_id_length);
      else
	node_id = g_memdup (packet + 2, node_id_length);
      break;
    case IP:
      if (node_type == SRC)
	node_id = g_memdup (packet + l3_offset + 12, node_id_length);
      else
	node_id = g_memdup (packet + l3_offset + 16, node_id_length);
      break;
    case TCP:
      node_id = g_malloc (node_id_length);
      if (node_type == SRC)
	{
	  guint16 port;
	  g_memmove (node_id, packet + l3_offset + 12, 4);
	  port = ntohs (*(guint16 *) (packet + l3_offset + 20));
	  g_memmove (node_id + 4, &port, 2);
	}
      else
	{
	  guint16 port;
	  g_memmove (node_id, packet + l3_offset + 16, 4);
	  port = ntohs (*(guint16 *) (packet + l3_offset + 22));
	  g_memmove (node_id + 4, &port, 2);
	}
      break;
    default:
      g_error (_("Reached default in get_node_id"));
    }

  return node_id;
}				/* get_node_id */

/* Returns a pointer to a set of octects that define a link for the
 * current mode in this particular packet */
static guint8 *
get_link_id (const guint8 * packet)
{
  static guint8 *link_id = NULL;
  guint16 port;

  if (link_id)
    {
      g_free (link_id);
      link_id = NULL;
    }


  switch (mode)
    {
    case ETHERNET:
      link_id = g_malloc (2 * node_id_length);
      g_memmove (link_id, packet + 6, node_id_length);
      g_memmove (link_id + 6, packet, node_id_length);
      break;
    case FDDI:
      link_id = g_malloc (2 * node_id_length);
      g_memmove (link_id, packet + 7, node_id_length);
      g_memmove (link_id + 6, packet + 1, node_id_length);
      break;
    case IEEE802:
      link_id = g_malloc (2 * node_id_length);
      g_memmove (link_id, packet + 8, node_id_length);
      g_memmove (link_id + 6, packet + 2, node_id_length);
      break;
    case IP:
      link_id = g_memdup (packet + l3_offset + 12, 2 * node_id_length);
      break;
    case TCP:

      link_id = g_malloc (2 * node_id_length);
      g_memmove (link_id, packet + l3_offset + 12, 4);
      port = ntohs (*(guint16 *) (packet + l3_offset + 20));
      g_memmove (link_id + 4, &port, 2);
      g_memmove (link_id + 6, packet + l3_offset + 16, 4);
      port = ntohs (*(guint16 *) (packet + l3_offset + 22));
      g_memmove (link_id + 10, &port, 2);
      break;
    default:
      g_error (_("Unsopported ape mode in get_link_id"));
    }
  return link_id;
}				/* get_link_id */


/* We update node information for each new packet that arrives in the
 * network. If the node the packet refers to is unknown, we
 * create it. */
static void
add_node_packet (const guint8 * packet,
		 const packet_t * packet_info,
		 const guint8 * node_id, packet_direction direction)
{
  node_t *node;
  node = g_tree_lookup (nodes, node_id);
  if (node == NULL)
    node = create_node (packet, node_id);

  /* We add a packet to the list of packets to/from that host which we want
   * to account for */
  node->packets = g_list_prepend (node->packets, packet_info);

  /* We update the node's protocol stack with the protocol
   * information this packet is bearing */
  add_protocol (node->protocols, packet_info->prot, phdr, NULL, NULL);

  /* We update node info */
  node->accumulated += packet_info->size;
  if (direction == INBOUND)
    node->accumulated_in += packet_info->size;
  else
    node->accumulated_out += packet_info->size;
  node->aver_accu += packet_info->size;
  if (direction == INBOUND)
    node->aver_accu_in += packet_info->size;
  else
    node->aver_accu_out += packet_info->size;
  node->last_time = now;
  node->n_packets++;

  /* If this is the first packet we've heard from the node in a while, 
   * we add it to the list of new nodes so that the main app know this 
   * node is active again */
  if (node->n_packets == 1)
    new_nodes = g_list_prepend (new_nodes, node);

  /* Update names list for this node */
  get_packet_names (node->protocols, packet, packet_info->size,
		    packet_info->prot, direction);

  update_node (node->node_id, node, NULL);

}				/* add_node_packet */


/* Save as above plus we update protocol aggregate information */
static void
add_link_packet (const guint8 * packet, const packet_t * packet_info,
		 const guint8 * link_id)
{
  link_t *link;

  link = g_tree_lookup (links, link_id);
  if (!link)
    link = create_link (packet, link_id);

  /* We add the packet structure the list of
   * packets that this link has */
  link->packets = g_list_prepend (link->packets, packet_info);

  /* We update the link's protocol stack with the protocol
   * information this packet is bearing */
  add_protocol (link->protocols, packet_info->prot, phdr, NULL, NULL);

  /* We update link info */
  link->accumulated += packet_info->size;
  link->last_time = now;
  link->n_packets++;

  update_link (link);

}				/* add_link_packet */

/* Allocates a new node structure, and adds it to the
 * global nodes binary tree */
static node_t *
create_node (const guint8 * packet, const guint8 * node_id)
{
  node_t *node = NULL;
  guint i = STACK_SIZE;

  node = g_malloc (sizeof (node_t));

  node->node_id = g_memdup (node_id, node_id_length);
  node->name = NULL;
  node->numeric_name = NULL;
  /* TODO remove these two, shouldn't be used anymore */
  /* We initialize the ip_address, although it won't be
   * used in many cases */
  node->ip_address = 0;
  node->numeric_ip = NULL;

  node->name = g_string_new (print_mem (node_id, node_id_length));
  node->numeric_name = g_string_new (print_mem (node_id, node_id_length));

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

  g_tree_insert (nodes, node->node_id, node);	/* Add it to the main tree of nodes */

  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	 _("Creating node: %s. Number of nodes %d"),
	 node->name->str, g_tree_nnodes (nodes));

  return node;
}				/* create_node */


/* Allocates a new link structure, and adds it to the
 * global links binary tree */
static link_t *
create_link (const guint8 * packet, const guint8 * link_id)
{
  link_t *link;
  guint i = STACK_SIZE;
  node_t *node;

  link = g_malloc (sizeof (link_t));

  link->link_id = g_memdup (link_id, 2 * node_id_length);
  link->average = 0;
  link->n_packets = 0;
  link->accumulated = 0;
  link->packets = NULL;
  link->src_name = NULL;
  link->dst_name = NULL;
  while (i + 1)
    {
      link->protocols[i] = NULL;
      link->main_prot[i] = NULL;
      i--;
    }
  g_tree_insert (links, link->link_id, link);
  node = g_tree_lookup (nodes, link_id);
  link->src_name = g_strdup (node->name->str);
  node = g_tree_lookup (nodes, (link_id + node_id_length));
  link->dst_name = g_strdup (node->name->str);

  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	 _("Creating link: %s-%s. Number of links %d"),
	 link->src_name, link->dst_name, g_tree_nnodes (links));

  return link;
}				/* create_link */


/* Callback function everytime a dns_lookup function is finished */
static void
dns_ready (gpointer data, gint fd, GdkInputCondition cond)
{
  dns_ack ();
}


/* For a given protocol stack, it updates the both the global 
 * protocols and specific node and link list */
void
add_protocol (GList ** protocols, const gchar * stack,
	      struct pcap_pkthdr phdr, const guint8 *src_id,
	      const guint8 *dst_id)
{
  GList *protocol_item = NULL;
  protocol_t *protocol_info = NULL;
  gchar **tokens = NULL;
  guint i = 0;
  tokens = g_strsplit (stack, "/", 0);

  for (; i <= STACK_SIZE; i++)
    {
      if ((protocol_item = g_list_find_custom (protocols[i],
					       tokens[i], protocol_compare)))
	{
	  protocol_info = protocol_item->data;
	  protocol_info->accumulated += phdr.len;
	  protocol_info->aver_accu += phdr.len;
	  protocol_info->n_packets++;
	}
      else
	{
	  protocol_info = g_malloc (sizeof (protocol_t));
	  protocol_info->name = g_strdup (tokens[i]);
	  protocol_info->accumulated = phdr.len;
	  protocol_info->aver_accu = phdr.len;
	  protocol_info->average = 0;
	  protocol_info->n_packets = 1;
	  protocol_info->node_names = NULL;
	  protocol_info->node_ids = NULL;
	  protocols[i] = g_list_prepend (protocols[i], protocol_info);
	}
       
       /* For the global protocols list, take note of nodes that are using
	* this protocol */
       if (src_id)
	 {
	    if (!g_list_find (protocol_info->node_ids, src_id))
	      protocol_info->node_ids=g_list_prepend (protocol_info->node_ids, src_id);
	 }
       if (dst_id)
	 {
	    if (!g_list_find (protocol_info->node_ids, src_id))
	      protocol_info->node_ids=g_list_prepend (protocol_info->node_ids, src_id);
	 }
    }
  g_strfreev (tokens);
  tokens = NULL;
}				/* add_protocol */


/* Calls update_node for every node. This is actually a function that
 shouldn't be called often, because it might take a very long time 
 to complete */
void
update_nodes (void)
{
  guint n_nodes_before, n_nodes_after;

  do
    {
      n_nodes_before = g_tree_nnodes (nodes);
      g_tree_traverse (nodes, (GTraverseFunc) update_node, G_IN_ORDER, NULL);
      n_nodes_after = g_tree_nnodes (nodes);
    }
  while (n_nodes_before != n_nodes_after);

}				/* update_nodes */

/* Deletes all data from a node, and possibly the node itself */
static gint
update_node (guint8 * node_id, node_t * node, gpointer pointer)
{
  struct timeval diff;
  guint8 *node_id = NULL;
  GList *protocol_item = NULL;
  protocol_t *protocol_info = NULL;
  guint i = STACK_SIZE;

  if (node->packets)
    update_packet_list (node->packets, (guint8 *) node, NODE);


  if (node->n_packets == 0)
    {

      diff = substract_times (now, node->last_time);

#if 0
      /* Delete node if we stop the capture */
      if (status == STOP)
#endif
#if 1
	/* Remove node if node is too old or if capture is stopped */
	if ((IS_OLDER (diff, node_timeout_time) && node_timeout_time)
	    || (status == STOP))
#endif
	  {

	    g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
		   _("Removing node: %s. Number of nodes %d"),
		   node->name->str, g_tree_nnodes (nodes) - 1);
	    node_id = node->node_id;	/* Since we are freeing the node
					 * we must free its members as well 
					 * but if we free the id then we will
					 * not be able to find the link again 
					 * to free it, thus the intermediate variable */

	    /* First thing we do is delete the node for the list of new_nodes,
	     * if it's there */
	    new_nodes = g_list_remove (new_nodes, node);

	    g_string_free (node->name, TRUE);
	    node->name = NULL;
	    g_string_free (node->numeric_name, TRUE);
	    node->numeric_name = NULL;
	    if (node->numeric_ip)
	      {
		g_string_free (node->numeric_ip, TRUE);
		node->numeric_ip = NULL;
	      }
	    for (; i + 1; i--)
	      if (node->main_prot[i])
		{
		  g_free (node->main_prot[i]);
		  node->main_prot[i] = NULL;
		}
	    i = 0;
	    while (i <= STACK_SIZE)
	      {

		while (node->protocols[i])
		  {
		    protocol_item = node->protocols[i];
		    protocol_info = protocol_item->data;

		    if (!protocol_info->accumulated)
		      {
			GList *name_item = NULL;
			name_t *name;
			g_free (protocol_info->name);
			protocol_info->name = NULL;

			while (protocol_info->node_names)
			  {
			    name_item = protocol_info->node_names;
			    name = name_item->data;
			    g_free (name->node_id);
			    g_string_free (name->name, TRUE);
			    g_string_free (name->numeric_name, TRUE);
			    protocol_info->node_names =
			      g_list_remove_link (protocol_info->node_names,
						  name_item);
			    g_free (name);
			    g_list_free (name_item);
			  }

			node->protocols[i] =
			  g_list_remove_link (node->protocols[i],
					      protocol_item);
			g_free (protocol_info);
			g_list_free (protocol_item);
		      }
		  }
		i++;
	      }

	    g_free (node);
	    g_tree_remove (nodes, node_id);
	     
	    /* Remove all mentions to this node in the globals protocols list */
	    forget_node_from_protocols (node_id);
	    
	    g_free (node_id);
	    node = NULL;
	    return TRUE;	/* I've checked it's not safe to traverse 
				 * while deleting, so we return TRUE to stop
				 * the traversion (Does that word exist? :-) */
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

/* Remove all mentions to this node in the globals protocols list */
static void
forget_node_from_protocols (guint8 *node_id)
{
   GList *protocol_item = NULL;
   protocol_t *protocol = NULL;
   guint i = 0;

   for (; i <= STACK_SIZE; i++)
     {
	protocol_item = protocols[i];
	while (protocol_item)
	    {
	       protocol = protocol_item->data;
	       protocol->node_ids = g_list_remove (protocol->node_ids, node_id);
	       protocol_item = protocol_item->next;
	    }
     }
   
}				/* forget_node_from_protocols */

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
  while (node && node->node_id && !g_tree_lookup (nodes, node->node_id))
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


link_t *
update_link (link_t * link)
{
  struct timeval diff;
  guint8 *link_id;
  guint i = STACK_SIZE;

  if (link->packets)
    update_packet_list (link->packets, (guint8 *) link, LINK);

  diff = substract_times (now, link->last_time);

  if (link->n_packets == 0)
    {


#if 0
      if (status == STOP)
#endif
#if 1
	/* Remove link if it is too old or if capture is stopped */
	if ((IS_OLDER (diff, link_timeout_time) && link_timeout_time)
	    || (status == STOP))
#endif
	  {
	    link_id = link->link_id;	/* Since we are freeing the link
					 * we must free its members as well 
					 * but if we free the id then we will
					 * not be able to find the link again 
					 * to free it, thus the intermediate variable */
	    for (; i + 1; i--)
	      if (link->main_prot[i])
		{
		  g_free (link->main_prot[i]);
		  link->main_prot[i] = NULL;
		}
	    g_free (link->src_name);
	    link->src_name = NULL;
	    g_free (link->dst_name);
	    link->dst_name = NULL;
	    g_free (link);
	    g_tree_remove (links, link_id);
	    g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
		   _("Removing link. Number of links %d"),
		   g_tree_nnodes (links));

	    g_free (link_id);
	    link = NULL;

	  }
	else
	  {
	    /* The packet list structure has already been freed in
	     * check_packets */
	    link->packets = NULL;
	    link->accumulated = 0;
	    while (i + 1)
	      {
		link->protocols[i] = NULL;
		i--;
	      }
	  }
    }

  return link;

}				/* update_link */

/* This function is called to discard packets from the list 
 * of packets beloging to a node or a link, and to calculate
 * the average traffic for that node or link */
void
update_packet_list (GList * packets, guint8 * parent,
		    enum packet_belongs belongs_to)
{
  struct timeval difference;
  guint i = STACK_SIZE;
  node_t *node = NULL;
  link_t *link = NULL;
  gdouble usecs_from_oldest;	/* usecs since the first valid packet */
  GList *packet_l_e = NULL;	/* Packets is a list of packets.
				 * packet_l_e is always the latest (oldest)
				 * list element */
  packet_t *packet = NULL;

  packet_l_e = g_list_last (packets);

  /* Going from oldest to newer, delete all irrelevant packets */
  while (check_packet (packet_l_e, &packet_l_e, parent, belongs_to));

  /* TODO Move all this below to update_node and update_link */
  /* If there still is relevant packets, then calculate average
   * traffic and update names*/

  if (packet_l_e)
    {
      packet_l_e = g_list_last (packets);
      packet = (packet_t *) packet_l_e->data;
      node = (node_t *) parent;
      link = (link_t *) parent;
      difference = substract_times (now, packet->timestamp);
      usecs_from_oldest = difference.tv_sec * 1000000 + difference.tv_usec;

      /* average in bps, so we multiply by 8 and 1000000 */
      if (belongs_to == NODE)
	{
	  node->average = 8000000 * node->aver_accu / usecs_from_oldest;
	  node->average_in = 8000000 * node->aver_accu_in / usecs_from_oldest;
	  node->average_out =
	    8000000 * node->aver_accu_out / usecs_from_oldest;
	  while (i + 1)
	    {
	      if (node->main_prot[i])
		g_free (node->main_prot[i]);
	      node->main_prot[i]
		= get_main_prot (packets, node->protocols, i);
	      i--;
	    }
	  update_node_names (node);
	}
      else
	{
	  link->average = 8000000 * link->accumulated / usecs_from_oldest;
	  /* We look for the most used protocol for this link */
	  while (i + 1)
	    {
	      if (link->main_prot[i])
		g_free (link->main_prot[i]);
	      link->main_prot[i]
		= get_main_prot (packets, link->protocols, i);
	      i--;
	    }
	}

    }
}				/* update_packet_list */

/* Sets the node->name and node->numeric_name to the most used of 
 * the default name for the current mode */
static void
update_node_names (node_t * node)
{
  guint i = STACK_SIZE;
  GList *protocol_item = NULL;
  protocol_t *protocol = NULL;

  /* TODO Check if it's while i or while i+1. 
   * Then fix it in other places */
  while (i + 1)
    {
      if (node->protocols[i])
	{
	  protocol_item = protocols[i];
	  for (; protocol_item; protocol_item = protocol_item->next)
	    {
	      protocol = (protocol_t *) (protocol_item->data);
	      protocol->node_names
		= g_list_sort (protocol->node_names, names_freq_compare);
	    }
	}
      i--;
    }

  switch (mode)
    {
    case ETHERNET:
      set_node_name (node,
		     "ETH_II,SOLVED;802.2,SOLVED;803.3,SOLVED;"
		     "NETBIOS-DGM,n;NETBIOS-SSN,n;IP,n;ARP,n;"
		     "ETH_II,n;802.2,n;802.3,n");
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
      for (j = STACK_SIZE; j && cont; j--)	/* We don't do level 0,
						 * which has the topmost prot */
	{
	  tokens = g_strsplit (prots[i], ",", 0);
	  protocol_item = g_list_find_custom (node->protocols[j],
					      tokens[0], protocol_compare);
	  if (protocol_item)
	    {
	      protocol = (protocol_t *) (protocol_item->data);
	      name_item = protocol->node_names;
	      if (name_item)
		{
		  name = (name_t *) (name_item->data);
		  /* If we require this protocol to be solved and it's not,
		   * the we have to go on */
		  if (strcmp (tokens[1], "SOLVED") || name->solved)
		    {
		      if (node->name)
			if (strcmp (node->name->str, name->name->str))
			  g_my_debug ("Switching node name from %s to %s",
				      node->name->str, name->name->str);

		      g_string_assign (node->name, name->name->str);
		      g_string_assign (node->numeric_name,
				       name->numeric_name->str);
		      cont = FALSE;
		    }
		}
	    }
	  g_strfreev (tokens);
	  tokens = NULL;

	}
    }
  g_strfreev (prots);
  prots = NULL;
}				/* set_node_name */


/* Finds the most commmon protocol of all the packets in a
 * given link (only link, by now) */
static gchar *
get_main_prot (GList * packets, GList ** protocols, guint level)
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
check_packet (GList * packets, GList ** packet_l_e,
	      guint8 * parent, enum packet_belongs belongs_to)
{

  struct timeval result;
  double time_comparison;
  guint i = 0;
  node_t *node = NULL;
  link_t *link = NULL;
  GList *protocol_item = NULL;
  protocol_t *protocol_info = NULL;
  gchar **tokens = NULL;
  packet_t *packet = NULL;
  packet_direction direction;
  static packet_direction last_lo_direction = INBOUND;

  packet = (packet_t *) packets->data;

  if (!packet)
    {
      g_warning (_("Null packet in check_packet"));
      return FALSE;
    }

  result = substract_times (now, packet->timestamp);



  /* If this packet is older than the averaging time,
   * then it is removed, and the process continues.
   * Else, we are done. 
   * For links, if the timeout time is smaller than the
   * averaging time, we use that instead */

  if (belongs_to == NODE)
    if (node_timeout_time)
      time_comparison = (node_timeout_time > averaging_time) ?
	averaging_time : node_timeout_time;
    else
      time_comparison = averaging_time;
  else if (link_timeout_time)
    time_comparison = (link_timeout_time > averaging_time) ?
      averaging_time : link_timeout_time;
  else
    time_comparison = averaging_time;

  node = (node_t *) parent;
  link = (link_t *) parent;

  /* If this packet is too old, we discard it */
  /* We also delete all packets if capture is stopped */
  if (IS_OLDER (result, time_comparison) || (status == STOP))
    {

      if (belongs_to == NODE)
	{
	  /* Find the direction of the packet */
	  if (!memcmp (packet->dst_id, packet->src_id, node_id_length))
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
	  else if (!memcmp (packet->dst_id, node->node_id, node_id_length))
	    direction = INBOUND;
	  else if (!memcmp (packet->src_id, node->node_id, node_id_length))
	    direction = OUTBOUND;
	  else
	    {
	      g_my_critical
		("Packet does not belong to node in check_packet!");
	      exit (1);
	    }

	  /* Substract this packet's length to the accumulated */
	  node->aver_accu -= packet->size;
	  if (direction == INBOUND)
	    node->aver_accu_in -= packet->size;
	  else
	    node->aver_accu_out -= packet->size;
	  /* Decrease the number of packets */
	  node->n_packets--;
	  /* If it was the last packet in the queue, set
	     * average to 0. It has to be done here because
	     * otherwise average calculation in update_packet list 
	     * requires some packets to exist */
	  if (!node->aver_accu)
	    node->average = 0;

	  /* We remove protocol aggregate information */
	  tokens = g_strsplit (packet->prot, "/", 0);
	  while ((i <= STACK_SIZE) && tokens[i])
	    {
	      protocol_item = g_list_find_custom (node->protocols[i],
						  tokens[i],
						  protocol_compare);
	      protocol_info = protocol_item->data;
	      protocol_info->accumulated -= packet->size;
	      i++;
	    }
	  g_strfreev (tokens);
	  tokens = NULL;

	}
      else
	/* belongs to LINK */
	{
	  /* See above for explanations */
	  link->accumulated -= packet->size;
	  link->n_packets--;
	  if (!link->accumulated)
	    link->average = 0;

	  /* We remove protocol aggregate information */
	  tokens = g_strsplit (packet->prot, "/", 0);
	  while ((i <= STACK_SIZE) && tokens[i])
	    {

	      protocol_item = g_list_find_custom (link->protocols[i],
						  tokens[i],
						  protocol_compare);
	      protocol_info = protocol_item->data;
	      protocol_info->accumulated -= packet->size;
	      if (!protocol_info->accumulated)
		{
		  GList *name_item = NULL;
		  name_t *name;
		  g_free (protocol_info->name);
		  protocol_info->name = NULL;
#if 1
		  while (protocol_info->node_names)
		    {
		      name_item = protocol_info->node_names;
		      name = name_item->data;
		      g_free (name->node_id);
		      g_string_free (name->name, TRUE);
		      g_string_free (name->numeric_name, TRUE);
		      protocol_info->node_names =
			g_list_remove_link (protocol_info->node_names,
					    name_item);
		      g_free (name);
		      g_list_free (name_item);
		    }
#endif
		  link->protocols[i] =
		    g_list_remove_link (link->protocols[i], protocol_item);
		  g_free (protocol_info);
		  g_list_free (protocol_item);
		}
	      i++;
	    }
	  g_strfreev (tokens);
	  tokens = NULL;
	}

      packet->ref_count--;

      if (!packet->ref_count)
	{
	  if (packet->prot)
	    {
	      g_free (packet->prot);
	      packet->prot = NULL;
	    }
	  g_free (packet->src_id);
	  g_free (packet->dst_id);
	  g_free (packet);
	  packets->data = packet = NULL;
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

/* Comparison function used to order the (GTree *) nodes
 * and canvas_nodes heard on the network */
gint
node_id_compare (gconstpointer a, gconstpointer b)
{
  int i;

  i = node_id_length - 1;

  g_assert (a != NULL);
  g_assert (b != NULL);


  while (i)
    {
      if (((guint8 *) a)[i] < ((guint8 *) b)[i])
	{
	  return -1;
	}
      else if (((guint8 *) a)[i] > ((guint8 *) b)[i])
	return 1;
      i--;
    }

  return 0;
}				/* node_id_compare */

/* Comparison function used to order the (GTree *) links
 * and canvas_links heard on the network */
gint
link_id_compare (gconstpointer a, gconstpointer b)
{
  int i;

  i = 2 * node_id_length - 1;

  g_return_val_if_fail (a != NULL, 1);	/* This shouldn't happen.
					 * We arbitrarily passing 1 to
					 * the comparison */
  g_return_val_if_fail (b != NULL, 1);


  while (i)
    {
      if (((guint8 *) a)[i] < ((guint8 *) b)[i])
	{
	  return -1;
	}
      else if (((guint8 *) a)[i] > ((guint8 *) b)[i])
	return 1;
      i--;
    }

  return 0;
}				/* link_id_compare */

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

/* Comparison function to sort protocols by their accumulated traffic */
static gint
names_freq_compare (gconstpointer a, gconstpointer b)
{
  name_t *name_a, *name_b;

  g_assert (a != NULL);
  g_assert (b != NULL);

  name_a = (name_t *) a;
  name_b = (name_t *) b;

  if (name_a->accumulated > name_b->accumulated)
    return -1;
  if (name_a->accumulated < name_b->accumulated)
    return 1;
  return 0;
}				/* names_freq_compare */


/* Returns a timeval structure with the time difference between to
 * other timevals. result = a - b */
struct timeval
substract_times (struct timeval a, struct timeval b)
{
  struct timeval result;

  /* Perform the carry for the later subtraction by updating Y. */
  if (a.tv_usec < b.tv_usec)
    {
      int nsec = (b.tv_usec - a.tv_usec) / 1000000 + 1;
      b.tv_usec -= 1000000 * nsec;
      b.tv_sec += nsec;
    }
  if (a.tv_usec - b.tv_usec > 1000000)
    {
      int nsec = (a.tv_usec - b.tv_usec) / 1000000;
      b.tv_usec += 1000000 * nsec;
      b.tv_sec -= nsec;
    }

  result.tv_sec = a.tv_sec - b.tv_sec;
  result.tv_usec = a.tv_usec - b.tv_usec;

  return result;


}				/* substract_times */


/* Next three functions copied directly from ethereal packet.c
 * by Gerald Combs */

/* Output has to be copied elsewhere */
/* TODO should I dump this funtion now that I have dns_lookup? */
gchar *
ip_to_str (const guint8 * ad)
{
  static gchar str[3][16];
  static gchar *cur;
  gchar *p;
  int i;
  guint32 octet;
  guint32 digit;

  if (cur == &str[0][0])
    {
      cur = &str[1][0];
    }
  else if (cur == &str[1][0])
    {
      cur = &str[2][0];
    }
  else
    {
      cur = &str[0][0];
    }
  p = &cur[16];
  *--p = '\0';
  i = 3;
  for (;;)
    {
      octet = ad[i];
      *--p = (octet % 10) + '0';
      octet /= 10;
      digit = octet % 10;
      octet /= 10;
      if (digit != 0 || octet != 0)
	*--p = digit + '0';
      if (octet != 0)
	*--p = octet + '0';
      if (i == 0)
	break;
      *--p = '.';
      i--;
    }
  return p;
}				/* ip_to_str */

/* (toledo) This function I copied from capture.c of ethereal it was
 * without comments, but I believe it keeps three different
 * strings conversions in memory so as to try to make sure that
 * the conversions made will be valid in memory for a longer
 * period of time */

/* Places char punct in the string as the hex-digit separator.
 * If punct is '\0', no punctuation is applied (and thus
 * the resulting string is 5 bytes shorter)
 */

gchar *
ether_to_str_punct (const guint8 * ad, char punct)
{
  static gchar str[3][18];
  static gchar *cur;
  gchar *p;
  int i;
  guint32 octet;
  static const gchar hex_digits[16] = "0123456789abcdef";

  if (cur == &str[0][0])
    {
      cur = &str[1][0];
    }
  else if (cur == &str[1][0])
    {
      cur = &str[2][0];
    }
  else
    {
      cur = &str[0][0];
    }
  p = &cur[18];
  *--p = '\0';
  i = 5;
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
  return p;
}				/* ether_to_str_punct */



/* Wrapper for the most common case of asking
 * for a string using a colon as the hex-digit separator.
 */
gchar *
ether_to_str (const guint8 * ad)
{
  return ether_to_str_punct (ad, ':');
}				/* ether_to_str */


gchar *
print_mem (const guint8 * ad, guint length)
{
  static gchar str[3][50];
  static gchar *cur;
  char punct = ':';
  gchar *p;
  int i;
  guint32 octet;
  static const gchar hex_digits[16] = "0123456789abcdef";

  if (cur == &str[0][0])
    {
      cur = &str[1][0];
    }
  else if (cur == &str[1][0])
    {
      cur = &str[2][0];
    }
  else
    {
      cur = &str[0][0];
    }
  p = &cur[18];
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
  return p;
}				/* print_mem */


void
dump_node_info (node_t * node)
{

  GList *protocol_item = NULL, *name_item = NULL;
  protocol_t *protocol = NULL;
  name_t *name = NULL;
  guint i = 1;

  if (!node)
    return;

  if (node->name)
    g_my_info ("NODE %s INFORMATION", node->name->str);

  for (; i <= STACK_SIZE; i++)
    {
      if (node->protocols[i])
	{
	  g_my_info ("Protocol level %d information", i);
	  protocol_item = node->protocols[i];
	  while (protocol_item)
	    {
	      protocol = protocol_item->data;
	      g_my_info ("\tProtocol %s", protocol->name);
	      if ((name_item = protocol->node_names))
		{
		  GString *names = NULL;
		  while (name_item)
		    {
		      if (!names)
			names = g_string_new ("");
		      name = name_item->data;
		      names = g_string_append (names, name->name->str);
		      names = g_string_append (names, " ");
		      name_item = name_item->next;
		    }
		  g_my_info ("\t\tName: %s", names->str);
		  g_string_free (names, TRUE);
		}
	      protocol_item = protocol_item->next;
	    }
	}
    }

}				/* dump_node_info */
