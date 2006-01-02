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

#include <glib.h>
#include <gtk/gtk.h>
#include "datastructs.h"
#include "globals.h"

/*
 ***********************************************************************
 *
 * proto->color hash table support functions
 *
 ***********************************************************************
*/
static GHashTable *protohash = NULL; /* the hash table containing proto,color pairs*/
static GList *free_color_list = NULL; /* the list of colors without protocol */
static GList *current_free = NULL; /* current ptr to free color */

/* adds or replaces the protoname item */
static gboolean protohash_set(gchar *protoname, GdkColor protocolor);



static gboolean 
protohash_init(void)
{
   if (protohash)
      return TRUE; /* already ok */
   
   protohash = g_hash_table_new_full(g_str_hash,
                                     g_str_equal,
                                     g_free,
                                     g_free);
   return protohash != NULL;
}

/* clears the proto hash */
void 
protohash_clear(void)
{
   if (protohash)
   {
      g_hash_table_destroy(protohash);
      protohash=NULL;
   }
   
   while (free_color_list)
     {
       g_free(free_color_list->data);
       free_color_list = g_list_delete_link(free_color_list,free_color_list);
     }
  current_free = NULL;
}

/* adds or replaces the protoname item */
static gboolean 
protohash_set(gchar *protoname, GdkColor protocolor)
{
   GdkColor *newc=NULL;
   
   if (!protohash && ! protohash_init())
      return FALSE;

   /* hash tables and lists operate only on ptr's externally allocated ... */
   newc = g_malloc(sizeof(GdkColor));
   g_assert(newc);
   g_memmove(newc, &protocolor, sizeof(GdkColor));

   /* If the protocol is missing, the color is added to the cycle list,
    * otherwise the pair (proto,color) is placed in the hash */
   if (protoname)
     g_hash_table_insert(protohash, g_strdup(protoname), newc);
   else
     {
       free_color_list = g_list_prepend(free_color_list, newc);
       current_free = free_color_list;
     }

   return TRUE;
}

/* returns the proto color if exists, NULL otherwise */
GdkColor *
protohash_get(const gchar *protoname)
{
  GdkColor *color;
  g_assert(protoname); /* proto must be valid - note: empty IS valid, NULL no*/
  g_assert(protohash);

  color = (GdkColor *)g_hash_table_lookup(protohash, protoname);
  if (!color)
    {
      color = (GdkColor *)current_free->data;
      current_free = current_free->next;
      if (!current_free)
        current_free = free_color_list;
    }
  g_my_debug ("Protocol %s in color 0x%2.2x%2.2x%2.2x", 
              protoname, color->red, color->green, color->blue);
  return color;
}

/* fills the hash from a pref vector */
gboolean 
protohash_read_prefvect(gchar **colors, gint n_colors)
{
  int i;
  GdkColor gdk_color;
  
  protohash_clear();

  for (i = 0; i < n_colors; i++)
    {
      gchar **colors_protocols = NULL;

      colors_protocols = g_strsplit (colors[i], ";", 0);

      /* converting color */
      gdk_color_parse (colors_protocols[0], &gdk_color);

      protohash_set(colors_protocols[1], gdk_color);

      g_strfreev(colors_protocols);

    }

  if (!free_color_list)
    {
      /* the list of color available for cycling is empty, so we add a grey */
      gdk_color_parse ("#7f7f7f", &gdk_color);
      protohash_set(NULL, gdk_color);
    }
    return TRUE;
}
