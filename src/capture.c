
#include <pcap.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "capture.h"

#include <ctype.h>

char ascii[20];


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
      if (((guint8 *)a)[i] < ((guint8 *)b)[i])
	{
	  return -1;
	}
      else if (((guint8 *)a)[i] > ((guint8 *)b)[i]) 
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
  node->ether_addr = g_memdup (ether_addr,6);
  node->average = 0;
  node->n_packets = 0;
  node->name = g_string_new (ether_to_str (ether_addr));
  g_tree_insert (nodes, node->ether_addr, node);
  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, \
	 "Creating node: %s. Number of nodes %d", \
	 node->name->str, \
	 g_tree_nnodes (nodes));

  return node;
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
  node->average += phdr.len;
  node->n_packets++;

  node = g_tree_lookup (nodes, dst);
  if (node == NULL)
    node = create_node (dst);
  node->average += phdr.len;
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
  printf ("pcap_fd: %d\n", pcap_fd);
  error = gdk_input_add (pcap_fd,
			 GDK_INPUT_READ,
			 packet_read,
			 pch);
  printf ("gdk_input_add error: %d\n", error);

  init_data ();

}
