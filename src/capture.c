

#include <pcap.h>
#include <gnome.h>

#include "capture.h"
#include "resolv.h"

#include <ctype.h>

#include <gnome.h>

extern guint averaging_time;
enum packet_belongs
  {
    NODE = 0, LINK = 1
  };

/* Places char punct in the string as the hex-digit separator.
 * If punct is '\0', no punctuation is applied (and thus
 * the resulting string is 5 bytes shorter)
 */

/* (toledo) This function I copied from capture.c of ethereal it was
 * without comments, but I believe it keeps three different
 * strings conversions in memory so as to try to make sure that
 * the conversions made will be valid in memory for a longer
 * period of time */

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
ether_compare (gconstpointer a, gconstpointer b)
{
  int i = 5;

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
}				/* ether_compare */

/* Comparison function used to order the (GTree *) links
 * and canvas_links heard on the network */
gint
link_compare (gconstpointer a, gconstpointer b)
{
  int i = 11;

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
}				/* link_compare */



/* Allocates a new node structure, and adds it to the
 * global nodes binary tree */
node_t *
create_node (const guint8 * ether_addr)
{
  node_t *node;

  node = g_malloc (sizeof (node_t));
  node->ether_addr = g_memdup (ether_addr, 6);
  node->average = 0;
  node->n_packets = 0;
  node->accumulated = 0;
  node->name = g_string_new (get_ether_name (ether_addr));
  node->packets = NULL;

  g_tree_insert (nodes, node->ether_addr, node);
  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	 _ ("Creating node: %s. Number of nodes %d"),
	 node->name->str,
	 g_tree_nnodes (nodes));

  return node;
}				/* create_node */


/* Allocates a new link structure, and adds it to the
 * global links binary tree */
link_t *
create_link (const guint8 * ether_link)
{
  link_t *link;

  link = g_malloc (sizeof (link_t));
  link->ether_link = g_memdup (ether_link, 12);
  link->average = 0;
  link->n_packets = 0;
  link->accumulated = 0;
  link->packets = NULL;
  g_tree_insert (links, link->ether_link, link);
  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	 _ ("Creating link: %s-%s. Number of links %d"),
	 get_ether_name (ether_link), get_ether_name (ether_link + 6),
	 g_tree_nnodes (links));

  return link;
}				/* create_link */

GList *
check_packet (GList * packets, struct timeval now, enum packet_belongs belongs_to)
{

  struct timeval packet_time;
  struct timeval result;
  packet_t *packet;
  packet = (packet_t *) packets->data;

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

/*  printf ("sec %d, usec %d\n",result.tv_sec,result.tv_usec); */

  /* If this node is older than the averaging time,
   * then it is removed, and the process continues.
   * Else, we are done. */

  if ((result.tv_sec * 1000000 + result.tv_usec) > averaging_time)
    {
      if (belongs_to == NODE)
	{
	  ((node_t *) (packet->parent))->accumulated -=
	    packet->size;
	}
      else
	{
	  ((link_t *) (packet->parent))->accumulated -=
	    packet->size;
	}

      g_free (packets->data);
      packets = packets->prev;
      g_list_remove (packets, packets->next->data);
      return (packets);
    }

  return NULL;
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

  src = pcap_packet;
  dst = pcap_packet + 6;

  node = g_tree_lookup (nodes, src);
  if (node == NULL)
    node = create_node (src);

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
    node = create_node (dst);

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
  link = g_tree_lookup (links, src);	/* The comparison function actually
					 * looks at both src and dst */
  if (!link)
    link = create_link (src);

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

  char ebuf[100];
  if (!((pcap_t *) pch = pcap_open_live ("eth0", MAXSIZE, TRUE, 100, ebuf)))
    {
      g_log (G_LOG_DOMAIN, G_LOG_LEVEL_ERROR,
	     _ ("You need to be root to run this program"));
    }

  pcap_fd = pcap_fileno (pch);
  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "pcap_fd: %d", pcap_fd);
  gdk_input_add (pcap_fd,
		 GDK_INPUT_READ,
		 packet_read,
		 pch);

  nodes = g_tree_new (ether_compare);
  links = g_tree_new (link_compare);

}
