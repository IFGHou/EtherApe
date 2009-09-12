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
static GList *cycle_color_list = NULL; /* the list of colors without protocol */
static GList *current_cycle = NULL; /* current ptr to free color */

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
   
   while (cycle_color_list)
     {
       g_free(cycle_color_list->data);
       cycle_color_list = g_list_delete_link(cycle_color_list,cycle_color_list);
     }
  current_cycle = NULL;
}

/* adds or replaces the protoname item */
static gboolean 
protohash_set(gchar *protoname, GdkColor protocolor)
{
   if (!protohash && ! protohash_init())
      return FALSE;

   /* N.B. hash tables and lists operate only on ptr's externally allocated ... */

   /* if a protocol is specified, we put the pair (proto,color) in the hash */
   if (protoname && *protoname)
     g_hash_table_insert(protohash, g_strdup(protoname), 
                         g_memdup(&protocolor, sizeof(GdkColor)));

  /* Without protocol, or if we want also registered colors in the cycle
    * list, we add the color to the cycle list */
   if (!protoname || !*protoname || pref.cycle)
     {
       cycle_color_list = g_list_prepend(cycle_color_list, 
                                   g_memdup(&protocolor, sizeof(GdkColor)));
       current_cycle = cycle_color_list;
     }

   return TRUE;
}

/* resets the cycle color to start of list */
void
protohash_reset_cycle(void)
{
  current_cycle = cycle_color_list;
}

/* returns the proto color if exists, NULL otherwise */
GdkColor
protohash_get(const gchar *protoname)
{
  GdkColor *color;
  g_assert(protoname); /* proto must be valid - note: empty IS valid, NULL no*/
  g_assert(protohash);

  color = (GdkColor *)g_hash_table_lookup(protohash, protoname);
  if (!color)
    {
      /* color not found, take from cycle list */
      color = (GdkColor *)current_cycle->data;

      /* add to hash */
      g_hash_table_insert(protohash, g_strdup(protoname), 
                         g_memdup(color, sizeof(GdkColor)));

      /* advance cycle */
      current_cycle = current_cycle->next;
      if (!current_cycle)
        current_cycle = cycle_color_list;
    }
/*  g_my_debug ("Protocol %s in color 0x%2.2x%2.2x%2.2x", 
              protoname, color->red, color->green, color->blue); */
  return *color;
}

/* fills the hash from a pref vector */
gboolean 
protohash_read_prefvect(gchar **colors)
{
  int i;
  GdkColor gdk_color;
  
  protohash_clear();

  /* fills with colors */
  for (i = 0; colors[i]; ++i)
    {
      gchar **colors_protocols, **protos;
      int j;

      colors_protocols = g_strsplit_set(colors[i], "; \t\n", 0);

      /* converting color */
      gdk_color_parse (colors_protocols[0], &gdk_color);

      if (!colors_protocols[1] || !strlen(colors_protocols[1]))
        protohash_set(colors_protocols[1], gdk_color);
      else
        {
          /* multiple protos, split them */
          protos = g_strsplit_set(colors_protocols[1], ", \t\n", 0);
          for (j = 0 ; protos[j] ; ++j)
            if (protos[j] && *protos[j])
              protohash_set(protos[j], gdk_color);
          g_strfreev(protos);
        }
      g_strfreev(colors_protocols);
    }

  if (!cycle_color_list)
    {
      /* the list of color available for unmapped protocols is empty, 
       * so we add a grey */
      gdk_color_parse ("#7f7f7f", &gdk_color);
      protohash_set(NULL, gdk_color);
    }
  else
    cycle_color_list = g_list_reverse(cycle_color_list); /* list was reversed */
  return TRUE;
}



/* compacts the array of colors/protocols mappings by collapsing identical
 * colors - frees the input array */
gchar **protohash_compact(gchar **colors)
{
   int i;
   gchar **compacted;
   GList *work;
   GList *el;

   /* constructs a list with unique colors. We use a list to maintain the
      fill order of the dialog. This is less surprising for the user. */
   work = NULL;
   for (i = 0; colors[i] ; ++i)
    {
      gchar **colors_protocols;

      colors_protocols = g_strsplit_set(colors[i], "; \t\n", 0);

      colors_protocols[1] = remove_spaces(colors_protocols[1]);
      
      for (el = g_list_first(work) ; el ; el = g_list_next(el) )
        {
          gchar **col=(gchar **)(el->data);
          if (!g_ascii_strcasecmp(col[0], colors_protocols[0]))
            {
              /* found same color, append protocol */
              gchar *old = col[1];
              if (colors_protocols[1] && *colors_protocols[1])
                {
                  if (old)
                    col[1] = g_strjoin(",", old, colors_protocols[1], NULL);
                  else
                    col[1] = g_strdup(colors_protocols[1]);
                  g_free(old);
                }
              break;
            }
        }

      if (el)
        g_strfreev(colors_protocols); /* found, free temporary */
      else
        {
          /* color not found, adds to list - no need to free here */
          work = g_list_prepend(work, colors_protocols); 
        }    
    }

  /* reverse list to match original order (with GList, prepend+reverse is more 
     efficient than append */
  work = g_list_reverse(work);
  
  /* now scans the list filling the protostring */
  compacted = malloc( sizeof(gchar *) * (g_list_length(work) + 1) );
  i = 0;
  for (el = g_list_first(work) ; el ; el = g_list_next(el) )
    {
      gchar **col=(gchar **)(el->data);
      compacted[i++] = g_strjoin(";", col[0], col[1], NULL);
      g_strfreev(col);
    }
  compacted[i] = NULL;
  g_list_free(work);
  g_strfreev(colors);
  return compacted;
}

gchar *remove_spaces(gchar *str)
{
  char *out = str;
  char *cur = str;
  if (str)
    {
      for (cur = str ; *cur ; ++cur)
        if ( !g_ascii_isspace((guchar)(*cur)))
          *out++ = *cur;
      *out = '\0';
    }
  return str;
}
