#include <pcap.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "capture.h"

#include <ctype.h>

char ascii[20];


/* Places char punct in the string as the hex-digit separator.
 *  * If punct is '\0', no punctuation is applied (and thus
 *  * the resulting string is 5 bytes shorter)
 *  */


gchar *
  ether_to_str_punct(const guint8 *ad, char punct) {
       static gchar  str[3][18];
       static gchar *cur;
       gchar        *p;
       int          i;
       guint32      octet;
       static const gchar hex_digits[16] = "0123456789abcdef";
     
       if (cur == &str[0][0]) {
	      cur = &str[1][0];
       } else if (cur == &str[1][0]) {
	      cur = &str[2][0];
       } else {
	      cur = &str[0][0];
       }
       p = &cur[18];
       *--p = '\0';
       i = 5;
       for (;;) {
	      octet = ad[i];
	      *--p = hex_digits[octet&0xF];
	      octet >>= 4;
	      *--p = hex_digits[octet&0xF];
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
  ether_to_str(const guint8 *ad)
{
           return ether_to_str_punct(ad, ':');
}


guint
node_hash (gconstpointer v)
{
  int hash_val = 0;
  memcpy (&hash_val, v, sizeof (guint));
  return hash_val;
}


void
init_data (void)
{
  nodes = g_hash_table_new (node_hash, g_int_equal);
  links = g_hash_table_new (NULL, NULL);
}

node_t *
create_node (guint8 ether_addr)
{
  node_t *node;
  node = g_malloc (sizeof (node_t));
  node->ether_addr=ether_addr;
  node->average = 0;
  g_hash_table_insert (nodes, &node->ether_addr, node);

  return node;
}

void
packet_read (pcap_t * pch,
	     gint source,
	     GdkInputCondition condition)
{
  struct pcap_pkthdr phdr;
  gchar packet[MAXSIZE];
  guint8 src, dst;
  node_t *node;

  char *pcap_packet;



      /* We copy the next available packet */
      memcpy (packet, pcap_packet = (gchar *)pcap_next (pch, &phdr), phdr.caplen);

      memcpy (&src, (char *) packet, 6);
      memcpy (&dst, (char *) (packet + 6), 6);

      node = g_hash_table_lookup (nodes, &src);
      if (node == NULL)
	node = create_node (src);
      node->average += phdr.len;

      node = g_hash_table_lookup (nodes, &dst);
      if (node == NULL)
	node = create_node (dst);
      node->average += phdr.len;

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
