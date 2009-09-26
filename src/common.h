/* EtherApe
 * Copyright (C) 2001 Juan Toledo
 * $Id$
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
 
#ifndef ETHERAPE_COMMON_H
#define ETHERAPE_COMMON_H

/* disable deprecated gnome functions */
/* #define G_DISABLE_DEPRECATED 1 */

#include "config.h"


#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#elif HAVE_TIME_H
#include <time.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <pcap.h>
#include <glib.h>

#ifdef ENABLE_NLS
#include <libintl.h>
#ifndef _
#define _(String) gettext(String)
#endif
#else
#ifndef _
#define _(String) (String)
#endif
#endif

#ifndef MAXDNAME
#define MAXDNAME        1025	/* maximum domain name length */
#endif

/* Enumerations */

/* Since gdb does understand enums and not defines, and as 
 * way to make an easier transition to a non-pcap etherape,
 * I define my own enum for link type codes */
/* See /usr/include/net/bpf.h for a longer description of
 * some of the options */
typedef enum
{
  L_NULL = DLT_NULL,		/* no link-layer encapsulation */
  L_EN10MB = DLT_EN10MB,	/* Ethernet (10Mb) */
  L_EN3MB = DLT_EN3MB,		/* Experimental Ethernet (3Mb) */
  L_AX25 = DLT_AX25,		/* Amateur Radio AX.25 */
  L_PRONET = DLT_PRONET,	/* Proteon ProNET Token Ring */
  L_CHAOS = DLT_CHAOS,		/* Chaos */
  L_IEEE802 = DLT_IEEE802,	/* IEEE 802 Networks */
  L_ARCNET = DLT_ARCNET,	/* ARCNET */
  L_SLIP = DLT_SLIP,		/* Serial Line IP */
  L_PPP = DLT_PPP,		/* Point-to-point Protocol */
  L_FDDI = DLT_FDDI,		/* FDDI */
  L_ATM_RFC1483 = DLT_ATM_RFC1483,	/* LLC/SNAP encapsulated atm */
  L_RAW = DLT_RAW,		/* raw IP */
  L_SLIP_BSDOS = DLT_SLIP_BSDOS,	/* BSD/OS Serial Line IP */
  L_PPP_BSDOS = DLT_PPP_BSDOS,	/* BSD/OS Point-to-point Protocol */
#ifdef DLT_ATM_CLIP
  L_ATM_CLIP = DLT_ATM_CLIP,	/* Linux Classical-IP over ATM */
#endif
#ifdef DLT_PPP_SERIAL
  L_PPP_SERIAL = DLT_PPP_SERIAL,	/* PPP over serial with HDLC encapsulation */
#endif
#ifdef DLT_C_HDLC
  L_C_HDLC = DLT_C_HDLC,	/* Cisco HDLC */
#endif
#ifdef DLT_IEEE802_11
  L_IEEE802_11 = DLT_IEEE802_11,	/* IEEE 802.11 wireless */
#endif
#ifdef DLT_LOOP
  L_LOOP = DLT_LOOP,		/* OpenBSD loopback */
#endif
#ifdef DLT_LINUX_SLL
  L_LINUX_SLL = DLT_LINUX_SLL	/* Linux cooked sockets */
#endif
}
link_type_t;

typedef enum
{
  /* Beware! The value given by the option widget is dependant on
   * the order set in glade! */
  LINEAR = 0,
  LOG = 1,
  SQRT = 2
}
size_mode_t;

typedef enum
{
  INST_TOTAL = 0,
  INST_INBOUND = 1,
  INST_OUTBOUND = 2,
  ACCU_TOTAL = 3,
  ACCU_INBOUND = 4,
  ACCU_OUTBOUND = 5
}
node_size_variable_t;

typedef enum
{
  DEFAULT = -1,
  ETHERNET = 0,
  FDDI,
  IEEE802,
  IP,
  TCP
}
apemode_t;

/* Macros */
#define g_my_debug(format, args...)      g_log (G_LOG_DOMAIN, \
						  G_LOG_LEVEL_DEBUG, \
						  format, ##args)
#define g_my_info(format, args...)      g_log (G_LOG_DOMAIN, \
						  G_LOG_LEVEL_INFO, \
						  format, ##args)
#define g_my_critical(format, args...)      g_log (G_LOG_DOMAIN, \
						      G_LOG_LEVEL_CRITICAL, \
						      format, ##args)

#define IS_OLDER(diff, timeout)        ((diff.tv_sec > timeout/1000) | \
					    (diff.tv_usec/1000 >= timeout))



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

#define pletohs(p)  ((guint16)                       \
			((guint16)*((guint8 *)(p)+1)<<8|  \
			    (guint16)*((guint8 *)(p)+0)<<0))


/* Takes the hi_nibble value from a byte */
#define hi_nibble(b) ((b & 0xf0) >> 4)

#endif /* ETHERAPE_COMMON_H */
