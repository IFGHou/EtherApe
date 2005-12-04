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


/* Next three functions copied directly from ethereal packet.c
 * by Gerald Combs */

/* Output has to be copied elsewhere */
/* TODO should I dump this funtion now that I have dns_lookup? */
gchar *
ip_to_str (const guint8 * ad)
{
  static gchar str[3][16];
  static gchar *cur;
  gchar *p;
  int i;
  guint32 octet;
  guint32 digit;

  if (cur == &str[0][0])
    {
      cur = &str[1][0];
    }
  else if (cur == &str[1][0])
    {
      cur = &str[2][0];
    }
  else
    {
      cur = &str[0][0];
    }
  p = &cur[16];
  *--p = '\0';
  i = 3;
  for (;;)
    {
      octet = ad[i];
      *--p = (octet % 10) + '0';
      octet /= 10;
      digit = octet % 10;
      octet /= 10;
      if (digit != 0 || octet != 0)
	*--p = digit + '0';
      if (octet != 0)
	*--p = octet + '0';
      if (i == 0)
	break;
      *--p = '.';
      i--;
    }
  return p;
}				/* ip_to_str */

/* (toledo) This function I copied from capture.c of ethereal it was
 * without comments, but I believe it keeps three different
 * strings conversions in memory so as to try to make sure that
 * the conversions made will be valid in memory for a longer
 * period of time */

/* Places char punct in the string as the hex-digit separator.
 * If punct is '\0', no punctuation is applied (and thus
 * the resulting string is 5 bytes shorter)
 */

gchar *
ether_to_str_punct (const guint8 * ad, char punct)
{
  static gchar str[3][18];
  static gchar *cur;
  gchar *p;
  int i;
  guint32 octet;
  static const gchar hex_digits[16] = "0123456789abcdef";

  if (cur == &str[0][0])
    {
      cur = &str[1][0];
    }
  else if (cur == &str[1][0])
    {
      cur = &str[2][0];
    }
  else
    {
      cur = &str[0][0];
    }
  p = &cur[18];
  *--p = '\0';
  i = 5;
  for (;;)
    {
      octet = ad[i];
      *--p = hex_digits[octet & 0xF];
      octet >>= 4;
      *--p = hex_digits[octet & 0xF];
      if (i == 0)
	break;
      if (punct)
	*--p = punct;
      i--;
    }
  return p;
}				/* ether_to_str_punct */



/* Wrapper for the most common case of asking
 * for a string using a colon as the hex-digit separator.
 */
gchar *
ether_to_str (const guint8 * ad)
{
  return ether_to_str_punct (ad, ':');
}				/* ether_to_str */
