/* eth_resolv.h
 * Definitions for network object lookup
 *
 * $Id$
 *
 * Originally by Laurent Deniel <deniel@worldnet.fr>
 * Modified for etherape by Juan Toledo <toledo@users.sourceforge.net>
 *
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __ETH_RESOLV_H__
#define __ETH_RESOLV_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* Functions in resolv.c */

/* get_tcp_port returns the TCP port name or "%d" if not found */
extern char *get_tcp_port (u_int port);

/* get_ether_name returns the logical name if found in ethers files else
   "<vendor>_%02x:%02x:%02x" if the vendor code is known else
   "%02x:%02x:%02x:%02x:%02x:%02x" */
extern char *get_ether_name (const u_char * addr);

#endif /* __ETH_RESOLV_H__ */
