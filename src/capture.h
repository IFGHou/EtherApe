#define MAXSIZE 60
#define PCAP_TIMEOUT 250
#include <sys/time.h>
#include <pcap.h>

/* Pointer versions of ntohs and ntohl.  Given a pointer to a member of a
 * byte array, returns the value of the two or four bytes at the pointer.
 */
#define pntohs(p)  ((guint16)                       \
		       ((guint16)*((guint8 *)p+0)<<8|  \
			   (guint16)*((guint8 *)p+1)<<0))

#define pntohl(p)  ((guint32)*((guint8 *)p+0)<<24|  \
		       (guint32)*((guint8 *)p+1)<<16|  \
		       (guint32)*((guint8 *)p+2)<<8|   \
		       (guint32)*((guint8 *)p+3)<<0)

GTree *nodes;
GTree *links;


/* 
 * Enumerations 
 */

/* Modes of operation */
typedef enum
  {
    DEFAULT = -1,
    ETHERNET = 0,
    IP = 1,
    TCP = 2,
    UDP = 3
  }
apemode_t;


/* Since gdb does understand enums and not defines, and as 
 * way to make an easier transition to a non-pcap etherape,
 * I define my own enum for link type codes */
typedef enum
  {
    L_NULL = DLT_NULL,		/* no link-layer encapsulation */
    L_EN10MB = DLT_EN10MB,	/* Ethernet (10Mb) */
    L_EN3MB = DLT_EN3MB,	/* Experimental Ethernet (3Mb) */
    L_AX25 = DLT_AX25,		/* Amateur Radio AX.25 */
    L_PRONET = DLT_PRONET,	/* Proteon ProNET Token Ring */
    L_CHAOS = DLT_CHAOS,	/* Chaos */
    L_IEEE802 = DLT_IEEE802,	/* IEEE 802 Networks */
    L_ARCNET = DLT_ARCNET,	/* ARCNET */
    L_SLIP = DLT_SLIP,		/* Serial Line IP */
    L_PPP = DLT_PPP,		/* Point-to-point Protocol */
    L_FDDI = DLT_FDDI,		/* FDDI */
    L_ATM_RFC1483 = DLT_ATM_RFC1483,	/* LLC/SNAP encapsulated atm */
    L_RAW = DLT_RAW,		/* raw IP */
    L_SLIP_BSDOS = DLT_SLIP_BSDOS,	/* BSD/OS Serial Line IP */
    L_PPP_BSDOS = DLT_PPP_BSDOS	/* BSD/OS Point-to-point Protocol */
  }
link_type_t;


/* Flag to indicate whether a packet belongs to a node or a link */
enum packet_belongs
  {
    NODE = 0, LINK = 1
  };

/* Used on some functions to indicate how to operate on the node info
 * depending on what side of the comm the node was at */
enum create_node_type
  {
    SRC = 0,
    DST = 1
  };


/* 
 * Structures 
 */

/* Node information */
typedef struct
  {
    guint8 *node_id;		/* pointer to the node identification
				 * could be an ether or ip address*/
    GString *name;		/* String with a readable default name of the node */
    GString *numeric_name;	/* String with a numeric representation of the id */
    guint32 ip_address;		/* Needed by the resolver */
    gdouble average;		/* Average bytes in or out in the last x ms */
    gdouble accumulated;	/* Accumulated bytes in the last x ms */
    guint n_packets;		/* Number of total packets received */
    struct timeval last_time;	/* Timestamp of the last packet to be added */
    GList *packets;		/* List of packets sizes in or out and
				 * its sizes. Used to calculate average
				 * traffic */
  }
node_t;

/* Link information */
typedef struct
  {
    guint8 *link_id;		/* pointer to guint8 containing src and
				 * destination nodes_id's of the link */
    double average;
    double accumulated;
    guint n_packets;
    gchar *prot;		/* Most common protocol for the link */
    struct timeval last_time;	/* Timestamp of the last packet added */
    GList *packets;
  }
link_t;

/* Information about each packet heard on the network */
typedef struct
  {
    guint size;			/* Size in bytes of the packet */
    struct timeval timestamp;	/* Time at which the packet was heard */
    gchar *prot;		/* Protocol type the packet was carrying */
    gpointer parent;		/* Pointer to the link or node owner of the 
				 * packet */
  }
packet_t;


/* 
 * Exported functions 
 */

void init_capture (void);
gchar *ip_to_str (const guint8 * ad);
gchar *ether_to_str_punct (const guint8 * ad, char punct);
gchar *ether_to_str (const guint8 * ad);
void update_packet_list (GList * packets, enum packet_belongs belongs_to);
struct timeval substract_times (struct timeval a, struct timeval b);
