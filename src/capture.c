/* Etherape
 * Copyright (C) 2000 Juan Toledo
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

#include "capture.h"
#include "resolv.h"


double averaging_time = 10000000;	/* Microseconds of time we consider to
					 * calculate traffic averages */
double link_timeout_time = 2000000;	/* After this time
					 * has passed with no traffic in a 
					 * link, it disappears */
double node_timeout_time = 10000000;	/* After this time has passed 
					 * with no traffic in/out a 
					 * node, it disappears */
guint node_id_length;		/* Length of the node_id key. Depends
				 * on the mode of operation */
apemode_t mode;			/* Mode of operation. Can be
				 * ETHERNET, IP, UDP or TCP */
link_type_t linktype;		/* Type of device we are listening to */
guint l3_offset;		/* Offset to the level 3 protocol data
				 * Depends of the linktype */

/* Extern global variables */

extern gboolean numeric;
extern gboolean dns;
extern gchar *interface;
extern gchar *filter;

/* Next three functions copied directly from ethereal packet.c
 * by Gerald Combs */

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
}

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
}



/* Wrapper for the most common case of asking
 * for a string using a colon as the hex-digit separator.
 */
gchar *
ether_to_str (const guint8 * ad)
{
  return ether_to_str_punct (ad, ':');
}



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


/* Fills in the strings that characterize the node */
void
fill_names (node_t * node, const guint8 * node_id, const guint8 * packet)
{
  switch (mode)
    {
    case ETHERNET:
      node->numeric_name = g_string_new (ether_to_str (node_id));
      if (numeric)
	node->name = g_string_new (ether_to_str (node_id));
      else
	{
	  /* Code to display the IP address if host is not found
	   * in /etc/ethers. It is ugly, but that's because 
	   * this is not right in the first place. */
	  /* We look for the ip side only if it is an IP packet and 
	   * the host is not found in /etc/ethers */
	  if (!strcmp (ether_to_str (node_id), get_ether_name (node_id)) &&
	      (packet[12] == 0x08) && (packet[13] == 0x00))
	    {
	      const guint8 *ip_address;
	      /* We do not know whether this was a source or destination
	       * node, so we have to check it out */
	      if (!node_id_compare (node_id, packet + 6))
		ip_address = packet + 26; /* SRC packet */
	      else
		ip_address = packet + 30;
	      if (dns)
		node->name = g_string_new (get_hostname (*(guint32 *) ip_address));
	      else
		node->name = g_string_new (ip_to_str (ip_address));
	    }
	  else
	    node->name = g_string_new (get_ether_name (node_id));
	}
      break;
    case IP:
      node->numeric_name = g_string_new (ip_to_str (node_id));
      if (numeric)
	node->name = g_string_new (ip_to_str (node_id));
      else
	{
	  if (dns)
	    node->name = g_string_new (get_hostname (*(guint32 *) node_id));
	  else
	    node->name = g_string_new (ip_to_str (node_id));
	}
      break;
    case TCP:
      node->numeric_name = g_string_new (ip_to_str (node_id));
      node->numeric_name = g_string_append_c (node->numeric_name, ':');
      node->numeric_name = g_string_append (node->numeric_name,
					    g_strdup_printf ("%d",
					       *(guint16 *) (node_id + 4)));
      if (dns)
	node->name = g_string_new (get_hostname (*(guint32 *) node_id));
      else
	node->name = g_string_new (ip_to_str (node_id));
      node->name = g_string_append_c (node->name, ':');
      if (numeric)
	node->name = g_string_append (node->name,
				      g_strdup_printf ("%d",
					       *(guint16 *) (node_id + 4)));
      else
	node->name = g_string_append (node->name,
				 get_tcp_port (*(guint16 *) (node_id + 4)));


      break;

    default:
      /* TODO Write proper assertion code here */
      g_error (_ ("Cobadde! Pecadorl!"));
    }
}


/* Allocates a new node structure, and adds it to the
 * global nodes binary tree */
node_t *
create_node (const guint8 * packet, const guint8 * node_id)
{
  node_t *node;
  gchar *na;

  na = g_strdup (_ ("n/a"));

  node = g_malloc (sizeof (node_t));

  node->node_id = g_memdup (node_id, node_id_length);

  fill_names (node, node_id, packet);

  node->average = 0;
  node->n_packets = 0;
  node->accumulated = 0;

  node->packets = NULL;

  g_tree_insert (nodes, node->node_id, node);
  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	 _ ("Creating node: %s. Number of nodes %d"),
	 node->name->str,
	 g_tree_nnodes (nodes));

  return node;
}				/* create_node */


/* Allocates a new link structure, and adds it to the
 * global links binary tree */
link_t *
create_link (const guint8 * packet, const guint8 * link_id)
{
  link_t *link;

  link = g_malloc (sizeof (link_t));
#if 0
  if (interape)
    {
      link->link_id = g_memdup (packet + 26, 8);
    }
  else
    {
      link->link_id = g_memdup (packet, 12);
    }
#endif /* TODO remove this */

  link->link_id = g_memdup (link_id, 2 * node_id_length);
  link->average = 0;
  link->n_packets = 0;
  link->accumulated = 0;
  link->packets = NULL;
  g_tree_insert (links, link->link_id, link);

/* TODO make proper debugging output */
#if 0
  if (interape)
    g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	   _ ("Creating link: %s-%s. Number of links %d"),
	   ip_to_str (packet + 26), ip_to_str (packet + 30),
	   g_tree_nnodes (links));
  else
    g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	   _ ("Creating link: %s-%s. Number of links %d"),
	   get_ether_name (packet + 6), get_ether_name (packet),
	   g_tree_nnodes (links));
#endif

  return link;
}				/* create_link */

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

/* Make sure this particular packet in a list of packets beloging to 
 * either o link or a node is young enough to be relevant. Else
 * remove it from the list */
GList *
check_packet (GList * packets, struct timeval now, enum packet_belongs belongs_to)
{

  struct timeval result;
  double time_comparison;

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


  if ((result.tv_sec * 1000000 + result.tv_usec) > time_comparison)
    {
      if (belongs_to == NODE)
	{
	  ((node_t *) (packet->parent))->accumulated -=
	    packet->size;
	  ((node_t *) (packet->parent))->n_packets--;
	}
      else
	{
	  ((link_t *) (packet->parent))->accumulated -=
	    packet->size;
	  ((link_t *) (packet->parent))->n_packets--;
	}

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


/* This function is called to discard packets from the list 
 * of packets beloging to a node or a link, and to calculate
 * the average traffic for that node or link */
void
update_packet_list (GList * packets, enum packet_belongs belongs_to)
{
  struct timeval now, difference;
  gdouble usecs_from_oldest;	/* usecs since the first valid packet */
  GList *packet_l_e;		/* Packets is a list of packets.
				 * packet_l_e is always the latest (oldest)
				 * list element */
  packet_t *packet;

  gettimeofday (&now, NULL);
  packet_l_e = g_list_last (packets);

  /* Going from oldest to newer, delete all irrelevant packets */
  while ((packet_l_e = check_packet (packet_l_e, now, belongs_to)));

  /* Get oldest relevant packet */
  packet_l_e = g_list_last (packets);
  packet = (packet_t *) packet_l_e->data;
  /* Calculate average traffic */
  if (packet)
    {
      difference = substract_times (now, packet->timestamp);
      usecs_from_oldest = difference.tv_sec * 1000000 + difference.tv_usec;

      /* average in bps, so we multiply by 8 */
      if (belongs_to == NODE)
	((node_t *) (packet->parent))->average = 8 *
	  ((node_t *) (packet->parent))->accumulated / usecs_from_oldest;
      else
	((link_t *) (packet->parent))->average = 8 *
	  ((link_t *) (packet->parent))->accumulated / usecs_from_oldest;
    }
}

guint8 *
get_node_id (const guint8 * packet, enum create_node_type node_type)
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
      g_error (_ ("Dise que viene ese pedaso de vacarll!!!"));
    }

  return node_id;
}

guint8 *
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
}

/* This function is called everytime there is a new packet in
 * the network interface. It then updates traffic information
 * for the appropriate nodes and links */
void
packet_read (pcap_t * pch,
	     gint source,
	     GdkInputCondition condition)
{
  struct pcap_pkthdr phdr;
  guint8 *src_id, *dst_id, *link_id, *pcap_packet;
  node_t *node;
  packet_t *packet_info;
  link_t *link;
  /* I have to love how RedHat messes with libraries.
   * I'm forced to use my own timestamp since the phdr is
   * different in RedHat6.1 >:-( */
  struct timeval now;

  pcap_packet = (guint8 *) pcap_next (pch, &phdr);

  if (!pcap_packet)
    return;

  gettimeofday (&now, NULL);

  src_id = get_node_id (pcap_packet, SRC);

  node = g_tree_lookup (nodes, src_id);
  if (node == NULL)
    node = create_node (pcap_packet, src_id);

  /* We add a packet to the list of packets to/from that host which we want
   * to account for */
  packet_info = g_malloc (sizeof (packet_t));
  packet_info->size = phdr.len;
  packet_info->timestamp = now;
  packet_info->parent = node;
  node->packets = g_list_prepend (node->packets, packet_info);
  node->accumulated += phdr.len;
  node->last_time = now;
  /* Now we clean all packets we don't care for anymore */
  update_packet_list (node->packets, NODE);
  node->n_packets++;

  /* Now we do the same with the destination node */

  dst_id = get_node_id (pcap_packet, DST);

  node = g_tree_lookup (nodes, dst_id);
  if (node == NULL)
    node = create_node (pcap_packet, dst_id);

  /* We add a packet to the list of packets to/from that host which we want
   * to account for */
  packet_info = g_malloc (sizeof (packet_t));
  packet_info->size = phdr.len;
  packet_info->timestamp = now;
  packet_info->parent = node;
  node->packets = g_list_prepend (node->packets, packet_info);
  node->accumulated += phdr.len;
  node->last_time = now;
  /* Now we clean all packets we don't care for anymore */
  update_packet_list (node->packets, NODE);
  node->n_packets++;


  link_id = get_link_id (pcap_packet);

  /* And now we update link traffic information for this packet */

  link = g_tree_lookup (links, link_id);

  if (!link)
    link = create_link (pcap_packet, link_id);

  packet_info = g_malloc (sizeof (link_t));
  packet_info->size = phdr.len;
  packet_info->timestamp = now;
  packet_info->parent = link;
  link->packets = g_list_prepend (link->packets, packet_info);
  link->accumulated += phdr.len;
  link->last_time = now;
  /* Now we clean all packets we don't care for anymore */
  update_packet_list (link->packets, LINK);
  link->n_packets++;



}				/* packet_read */


void
init_capture (void)
{

  gint pcap_fd;
  pcap_t *pch;
  gchar *device;
  gchar ebuf[300];
  static bpf_u_int32 netnum, netmask;
  static struct bpf_program fp;

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

  if (!((pcap_t *) pch = pcap_open_live (device, MAXSIZE, TRUE, PCAP_TIMEOUT, ebuf)))
    {
      g_error (_ ("Error opening %s : %s - perhaps you need to be root?"),
	       device,
	       ebuf);
    }

  if (filter)
    {
      gboolean ok = 1;
      /* A capture filter was specified; set it up. */
      if (pcap_lookupnet (device, &netnum, &netmask, ebuf) < 0)
	{
	  g_warning (_ ("Can't use filter:  Couldn't obtain netmask info (%s)."), ebuf);
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
	  g_warning (_ ("Can't install filter (%s)."),
		     pcap_geterr (pch));
	}
    }

  pcap_fd = pcap_fileno (pch);
  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "pcap_fd: %d", pcap_fd);
  gdk_input_add (pcap_fd,
		 GDK_INPUT_READ,
		 (GdkInputFunction) packet_read,
		 pch);

  linktype = pcap_datalink (pch);

  /* l3_offset is equal to the size of the link layer header */

  switch (linktype)
    {
    case L_EN10MB:
      l3_offset = 14;
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
#if 0
      interape = 1;
#endif
      break;
    case TCP:
      node_id_length = 6;
      break;
    default:
      g_error (_ ("Ape mode not yet supported"));
    }

  /* Initiate non blocking dns resolver if it might be used */
  if (((mode == IP) || (mode == TCP) || (mode == UDP)) && !numeric)
    gnome_dns_init (2);

  nodes = g_tree_new (node_id_compare);
  links = g_tree_new (link_id_compare);

}
