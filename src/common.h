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
  APEMODE_DEFAULT = -1,
  LINK6 = 0,        /* data link level with 6 bits of address */
  IP,
  TCP
}
apemode_t;

typedef struct __attribute__ ((packed))
{
  union __attribute__ ((packed))
  {
    struct __attribute__ ((packed))
    {
      guint32 type; /* address family: AF_INET or AF_INET6 */
      union __attribute__ ((packed))
      {
        guint8 addr8[16];
        guint32 addr32[4];
        guint32 addr32_v4;
        guint8 addr_v4[4];  /* 32-bit  */
        guint8 addr_v6[16]; /* 128-bit */
      };
    };
    guint8 all8[4*5];
  };
}
address_t;

/* Macros */
#define address_copy(dst, src) memmove((dst), (src), sizeof(address_t))
#define address_clear(dst) memset((dst), 0, sizeof(address_t))
#define address_len(type) ((type)==AF_INET?32/8:(type)==AF_INET6?128/8:0)
#define is_addr_eq(dst, src) (memcmp((dst), (src), sizeof(address_t))==0)
#define is_addr_gt(dst, src) (memcmp((dst), (src), sizeof(address_t))>0)
#define is_addr_lt(dst, src) (memcmp((dst), (src), sizeof(address_t))<0)
#define is_addr_ge(dst, src) (memcmp((dst), (src), sizeof(address_t))>=0)
#define is_addr_le(dst, src) (memcmp((dst), (src), sizeof(address_t))<=0)

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
#define pntohs(p)  ntohs(*((guint16 *)(p)))
#define phtons(p)  htons(*((guint16 *)(p)))
#define pntohl(p)  ntohl(*((guint32 *)(p)))
#define phtonl(p)  htonl(*((guint32 *)(p)))

/* Takes the hi_nibble value from a byte */
#define hi_nibble(b) ((b & 0xf0) >> 4)

#endif /* ETHERAPE_COMMON_H */
