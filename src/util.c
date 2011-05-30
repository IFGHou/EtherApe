/* This is pretty messy because it is pretty much copied as is from 
 * ethereal. I should probably clean it up some day */


/* util.c
 * Utility routines
 *
 * $Id$
 *
 * Ethereal - Network traffic analyzer
 * By Gerald Combs <gerald@zing.org>
 * Copyright 1998 Gerald Combs
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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_INET_NTOP
#include <arpa/inet.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#elif !defined(INET6_ADDRSTRLEN)
#define INET6_ADDRSTRLEN        46
#endif
#include <pwd.h>
#include <locale.h>

#include "appdata.h"
#include "util.h"

#include <pcap.h> /*JTC*/


/* use only pcap to obtain the interface list */

GList *
interface_list_create(GString *err_str)
{
  GList *il = NULL;
  pcap_if_t *pcap_devlist = NULL;
  pcap_if_t *curdev;
  char pcap_errstr[1024]="";

  g_string_assign(err_str, "");

  if (pcap_findalldevs(&pcap_devlist, pcap_errstr) < 0)
    {
      /* can't obtain interface list from pcap */
      g_string_printf (err_str, "Getting interface list from pcap failed: %s",
	       pcap_errstr);
      return NULL;
    }

  /* We want to list the interfaces in order, but with loopbacks last. Since
   * glist_append must iterate over all elements (!!!), we use g_list_prepend
   * then reverse the list (stupid Glist!) */
    
  /* iterate on all pcap devices, skipping loopbacks*/
  for (curdev = pcap_devlist ; curdev ; curdev = curdev->next)
    {
      if (PCAP_IF_LOOPBACK == curdev->flags)
        continue; /* skip loopback */

      il = g_list_prepend(il, g_strdup(curdev->name));
    }

  /* loopbacks added last */
  for (curdev = pcap_devlist ; curdev ; curdev = curdev->next)
    {
      if (PCAP_IF_LOOPBACK != curdev->flags)
        continue; /* only loopback */

      il = g_list_prepend(il, g_strdup(curdev->name));
    }

  /* reverse list*/
  il = g_list_reverse(il);

  /* release pcap list */
  pcap_freealldevs(pcap_devlist);

  /* return ours */
  return il;
}

static void
interface_list_free_cb(gpointer data, gpointer user_data)
{
  g_free (data);
}

void
interface_list_free(GList * if_list)
{
  if (if_list)
    {
      g_list_foreach (if_list, interface_list_free_cb, NULL);
      g_list_free (if_list);
    }
}


const char *
get_home_dir (void)
{
  char *env_value;
  static const char *home = NULL;
  struct passwd *pwd;

  /* Return the cached value, if available */
  if (home)
    return home;

  env_value = getenv ("HOME");

  if (env_value)
    {
      home = env_value;
    }
  else
    {
      pwd = getpwuid (getuid ());
      if (pwd != NULL)
	{
	  /* This is cached, so we don't need to worry
	     about allocating multiple ones of them. */
	  home = g_strdup (pwd->pw_dir);
	}
      else
	home = "/tmp";
    }

  return home;
}

/* safe strncpy */
char *
safe_strncpy (char *dst, const char *src, size_t maxlen)
{
  
if (maxlen < 1)
    return dst;
  strncpy (dst, src, maxlen - 1);	/* no need to copy that last char */
  dst[maxlen - 1] = '\0';
  return dst;
}

/* safe strncat */
char *
safe_strncat (char *dst, const char *src, size_t maxlen)
{
  size_t lendst = strlen (dst);
  if (lendst >= maxlen)
    return dst;			/* already full, nothing to do */
  strncat (dst, src, maxlen - strlen (dst));
  dst[maxlen - 1] = '\0';
  return dst;
}

/* Next three functions copied directly from ethereal packet.c
 * by Gerald Combs */

/* Output has to be copied elsewhere */
const gchar *
ipv4_to_str (const guint8 * ad)
{
#ifdef HAVE_INET_NTOP
  static char buf[INET6_ADDRSTRLEN];
  if (!inet_ntop(AF_INET, ad, buf, sizeof(buf)))
    return "<invalid IPv4 address>";
  return buf;
#else
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
#endif
}				/* ipv4_to_str */

/* (toledo) This function I copied from capture.c of ethereal it was
 * without comments, but I believe it keeps three different
 * strings conversions in memory so as to try to make sure that
 * the conversions made will be valid in memory for a longer
 * period of time */

/* Places char punct in the string as the hex-digit separator.
 * If punct is '\0', no punctuation is applied (and thus
 * the resulting string is 5 bytes shorter)
 */

static const gchar *
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
const gchar *
ether_to_str (const guint8 * ad)
{
  return ether_to_str_punct (ad, ':');
}				/* ether_to_str */

/*
 * These functions are for IP/IPv6 handling
 */
const gchar *ipv6_to_str (const guint8 *ad)
{
  if (!ad)
    return "<null addr>";
#ifdef HAVE_INET_NTOP
  static char buf[INET6_ADDRSTRLEN];
  if (!inet_ntop(AF_INET6, ad, buf, sizeof(buf))) 
    return "<invalid IPv6 address>";
  return buf;
#else
  static gchar str[3][40];
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
  p = &cur[40];
  *--p = '\0';
  i = 15;
  for (;;)
    {
      octet = ad[i];
      *--p = hex_digits[octet & 0xF];
      octet >>= 4;
      *--p = hex_digits[octet & 0xF];
      i--;
      octet = ad[i];
      *--p = hex_digits[octet & 0xF];
      octet >>= 4;
      *--p = hex_digits[octet & 0xF];
      if (i == 0)
    break;
	  *--p = ':';
      i--;
    }
  return p;
#endif
}				/* ipv6_to_str */

const gchar *
address_to_str (const address_t * ad)
{
  if (!ad)
    return "<null addr>";
  switch (ad->type)
    {
    case AF_INET:
      return ipv4_to_str(ad->addr_v4);
    case AF_INET6:
      return ipv6_to_str(ad->addr_v6);
    default:
      return "<invalid address family>";
    }
}				/* address_to_str */

const gchar *
type_to_str (const address_t * ad)
{
  if (!ad)
    return "<null addr>";
  switch (ad->type)
    {
    case AF_INET:
      return "IP";
    case AF_INET6:
      return "IPv6";
    default:
      return "<invalid address family>";
    }
}				/* type_to_str */

/************************************************
 *
 * xml helpers 
 *
 *************************************************/

/* returns a new string containing the named tag */
gchar *xmltag(const gchar *name, const gchar *fmt, ...)
{
  gchar *msg;
  gchar *xml;
  va_list ap;
  va_start(ap, fmt);
  msg = g_strdup_vprintf(fmt, ap);
  va_end(ap);
  xml = g_strdup_printf("<%s>%s</%s>\n", name, msg, name);
  g_free(msg);
  return xml;
}


/************************************************
 *
 * compatibility functions for CENTOS5 and RHEL5
 *
 *************************************************/
#if !defined(HAVE_G_QUEUE_INIT)

void compat_g_queue_init(GQueue *gq)
{
  g_assert(gq != NULL);
  gq->head = NULL;
  gq->tail = NULL;
  gq->length = 0;
}

gchar *compat_gdk_color_to_string(const GdkColor *color)
{
  g_assert(color != NULL);
  return g_strdup_printf("#%4.4x%4.4x%4.4x", color->red, color->green, color->blue);
}
#endif 
