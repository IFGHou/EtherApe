#include <pcap.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "capture.h"

#include <ctype.h>

#include <gnome.h>

extern guint averaging_time;

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
 *  * for a string using a colon as the hex-digit separator.
 *  */
gchar *
ether_to_str (const guint8 * ad)
{
  return ether_to_str_punct (ad, ':');
}



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
}


void
init_data (void)
{
  nodes = g_tree_new (ether_compare);
}

node_t *
create_node (const guint8 * ether_addr)
{
  node_t *node;

  node = g_malloc (sizeof (node_t));
  node->ether_addr = g_memdup (ether_addr, 6);
  node->average = 0;
  node->n_packets = 0;
  node->name = g_string_new (ether_to_str (ether_addr));
  node->packets = NULL;

  g_tree_insert (nodes, node->ether_addr, node);
  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, \
	 _ ("Creating node: %s. Number of nodes %d"), \
	 node->name->str, \
	 g_tree_nnodes (nodes));

  return node;
}

GList *
check_packet (GList * packets, struct timeval now)
{

  struct timeval packet_time, result;
  packet_time = ((packet_t *) packets->data)->timestamp;

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

  /* If this node is older than the averaging time,
   * then it is removed, and the process continues.
   * Else, we are done. */

  if ((now.tv_sec * 1000000 + now.tv_usec -
       packet_time.tv_sec * 1000000 - packet_time.tv_usec)
      > averaging_time)
    {
      ((packet_t *) packets->data)->parent_node->accumulated -=
	((packet_t *) packets->data)->size;
      g_free (packets->data);
      packets = packets->prev;
      g_list_remove (packets, packets->next->data);
      return (packets);
    }

  return NULL;
}

void
update_packet_list (GList * packets)
{
  struct timeval now;
  gettimeofday (&now, NULL);
  packets = g_list_last (packets);

  while ((packets = check_packet (packets, now)));
}

void
packet_read (pcap_t * pch,
	     gint source,
	     GdkInputCondition condition)
{
  struct pcap_pkthdr phdr;
  gchar packet[MAXSIZE];
  guint8 src[6], dst[6];
  node_t *node;
  packet_t *packet_info;

  gchar *pcap_packet;

  /* We copy the next available packet */
  memcpy (packet, pcap_packet = (gchar *) pcap_next (pch, &phdr), phdr.caplen);

  memcpy (src, (char *) pcap_packet, 6);
  memcpy (dst, (char *) pcap_packet + 6, 6);

#if 0
  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, \
	 "source: %s, destination: %s", \
	 ether_to_str (src), \
	 ether_to_str (dst));
#endif

  node = g_tree_lookup (nodes, src);
  if (node == NULL)
    node = create_node (src);

  /* We add a packet to the list of packets to/from that host which we want
   * to account for */
  packet_info = g_malloc (sizeof (packet_t));
  packet_info->size = phdr.len;
  gettimeofday (&(packet_info->timestamp), NULL);
  packet_info->parent_node = node;
  node->packets = g_list_prepend (node->packets, packet_info);
  node->accumulated += phdr.len;
  /* Now we clean all packets we don't care for anymore */
  update_packet_list (node->packets);
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
  packet_info->parent_node = node;
  node->packets = g_list_prepend (node->packets, packet_info);
  node->accumulated += phdr.len;
  /* Now we clean all packets we don't care for anymore */
  update_packet_list (node->packets);
  node->n_packets++;


}


void
init_capture (void)
{

  gint pcap_fd;
  gint error;
  pcap_t *pch;

  char ebuf[100];
  (pcap_t *) pch = pcap_open_live ("eth0", MAXSIZE, TRUE, 100, ebuf);
  pcap_fd = pcap_fileno (pch);
  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "pcap_fd: %d", pcap_fd);
  gdk_input_add (pcap_fd,
		 GDK_INPUT_READ,
		 packet_read,
		 pch);
  init_data ();

}
