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
static GHashTable *protohash = NULL;

static void 
protohash_item_to_prefvect(gpointer key,
                           gpointer value,
                           gpointer user_data)
{
   gchar *proto = (gchar *)key;
   GdkColor *color = (GdkColor *)value;
   gint *i = (gint *)user_data;
   g_assert( proto && color && i);
   
   pref.colors[*i] = g_strdup_printf ("#%02x%02x%02x%s%s", *color, 
                color->red >> 8, color->green >> 8, color->blue >> 8, 
                (*proto) ? ":" :"", proto);
}


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
}

/* adds or replaces the protoname item */
gboolean 
protohash_set(gchar *protoname, GdkColor protocolor)
{
   GdkColor *newc=NULL;
   
   g_assert(protoname); /* proto must be valid - note: empty IS valid, Null no*/
  
   if (!protohash && ! protohash_init())
      return FALSE;

   /* hash tables operates only on ptr's externally allocated ... */
   newc = g_malloc(sizeof(GdkColor));
   g_assert(newc);
   g_memmove(newc, &protocolor, sizeof(GdkColor));
   
   g_hash_table_insert(protohash, g_strdup(protoname), newc);
   
   return TRUE;
}

/* returns the proto color if exists, NULL otherwise */
GdkColor *
protohash_get(gchar *protoname)
{
   g_assert(protoname); /* proto must be valid - note: empty IS valid, Null no*/
 
   if (!protohash && ! protohash_init())
      return NULL;

   return (GdkColor *)g_hash_table_lookup(protohash, protoname);
}

/* fills the hash from a pref vector */
gboolean 
protohash_read_prefvect(gchar **colors, gint n_colors)
{
   int i;
   
   protohash_clear();
   
  for (i = 0; i < n_colors; i++)
    {
      GdkColor gdk_color;
      gchar **colors_protocols = NULL;
      gchar *protocol = NULL;

      colors_protocols = g_strsplit (colors[i], ";", 0);

      /* converting color */
      gdk_color_parse (colors_protocols[0], &gdk_color);

      /* converting proto name */
      if (!colors_protocols[1])
	protocol = "";
      else
	protocol = colors_protocols[1];

      if (!protohash_set(protocol, gdk_color))
         return FALSE;
    }
    
    return TRUE;
}

/* fills the pref vector from the hash */
gboolean 
protohash_write_prefvect()
{
  gint i;

  while (pref.colors && pref.n_colors)
    {
      g_free (pref.colors[pref.n_colors - 1]);
      pref.n_colors--;
    }
  g_free (pref.colors);
  pref.colors = NULL;

   if (!protohash && ! protohash_init())
      return FALSE;
   
  pref.n_colors = g_hash_table_size(protohash);
  pref.colors = g_malloc (sizeof (gchar *) * pref.n_colors);

  i=0;
  g_hash_table_foreach(protohash, protohash_item_to_prefvect, &i); 

  return TRUE;   
}		
