#define MAXSIZE 60
#include <sys/time.h>

GTree *nodes;
GTree *links;

typedef struct 
  {
    guint8 *ether_addr;		/* pointer to the hardware address of the node */
    GString *name;		/* String with a readable name of the node */
    double average;		/* Average bytes in or out in the last x ms */
    glong accumulated;		/* Accumulated bytes in the last x ms */
    guint n_packets;		/* Number of total packets received */
    GList *packets;		/* List of packets sizes in or out and
				   * its sizes. Used to calculate average
				   * traffic */
  }
node_t;


typedef struct 
{
   guint8 *ether_link;		/* pointer to guint8[12] containing src and
				 * destination hardware addresses of the link */
   double average;
   glong accumulated;
   guint n_packets;
   GList *packets;
}
link_t;

typedef struct 
  {
    guint size;
    struct timeval timestamp;
    gpointer parent;
  }
packet_t;

void init_capture (void);


gchar *ether_to_str_punct (const guint8 * ad, char punct);
gchar *ether_to_str (const guint8 * ad);
