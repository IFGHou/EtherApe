/* EtherApe
 * Copyright (C) 2001 Juan Toledo
 * $Id$
 *
 * This file is mostly a rehash of algorithms found in
 * packet-*. of ethereal
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "globals.h"
#include <ctype.h>
#include <string.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include "prot_types.h"
#include "util.h"
#include "decode_proto.h"
#include "protocols.h"
#include "conversations.h"
#include "datastructs.h"
#include "node.h"
#include "links.h"
#include "names.h"

#define TCP_FTP 21
#define TCP_NETBIOS_SSN 139
#define UDP_NETBIOS_NS 138

/* Enums */
enum rpc_type
{
  RPC_CALL = 0,
  RPC_REPLY = 1
};

enum rpc_program
{
  PORTMAP_PROGRAM = 100000,
  NFS_PROGRAM = 100003,
  YPSERV_PROGRAM = 100004,
  MOUNT_PROGRAM = 100005,
  YPBIND_PROGRAM = 100007,
  YPPASSWD_PROGRAM = 100009,
  REXEC_PROGRAM = 100017,
  STAT_PROGRAM = 100024,
  BOOTPARAMS_PROGRAM = 100026,
  NLM_PROGRAM = 100021,
  YPXFR_PROGRAM = 100069,
  KERBPROG_PROGRAM = 100078
};

/* internal types */
typedef struct 
{
  const guint8 *original_packet; /* original start of packet */
  guint original_len; /* total captured lenght */

  const guint8 *cur_packet; /* pointer to current level start of packet */
  guint cur_len;        /* current level remaining length */

  packet_protos_t *pr; /* detected protocol stack */
  guint cur_level;      /* current protocol depth on stack */

  /* node ids */
  node_id_t dst_node_id; 
  node_id_t src_node_id;
  
  /* These are used for conversations */
  address_t global_src_address;
  address_t global_dst_address;
  guint16 global_src_port;
  guint16 global_dst_port;

} decode_proto_t;

/* extracts the protocol stack from packet */
static void get_packet_prot (decode_proto_t *dp);

/* starts a new decode, allocating a new packet_protos_t */
static void decode_proto_start(decode_proto_t *dp, 
                               const guint8 *pkt, guint caplen);

/* sets protoname at current level, and passes at next level */
static void decode_proto_add(decode_proto_t *dp, const gchar *fmt, ...);

/* advances current packet start to prepare for next protocol */
static void add_offset(decode_proto_t *dp, guint offset);

static void add_node_packet (const guint8 * packet,
                             guint raw_size,
			     packet_info_t * packet_info,
                             const node_id_t *node_id,
			     packet_direction direction);

/* decoder func proto */
typedef void (*get_fun)(decode_proto_t *dp);

/* specific decoders declarations */
static void get_loop(decode_proto_t *dp);
static void get_eth_type (decode_proto_t *dp);
static void get_fddi_type (decode_proto_t *dp);
static void get_ieee802_5_type (decode_proto_t *dp);
static void get_eth_II (decode_proto_t *dp, etype_t etype);
static void get_eth_802_3 (decode_proto_t *dp, ethhdrtype_t ethhdr_type);
static void get_radiotap (decode_proto_t *dp);
static void get_ppi (decode_proto_t *dp);
static void get_wlan (decode_proto_t *dp);
static void get_linux_sll (decode_proto_t *dp);

static void get_llc (decode_proto_t *dp);
static void get_ip (decode_proto_t *dp);
static void get_ipx (decode_proto_t *dp);
static void get_tcp (decode_proto_t *dp);
static void get_udp (decode_proto_t *dp);

static void get_netbios (decode_proto_t *dp);
static void get_netbios_ssn (decode_proto_t *dp);
static void get_netbios_dgm (decode_proto_t *dp);
static void get_ftp (decode_proto_t *dp);

static gboolean get_rpc (decode_proto_t *dp, gboolean is_udp);
static guint16 choose_port (guint16 a, guint16 b);
static void append_etype_prot (decode_proto_t *dp, etype_t etype);

/* etherape has to handle several data link layer (OSI L2) packet types.
 * The following table tries to make easier adding new types.
 * End of table is signaled by an entry with lt_desc NULL */
typedef struct linktype_data_tag
{
  const gchar *lt_desc; /* linktype description */
  unsigned int dlt_linktype; /* pcap link type (DLT_xxxx defines) */
  apemode_t l2_idtype;   /* link level address slot in node_id_t */
  get_fun fun;  /* linktype base decoder function */
    
} linktype_data_t;



/* current link type entry */
const linktype_data_t *lkentry = NULL;

static linktype_data_t linktypes[] = {
 {"Ethernet",     DLT_EN10MB,    LINK6, get_eth_type },
 {"RAW",          DLT_RAW,       IP,    get_ip    }, /* raw IP like PPP,SLIP */
#ifdef DLT_LINUX_SLL
 {"LINUX_SLL",    DLT_LINUX_SLL, IP,    get_linux_sll }, /* Linux cooked */
#endif  
 {"BSD Loopback", DLT_NULL,      IP,    get_loop }, /* ignore l2 data */
 {"OpenBSD Loopback", DLT_LOOP,  IP,    get_loop }, /* ignore l2 data */
 {"FDDI",         DLT_FDDI,    LINK6,   get_fddi_type },
 {"IEEE802.5",    DLT_IEEE802, LINK6,   get_ieee802_5_type  }, /* Token Ring */
 {"WLAN",   DLT_IEEE802_11,    LINK6,   get_wlan }, 
 /* Wireless with radiotap header */
 {"WLAN+RTAP",  DLT_IEEE802_11_RADIO, LINK6, get_radiotap }, 
 {"PPI",  DLT_PPI, LINK6, get_ppi }, /* PPI encapsulation */
 {NULL,   0, 0, NULL } /* terminating entry, must be last */
};

/* ------------------------------------------------------------
 * L2 offset decoders
 * ------------------------------------------------------------*/

/* Sets the correct linktype entry. Returns false if not found */
gboolean setup_link_type(unsigned int linktype)
{
  int i;

  lkentry = NULL;
  for (i = 0; linktypes[i].lt_desc != NULL ; ++i)
    {
      if (linktypes[i].dlt_linktype == linktype)
        {
          lkentry = linktypes + i;
          g_my_info (_("Link type is %s"), lkentry->lt_desc);
          return TRUE;
        }
    }

  return FALSE; /* link type not supported */
}

/* true if current device captures l2 data */ 
gboolean has_linklevel(void)
{
  if (lkentry)
    return lkentry->l2_idtype == LINK6;
  else
    return FALSE;
}

/* ------------------------------------------------------------
 * Implementation
 * ------------------------------------------------------------*/
/* starts a new decode, allocating a new packet_protos_t */
void decode_proto_start(decode_proto_t *dp, const guint8 *pkt, guint caplen)
{
  dp->original_packet = pkt;
  dp->original_len = caplen;
  dp->cur_packet = pkt;
  dp->cur_len = caplen;
  dp->pr = packet_protos_init();
  dp->cur_level = 1; /* level zero is topmost protocol, will be filled later */
  node_id_clear(&dp->dst_node_id);
  node_id_clear(&dp->src_node_id);
  address_clear(&dp->global_src_address);
  address_clear(&dp->global_dst_address);
  dp->global_src_port = 0;
  dp->global_dst_port = 0;
}
void decode_proto_add(decode_proto_t *dp, const gchar *fmt, ...)
{
  va_list ap;
  if (dp->cur_level <= STACK_SIZE)
    {
      va_start(ap, fmt);
      dp->pr->protonames[dp->cur_level] = g_strdup_vprintf(fmt, ap);
      va_end(ap);
      dp->cur_level++;
    }
  else
    g_warning("protocol \"%.10s\" too deeply nested, ignored", fmt ? fmt : "");
}

static void add_offset(decode_proto_t *dp, guint offset)
{
  if (dp->cur_len < offset)
    dp->cur_len = 0; /* no usable data remaining */
  else
    {
      dp->cur_packet += offset;
      dp->cur_len -= offset; 
    }
}

/* This function is called everytime there is a new packet in
 * the network interface. It then updates traffic information
 * for the appropriate nodes and links 
 * Receives both the captured (raw) size and the real packet size */
void packet_acquired(guint8 * raw_packet, guint raw_size, guint pkt_size)
{
  packet_info_t *packet;
  link_id_t link_id;
  decode_proto_t decp;

  g_assert (raw_packet != NULL);
  if (!lkentry || !lkentry->fun)
    {
      g_error(_("Data link entry not initialized"));
      return;  /* g_error() should abort, but just to be sure ... */
    }

  decode_proto_start(&decp, raw_packet, raw_size);
                                       
  /* create a packet structure to hold data */
  packet = g_malloc (sizeof (packet_info_t));
  g_assert(packet);
  
  packet->size = pkt_size;
  packet->timestamp = now;
  packet->ref_count = 0;

  /* Get the protocol tree */
  get_packet_prot (&decp);
  if (!decp.pr)
    {
      /* fatal error, discard packet */
      g_free(packet); 
      return;
    }
  packet->prot_desc = decp.pr;

  n_packets++;
  total_mem_packets++;

  /* Add this packet information to the src and dst nodes. If they
   * don't exist, create them */
  add_node_packet (raw_packet, raw_size, packet, &decp.src_node_id, OUTBOUND);
  add_node_packet (raw_packet, raw_size, packet, &decp.dst_node_id, INBOUND);

  /* And now we update link traffic information for this packet */
  link_id.src = decp.src_node_id;
  link_id.dst = decp.dst_node_id;
  links_catalog_add_packet(&link_id, packet);

  /* finally, update global protocol stats */
  protocol_summary_add_packet(packet);
}


/* We update node information for each new packet that arrives in the
 * network. If the node the packet refers to is unknown, we
 * create it. */
static void
add_node_packet (const guint8 * raw_packet,
                 guint raw_size,
		 packet_info_t * packet,
                 const node_id_t *node_id,
		 packet_direction direction)
{
  node_t *node;

  node = nodes_catalog_find(node_id);
  if (node == NULL)
    {
      /* creates the new node, adding it to the catalog */
      node = nodes_catalog_new(node_id);
      g_assert(node);
    }

  traffic_stats_add_packet(&node->node_stats, packet, direction);

  /* If this is the first packet we've heard from the node in a while, 
   * we add it to the list of new nodes so that the main app know this 
   * node is active again */
  if (node->node_stats.pkt_list.length == 1)
    new_nodes_add(node);

  /* Update names list for this node */
  get_packet_names (&node->node_stats.stats_protos, raw_packet, raw_size,
		    packet->prot_desc, direction, lkentry->dlt_linktype);

}				/* add_node_packet */

static void get_packet_prot (decode_proto_t *dp)
{
  guint i;

  g_assert(lkentry && lkentry->fun);
  lkentry->fun(dp);

  /* first position is top proto */
  for (i = STACK_SIZE ; i>0 ; --i)
    {
      if (dp->pr->protonames[i])
        {
          dp->pr->protonames[0] = g_strdup(dp->pr->protonames[i]);
          break;
        }
    }
}				/* get_packet_prot */

/* ------------------------------------------------------------
 * Private functions
 * ------------------------------------------------------------*/

/* bsd loopback */
static void get_loop(decode_proto_t *dp)
{
  if (lkentry->dlt_linktype == DLT_LOOP)
    decode_proto_add(dp, "LOOP");
  else
    decode_proto_add(dp, "NULL");
  
  add_offset(dp, 4);
  get_ip(dp);
}

static void get_eth_type (decode_proto_t *dp)
{
  guint16 ethsize;
  guint size_offset; /* offset of size field */
  ethhdrtype_t ethhdr_type = ETHERNET_II;	/* Default */

  if (dp->cur_len < 20)
    return; /* not big enough */

  size_offset = 12; /* in a non VLAN packet offset of size field is 12 bytes */
  
  /* the 16 bit field at offset 12 can have several meanings:
   * in 802.3 is the packet size (<= 1500 or 0x5DC)
   * in Ethernet II the size is really a packet type (>=1536/0x600)
   * a packet with 802.1Q VLAN tag has a type of 0x8100. 
   * Jumbo frames pose a challenge because they are 802.3, but size > 1500
   * Right now we don't support jumbo frames.
   */
  ethsize = pntohs (dp->cur_packet + size_offset);

  if (ethsize == ETHERTYPE_VLAN)
    {
      /* 802.1Q VLAN tagged packet. The 4 byte VLAN header is inserted between
       * source addr and length */
      decode_proto_add(dp, "802.1Q");
      if (DEBUG_ENABLED)
        {
          guint16 vlanid = pntohs(dp->cur_packet + 14) & 0x0fff;
          g_my_debug("VLAN id: %u", vlanid);
        }
      size_offset = 16; 
      ethsize = pntohs (dp->cur_packet + size_offset); /* get the real size */
    }

  if (ethsize <= 1500)
    {
      /* 802.3 ethernet */
      
      /* Is there an 802.2 layer? I can tell by looking at the first 2
       *      bytes after the 802.3 header. If they are 0xffff, then what
       *      follows the 802.3 header is an IPX payload, meaning no 802.2.
       *      (IPX/SPX is they only thing that can be contained inside a
       *      straight 802.3 cur_packet). A non-0xffff value means that 
       *      there's an 802.2 layer inside the 802.3 layer */
      if (dp->cur_packet[size_offset+2] == 0xff && 
          dp->cur_packet[size_offset+3] == 0xff)
        ethhdr_type = ETHERNET_802_3;
      else
        ethhdr_type = ETHERNET_802_2;

      /* Oh, yuck.  Cisco ISL frames require special interpretation of the
       *     destination address field; fortunately, they can be recognized by
       *     checking the first 5 octets of the destination address, which are
       *     01-00-0C-00-00 for ISL frames. */
      if (dp->cur_packet[0] == 0x01 && dp->cur_packet[1] == 0x00 && 
          dp->cur_packet[2] == 0x0C && dp->cur_packet[3] == 0x00 && 
          dp->cur_packet[4] == 0x00)
        {
          /* TODO Analyze ISL frames */
          decode_proto_add(dp, "ISL");
          return;
        }
    }

  /* node ids */
  dp->dst_node_id.node_type = LINK6;
  g_memmove(dp->dst_node_id.addr.eth, dp->cur_packet + 0, 
            sizeof(dp->dst_node_id.addr.eth));
  dp->src_node_id.node_type = LINK6;
  g_memmove(dp->src_node_id.addr.eth, dp->cur_packet + 6, 
            sizeof(dp->src_node_id.addr.eth));

  add_offset(dp, size_offset + 2);

  if (ethhdr_type == ETHERNET_802_3)
    {
      decode_proto_add(dp, "802.3-RAW");
      return;
    }

  if (ethhdr_type == ETHERNET_802_2)
    {
      decode_proto_add(dp, "802.3");
      get_eth_802_3 (dp, ethhdr_type);
      return;
    }

  /* Else, it's ETHERNET_II, so the size is really a type field */
  decode_proto_add(dp, "ETH_II");
  get_eth_II (dp, (etype_t)ethsize);
}				/* get_eth_type */

static void
get_eth_802_3 (decode_proto_t *dp, ethhdrtype_t ethhdr_type)
{
  switch (ethhdr_type)
    {
    case ETHERNET_802_2:
      get_llc (dp);
      break;
    case ETHERNET_802_3:
      get_ipx (dp);
      break;
    default:
      break;
    }
}				/* get_eth_802_3 */

static void
get_fddi_type (decode_proto_t *dp)
{
  decode_proto_add(dp, "FDDI");

  if (dp->cur_len < 14)
    return; /* not big enough */

  decode_proto_add(dp, "LLC");

  /* node ids */
  dp->dst_node_id.node_type = LINK6;
  g_memmove(dp->dst_node_id.addr.eth, dp->cur_packet + 1, 
            sizeof(dp->dst_node_id.addr.eth));
  dp->src_node_id.node_type = LINK6;
  g_memmove(dp->src_node_id.addr.eth, dp->cur_packet + 7, 
            sizeof(dp->src_node_id.addr.eth));
  
  /* Ok, this is only temporary while I truly dissect LLC 
   * and fddi */
  if (dp->cur_len < 21)
    return; /* not big enough */

  if ((dp->cur_packet[19] == 0x08) && (dp->cur_packet[20] == 0x00))
   {
      add_offset(dp, 21);
      get_ip (dp);
    }
}				/* get_fddi_type */

static void
get_ieee802_5_type (decode_proto_t *dp)
{
  decode_proto_add(dp, "Token Ring");

  if (dp->cur_len < 15)
    return; /* not big enough */

  /* node ids */
  dp->dst_node_id.node_type = LINK6;
  g_memmove(dp->dst_node_id.addr.eth, dp->cur_packet + 2, 
            sizeof(dp->dst_node_id.addr.eth));
  dp->src_node_id.node_type = LINK6;
  g_memmove(dp->src_node_id.addr.eth, dp->cur_packet + 8, 
            sizeof(dp->src_node_id.addr.eth));

  if (dp->cur_len < 22)
    return; /* not big enough */

  /* As with FDDI, we only support LLC by now */
  decode_proto_add(dp, "LLC");

  if ((dp->cur_packet[20] == 0x08) && (dp->cur_packet[21] == 0x00))
    {
      add_offset(dp, 22);
      get_ip (dp);
    }

}

static void
get_eth_II (decode_proto_t *dp, etype_t etype)
{
  if (etype == ETHERTYPE_IP || etype == ETHERTYPE_IPv6)
    get_ip (dp);
  else if (etype == ETHERTYPE_IPX)
    get_ipx (dp);
  else
    append_etype_prot (dp, etype);
    
}				/* get_eth_II */

/* handles radiotap header */
static void get_radiotap(decode_proto_t *dp)
{
  guint16 rtlen;

  if (dp->cur_len < 32)
    {
      g_warning (_("Radiotap:captured size too small, packet discarded"));
      decode_proto_add(dp, "RADIOTAP");
      return;
    }

  /* radiotap hdr has 8 bit of version, plus 8bit of padding, followed by
   * 16bit len field. We don't need to parse the header, just skip it 
   * Note: header little endian */
#ifdef WORDS_BIGENDIAN
  rtlen = phtons(dp->cur_packet+2);
#else
  rtlen = *(guint16 *)(dp->cur_packet+2);
#endif

  add_offset(dp, rtlen);
  get_wlan(dp);
}

/* handles PPI (Per Packet Incapsulation) header */
static void get_ppi(decode_proto_t *dp)
{
  static const linktype_data_t *pph_lkentry = NULL;
  guint16 pph_len;
  guint32 pph_dlt;
  int i;

  if (dp->cur_len < 64)
    {
      g_warning (_("PPI:captured size too small, packet discarded"));
      decode_proto_add(dp, "PPI");
      return;
    }

  /* PPI hdr has 8 bit of version, plus 8bit of flags, followed by
   * 16bit len field and finally by a 32 bit DLT number. 
   * Between header and data there could be some optional information fields.
   * We don't need to parse header or fields, just skip it 
   * Note: all ppi dati are in little-endian order */
#ifdef WORDS_BIGENDIAN
  pph_len = phtons(dp->cur_packet+2);
  pph_dlt = phtonl(dp->cur_packet+4);
#else
  pph_len = *(guint16 *)(dp->cur_packet+2);
  pph_dlt = *(guint32 *)(dp->cur_packet+4);
#endif

  add_offset(dp, pph_len);
  /* if the last packet seen has a different dlt type, we rescan the table */
  if (!pph_lkentry || pph_lkentry->dlt_linktype != pph_dlt)
    {
      pph_lkentry = NULL;
      for (i = 0; linktypes[i].lt_desc != NULL ; ++i)
        {
          if (linktypes[i].dlt_linktype == pph_dlt)
            {
              pph_lkentry = linktypes + i;
              pph_lkentry->fun(dp);
            }
        }
      g_warning (_("PPI:unsupported link type %u, packet discarded"), pph_dlt);
      return;
    }
  pph_lkentry->fun(dp);
}

static void decode_wlan_mgmt(decode_proto_t *dp, uint8_t subtype)
{
  switch (subtype)
    {
      case 0: /* association request */
      case 1: /* association response */
      case 2: /* REassociation request */
      case 3: /* REassociation response */
        decode_proto_add(dp, "WLAN-ASSOC");
        break;
      case 4: /* probe req */
      case 5: /* probe resp */
        decode_proto_add(dp, "WLAN-PROBE");
        break;
      case 8: 
        decode_proto_add(dp, "WLAN-BEACON");
        break;
      case 9: 
        decode_proto_add(dp, "WLAN-ATIM");
        break;
      case 10: /* deassociation */
        decode_proto_add(dp, "WLAN-DEASSOC");
        break;
      case 11: /* authentication */
      case 12: /* DEauthentication */
        decode_proto_add(dp, "WLAN-AUTH");
        break;
      default:
        decode_proto_add(dp, "WLAN-MGMT-UNKN");
        break;
    }
}

/* ieee802.11 wlans */
static void get_wlan(decode_proto_t *dp)
{
  uint8_t type;
  uint8_t subtype;
  uint8_t wep;
  uint8_t fromtods;
  uint8_t dstofs;
  uint8_t srcofs;

  decode_proto_add(dp, "IEE802.11"); /* experimental */
  if (dp->cur_len < 10)
    {
      g_warning (_("wlan:captured size too small, packet discarded"));
      return;
    }

  /* frame control field: two bytes, network order
   * frame type is in bits 2-3, subtype 4-7, tods 8, fromds 9, wep 14 */
  type = (dp->cur_packet[0] >> 2) & 0x03;
  subtype = (dp->cur_packet[0] >> 4) & 0x0f;
  fromtods = (dp->cur_packet[1]) & 0x03;
  wep = (dp->cur_packet[1] >> 6) & 0x01;

  /* WLAN packets can have up to four (!) addresses, while EtherApe handles
   * only two :-)
   * We *could* register the packet twice or even three times, to show the full
   * traffic, but while useful to understand how WLAN really works usually one
   * prefers to ignore switches, APs and so on, so we try to decode only the
   * SA/DA addresses. AP ones are used only for station to AP traffic.
   * Note:
   * In monitor mode we could pick up a packet multiple times: for example first
   * from node A to AP, then from AP to node B.
   * In that case, traffic statistics will be wrong! */
  if (fromtods > 3)
    {
      g_warning (_("Invalid tofromds field in WLAN packet: 0x%x"), fromtods);
      return;
    }

  /* a1:4 a2:10 a3:16 a4:24 */
  switch (fromtods)  
    {
      case 0:
        /* fromds:0, tods:0 ---> DA=addr1, SA=addr2 */
        dstofs = 4;
        srcofs = 10;
        break;
      case 1:
        /* fromds:0, tods:1 ---> DA=addr3, SA=addr2 */
        dstofs = 16;
        srcofs = 10;
        break;
      case 2:
        /* fromds:1, tods:0 ---> DA=addr1, SA=addr3 */
        dstofs = 4;
        srcofs = 16;
        break;
      case 3:
        /* fromds:1, tods:1 ---> DA=addr3, SA=addr4 */
        dstofs = 16;
        srcofs = 24;
        break;
    }

  if (dp->cur_len < dstofs + 6)
    {
      g_warning (_("wlan:captured size too small, packet discarded"));
      return;
    }
  dp->dst_node_id.node_type = LINK6;
  g_memmove(dp->dst_node_id.addr.eth, dp->cur_packet + dstofs, 
              sizeof(dp->dst_node_id.addr.eth));

  /* for type 1 frames (control) only RTS (subtype 12) has two addresses */
  if (type != 1 || subtype == 12)
    {
      /* source addr present */
      if (dp->cur_len < srcofs + 6)
        {
          g_warning (_("wlan:captured size too small, packet discarded"));
          return;
        }
      dp->src_node_id.node_type = LINK6;
      g_memmove(dp->src_node_id.addr.eth, dp->cur_packet + srcofs, 
                sizeof(dp->src_node_id.addr.eth));
    }

  if (fromtods != 3)
    add_offset(dp, 24); 
  else
    add_offset(dp, 30); /* fourth address present */
  
  switch ( type )
    {
      case 2:
        /* data frame */
        if (!wep)
          {
            if (subtype == 8)
               add_offset(dp, 2); /* QOS info present */
              get_llc(dp);
          }
        else
          decode_proto_add(dp, "WLAN-CRYPTED");
        break;

      case 0: 
        /* mgmt frame */
        decode_wlan_mgmt(dp, subtype);
        break;

      case 1: 
        /* control frame */
        decode_proto_add(dp, "WLAN-CTRL");
        break;

      default:
        g_warning (_("wlan:unknown frame type 0x%x, decode aborted"), type);
        return;
    }
}

/* Gets the protocol type out of the linux-sll header.
 * I have no real idea of what can be there, but since IP
 * is 0x800 I guess it follows ethernet specifications */
static void
get_linux_sll (decode_proto_t *dp)
{
  etype_t etype;

  decode_proto_add(dp, "LINUX-SLL");

  etype = pntohs (&dp->cur_packet[14]);

  add_offset(dp, 16);
  if (etype == ETHERTYPE_IP || etype == ETHERTYPE_IPv6)
    get_ip (dp);
  else if (etype == ETHERTYPE_IPX)
    get_ipx (dp);
  else
    append_etype_prot (dp, etype);
}				/* get_linux_sll_type */

static void
get_llc (decode_proto_t *dp)
{
#define XDLC_I          0x00	/* Information frames */
#define XDLC_U          0x03	/* Unnumbered frames */
#define XDLC_UI         0x00	/* Unnumbered Information */
#define XDLC_IS_INFORMATION(control) \
     (((control) & 0x1) == XDLC_I || (control) == (XDLC_UI|XDLC_U))

  sap_type_t dsap, ssap;
  guint16 control;

  if (dp->cur_len < 4)
    return;

  dsap = dp->cur_packet[0];
  ssap = dp->cur_packet[1];

  if (dsap == SAP_SNAP && ssap == SAP_SNAP)
    {
      /* LLC SNAP: has an additional ethernet II type added */
      etype_t eth2_type;
      eth2_type = (etype_t) pntohs (dp->cur_packet + 6);
      add_offset(dp, 8);
      get_eth_II (dp, eth2_type);
      return;
    }

  decode_proto_add(dp, "LLC");

  /* To get this control value is actually a lot more
   * complicated than this, see xdlc.c in ethereal,
   * but I'll try like this, it seems it works for my pourposes at
   * least most of the time */
  control = dp->cur_packet[2];

  if (!XDLC_IS_INFORMATION (control))
    return;

  add_offset(dp, 3);

  switch (dsap)
    {
    case SAP_NULL:
      decode_proto_add(dp, "LLC-NULL");
      break;
    case SAP_LLC_SLMGMT:
      decode_proto_add(dp, "LLC-SLMGMT");
      break;
    case SAP_SNA_PATHCTRL:
      decode_proto_add(dp, "PATHCTRL");
      break;
    case SAP_IP:
      get_ip(dp);
      break;
    case SAP_SNA1:
      decode_proto_add(dp, "SNA1");
      break;
    case SAP_SNA2:
      decode_proto_add(dp, "SNA2");
      break;
    case SAP_PROWAY_NM_INIT:
      decode_proto_add(dp, "PROWAY-NM-INIT");
      break;
    case SAP_TI:
      decode_proto_add(dp, "TI");
      break;
    case SAP_BPDU:
      decode_proto_add(dp, "BPDU");
      break;
    case SAP_RS511:
      decode_proto_add(dp, "RS511");
      break;
    case SAP_X25:
      decode_proto_add(dp, "X25");
      break;
    case SAP_XNS:
      decode_proto_add(dp, "XNS");
      break;
    case SAP_NESTAR:
      decode_proto_add(dp, "NESTAR");
      break;
    case SAP_PROWAY_ASLM:
      decode_proto_add(dp, "PROWAY-ASLM");
      break;
    case SAP_ARP:
      decode_proto_add(dp, "ARP");
      break;
    case SAP_SNAP:
      /* We are not supposed to reach this point */
      g_warning ("Reached SNAP while checking for DSAP in get_llc");
      decode_proto_add(dp, "LLC-SNAP");
      break;
    case SAP_VINES1:
      decode_proto_add(dp, "VINES1");
      break;
    case SAP_VINES2:
      decode_proto_add(dp, "VINES2");
      break;
    case SAP_NETWARE:
      get_ipx (dp);
      break;
    case SAP_NETBIOS:
      decode_proto_add(dp, "NETBIOS");
      get_netbios (dp);
      break;
    case SAP_IBMNM:
      decode_proto_add(dp, "IBMNM");
      break;
    case SAP_RPL1:
      decode_proto_add(dp, "RPL1");
      break;
    case SAP_UB:
      decode_proto_add(dp, "UB");
      break;
    case SAP_RPL2:
      decode_proto_add(dp, "RPL2");
      break;
    case SAP_OSINL:
      decode_proto_add(dp, "OSINL");
      break;
    case SAP_GLOBAL:
      decode_proto_add(dp, "LLC-GLOBAL");
      break;
    }

}				/* get_llc */

static void
get_ip (decode_proto_t *dp)
{
  guint16 fragment_offset;
  iptype_t ip_type;
  int ip_version, ip_hl;

  if (dp->cur_len < 20)
    return; 

  ip_version = (dp->cur_packet[0] >> 4) & 15;
  switch (ip_version)
    {
    case 4:
      ip_hl = (dp->cur_packet[0] & 15) << 2;
      if (ip_hl < 20)
        return;
      decode_proto_add(dp, "IP");

      ip_type = dp->cur_packet[9];
      fragment_offset = pntohs (dp->cur_packet + 6);
      fragment_offset &= 0x0fff;

      if (pref.mode !=  LINK6)
        {
          /* we want node higher level node ids */
          dp->dst_node_id.node_type = IP;
          address_clear(&dp->dst_node_id.addr.ip);
          dp->dst_node_id.addr.ip.type = AF_INET;
          g_memmove(dp->dst_node_id.addr.ip.addr_v4, dp->cur_packet + 16, 
                    sizeof(dp->dst_node_id.addr.ip.addr_v4));
          dp->src_node_id.node_type = IP;
          address_clear(&dp->src_node_id.addr.ip);
          dp->src_node_id.addr.ip.type = AF_INET;
          g_memmove(dp->src_node_id.addr.ip.addr_v4, dp->cur_packet + 12, 
                    sizeof(dp->src_node_id.addr.ip.addr_v4));
        }

      add_offset(dp, ip_hl);
      break;
    case 6:
      if (dp->cur_len < 40)
        return; 
      decode_proto_add(dp, "IPV6");

      ip_type = dp->cur_packet[6];
      fragment_offset = 0;

      if (pref.mode !=  LINK6)
        {
          /* we want higher level node ids */
          dp->dst_node_id.node_type = IP;
          dp->dst_node_id.addr.ip.type = AF_INET6;
          g_memmove(dp->dst_node_id.addr.ip.addr_v6, dp->cur_packet + 24, 
                    sizeof(dp->dst_node_id.addr.ip.addr_v6));
          dp->src_node_id.node_type = IP;
          dp->src_node_id.addr.ip.type = AF_INET6;
          g_memmove(dp->src_node_id.addr.ip.addr_v6, dp->cur_packet + 8, 
                    sizeof(dp->src_node_id.addr.ip.addr_v6));
        }

      add_offset(dp, 40);
      break;
    default:
      return;
    }

  /* This is used for conversations */
  address_copy(&dp->global_src_address, &dp->src_node_id.addr.ip);
  address_copy(&dp->global_dst_address, &dp->dst_node_id.addr.ip);

  switch (ip_type)
    {
    case IP_PROTO_ICMP:
      decode_proto_add(dp, "ICMP");
      break;
    case IP_PROTO_TCP:
      if (fragment_offset)
	decode_proto_add(dp, "TCP_FRAGMENT");
      else
        get_tcp (dp);
      break;
    case IP_PROTO_UDP:
      if (fragment_offset)
	decode_proto_add(dp, "UDP_FRAGMENT");
      else
        get_udp (dp);
      break;
    case IP_PROTO_IGMP:
      decode_proto_add(dp, "IGMP");
      break;
    case IP_PROTO_GGP:
      decode_proto_add(dp, "GGP");
      break;
    case IP_PROTO_IPIP:
      decode_proto_add(dp, "IPIP");
      break;
#if 0				/* TODO How come IPIP and IPV4 share the same number? */
    case IP_PROTO_IPV4:
      decode_proto_add(dp, "IPV4");
      break;
#endif
    case IP_PROTO_EGP:
      decode_proto_add(dp, "EGP");
      break;
    case IP_PROTO_PUP:
      decode_proto_add(dp, "PUP");
      break;
    case IP_PROTO_IDP:
      decode_proto_add(dp, "IDP");
      break;
    case IP_PROTO_TP:
      decode_proto_add(dp, "TP");
      break;
    case IP_PROTO_IPV6:
      decode_proto_add(dp, "IPV6");
      break;
    case IP_PROTO_ROUTING:
      decode_proto_add(dp, "ROUTING");
      break;
    case IP_PROTO_FRAGMENT:
      decode_proto_add(dp, "FRAGMENT");
      break;
    case IP_PROTO_RSVP:
      decode_proto_add(dp, "RSVP");
      break;
    case IP_PROTO_GRE:
      decode_proto_add(dp, "GRE");
      break;
    case IP_PROTO_ESP:
      decode_proto_add(dp, "ESP");
      break;
    case IP_PROTO_AH:
      decode_proto_add(dp, "AH");
      break;
    case IP_PROTO_ICMPV6:
      decode_proto_add(dp, "ICPMPV6");
      break;
    case IP_PROTO_NONE:
      decode_proto_add(dp, "NONE");
      break;
    case IP_PROTO_DSTOPTS:
      decode_proto_add(dp, "DSTOPTS");
      break;
    case IP_PROTO_VINES:
      decode_proto_add(dp, "VINES");
      break;
    case IP_PROTO_EIGRP:
      decode_proto_add(dp, "EIGRP");
      break;
    case IP_PROTO_OSPF:
      decode_proto_add(dp, "OSPF");
      break;
    case IP_PROTO_ENCAP:
      decode_proto_add(dp, "ENCAP");
      break;
    case IP_PROTO_PIM:
      decode_proto_add(dp, "PIM");
      break;
    case IP_PROTO_IPCOMP:
      decode_proto_add(dp, "IPCOMP");
      break;
    case IP_PROTO_VRRP:
      decode_proto_add(dp, "VRRP");
      break;
    default:
      decode_proto_add(dp, "IP_UNKNOWN");
    }
}

static void
get_ipx (decode_proto_t *dp)
{
  ipx_socket_t ipx_dsocket, ipx_ssocket;
  guint16 ipx_length;
  ipx_type_t ipx_type;

  /* Make sure this is an IPX cur_packet */
  if (dp->cur_len < 30 || *(guint16 *) (dp->cur_packet) != 0xffff)
    return;

  decode_proto_add(dp, "IPX");

  ipx_dsocket = pntohs (dp->cur_packet + 16);
  ipx_ssocket = pntohs (dp->cur_packet + 28);
  ipx_type = *(guint8 *) (dp->cur_packet + 5);
  ipx_length = pntohs (dp->cur_packet + 2);

  switch (ipx_type)
    {
      /* Look at the socket with these two types */
    case IPX_PACKET_TYPE_PEP:
    case IPX_PACKET_TYPE_IPX:
      break;
    case IPX_PACKET_TYPE_RIP:
      decode_proto_add(dp, "IPX-RIP");
      break;
    case IPX_PACKET_TYPE_ECHO:
      decode_proto_add(dp, "IPX-ECHO");
      break;
    case IPX_PACKET_TYPE_ERROR:
      decode_proto_add(dp, "IPX-ERROR");
      break;
    case IPX_PACKET_TYPE_SPX:
      decode_proto_add(dp, "IPX-SPX");
      break;
    case IPX_PACKET_TYPE_NCP:
      decode_proto_add(dp, "IPX-NCP");
      break;
    case IPX_PACKET_TYPE_WANBCAST:
      decode_proto_add(dp, "IPX-NetBIOS");
      break;
    }

  if ((ipx_type != IPX_PACKET_TYPE_IPX) && (ipx_type != IPX_PACKET_TYPE_PEP)
      && (ipx_type != IPX_PACKET_TYPE_WANBCAST))
    return;

  if ((ipx_dsocket == IPX_SOCKET_SAP) || (ipx_ssocket == IPX_SOCKET_SAP))
    decode_proto_add(dp, "IPX-SAP");
  else if ((ipx_dsocket == IPX_SOCKET_ATTACHMATE_GW)
	   || (ipx_ssocket == IPX_SOCKET_ATTACHMATE_GW))
    decode_proto_add(dp, "ATTACHMATE-GW");
  else if ((ipx_dsocket == IPX_SOCKET_PING_NOVELL)
	   || (ipx_ssocket == IPX_SOCKET_PING_NOVELL))
    decode_proto_add(dp, "PING-NOVELL");
  else if ((ipx_dsocket == IPX_SOCKET_NCP) || (ipx_ssocket == IPX_SOCKET_NCP))
    decode_proto_add(dp, "IPX-NCP");
  else if ((ipx_dsocket == IPX_SOCKET_IPXRIP)
	   || (ipx_ssocket == IPX_SOCKET_IPXRIP))
    decode_proto_add(dp, "IPX-RIP");
  else if ((ipx_dsocket == IPX_SOCKET_NETBIOS)
	   || (ipx_ssocket == IPX_SOCKET_NETBIOS))
    decode_proto_add(dp, "IPX-NetBIOS");
  else if ((ipx_dsocket == IPX_SOCKET_DIAGNOSTIC)
	   || (ipx_ssocket == IPX_SOCKET_DIAGNOSTIC))
    decode_proto_add(dp, "IPX-DIAG");
  else if ((ipx_dsocket == IPX_SOCKET_SERIALIZATION)
	   || (ipx_ssocket == IPX_SOCKET_SERIALIZATION))
    decode_proto_add(dp, "IPX-SERIAL.");
  else if ((ipx_dsocket == IPX_SOCKET_ADSM)
	   || (ipx_ssocket == IPX_SOCKET_ADSM))
    decode_proto_add(dp, "IPX-ADSM");
  else if ((ipx_dsocket == IPX_SOCKET_EIGRP)
	   || (ipx_ssocket == IPX_SOCKET_EIGRP))
    decode_proto_add(dp, "EIGRP");
  else if ((ipx_dsocket == IPX_SOCKET_WIDE_AREA_ROUTER)
	   || (ipx_ssocket == IPX_SOCKET_WIDE_AREA_ROUTER))
    decode_proto_add(dp, "IPX W.A. ROUTER");
  else if ((ipx_dsocket == IPX_SOCKET_TCP_TUNNEL)
	   || (ipx_ssocket == IPX_SOCKET_TCP_TUNNEL))
    decode_proto_add(dp, "IPX-TCP-TUNNEL");
  else if ((ipx_dsocket == IPX_SOCKET_UDP_TUNNEL)
	   || (ipx_ssocket == IPX_SOCKET_UDP_TUNNEL))
    decode_proto_add(dp, "IPX-UDP-TUNNEL");
  else if ((ipx_dsocket == IPX_SOCKET_NWLINK_SMB_SERVER)
	   || (ipx_ssocket == IPX_SOCKET_NWLINK_SMB_SERVER)
	   || (ipx_dsocket == IPX_SOCKET_NWLINK_SMB_NAMEQUERY)
	   || (ipx_ssocket == IPX_SOCKET_NWLINK_SMB_NAMEQUERY)
	   || (ipx_dsocket == IPX_SOCKET_NWLINK_SMB_REDIR)
	   || (ipx_ssocket == IPX_SOCKET_NWLINK_SMB_REDIR)
	   || (ipx_dsocket == IPX_SOCKET_NWLINK_SMB_MAILSLOT)
	   || (ipx_ssocket == IPX_SOCKET_NWLINK_SMB_MAILSLOT)
	   || (ipx_dsocket == IPX_SOCKET_NWLINK_SMB_MESSENGER)
	   || (ipx_ssocket == IPX_SOCKET_NWLINK_SMB_MESSENGER)
	   || (ipx_dsocket == IPX_SOCKET_NWLINK_SMB_BROWSE)
	   || (ipx_ssocket == IPX_SOCKET_NWLINK_SMB_BROWSE))
    decode_proto_add(dp, "IPX-SMB");
  else if ((ipx_dsocket == IPX_SOCKET_SNMP_AGENT)
	   || (ipx_ssocket == IPX_SOCKET_SNMP_AGENT)
	   || (ipx_dsocket == IPX_SOCKET_SNMP_SINK)
	   || (ipx_ssocket == IPX_SOCKET_SNMP_SINK))
    decode_proto_add(dp, "SNMP");
  else
    g_my_debug ("Unknown IPX ports %d, %d", ipx_dsocket, ipx_ssocket);

}				/* get_ipx */

static void
get_tcp (decode_proto_t *dp)
{
  const port_service_t *src_service, *dst_service, *chosen_service;
  port_type_t src_port, dst_port, chosen_port;
  guint8 th_off_x2;
  guint8 tcp_len;
  const gchar *str;
  gboolean src_pref = FALSE;
  gboolean dst_pref = FALSE;

  decode_proto_add(dp, "TCP");
  dp->global_src_port = src_port = pntohs (dp->cur_packet);
  dp->global_dst_port = dst_port = pntohs (dp->cur_packet + 2);

  if (pref.mode ==  TCP)
    {
      /* tcp mode node ids have both addr and port - to work we need 
       * to already have an IP node id */
      g_assert(dp->dst_node_id.node_type == IP);
      dp->dst_node_id.node_type = TCP;
      address_copy(&dp->dst_node_id.addr.tcp4.host, &dp->global_dst_address);
      dp->dst_node_id.addr.tcp4.port = dp->global_dst_port;

      g_assert(dp->src_node_id.node_type == IP);
      dp->src_node_id.node_type = TCP;
      address_copy(&dp->src_node_id.addr.tcp4.host, &dp->global_src_address);
      dp->src_node_id.addr.tcp4.port = dp->global_src_port;
    }

  th_off_x2 = *(guint8 *) (dp->cur_packet + 12);
  tcp_len = hi_nibble (th_off_x2) * 4;	/* TCP header length, in bytes */

  add_offset(dp, tcp_len);

  /* Check whether this cur_packet belongs to a registered conversation */
  if ((str = find_conversation (&dp->global_src_address, &dp->global_dst_address,
				src_port, dst_port)))
    {
      decode_proto_add(dp, str);
      return;
    }

  /* It's not possible to know in advance whether an UDP
   * cur_packet is an RPC cur_packet. We'll try */
  /* False means we are calling rpc from a TCP cur_packet */
  if (get_rpc (dp, FALSE))
    return;

  if (src_port==TCP_NETBIOS_SSN || dst_port==TCP_NETBIOS_SSN)
    {
      get_netbios_ssn (dp);
      return;
    }
  else if (src_port==TCP_FTP || dst_port == TCP_FTP)
    {
      get_ftp (dp);
      return;
    }

  src_service = services_tcp_find(src_port);
  dst_service = services_tcp_find(dst_port);
  chosen_port = choose_port (src_port, dst_port);

  if (!src_service && !dst_service)
    {
      if (pref.group_unk)
        decode_proto_add(dp, "TCP-UNKNOwN");
      else
        {
          if (chosen_port == src_port)
            decode_proto_add(dp, "TCP:%d-%d", chosen_port, dst_port);
          else
            decode_proto_add(dp, "TCP:%d-%d", chosen_port, src_port);
        }
      return;
    }

  /* if one of the services has user-defined color, then we favor it,
   * otherwise chosen_port wins */
  if (src_service)
    src_pref = src_service->preferred;
  if (dst_service)
    dst_pref = dst_service->preferred;
  
  if (src_pref && !dst_pref)
    chosen_service = src_service;
  else if (!src_pref && dst_pref)
    chosen_service = dst_service;
  else if (!dst_service || ((src_port == chosen_port) && src_service))
    chosen_service = src_service;
  else
    chosen_service = dst_service;

  decode_proto_add(dp, "%s", chosen_service->name);
}				/* get_tcp */

static void
get_udp (decode_proto_t *dp)
{
  const port_service_t *src_service, *dst_service, *chosen_service;
  port_type_t src_port, dst_port, chosen_port;
  gboolean src_pref = FALSE;
  gboolean dst_pref = FALSE;

  decode_proto_add(dp, "UDP");
  dp->global_src_port = src_port = pntohs (dp->cur_packet);
  dp->global_dst_port = dst_port = pntohs (dp->cur_packet + 2);

  if (pref.mode ==  TCP)
    {
      /* tcp/udp mode node ids have both addr and port - to work we need 
       * to already have an IP node id */
      g_assert(dp->dst_node_id.node_type == IP);
      dp->dst_node_id.node_type = TCP;
      address_copy(&dp->dst_node_id.addr.tcp4.host, &dp->global_dst_address);
      dp->dst_node_id.addr.tcp4.port = dp->global_dst_port;

      g_assert(dp->src_node_id.node_type == IP);
      dp->src_node_id.node_type = TCP;
      address_copy(&dp->src_node_id.addr.tcp4.host, &dp->global_src_address);
      dp->src_node_id.addr.tcp4.port = dp->global_src_port;
    }

  add_offset(dp, 8);

  /* It's not possible to know in advance whether an UDP
   * cur_packet is an RPC cur_packet. We'll try */
  if (get_rpc (dp, TRUE))
    return;

  if (src_port==UDP_NETBIOS_NS || dst_port==UDP_NETBIOS_NS)
    {
      get_netbios_dgm (dp);
      return;
    }

  src_service = services_udp_find(src_port);
  dst_service = services_udp_find(dst_port);

  chosen_port = choose_port (src_port, dst_port);

  if (!dst_service && !src_service)
    {
      if (pref.group_unk)
        decode_proto_add(dp, "UDP-UNKNOWN");
      else
        {
          if (chosen_port == src_port)
            decode_proto_add(dp, "UDP:%d-%d", chosen_port, dst_port);
          else
            decode_proto_add(dp, "UDP:%d-%d", chosen_port, src_port);
        }
      return;
    }

  /* if one of the services has user-defined color, then we favor it,
   * otherwise chosen_port wins */
  if (src_service)
    src_pref = src_service->preferred;
  if (dst_service)
    dst_pref = dst_service->preferred;
  
  if (src_pref && !dst_pref)
    chosen_service = src_service;
  else if (!src_pref && dst_pref)
    chosen_service = dst_service;
  else if (!dst_service || ((src_port == chosen_port) && src_service))
    chosen_service = src_service;
  else
    chosen_service = dst_service;
  decode_proto_add(dp, "%s", chosen_service->name);
}				/* get_udp */

static gboolean
get_rpc (decode_proto_t *dp, gboolean is_udp)
{
  int rpcstart;
  uint32_t rpcver;
  enum rpc_type msg_type;
  enum rpc_program msg_program;
  const gchar *rpc_prot = NULL;

  /* Determine whether this is an RPC packet */
  if (dp->cur_len < 24)
    return FALSE;		/* not big enough */

  if (is_udp)
    rpcstart = 0;
  else
    rpcstart = 4;

  msg_type = pntohl (dp->cur_packet + rpcstart + 4);

  switch (msg_type)
    {
    case RPC_REPLY:
      /* RPC_REPLYs don't carry an rpc program tag */
      /* TODO In order to be able to dissect what is it's 
       * protocol I'd have to keep track of who sent
       * which call */
      if (!(rpc_prot = find_conversation (&dp->global_dst_address, 0,
					  dp->global_dst_port, 0)))
	return FALSE;
      decode_proto_add(dp, "ONC-RPC");
      decode_proto_add(dp, rpc_prot);
      return TRUE;

    case RPC_CALL:
      rpcver = pntohl (dp->cur_packet + rpcstart + 8);
      if (rpcver != 2)
        return FALSE; /* only ONC-RPC v2 */
  
      msg_program = pntohl (dp->cur_packet + rpcstart + 12);
      switch (msg_program)
	{
	case BOOTPARAMS_PROGRAM:
	  rpc_prot = "BOOTPARAMS";
	  break;
	case MOUNT_PROGRAM:
	  rpc_prot = "MOUNT";
	  break;
	case NLM_PROGRAM:
	  rpc_prot = "NLM";
	  break;
	case PORTMAP_PROGRAM:
	  rpc_prot = "PORTMAP";
	  break;
	case STAT_PROGRAM:
	  rpc_prot = "STAT";
	  break;
	case NFS_PROGRAM:
	  rpc_prot = "NFS";
	  break;
	case YPBIND_PROGRAM:
	  rpc_prot = "YPBIND";
	  break;
	case YPSERV_PROGRAM:
	  rpc_prot = "YPSERV";
	  break;
	case YPXFR_PROGRAM:
	  rpc_prot = "YPXFR";
	  break;
	case YPPASSWD_PROGRAM:
	  rpc_prot = "YPPASSWD";
	  break;
	case REXEC_PROGRAM:
	  rpc_prot = "REXEC";
	  break;
	case KERBPROG_PROGRAM:
	  rpc_prot = "KERBPROG";
	  break;
	default:
          if (msg_program >= 100000 && msg_program <= 101999)
            rpc_prot = "RPC-UNKNOWN";
          else
            return FALSE;
          break;
	}

      /* Search for an already existing conversation, if not, create one */
      g_assert(rpc_prot);
      if (!find_conversation (&dp->global_src_address, 0, dp->global_src_port, 0))
	add_conversation (&dp->global_src_address, 0,
			  dp->global_src_port, 0, rpc_prot);

      decode_proto_add(dp, "ONC-RPC");
      decode_proto_add(dp, rpc_prot);
      return TRUE;

    default:
      return FALSE;
    }
  
  return FALSE;
}				/* get_rpc */

/* This function is only called from a straight llc cur_packet,
 * never from an IP cur_packet */
void
get_netbios (decode_proto_t *dp)
{
#define pletohs(p)  ((guint16)                       \
			((guint16)*((guint8 *)(p)+1)<<8|  \
			    (guint16)*((guint8 *)(p)+0)<<0)) 

  guint16 hdr_len;

  /* Check that there is room for the minimum header */
  if (dp->cur_len < 5)
    return;

  hdr_len = pletohs (dp->cur_packet);

  /* If there is any data at all, it is SMB (or so I understand
   * from Ethereal's cur_packet-netbios.c */

  if (dp->cur_len > hdr_len)
    decode_proto_add(dp, "SMB");

}				/* get_netbios */

void
get_netbios_ssn (decode_proto_t *dp)
{
#define SESSION_MESSAGE 0
  guint8 mesg_type;

  decode_proto_add(dp, "NETBIOS-SSN");

  mesg_type = dp->cur_packet[0];

  if (mesg_type == SESSION_MESSAGE)
    decode_proto_add(dp, "SMB");

  /* TODO Calculate new dp->offset whenever we have
   * a "dissector" for a higher protocol */
}				/* get_netbions_ssn */

void
get_netbios_dgm (decode_proto_t *dp)
{
  guint8 mesg_type;

  decode_proto_add(dp, "NETBIOS-DGM");

  mesg_type = dp->cur_packet[0];

  /* Magic numbers copied from ethereal, as usual
   * They mean Direct (unique|group|broadcast) datagram */
  if (mesg_type == 0x10 || mesg_type == 0x11 || mesg_type == 0x12)
    decode_proto_add(dp, "SMB");

  /* TODO Calculate new dp->offset whenever we have
   * a "dissector" for a higher protocol */
}				/* get_netbios_dgm */

void
get_ftp (decode_proto_t *dp)
{
  gchar *mesg = NULL;
  gchar *str;
  guint hi_byte, low_byte;
  guint16 server_port;
  guint size = dp->cur_len;
  guint i = 0;

  decode_proto_add(dp, "FTP");
  if (dp->cur_len < 3)
    return;			/* not big enough */

  if ((gchar) dp->cur_packet[0] != '2'
      || (gchar) dp->cur_packet[1] != '2'
      || (gchar) dp->cur_packet[2] != '7')
    return;

  /* We have a passive message. Get the port */
  mesg = g_malloc (size + 1);
  g_assert(mesg);

  memcpy (mesg, dp->cur_packet, size);
  mesg[size] = '\0';

  g_my_debug ("Found FTP passive command: %s", mesg);

  str = strtok (mesg, "(,)");
  g_my_debug ("FTP Token: %s", str ? str : "NULL");
  for (i = 0; i < 4 && str; i++)
    {
      str = strtok (NULL, "(,)");
      g_my_debug ("FTP Token: %s", str ? str : "NULL");
    }

  str = strtok (NULL, "(,)");
  g_my_debug ("FTP Token: %s", str ? str : "NULL");
  if (!str || !sscanf (str, "%d", &hi_byte))
    {
      g_free(mesg);
      return;
    }
  str = strtok (NULL, "(,)");
  g_my_debug ("FTP Token: %s", str ? str : "NULL");
  if (!str || !sscanf (str, "%d", &low_byte))
    {
      g_free(mesg);
      return;
    }

  server_port = hi_byte * 256 + low_byte;
  g_my_debug ("FTP Hi: %d. Low: %d. Passive port is %d", hi_byte, low_byte,
	      server_port);

  /* A port number zero means any port */
  add_conversation (&dp->global_src_address, &dp->global_dst_address,
		    server_port, 0, "FTP-PASSIVE");

  g_free (mesg);
}

/* Given two port numbers, it returns the 
 * one that is a privileged port if the other
 * is not. If not, just returns the lower numbered */
static guint16
choose_port (guint16 a, guint16 b)
{
  guint ret;

  if ((a < 1024) && (b >= 1024))
    ret = a;
  else if ((a >= 1024) && (b < 1024))
    ret = b;
  else if (a <= b)
    ret = a;
  else
    ret = b;

  return ret;

}				/* choose_port */

static void
append_etype_prot (decode_proto_t *dp, etype_t etype)
{
  switch (etype)
    {
    case ETHERTYPE_IP:
      decode_proto_add(dp, "IP");
      break;
    case ETHERTYPE_ARP:
      decode_proto_add(dp, "ARP");
      break;
    case ETHERTYPE_IPv6:
      decode_proto_add(dp, "IPV6");
      break;
    case ETHERTYPE_X25L3:
      decode_proto_add(dp, "X25L3");
      break;
    case ETHERTYPE_REVARP:
      decode_proto_add(dp, "REVARP");
      break;
    case ETHERTYPE_ATALK:
      decode_proto_add(dp, "ATALK");
      break;
    case ETHERTYPE_AARP:
      decode_proto_add(dp, "AARP");
      break;
    case ETHERTYPE_IPX:
      break;
    case ETHERTYPE_VINES:
      decode_proto_add(dp, "VINES");
      break;
    case ETHERTYPE_TRAIN:
      decode_proto_add(dp, "TRAIN");
      break;
    case ETHERTYPE_LOOP:
      decode_proto_add(dp, "LOOP");
      break;
    case ETHERTYPE_PPPOED:
      decode_proto_add(dp, "PPPOED");
      break;
    case ETHERTYPE_PPPOES:
      decode_proto_add(dp, "PPPOES");
      break;
    case ETHERTYPE_VLAN:
      decode_proto_add(dp, "VLAN");
      break;
    case ETHERTYPE_SNMP:
      decode_proto_add(dp, "SNMP");
      break;
    case ETHERTYPE_DNA_DL:
      decode_proto_add(dp, "DNA-DL");
      break;
    case ETHERTYPE_DNA_RC:
      decode_proto_add(dp, "DNA-RC");
      break;
    case ETHERTYPE_DNA_RT:
      decode_proto_add(dp, "DNA-RT");
      break;
    case ETHERTYPE_DEC:
      decode_proto_add(dp, "DEC");
      break;
    case ETHERTYPE_DEC_DIAG:
      decode_proto_add(dp, "DEC-DIAG");
      break;
    case ETHERTYPE_DEC_CUST:
      decode_proto_add(dp, "DEC-CUST");
      break;
    case ETHERTYPE_DEC_SCA:
      decode_proto_add(dp, "DEC-SCA");
      break;
    case ETHERTYPE_DEC_LB:
      decode_proto_add(dp, "DEC-LB");
      break;
    case ETHERTYPE_MPLS:
      decode_proto_add(dp, "MPLS");
      break;
    case ETHERTYPE_MPLS_MULTI:
      decode_proto_add(dp, "MPLS-MULTI");
      break;
    case ETHERTYPE_LAT:
      decode_proto_add(dp, "LAT");
      break;
    case ETHERTYPE_PPP:
      decode_proto_add(dp, "PPP");
      break;
    case ETHERTYPE_WCP:
      decode_proto_add(dp, "WCP");
      break;
    case ETHERTYPE_3C_NBP_DGRAM:
      decode_proto_add(dp, "3C-NBP-DGRAM");
      break;
    case ETHERTYPE_ETHBRIDGE:
      decode_proto_add(dp, "ETHBRIDGE");
      break;
    case ETHERTYPE_UNK:
      decode_proto_add(dp, "ETH_UNKNOWN");
    }
}				/* append_etype_prot */

