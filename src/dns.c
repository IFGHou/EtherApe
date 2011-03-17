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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdint.h>
#include "appdata.h"
#include "dns.h"
#include "direct_resolve.h"
#include "thread_resolve.h"

/* initialize dns interface */
int dns_open (void)
{
#ifdef USE_DIRECTDNS
   return direct_open();
#else
   return thread_open();
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

/* resolves address and returns its fqdn */
const char *dns_lookup (address_t *address)
{
#ifdef USE_DIRECTDNS
   return direct_lookup (address);
#else
   return thread_lookup (address);
#endif   
}


