#include <pcap.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "capture.h"

#include <ctype.h>

char ascii[20];

char *
ethertoascii (char *ether_addr)
{

  int i;
      char a;
      char b;
   

  if (!ether_addr)
    g_error ("Null pointer to ethertoascii\n");
  for (i = 0; i < 6; i++)
    {
      a = b = ether_addr[i];
      a = a >> 4;
      b = b & 15;
      ascii[3 * i] = toascii (a);
      ascii[3 * i + 1] = toascii (b);
      ascii[3 * i + 2] = ':';
    }
  ascii[18] = '\0';
   
  return ascii;
}


guint
node_hash (gconstpointer v)
{
  int hash_val = 0;
  memcpy (&hash_val, v, sizeof (guint));
  g_print ("Hash %d\tAddress: %s\n", hash_val, ethertoascii ((char *)v));
  return hash_val;
}


void
init_data (void)
{
  nodes = g_hash_table_new (node_hash, g_int_equal);
  links = g_hash_table_new (NULL, NULL);
}

node_t *
create_node (gchar ether_addr[6])
{
  node_t *node;
  node = g_malloc (sizeof (node_t));
  memcpy (node->ether_addr, ether_addr, 6);
  node->average = 0;
  g_hash_table_insert (nodes, node->ether_addr, node);

  return node;
}

void
packet_read (pcap_t * pch,
	     gint source,
	     GdkInputCondition condition)
{
  struct pcap_pkthdr phdr;
  gchar packet[MAXSIZE];
  gchar src[6], dst[6];
  node_t *node;
  gint pcap_fd;
  fd_set set1;
  guint counter = 1;
  struct timeval timeout;

  char *pcap_packet;

  pcap_fd = pcap_fileno (pch);

  FD_ZERO (&set1);
  FD_SET (pcap_fd, &set1);
  timeout.tv_sec = 0;
  timeout.tv_usec = 1;

#if 0
  while (select (pcap_fd + 1, &set1, NULL, NULL, &timeout) != 0)
    {
#endif
      /* We copy the next available packet */
      memcpy (packet, pcap_packet = (gchar *)pcap_next (pch, &phdr), phdr.caplen);

      memcpy (src, packet, 6);
      memcpy (dst, (char *) (packet + 6), 6);

      node = g_hash_table_lookup (nodes, src);
      if (node == NULL)
	node = create_node (src);
      node->average += phdr.len;

      node = g_hash_table_lookup (nodes, dst);
      if (node == NULL)
	node = create_node (dst);
      node->average += phdr.len;

      counter++;
#if 0
    }
#endif

  g_print ("Paquetes: %d\tHosts:%d\n", counter, g_hash_table_size (nodes));
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
