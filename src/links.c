/* EtherApe
 * Copyright (C) 2001 Juan Toledo, 2005 Riccardo Ghetta
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

#include "globals.h"
#include "capture.h"
#include "links.h"

static GTree *all_links = NULL;			/* Has all links heard on the net */

/***************************************************************************
 *
 * link_id_t implementation
 *
 **************************************************************************/
/* Comparison function used to order the (GTree *) links
 * and canvas_links heard on the network */
gint
link_id_compare (const link_id_t *a, const link_id_t *b)
{
  int i;
  g_return_val_if_fail (a != NULL, 1);	/* This shouldn't happen.
					 * We arbitrarily passing 1 to
					 * the comparison */
  g_return_val_if_fail (b != NULL, 1);

  i = node_id_compare( &a->src, &b->src);
  if (i != 0)
    return i;
  
  return node_id_compare( &a->dst, &b->dst);

}				/* link_id_compare */

/***************************************************************************
 *
 * link_t implementation
 *
 **************************************************************************/
static gint update_link(link_id_t * link_id, link_t * link, gpointer delete_list_ptr);

/* creates a new link object */
link_t *link_create(const link_id_t *link_id)
{
  link_t *link;
  guint i = STACK_SIZE;
  node_t *node;

  link = g_malloc (sizeof (link_t));

  link->link_id = *link_id;
  link->average = 0;
  link->n_packets = 0;
  link->accumulated = 0;
  link->link_packets = NULL;
  link->src_name = NULL;
  link->dst_name = NULL;
  link->last_time = now;
  while (i + 1)
    {
      link->link_protocols[i] = NULL;
      link->main_prot[i] = NULL;
      i--;
    }
  node = nodes_catalog_find(&link_id->src);
  g_assert(node);
  link->src_name = g_strdup (node->name->str);

  node = nodes_catalog_find(&link_id->dst);
  g_assert(node);
  link->dst_name = g_strdup (node->name->str);

  return link;
}

/* destroys a link, releasing memory */
void link_delete(link_t *link)
{
  guint i;

  g_assert(link);

  for (i = STACK_SIZE; i + 1; i--)
    if (link->main_prot[i])
      {
        g_free (link->main_prot[i]);
        link->main_prot[i] = NULL;
      }
  
  while (link->link_packets)
    link->link_packets = packet_list_remove(link->link_packets);
  link->link_packets = NULL;
      
  g_free (link->src_name);
  link->src_name = NULL;
  g_free (link->dst_name);
  link->dst_name = NULL;
  g_free (link);
}

/* gfunc called by g_list_foreach to remove a link */
static void
gfunc_remove_link(gpointer data, gpointer user_data)
{
  links_catalog_remove( (const link_id_t *) data);
}

static void
link_purge_expired_packets(link_t * link)
{
  GList *packet_l_e = NULL;	/* Packets is a list of packets.
				 * packet_l_e is always the latest (oldest)
				 * list element */
  if (!link->link_packets)
    {
      link->n_packets = 0;
      return;
    }
  
  packet_l_e = g_list_last (link->link_packets);
  while (packet_l_e)
  {
    packet_t * packet = (packet_t *)(packet_l_e->data);
    if (check_packet(packet, link, LINK)) 
      break; /* packet valid, subsequent packets are younger, no need to go further */
    else
      {
        /* packet expired, remove from list - gets the new check position 
         * if this packet is the first of the list, all the previous packets
         * should be already destroyed. We check that remove never returns a
         * NEXT packet */
        GList *next=packet_l_e->next;
        packet_l_e = packet_list_remove(packet_l_e);
        g_assert(packet_l_e == NULL || packet_l_e != next );
        link->n_packets--;
        if (!link->link_packets)
          link->n_packets = 0;
      }
  } /* end while */

}

static gint
update_link(link_id_t* link_id, link_t * link, gpointer delete_list_ptr)
{
  struct timeval diff;

  g_assert(delete_list_ptr);

  if (link->link_packets)
    link_purge_expired_packets(link);

  /* If there still is relevant packets, then calculate average
   * traffic and update names*/
  if (link->n_packets)
    {
      struct timeval difference;
      guint i = STACK_SIZE;
      gdouble usecs_from_oldest;	/* usecs since the first valid packet */
      GList *packet_l_e;	/* Packets is a list of packets.
                                     * packet_l_e is always the latest (oldest)
                                     * list element */
      packet_t *packet;

      packet_l_e = g_list_last (link->link_packets);
      packet = (packet_t *) packet_l_e->data;

      difference = substract_times (now, packet->info.timestamp);
      usecs_from_oldest = difference.tv_sec * 1000000 + difference.tv_usec;

      /* average in bps, so we multiply by 8 and 1000000 */
      link->average = 8000000 * link->accumulated / usecs_from_oldest;
      /* We look for the most used protocol for this link */
      while (i + 1)
        {
          if (link->main_prot[i])
            g_free (link->main_prot[i]);
          link->main_prot[i]
            = get_main_prot (link->link_protocols, i);
          i--;
        }

    }
  else
    {
      diff = substract_times (now, link->last_time);

      /* Remove link if it is too old or if capture is stopped */
      if ((IS_OLDER (diff, pref.link_timeout_time)
           && pref.link_timeout_time) || (status == STOP))
        {
          GList **delete_list = (GList **)delete_list_ptr;
    
          g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                 _("Queuing link for remove"));

          /* adds current to list of links to delete */
          *delete_list = g_list_append( *delete_list, link_id);

        }
      else
        {
          /* link not expired */
          guint i = STACK_SIZE;
          
          /* The packet list structure has already been freed in
           * check_packet */
          link->link_packets = NULL;
          link->accumulated = 0;
          while (i + 1)
            {
              link->link_protocols[i] = NULL;
              i--;
            }
        }
    }

  return FALSE;
}


/***************************************************************************
 *
 * links catalog implementation
 *
 **************************************************************************/

/* links catalog compare function */
static gint links_catalog_compare(gconstpointer a, gconstpointer b, gpointer dummy)
{
  return link_id_compare( (const link_id_t *)a,  (const link_id_t *)b);
}

/* initializes the catalog */
void links_catalog_open(void)
{
  g_assert(!all_links);
  all_links = g_tree_new_full(links_catalog_compare, NULL, NULL, 
                              (GDestroyNotify)link_delete);
}

/* closes the catalog, releasing all links */
void links_catalog_close(void)
{
  if (all_links)
  {
    g_tree_destroy(all_links);
    all_links = NULL;
  }
}

/* insert a new link */
void links_catalog_insert(link_t *new_link)
{
  g_assert(all_links);
  g_assert(new_link);
 
  g_tree_insert (all_links, &new_link->link_id, new_link);

  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	 _("New link: %s-%s. Number of links %d"),
	 new_link->src_name, new_link->dst_name, links_catalog_size());
}

/* removes AND DESTROYS the named link from catalog */
void links_catalog_remove(const link_id_t *key)
{
  g_assert(all_links);
  g_assert(key);

  g_tree_remove (all_links, key);
}

/* finds a link */
link_t *links_catalog_find(const link_id_t *key)
{
  g_assert(all_links);
  g_assert(key);

  return g_tree_lookup (all_links, key);
}

/* finds a link, creating one if necessary */
link_t *links_catalog_find_create(const link_id_t *key)
{
  link_t *link;
  g_assert(all_links);
  g_assert(key);

  link = links_catalog_find(key);
  if (!link)
  {
    link = link_create(key);
    links_catalog_insert(link);
  }
  return link;
}

/* returns the current number of links in catalog */
gint links_catalog_size(void)
{
  g_assert(all_links);

  return g_tree_nnodes (all_links);
}

 /* calls the func for every link */
void links_catalog_foreach(GTraverseFunc func, gpointer data)
{
  g_assert(all_links);

  return g_tree_foreach(all_links, func, data);
}

/* Calls update_link for every link. This is actually a function that
 shouldn't be called often, because it might take a very long time 
 to complete */
void
links_catalog_update_all(void)
{
  GList *delete_list = NULL;

  /* we can't delete links while traversing the catalog, so while updating links
   * we fill a list with the expired link_id's */
  links_catalog_foreach((GTraverseFunc) update_link, &delete_list);

  /* after, remove all links on the list from catalog 
   * WARNING: after this call, the list items are also destroyed */
  g_list_foreach(delete_list, gfunc_remove_link, NULL);
  
  /* free the list - list items are already destroyed */
  g_list_free(delete_list);

  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
         _("Updated links. Active links %d"), links_catalog_size());
}
