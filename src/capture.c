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
void
init_capture (void)
{

  gint pcap_fd, dns_fd;
  guint i = STACK_SIZE;
  gchar *device;
  gchar ebuf[300];
  link_type_t linktype;		/* Type of device we are listening to */


  device = interface;
  if (!device)
    {
      device = pcap_lookupdev (ebuf);
      if (device == NULL)
	{
	  g_error (_ ("Error getting device: %s"), ebuf);
	  exit (1);
	}
    }

  if (!
      ((pcap_t *) pch =
       pcap_open_live (device, MAXSIZE, TRUE, PCAP_TIMEOUT, ebuf)))
    {
      g_error (_ ("Error opening %s : %s - perhaps you need to be root?"),
	       device, ebuf);
    }

  if (filter)
    set_filter (filter, device);
#if 0
  {
    gboolean ok = 1;
    /* A capture filter was specified; set it up. */
    if (pcap_lookupnet (device, &netnum, &netmask, ebuf) < 0)
      {
	g_warning (_
		   ("Can't use filter:  Couldn't obtain netmask info (%s)."),
		   ebuf);
	ok = 0;
      }
    if (ok && (pcap_compile (pch, &fp, filter, 1, netmask) < 0))
      {
	g_warning (_ ("Unable to parse filter string (%s)."),
		   pcap_geterr (pch));
	ok = 0;
      }
    if (ok && (pcap_setfilter (pch, &fp) < 0))
      {
	g_warning (_ ("Can't install filter (%s)."), pcap_geterr (pch));
      }
  }
#endif

  pcap_fd = pcap_fileno (pch);
  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "pcap_fd: %d", pcap_fd);
  gdk_input_add (pcap_fd,
		 GDK_INPUT_READ, (GdkInputFunction) packet_read, pch);

  linktype = pcap_datalink (pch);

  /* l3_offset is equal to the size of the link layer header */

  switch (linktype)
    {
    case L_EN10MB:
      if (mode == DEFAULT)
	mode = ETHERNET;
      l3_offset = 14;
      break;
    case L_RAW:		/* The case for PPP or SLIP, for instance */
      if (mode == DEFAULT)
	mode = IP;
      if (mode == ETHERNET)
	{
	  g_message (_ ("Mode not available in this device"));
	  /* TODO manage proper exit codes */
	  exit (1);
	}
      l3_offset = 0;
      break;
    default:
      g_error (_ ("Link type not yet supported"));
    }


  switch (mode)
    {
    case ETHERNET:
      node_id_length = 6;
      break;
    case IP:
      node_id_length = 4;
      break;
    case TCP:
      node_id_length = 6;
      break;
    default:
      g_error (_ ("Ape mode not yet supported"));
    }

  if (!numeric)
    {
      dns_open ();
      dns_fd = dns_waitfd ();
      gdk_input_add (dns_fd,
		     GDK_INPUT_READ, (GdkInputFunction) dns_ready, NULL);
    }



  nodes = g_tree_new (node_id_compare);
  links = g_tree_new (link_id_compare);
  while (i + 1)
    {
      protocols[i] = NULL;
      i--;
    }

}				/* init_capture */

gint
set_filter (gchar * filter, gchar * device)
{
  gchar ebuf[300];
  static bpf_u_int32 netnum, netmask;
  static struct bpf_program fp;
  static gchar *current_device = NULL;
  gboolean ok = 1;

  /* TODO pending to be more general, since we want to be able 
   * to change the capturing device in runtime. */
  if (!current_device)
    current_device = g_strdup (device);

  /* A capture filter was specified; set it up. */
  if (pcap_lookupnet (current_device, &netnum, &netmask, ebuf) < 0)
    {
      g_warning (_
		 ("Can't use filter:  Couldn't obtain netmask info (%s)."),
		 ebuf);
      ok = 0;
    }
  if (ok && (pcap_compile (pch, &fp, filter, 1, netmask) < 0))
    {
      g_warning (_ ("Unable to parse filter string (%s)."),
		 pcap_geterr (pch));
      ok = 0;
    }
  if (ok && (pcap_setfilter (pch, &fp) < 0))
    {
      g_warning (_ ("Can't install filter (%s)."), pcap_geterr (pch));
    }
}

/* This function is called everytime there is a new packet in
 * the network interface. It then updates traffic information
 * for the appropriate nodes and links */
static void
packet_read (pcap_t * pch, gint source, GdkInputCondition condition)
{
  struct pcap_pkthdr phdr;
  guint8 *src_id, *dst_id, *link_id, *pcap_packet;

  /* I have to love how RedHat messes with libraries.
   * I'm forced to use my own timestamp since the phdr is
   * different in RedHat6.1 >:-( */
  /* TODO Check again the redhat version and see how/where is the
   * timestamp defined there, so that I can use it and save lots of
   * OS calls */

  pcap_packet = (guint8 *) pcap_next (pch, &phdr);

  if (!pcap_packet)
    return;

  gettimeofday (&now, NULL);

  src_id = get_node_id (pcap_packet, SRC);
  add_node_packet (pcap_packet, phdr, src_id);

  /* Now we do the same with the destination node */
  dst_id = get_node_id (pcap_packet, DST);
  add_node_packet (pcap_packet, phdr, dst_id);

  link_id = get_link_id (pcap_packet);
  /* And now we update link traffic information for this packet */
  add_link_packet (pcap_packet, phdr, link_id);

}				/* packet_read */


/* Returns a pointer to a set of octects that define a link for the
 * current mode in this particular packet */
static guint8 *
get_node_id (const guint8 * packet, create_node_type_t node_type)
{
  static guint8 *node_id = NULL;

  if (node_id)
    g_free (node_id);

  switch (mode)
    {
    case ETHERNET:
      if (node_type == SRC)
	node_id = g_memdup (packet + 6, node_id_length);
      else
	node_id = g_memdup (packet, node_id_length);
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
      /* TODO Write proper assertion code here */
      g_error (_ ("Reached default in get_node_id"));
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
    g_free (link_id);

  switch (mode)
    {
    case ETHERNET:
      link_id = g_memdup (packet, 2 * node_id_length);
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
      g_error (_ ("Unsopported ape mode in get_link_id"));
    }
  return link_id;
}				/* get_link_id */


/* We update node information for each new packet that arrives in the
 * network. If the node the packet refers to is unknown, we
 * create it. */
static void
add_node_packet (const guint8 * packet, struct pcap_pkthdr phdr,
		 const guint8 * node_id)
{
  node_t *node;
  packet_t *packet_info;

  node = g_tree_lookup (nodes, node_id);
  if (node == NULL)
    node = create_node (packet, node_id);

  /* We add a packet to the list of packets to/from that host which we want
   * to account for */
  packet_info = g_malloc (sizeof (packet_t));
  packet_info->size = phdr.len;
  packet_info->timestamp = now;
  packet_info->parent = node;
  /* Right now we don't need protocol info for node packets. */
  packet_info->prot = NULL;
  node->packets = g_list_prepend (node->packets, packet_info);

  /* We update node info */
  node->accumulated += phdr.len;
  node->last_time = now;
  /* update_packet_list is now called in diagram.c
   * I'm not too happy about it since I want to have a clear
   * separation between data structures and presentation, but it
   * is a fact that the proper moment for packet cleaning is right
   * before presentation */
  node->n_packets++;

}				/* add_node_packet */

/* Save as above plus we update protocol aggregate information */
static void
add_link_packet (const guint8 * packet, struct pcap_pkthdr phdr,
		 const guint8 * link_id)
{
  link_t *link;
  packet_t *packet_info;
  GList *protocol_item;
  protocol_t *protocol_info;

  link = g_tree_lookup (links, link_id);
  if (!link)
    link = create_link (packet, link_id);

  /* We create a new packet structure and add it to the lists of
   * packet that this link has */
  packet_info = g_malloc (sizeof (link_t));
  packet_info->size = phdr.len;
  packet_info->timestamp = now;
  packet_info->parent = link;
  packet_info->prot = g_string_new (get_packet_prot (packet));
  link->packets = g_list_prepend (link->packets, packet_info);

  /* Have we heard this protocol at all? */
#if 0
  if (!(protocol_item = g_list_find_custom (protocols[0],
					    packet_info->prot->str,
					    protocol_compare)))
    {
      protocol_info = g_malloc (sizeof (protocol_t));
      protocol_info->name = g_strdup (packet_info->prot->str);
      protocols[0] = g_list_prepend (protocols[0], protocol_info);
    }
#endif
  add_protocol (protocols, packet_info->prot->str, phdr, FALSE);
  add_protocol (link->protocols, packet_info->prot->str, phdr, TRUE);
#if 0
  /* Have we already heard this protocol on this link? */
  if ((protocol_item = g_list_find_custom (link->protocols,
					   packet_info->prot->str,
					   protocol_compare)))
    {
      protocol_info = protocol_item->data;
      protocol_info->accumulated += phdr.len;
      protocol_info->n_packets++;
      /*g_message (protocol_info->name); */
    }
  else
    /* First time protocol. Will have to be created */
    {
      protocol_info = g_malloc (sizeof (protocol_t));
      protocol_info->name = g_strdup (packet_info->prot->str);
      /*g_message (protocol_info->name); */
      protocol_info->accumulated = phdr.len;
      protocol_info->n_packets = 1;
      link->protocols = g_list_prepend (link->protocols, protocol_info);
    }
#endif

  /* We update link info */
  link->accumulated += phdr.len;
  link->last_time = now;
  /* update_packet_list is now called in diagram.c */
  link->n_packets++;

}				/* add_link_packet */

/* Allocates a new node structure, and adds it to the
 * global nodes binary tree */
static node_t *
create_node (const guint8 * packet, const guint8 * node_id)
{
  node_t *node;
  gchar *na;

  na = g_strdup (_ ("n/a"));

  node = g_malloc (sizeof (node_t));

  node->node_id = g_memdup (node_id, node_id_length);
  node->name = NULL;
  node->numeric_name = NULL;
  /* We initialize the ip_address, although it won't be
   * used in many cases */
  node->ip_address = 0;
  node->numeric_ip = NULL;
  fill_names (node, node_id, packet);

  node->average = 0;
  node->n_packets = 0;
  node->accumulated = 0;

  node->packets = NULL;

  g_tree_insert (nodes, node->node_id, node);
  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	 _ ("Creating node: %s. Number of nodes %d"),
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
	 _ ("Creating link: %s-%s. Number of links %d"),
	 link->src_name, link->dst_name,
	 g_tree_nnodes (links));

  return link;
}				/* create_link */


/* Fills in the strings that characterize the node */
static void
fill_names (node_t * node, const guint8 * node_id, const guint8 * packet)
{

  gboolean fqdn;

  switch (mode)
    {
    case ETHERNET:

      fqdn = FALSE;

      if (!node->ip_address && packet && (packet[12] == 0x08)
	  && (packet[13] == 0x00))
	{
	  const guint8 *ip_address;
	  /* We do not know whether this was a source or destination
	   * node, so we have to check it out */
	  if (!node_id_compare (node_id, packet + 6))
	    ip_address = packet + 26;	/* SRC packet */
	  else
	    ip_address = packet + 30;
	  node->ip_address = ntohl (*(guint32 *) ip_address);
	}

      if (!node->numeric_name)
	node->numeric_name = g_string_new (ether_to_str (node_id));
      if (!node->numeric_ip && node->ip_address)
	{
	  guint32 net_ip_address;
	  net_ip_address = htonl (node->ip_address);
	  node->numeric_ip =
	    g_string_new (ip_to_str ((guint8 *) (&net_ip_address)));
	}
      if (numeric)
	{
	  if (!node->name)
	    node->name = g_string_new (ether_to_str (node_id));
	}
      else
	{
	  /* Code to display the IP address if host is not found
	   * in /etc/ethers. It is ugly, but that's because 
	   * this is not right in the first place. */
	  /* We look for the ip side only if it is an IP packet and 
	   * the host is not found in /etc/ethers */
	  if (!strcmp (ether_to_str (node_id), get_ether_name (node_id))
	      && (node->ip_address ||
		  (packet && (packet[12] == 0x08) && (packet[13] == 0x00))))
	    {
	      if (!node->name)
		node->name =
		  g_string_new (dns_lookup (node->ip_address, fqdn));
	      else
		g_string_assign (node->name,
				 dns_lookup (node->ip_address, fqdn));
	    }
	  else
	    node->name = g_string_new (get_ether_name (node_id));
	}
      break;

    case IP:

      fqdn = TRUE;

      if (!node->ip_address)
	node->ip_address = ntohl (*(guint32 *) node_id);

      if (!node->numeric_name)
	node->numeric_name = g_string_new (ip_to_str (node_id));

      if (numeric)
	{
	  if (!node->name)
	    node->name = g_string_new (ip_to_str (node_id));
	}
      else
	{
	  if (!node->name)
	    node->name = g_string_new (dns_lookup (node->ip_address, fqdn));
	  else
	    g_string_assign (node->name, dns_lookup (node->ip_address, fqdn));
	}
      break;

    case TCP:

      fqdn = TRUE;

      if (!node->ip_address)
	node->ip_address = ntohl (*(guint32 *) node_id);

      if (!node->numeric_name)
	{
	  node->numeric_name = g_string_new (ip_to_str (node_id));
	  node->numeric_name = g_string_append_c (node->numeric_name, ':');
	  node->numeric_name =
	    g_string_append (node->numeric_name,
			     g_strdup_printf ("%d",
					      *(guint16 *) (node_id + 4)));
	}

      if (numeric)
	{
	  if (!node->name)
	    {
	      node->name = g_string_new (ip_to_str (node_id));
	      node->name = g_string_append_c (node->name, ':');
	      node->name =
		g_string_append (node->name,
				 g_strdup_printf ("%d",
						  *(guint16 *)
						  (node_id + 4)));
	    }
	}
      else
	{
	  if (!node->name)
	    {
	      node->name = g_string_new (dns_lookup (node->ip_address, fqdn));
	      node->name = g_string_append_c (node->name, ':');
	      node->name = g_string_append (node->name,
					    get_tcp_port (*(guint16 *)
							  (node_id + 4)));
	    }
	  else
	    {
	      g_string_assign (node->name,
			       dns_lookup (node->ip_address, fqdn));
	      node->name = g_string_append_c (node->name, ':');
	      node->name = g_string_append (node->name,
					    get_tcp_port (*(guint16 *)
							  (node_id + 4)));
	    }
	}
      break;

    default:
      /* TODO Write proper assertion code here */
      g_error (_ ("Reached default in fill_names"));
    }
}				/* fill_names */


/* Callback function everytime a dns_lookup function is finished */
static void
dns_ready (gpointer data, gint fd, GdkInputCondition cond)
{
  dns_ack ();
}


/* For a given protocol stack, it updates the both the global 
 * protocols and specific link list */
void
add_protocol (GList ** protocols, gchar * stack, struct pcap_pkthdr phdr,
	      gboolean is_link)
{
  GList *protocol_item;
  protocol_t *protocol_info;
  gchar **tokens;
  guint i = 0;
  tokens = g_strsplit (stack, "/", 0);
  while (tokens[i] != NULL)
    {
      if ((protocol_item = g_list_find_custom (protocols[i],
					       tokens[i],
					       protocol_compare)))
	{
	  if (is_link)
	    {
	      protocol_info = protocol_item->data;
	      protocol_info->accumulated += phdr.len;
	      protocol_info->n_packets++;
	    }
	}
      else
	{
	  protocol_info = g_malloc (sizeof (protocol_t));
	  protocol_info->name = g_strdup (tokens[i]);
	  protocols[i] = g_list_prepend (protocols[i], protocol_info);
	  if (is_link)
	    {
	      protocol_info->accumulated = phdr.len;
	      protocol_info->n_packets = 1;
	    }
	}

      i++;
      if (i == 10)
	g_error (_ ("Higher than 10 items protocols stacks are not supported"));
    }
  g_strfreev (tokens);
}				/* add_protocol */

node_t *
update_node (node_t * node)
{
  struct timeval diff;
  guint8 *node_id;

  if (node->packets)
    update_packet_list (node->packets, NODE);
  if (node->n_packets == 0)
    {
      diff = substract_times (now, node->last_time);

      if (((diff.tv_sec * 1000000 + diff.tv_usec) > node_timeout_time)
	  && node_timeout_time)
	{
	  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
		 _ ("Removing node: %s. Number of node %d"),
		 node->name->str, g_tree_nnodes (nodes));
	  node_id = node->node_id;	/* Since we are freeing the node
					 * we must free its members as well 
					 * but if we free the id then we will
					 * not be able to find the link again 
					 * to free it, thus the intermediate variable */
	  g_string_free (node->name, TRUE);
	  g_string_free (node->numeric_name, TRUE);
	  if (node->numeric_ip)
	    g_string_free (node->numeric_ip, TRUE);
	  g_free (node);
	  g_tree_remove (nodes, node_id);
	  g_free (node_id);
	  node = NULL;
	}
      else
	{
	  node->packets = NULL;
	  node->accumulated = 0;	/* TODO: do we really need this here anymore? */
	}
    }

  return node;
}				/* update_node */


link_t *
update_link (link_t * link)
{
  struct timeval diff;
  guint8 *link_id;
  guint i = STACK_SIZE;

  if (link->packets)
    update_packet_list (link->packets, LINK);

  diff = substract_times (now, link->last_time);

  if (link->n_packets == 0)
    {

      if (((diff.tv_sec * 1000000 + diff.tv_usec) > link_timeout_time)
	  && link_timeout_time)
	{
	  link_id = link->link_id;	/* Since we are freeing the link
					 * we must free its members as well 
					 * but if we free the id then we will
					 * not be able to find the link again 
					 * to free it, thus the intermediate variable */
	  while (i + 1)
	    {
	      if (link->main_prot[i])
		g_free (link->main_prot[i]);
	      i--;
	    }
	  g_free (link->src_name);
	  g_free (link->dst_name);
	  g_free (link);
	  g_tree_remove (links, link_id);
	  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
		 _ ("Removing link. Number of links %d"),
		 g_tree_nnodes (canvas_links));

	  g_free (link_id);
	  link = NULL;

	}
      else
	{
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
update_packet_list (GList * packets, enum packet_belongs belongs_to)
{
  struct timeval difference;
  guint i = STACK_SIZE;
  node_t *node;
  link_t *link;
  gdouble usecs_from_oldest;	/* usecs since the first valid packet */
  GList *packet_l_e;		/* Packets is a list of packets.
				 * packet_l_e is always the latest (oldest)
				 * list element */
  packet_t *packet;

  packet_l_e = g_list_last (packets);

  /* Going from oldest to newer, delete all irrelevant packets */
  while ((packet_l_e = check_packet (packet_l_e, belongs_to)));

  /* Get oldest relevant packet */
  packet_l_e = g_list_last (packets);
  packet = (packet_t *) packet_l_e->data;


  /* TODO Move all this below to update_node and update_link */
  /* If there still is relevant packets, then calculate average
   * traffic and update names*/
  if (packet)
    {
      node = ((node_t *) (packet->parent));
      link = ((link_t *) (packet->parent));
      difference = substract_times (now, packet->timestamp);
      usecs_from_oldest = difference.tv_sec * 1000000 + difference.tv_usec;

      /* average in bps, so we multiply by 8 and 1000000 */
      if (belongs_to == NODE)
	{
	  node->average = 8000000 * node->accumulated / usecs_from_oldest;
	  fill_names (node, node->node_id, NULL);
	}
      else
	{
	  link->average = 8000000 * link->accumulated / usecs_from_oldest;
	  /* We look for the most used protocol for this link */
	  while (i + 1)
	    {
	      if (link->main_prot[i])
		g_free (link->main_prot[i]);
	      link->main_prot[i] = get_main_prot (packets, link, i);
	      i--;
	    }
	}

    }
}				/* update_packet_list */

/* Finds the most commmon protocol of all the packets in a
 * given link (only link, by now) */
static gchar *
get_main_prot (GList * packets, link_t * link, guint level)
{
  protocol_t *protocol;
  if (!(link->protocols[level]))
    return NULL;
  link->protocols[level]
    = g_list_sort (link->protocols[level], prot_freq_compare);
  protocol = (protocol_t *) link->protocols[level]->data;
  return g_strdup (protocol->name);
}				/* get_main_prot */

/* Make sure this particular packet in a list of packets beloging to 
 * either o link or a node is young enough to be relevant. Else
 * remove it from the list */
static GList *
check_packet (GList * packets, enum packet_belongs belongs_to)
{

  struct timeval result;
  double time_comparison;
  guint i = 0;
  node_t *node;
  link_t *link;
  GList *protocol_item;
  protocol_t *protocol_info;

  packet_t *packet;
  packet = (packet_t *) packets->data;

  if (!packet)
    {
      g_warning (_ ("Null packet in check_packet"));
      return NULL;
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

  node = ((node_t *) (packet->parent));
  link = ((link_t *) (packet->parent));

  /* If this packet is too old, we discard it */
  if ((result.tv_sec * 1000000 + result.tv_usec) > time_comparison)
    {

      if (belongs_to == NODE)
	{
	  /* Substract this packet's length to the accumulated */
	  node->accumulated -= packet->size;
	  /* Decrease the number of packets */
	  node->n_packets--;
	  /* If it was the last packet in the queue, set
	     * average to 0. It has to be done here because
	     * otherwise average calculation in update_packet list 
	     * requires some packets to exist */
	  if (!node->accumulated)
	    node->average = 0;
	}
      else
	/* belongs to LINK */
	{
	  gchar **tokens;
	  /* See above for explanations */
	  link->accumulated -= packet->size;
	  link->n_packets--;
	  if (!link->accumulated)
	    link->average = 0;

	  /* We remove protocol aggregate information */
	  tokens = g_strsplit (packet->prot->str, "/", 0);
	  while ((i < STACK_SIZE) && tokens[i])
	    {

	      protocol_item = g_list_find_custom (link->protocols[i],
						  tokens[i],
						  protocol_compare);
	      protocol_info = protocol_item->data;
	      protocol_info->accumulated -= packet->size;
	      if (!protocol_info->accumulated)
		{
		  g_free (protocol_info->name);
		  link->protocols[i] =
		    g_list_remove_link (link->protocols[i], protocol_item);
		  g_list_free (protocol_item);
		}
	      i++;
	    }
	  g_strfreev (tokens);
	}

      if (((packet_t *) (packets->data))->prot)
	g_string_free (((packet_t *) (packets->data))->prot, TRUE);
      g_free (packets->data);

      if (packets->prev)
	{
	  packets = packets->prev;
	  g_list_remove (packets, packets->next->data);
	  return (packets);	/* Old packet removed,
				 * keep on searching */
	}
      else
	{
	  g_list_free (packets);	/* Last packet removed,
					 * don't search anymore
					 */
	}
    }

  return NULL;			/* Last packet searched
				 * End search */
}				/* check_packet */

/* Comparison function used to order the (GTree *) nodes
 * and canvas_nodes heard on the network */
gint
node_id_compare (gconstpointer a, gconstpointer b)
{
  int i;

  i = node_id_length - 1;

  g_return_val_if_fail (a != NULL, 1);	/* This shouldn't happen.
					 * We arbitrarily pass 1 to
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
  return strcmp (((protocol_t *) a)->name, (gchar *) b);
}

/* Comparison function to sort protocols by their accumulated traffic */
static gint
prot_freq_compare (gconstpointer a, gconstpointer b)
{
  protocol_t *prot_a, *prot_b;

  prot_a = (protocol_t *) a;
  prot_b = (protocol_t *) b;

  if (prot_a->accumulated > prot_b->accumulated)
    return 1;
  if (prot_a->accumulated < prot_b->accumulated)
    return -1;
  return 0;
}				/* prot_freq_compare */


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
