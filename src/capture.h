#define MAXSIZE 60
#include <sys/time.h>

GTree *nodes;
GTree *links;

enum packet_belongs
  {
    NODE = 0, LINK = 1
  };

enum create_node_type
  {
    SRC = 0,
    DST = 1
  };

typedef struct
  {
    guint8 *ether_addr;		/* pointer to the hardware address of the node */
    GString *name;		/* String with a readable name of the node */
    double average;		/* Average bytes in or out in the last x ms */
    double accumulated;		/* Accumulated bytes in the last x ms */
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
    double accumulated;
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

/* Exported functions */

void init_capture (void);
gchar *ip_to_str(const guint8 *ad);
gchar *ether_to_str_punct (const guint8 * ad, char punct);
gchar *ether_to_str (const guint8 * ad);
void update_packet_list (GList * packets, enum packet_belongs belongs_to);
