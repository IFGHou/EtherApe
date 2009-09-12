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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include <pwd.h>

#include "globals.h"
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

