/* util.h
 * Utility definitions
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

#ifndef __UTIL_H__
#define __UTIL_H__

#include "appdata.h"

#ifdef __cplusplus
extern "C"
{
#endif				/* __cplusplus */

/* Returns the user's home directory, via the HOME environment
 * variable, or a default directory if HOME is not set */
  const char *get_home_dir (void);

  /* gets a list containing the names of available interfaces. Returns NULL 
   * if there is an error, putting also an error message into err_str.
   * The returned list MUST be freed with interface_list_free() */
  GList *interface_list_create(GString *err_str);
  void interface_list_free(GList * if_list);

  char *safe_strncpy (char *dst, const char *src, size_t maxlen);
  char *safe_strncat (char *dst, const char *src, size_t maxlen);

  /* utility functions */
  const gchar *ipv4_to_str(const guint8 * ad);
  const gchar *ether_to_str(const guint8 * ad);
  const gchar *ipv6_to_str(const guint8 *ad);
  const gchar *address_to_str(const address_t * ad);
  const gchar *type_to_str(const address_t * ad);

  /* xml helpers */
  gchar *xmltag(const gchar *name, const gchar *fmt, ...);

#if !defined(HAVE_G_QUEUE_INIT)
  void compat_g_queue_init(GQueue *gq);
  gchar *compat_gdk_color_to_string(const GdkColor *color);
#define g_queue_init compat_g_queue_init
#define gdk_color_to_string compat_gdk_color_to_string
#endif

  
#ifdef __cplusplus
}
#endif				/* __cplusplus */

#endif				/* __UTIL_H__ */
