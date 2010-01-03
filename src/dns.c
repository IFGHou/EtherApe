/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   common dns routines
*/

#include <config.h>
#include <stdint.h>
#include "globals.h"
#include "dns.h"
#include "direct_resolve.h"
#include "thread_resolve.h"


/* initialize dns interface */
void dns_open (void)
{
#ifdef USE_DIRECTDNS
   direct_open();
#else
   thread_open();
#endif   
}

/* close dns interface */
void dns_close(void)
{
#ifdef USE_DIRECTDNS
   direct_close();
#else
   thread_close();
#endif   
}

/* returns 1 if the current dns implementation has a socket wich needs a select() */
int dns_hasfd(void)
{
#ifdef USE_DIRECTDNS
   return direct_hasfd();
#else
   return thread_hasfd();
#endif   
}

/* returns the file descriptor associated with dns socket */
int dns_waitfd (void)
{
#ifdef USE_DIRECTDNS
   return direct_waitfd();
#else
   return thread_waitfd();
#endif   
}

/* called when the dns_waitfd socket has available data */
void dns_ack (void)
{
#ifdef USE_DIRECTDNS
   return direct_ack();
#else
   return thread_ack();
#endif   
}

/* resolves address and returns its fqdn (if the corresponding parameter is nonzero)
   or just the hostname (if fqdn is zero) */
char *dns_lookup (uint32_t address, int fqdn)
{
#ifdef USE_DIRECTDNS
   return direct_lookup (address, fqdn);
#else
   return thread_lookup (address, fqdn);
#endif   
}


