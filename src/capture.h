#define MAXSIZE 60

GHashTable *nodes;
GHashTable *links;

typedef struct _node
  {
    guint8 ether_addr;
    gchar *name;
    glong average;		/* Bytes in or out in the last x ms */
    guint n_packets;		/* Number of total packets received */
    GList *packets;		/* List of packets sizes in or out and
				   * its sizes. Used to calculate average
				   * traffic */
  }
node_t;

void init_capture (void);


gchar *ether_to_str_punct(const guint8 *ad, char punct);
gchar *ether_to_str(const guint8 *ad);

