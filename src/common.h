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
  DEFAULT = -1,
  LINK6 = 0,        /* data link level with 6 bits of address */
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
