#include <pcap.h>
#include <glib.h>
#include <gtk/gtk.h>

#define MAXSIZE 60

GHashTable *nodes;
GHashTable *links;

typedef struct _node
  {
    gchar ether_addr[6];
    gchar *name;
    glong average;		/* Bytes in or out in the last x ms */
    GList *packets;		/* List of packets sizes in or out and
				   * its sizes. Used to calculate average
				   * traffic */
  }
node_t;

void
init_data (void)
{
  nodes = g_hash_table_new (NULL,NULL);
  links = g_hash_table_new (NULL,NULL);
}

void
packet_read (pcap_t * pch,
	     gint source,
	     GdkInputCondition condition)
{
  struct pcap_pkthdr phdr;
  gchar packet[MAXSIZE];
  gchar src[6],dst[6];

  /* We copy the next available packet */
  memcpy (packet, pcap_next (pch, &phdr), phdr.caplen);

#if 0
  printf ("Ether source: %x:%x:%x:%x:%x:%x\n",
	  (int) packet[0], (int) packet[1], (int) packet[2],
	  (int) packet[3], (int) packet[4], (int) packet[5]);
#endif

  memcpy (src,packet,6);
  memcpy (dst,packet[6],6);

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

}
