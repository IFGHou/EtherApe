


#include <pcap.h>
#include <gnome.h>
#include <ctype.h>

#include "capture.h"
#include "resolv.h"



extern double averaging_time;
extern double link_timeout_time;
extern double node_timeout_time;
extern gboolean numeric;
extern gboolean dns;
extern gboolean interape;
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

  i = interape ? 3 : 5;		/* If we are in interape mode
				 * it's enough with 4 octects */

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

  i = interape ? 7 : 11;	/* If we are in interape mode
				 * it's enough with 4 octects */


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



/* Allocates a new node structure, and adds it to the
 * global nodes binary tree */
node_t *
create_node (const guint8 * packet, enum create_node_type node_type)
{
  node_t *node;
  const guint8 *ether_addr, *node_id;
  guint32 ip_addr;
  gchar *na;

  na = g_strdup (_ ("n/a"));

  if (node_type == SRC)
    {
      ether_addr = packet + 6;
      ip_addr = *(guint32 *) (packet + 26);
      node_id = interape ? packet + 26 : packet + 6;
    }
  else
    {
      ether_addr = packet;
      ip_addr = *(guint32 *) (packet + 30);
      node_id = interape ? packet + 30 : packet;
    }


  node = g_malloc (sizeof (node_t));

  if (interape)
    node->node_id = g_memdup (node_id, 4);
  else
    node->node_id = g_memdup (node_id, 6);

  node->ether_addr = g_memdup (ether_addr, 6);
  node->ether_numeric_str = g_string_new (ether_to_str (ether_addr));

  if ((packet[12] == 0x08) && (packet[13] == 0x00))
    {
      node->ip_addr = ip_addr;
      node->ip_numeric_str = g_string_new (ip_to_str ((guint8 *) (&ip_addr)));
      if (dns)
	node->ip_str = g_string_new (get_hostname (ip_addr));
      else
	node->ip_str = g_string_new (ip_to_str ((guint8 *) (&ip_addr)));
    }
  else
    {
      node->ip_addr = 0;
      node->ip_numeric_str = g_string_new (na);
      node->ip_str = g_string_new (na);
    }

  node->average = 0;
  node->n_packets = 0;
  node->accumulated = 0;

  if (!numeric)
    {
      /* If there is no proper definition in /etc/ethers, we try to get
       * the IP name of the host. Note this is inherently wrong and I feel
       * uneasy about leaving it by default, but let's make users happy by now.
       * We also make sure that it is an IP packet */

      node->ether_str = g_string_new (get_ether_name (ether_addr));

      if ((!strcmp (get_ether_name (ether_addr), ether_to_str (ether_addr)))
	  && (packet[12] == 0x08) && (packet[13] == 0x00)
	  && strcmp (get_ether_name (ether_addr), "ff:ff:ff:ff:ff:ff"))
	{

	  if (dns)
	    {
	      node->name = g_string_new (get_hostname (ip_addr));
	    }
	  else
	    {
	      node->name = g_string_new (ip_to_str ((guint8 *) (&ip_addr)));
	    }

	}
      else
	{
	  node->name = g_string_new (get_ether_name (ether_addr));
	}
    }
  else
    {
      node->ether_str = g_string_new (na);
      node->name = g_string_new (ether_to_str (ether_addr));
    }

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
create_link (const guint8 * packet)
{
  link_t *link;

  link = g_malloc (sizeof (link_t));
  if (interape)
    {
      link->link_id = g_memdup (packet + 26, 8);
    }
  else
    {
      link->link_id = g_memdup (packet, 12);
    }
  link->average = 0;
  link->n_packets = 0;
  link->accumulated = 0;
  link->packets = NULL;
  g_tree_insert (links, link->link_id, link);
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

  return link;
}				/* create_link */

GList *
check_packet (GList * packets, struct timeval now, enum packet_belongs belongs_to)
{

  struct timeval packet_time;
  struct timeval result;
  double time_comparison;

  packet_t *packet;
  packet = (packet_t *) packets->data;

//  if (!packet) return NULL; /* Last packet searched */

  packet_time.tv_sec = packet->timestamp.tv_sec;
  packet_time.tv_usec = packet->timestamp.tv_usec;


  /* Perform the carry for the later subtraction by updating Y. */
  if (now.tv_usec < packet_time.tv_usec)
    {
      int nsec = (packet_time.tv_usec - now.tv_usec) / 1000000 + 1;
      packet_time.tv_usec -= 1000000 * nsec;
      packet_time.tv_sec += nsec;
    }
  if (now.tv_usec - packet_time.tv_usec > 1000000)
    {
      int nsec = (now.tv_usec - packet_time.tv_usec) / 1000000;
      packet_time.tv_usec += 1000000 * nsec;
      packet_time.tv_sec -= nsec;
    }

  result.tv_sec = now.tv_sec - packet_time.tv_sec;
  result.tv_usec = now.tv_usec - packet_time.tv_usec;

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

void
update_packet_list (GList * packets, enum packet_belongs belongs_to)
{
  struct timeval now;
  gettimeofday (&now, NULL);
  packets = g_list_last (packets);

  while ((packets = check_packet (packets, now, belongs_to)));
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
  guint8 *src, *dst, *pcap_packet;
  node_t *node;
  packet_t *packet_info;
  link_t *link;

  pcap_packet = (guint8 *) pcap_next (pch, &phdr);

  if (!pcap_packet)
    return;

  if (interape)
    {
      src = pcap_packet + 26;
      dst = pcap_packet + 30;
    }
  else
    {
      src = pcap_packet + 6;
      dst = pcap_packet;
    }

  node = g_tree_lookup (nodes, src);
  if (node == NULL)
    node = create_node (pcap_packet, SRC);

  /* We add a packet to the list of packets to/from that host which we want
   * to account for */
  packet_info = g_malloc (sizeof (packet_t));
  packet_info->size = phdr.len;
  gettimeofday (&(packet_info->timestamp), NULL);
  packet_info->parent = node;
  node->packets = g_list_prepend (node->packets, packet_info);
  node->accumulated += phdr.len;
  /* Now we clean all packets we don't care for anymore */
  update_packet_list (node->packets, NODE);
  node->n_packets++;

  /* Now we do the same with the destination node */

  node = g_tree_lookup (nodes, dst);
  if (node == NULL)
    node = create_node (pcap_packet, DST);

  /* We add a packet to the list of packets to/from that host which we want
   * to account for */
  packet_info = g_malloc (sizeof (packet_t));
  packet_info->size = phdr.len;
  gettimeofday (&(packet_info->timestamp), NULL);
  packet_info->parent = node;
  node->packets = g_list_prepend (node->packets, packet_info);
  node->accumulated += phdr.len;
  /* Now we clean all packets we don't care for anymore */
  update_packet_list (node->packets, NODE);
  node->n_packets++;

  /* And now we update link traffic information for this packet */
  if (interape)
    link = g_tree_lookup (links, src);	/* The comparison function for
					 * the links tree actually
					 * looks at both src and dst,
					 * although we pass the pointer 
					 * src */
  else
    link = g_tree_lookup (links, dst);

  if (!link)
    link = create_link (pcap_packet);

  packet_info = g_malloc (sizeof (link_t));
  packet_info->size = phdr.len;
  gettimeofday (&(packet_info->timestamp), NULL);
  packet_info->parent = link;
  link->packets = g_list_prepend (link->packets, packet_info);
  link->accumulated += phdr.len;
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

  if (!((pcap_t *) pch = pcap_open_live (device, MAXSIZE, TRUE, 100, ebuf)))
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

  nodes = g_tree_new (node_id_compare);
  links = g_tree_new (link_id_compare);

}
