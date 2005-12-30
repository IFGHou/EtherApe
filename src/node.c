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
#include "node.h"
#include "protocols.h"
#include "capture.h"

static GTree *all_nodes = NULL;	/* Has all the nodes heard on the network */

/***************************************************************************
 *
 * utility functions
 *
 **************************************************************************/

/* Returns a timeval structure with the time difference between to
 * other timevals. result = a - b */
struct timeval
substract_times (struct timeval a, struct timeval b)
{
  struct timeval result;

  /* Perform the carry for the later subtraction by updating Y. */
  if (a.tv_usec < b.tv_usec)
    {
      int nsec = (b.tv_usec - a.tv_usec) / 1000000 + 1;
      b.tv_usec -= 1000000 * nsec;
      b.tv_sec += nsec;
    }
  if (a.tv_usec - b.tv_usec > 1000000)
    {
      int nsec = (a.tv_usec - b.tv_usec) / 1000000;
      b.tv_usec += 1000000 * nsec;
      b.tv_sec -= nsec;
    }

  result.tv_sec = a.tv_sec - b.tv_sec;
  result.tv_usec = a.tv_usec - b.tv_usec;

  return result;
}				/* substract_times */

/***************************************************************************
 *
 * traffic_stats_t implementation
 *
 **************************************************************************/
/* resets counters */
void traffic_stats_reset(traffic_stats_t *tf_stat)
{
  g_assert(tf_stat);
  tf_stat->average = 0;
  tf_stat->accumulated = 0;
  tf_stat->aver_accu = 0;
}

/***************************************************************************
 *
 * packet_stats_t implementation
 *
 **************************************************************************/

static void packet_stats_list_item_delete(gpointer data, gpointer dum)
{
  g_free(data); /* TODO: properly release memory */
}

/* initializes counters */
void packet_stats_initialize(packet_stats_t *pkt_stat)
{
  g_assert(pkt_stat);

  pkt_stat->pkt_list = NULL;
  pkt_stat->n_packets = 0;
  pkt_stat->last_time = now;

  traffic_stats_reset(&pkt_stat->stats);
  traffic_stats_reset(&pkt_stat->stats_in);
  traffic_stats_reset(&pkt_stat->stats_out);
}

/* releases memory */
void packet_stats_release(packet_stats_t *pkt_stat)
{
  g_assert(pkt_stat);

  /* release items and free list */
  g_list_foreach(pkt_stat->pkt_list, packet_stats_list_item_delete, NULL);
  g_list_free(pkt_stat->pkt_list);

  /* resets everything */
  packet_stats_initialize(pkt_stat);
}

/* adds a packet */
void 
packet_stats_add_packet(packet_stats_t *pkt_stat, 
                        packet_info_t *new_pkt, 
                        packet_direction dir)
{
  packet_list_item_t *newit;
  
  g_assert(pkt_stat);
  g_assert(new_pkt);

  newit = g_malloc( sizeof(packet_list_item_t) );

  /* increments refcount of packet */
  new_pkt->ref_count++;

  /* fills item, adding it to pkt list */
  newit->info = new_pkt;
  newit->direction = dir;

  pkt_stat->pkt_list = g_list_prepend (pkt_stat->pkt_list, newit);

  pkt_stat->n_packets++;
  pkt_stat->last_time = now;

  pkt_stat->stats.accumulated += new_pkt->size;
  pkt_stat->stats.aver_accu += new_pkt->size;

  switch (dir)
    {
    case INBOUND:
      pkt_stat->stats_in.accumulated += new_pkt->size;
      pkt_stat->stats_in.aver_accu += new_pkt->size;
      break;
    case OUTBOUND:
      pkt_stat->stats_out.accumulated += new_pkt->size;
      pkt_stat->stats_out.aver_accu += new_pkt->size;
      break;
    case EITHERBOUND:
      pkt_stat->stats_in.accumulated += new_pkt->size;
      pkt_stat->stats_in.aver_accu += new_pkt->size;
      pkt_stat->stats_out.accumulated += new_pkt->size;
      pkt_stat->stats_out.aver_accu += new_pkt->size;
      break;
    }

  /* note: averages are calculated later, by update_packet_list */
}

#if 0
void
packet_stats_purge_expired_packets(packet_stats_t *pkt_stat, double time_comparison)
{
  struct timeval result;
  GList *packet_l_e = NULL;	/* Packets is a list of packets.
				 * packet_l_e is always the latest (oldest)
				 * list element */

  packet_l_e = g_list_last (pkt_stat->pkt_list);
  while (packet_l_e)
  {
    packet_list_item_t * packet = (packet_list_item_t *)(packet_l_e->data);

    result = substract_times (now, packet->info->timestamp);
    if (!IS_OLDER (result, time_comparison) && (status != STOP))
      break; /* packet valid, subsequent packets are younger, no need to go further */

    /* packet expired, remove */
    protocol->aver_accu -= packet->info->size;
    if (!protocol->aver_accu)
      protocol->average = 0;

    /* packet expired, remove from list - gets the new check position 
     * if this packet is the first of the list, all the previous packets
     * should be already destroyed. We check that remove never returns a
     * NEXT packet */
    GList *next=packet_l_e->next;
    packet_l_e = packet_list_remove(packet_l_e);
    g_assert(packet_l_e == NULL || packet_l_e != next );
    protocol->n_packets--;
  }

  if (!packet_l_e)
    {
      /* removed all packets */
      protocol->n_packets = 0;
      protocol->packets=NULL;
    }
}
#endif

/***************************************************************************
 *
 * node_id_t implementation
 *
 **************************************************************************/


/* Comparison function used to order the (GTree *) nodes
 * and canvas_nodes heard on the network */
gint
node_id_compare (const node_id_t * na, const node_id_t * nb)
{
  const guint8 *ga = NULL;
  const guint8 *gb = NULL;
  int i = 0;

  g_assert (na != NULL);
  g_assert (nb != NULL);
  if (na->node_type < nb->node_type)
    return -1;
  else if (na->node_type > nb->node_type)
    return 1;

  /* same node type, compare */
  switch (na->node_type)
    {
    case ETHERNET:
      ga = na->addr.eth;
      gb = nb->addr.eth;
      i = sizeof(na->addr.eth);
      break;
    case FDDI:
      ga = na->addr.fddi;
      gb = nb->addr.fddi;
      i = sizeof(na->addr.fddi);
      break;
    case IEEE802:
      ga = na->addr.i802;
      gb = nb->addr.i802;
      i = sizeof(na->addr.i802);
      break;
    case IP:
      ga = na->addr.ip4;
      gb = nb->addr.ip4;
      i = sizeof(na->addr.ip4);
      break;
    case TCP:
      ga = na->addr.tcp4.host;
      gb = nb->addr.tcp4.host;
      i = sizeof(na->addr.tcp4.host)+sizeof(na->addr.tcp4.port); /* full struct size */
      break;
    default:
      g_error (_("Unsopported ape mode in node_id_compare"));
    }

  while (i)
    {
      if (ga[i] < gb[i])
	{
	  return -1;
	}
      else if (ga[i] > gb[i])
	return 1;
      --i;
    }

  return 0;
}				/* node_id_compare */

/***************************************************************************
 *
 * node_t implementation
 *
 **************************************************************************/
/* Allocates a new node structure */
node_t *
node_create(const node_id_t * node_id, const gchar *node_id_str)
{
  node_t *node = NULL;
  guint i = STACK_SIZE;

  node = g_malloc (sizeof (node_t));

  node->node_id = *node_id;

  node->name = NULL;
  node->numeric_name = NULL;

  node->name = g_string_new(node_id_str);
  node->numeric_name = g_string_new(node_id_str);

  node->average = node->average_in = node->average_out = 0;
  node->n_packets = 0;
  node->accumulated = node->accumulated_in = node->accumulated_out = 0;
  node->aver_accu = node->aver_accu_in = node->aver_accu_out = 0;

  node->packets = NULL;
  while (i + 1)
    {
      node->protocols[i] = NULL;
      node->main_prot[i] = NULL;
      i--;
    }


  return node;
}				/* create_node */

/* destroys a node */
void node_delete(node_t *node)
{
  GList *protocol_item = NULL;
  protocol_t *protocol_info = NULL;
  guint i = STACK_SIZE;

  if (!node)
    return; /* nothing to do */

  g_string_free (node->name, TRUE);
  node->name = NULL;
  g_string_free (node->numeric_name, TRUE);
  node->numeric_name = NULL;

  for (; i + 1; i--)
    if (node->main_prot[i])
      {
        g_free (node->main_prot[i]);
        node->main_prot[i] = NULL;
      }
  i = 0;
  while (i <= STACK_SIZE)
    {

      while (node->protocols[i])
        {
          protocol_item = node->protocols[i];
          protocol_info = protocol_item->data;

          if (!protocol_info->accumulated)
            {
              protocol_t_delete(protocol_info);
              node->protocols[i] =
                g_list_delete_link (node->protocols[i],
                                    protocol_item);
            }
        }
      i++;
    }

  while (node->packets)
    node->packets = packet_list_remove(node->packets);
  node->packets = NULL;

  g_free (node);
}

void
node_dump(node_t * node)
{

  GList *protocol_item = NULL, *name_item = NULL;
  protocol_t *protocol = NULL;
  name_t *name = NULL;
  guint i = 1;

  if (!node)
    return;

  if (node->name)
    g_my_info ("NODE %s INFORMATION", node->name->str);

  for (; i <= STACK_SIZE; i++)
    {
      if (node->protocols[i])
	{
	  g_my_info ("Protocol level %d information", i);
	  protocol_item = node->protocols[i];
	  while (protocol_item)
	    {
	      protocol = protocol_item->data;
	      g_my_info ("\tProtocol %s", protocol->name);
	      if ((name_item = protocol->node_names))
		{
		  GString *names = NULL;
		  while (name_item)
		    {
		      if (!names)
			names = g_string_new ("");
		      name = name_item->data;
		      names = g_string_append (names, name->name->str);
		      names = g_string_append (names, " ");
		      name_item = name_item->next;
		    }
		  g_my_info ("\t\tName: %s", names->str);
		  g_string_free (names, TRUE);
		}
	      protocol_item = protocol_item->next;
	    }
	}
    }

}

static void
node_subtract_packet_data(node_t *node, packet_list_item_t * packet)
{
  /* Substract this packet's length to the accumulated */
  node->aver_accu -= packet->info->size;
  if (packet->direction == INBOUND)
    node->aver_accu_in -= packet->info->size;
  else
    node->aver_accu_out -= packet->info->size;

  /* If it was the last packet in the queue, set
   * average to 0. It has to be done here because
   * otherwise average calculation in update_packet list 
   * requires some packets to exist */
  if (!node->aver_accu)
    node->average = 0;

  /* We remove protocol aggregate information */
  protocol_stack_sub_pkt(node->protocols, packet->info, TRUE);
}

/* Make sure this particular packet in a list of packets beloging to 
 * either o link or a node is young enough to be relevant. Else
 * remove it from the list */
/* TODO This whole function is a mess. I must take it to pieces
 * so that it is more readble and maintainable */
static void
node_purge_expired_packets(node_t *node)
{
  struct timeval result;
  double time_comparison;
  GList *packet_l_e = NULL;	/* Packets is a list of packets.
				 * packet_l_e is always the latest (oldest)
				 * list element */

  if (!node->packets)
    {
      node->n_packets = 0;
      return;
    }

  /* If this packet is older than the averaging time,
   * then it is removed, and the process continues.
   * Else, we are done. */
  if (pref.node_timeout_time)
    time_comparison = (pref.node_timeout_time > pref.averaging_time) ?
      pref.averaging_time : pref.node_timeout_time;
  else
    time_comparison = pref.averaging_time;

  packet_l_e = g_list_last (node->packets);
  while (packet_l_e)
    {
      packet_list_item_t * packet = (packet_list_item_t *)(packet_l_e->data);
      if (packet)
        {
          /* if the packet is old or capture is stopped, we purge */
          result = substract_times (now, packet->info->timestamp);
          if (!IS_OLDER (result, time_comparison) && (status != STOP))
            break; /* packet valid, subsequent packets are younger, no need to go further */

          /* expired packet, remove data */
          node_subtract_packet_data(node, packet);
        }

      /* packet null or expired, remove from list 
       * gets the new check position 
       * if this packet is the first of the list, all the previous packets
       * should be already destroyed. We check that remove never returns a
       * NEXT packet */
      GList *next=packet_l_e->next;
      packet_l_e = packet_list_remove(packet_l_e);
      g_assert(packet_l_e == NULL || packet_l_e != next );
      node->n_packets--;
        
    } /* end while */

  if (!packet_l_e)
    {
      /* removed all packets */
      node->n_packets = 0;
      node->packets=NULL;
    }
}

/* This function is called to discard packets from the list 
 * of packets beloging to a node or a link, and to calculate
 * the average traffic for that node or link */
gboolean
node_update(node_id_t * node_id, node_t *node, gpointer delete_list_ptr)
{
  struct timeval diff;

  g_assert(delete_list_ptr);

  if (node->packets)
    node_purge_expired_packets(node);

  if (node->packets)
    {
      guint i = STACK_SIZE;
      gdouble usecs_from_oldest;	/* usecs since the first valid packet */
      GList *packet_l_e;	/* Packets is a list of packets.
                                 * packet_l_e is always the latest (oldest)
                                 * list element */
      packet_list_item_t *packet;

      packet_l_e = g_list_last (node->packets);
      packet = (packet_list_item_t *) packet_l_e->data;

      diff = substract_times (now, packet->info->timestamp);
      usecs_from_oldest = diff.tv_sec * 1000000 + diff.tv_usec;

      /* average in bps, so we multiply by 8 and 1000000 */
      node->average = 8000000 * node->aver_accu / usecs_from_oldest;
      node->average_in = 8000000 * node->aver_accu_in / usecs_from_oldest;
      node->average_out =
        8000000 * node->aver_accu_out / usecs_from_oldest;
      while (i + 1)
        {
          if (node->main_prot[i])
            g_free (node->main_prot[i]);
          node->main_prot[i] = protocol_stack_find_most_used(node->protocols, i);
          i--;
        }
      update_node_names (node);

    }
  else
    {
      /* no packet remaining on node */
      diff = substract_times (now, node->last_time);

      /* Remove node if node is too old or if capture is stopped */
      if ((IS_OLDER (diff, pref.node_timeout_time)
           && pref.node_timeout_time) || (status == STOP))
        {
          GList **delete_list = (GList **)delete_list_ptr;

          g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                 _("Queuing node '%s' for remove"),node->name->str);

          /* First thing we do is delete the node from the list of new_nodes,
           * if it's there */
          new_nodes_remove(node);

          /* adds current to list of nodes to be delete */
          *delete_list = g_list_append( *delete_list, node_id);
        }
      else
        {
          /* The packet list structure has already been freed in
           * check_packets */
          node->packets = NULL;
          node->average = node->average_in = node->average_out = 0.0;
        }
    }

  return FALSE;
}				/* node_update */

/***************************************************************************
 *
 * new nodes methods
 *
 **************************************************************************/

static GList *new_nodes = NULL;	/* List that contains every new node not yet
				 * acknowledged by the main app with
				 * new_nodes_pop */

void new_nodes_clear(void)
{
  g_list_free (new_nodes);
  new_nodes = NULL;
}

void new_nodes_add(node_t *node)
{
  new_nodes = g_list_prepend (new_nodes, node);
}

void new_nodes_remove(node_t *node)
{
  new_nodes = g_list_remove (new_nodes, node);
}

/* Returns a node from the list of new nodes or NULL if there are no more 
 * new nodes */
node_t *
new_nodes_pop(void)
{
  node_t *node = NULL;
  GList *old_item = NULL;

  if (!new_nodes)
    return NULL;

  node = new_nodes->data;
  old_item = new_nodes;

  /* We make sure now that the node hasn't been deleted since */
  /* TODO Sometimes when I get here I have a node, but a null
   * node->node_id. What gives? */
  while (node && !nodes_catalog_find(&node->node_id))
    {
      g_my_debug
	("Already deleted node in list of new nodes, in new_nodes_pop");

      /* Remove this node from the list of new nodes */
      new_nodes = g_list_remove_link (new_nodes, new_nodes);
      g_list_free_1 (old_item);
      if (new_nodes)
	node = new_nodes->data;
      else
	node = NULL;
      old_item = new_nodes;
    }

  if (!new_nodes)
    return NULL;

  /* Remove this node from the list of new nodes */
  new_nodes = g_list_remove_link (new_nodes, new_nodes);
  g_list_free_1 (old_item);

  return node;
}				/* ape_get_new_node */

/***************************************************************************
 *
 * nodes catalog implementation
 *
 **************************************************************************/

/* nodes catalog compare function */
static gint nodes_catalog_compare(gconstpointer a, gconstpointer b, gpointer dummy)
{
  return node_id_compare( (const node_id_t *)a,  (const node_id_t *)b);
}

/* initializes the catalog */
void nodes_catalog_open(void)
{
  g_assert(!all_nodes);
  all_nodes = g_tree_new_full(nodes_catalog_compare, NULL, NULL, 
                              (GDestroyNotify)node_delete);
}

/* closes the catalog, releasing all nodes */
void nodes_catalog_close(void)
{
  if (all_nodes)
  {
    g_tree_destroy(all_nodes);
    all_nodes = NULL;
  }
}

/* insert a new node */
void nodes_catalog_insert(node_t *new_node)
{
  g_assert(all_nodes);
  g_assert(new_node);
 
  g_tree_insert (all_nodes, &new_node->node_id, new_node);
}

/* removes AND DESTROYS the named node from catalog */
void nodes_catalog_remove(const node_id_t *key)
{
  g_assert(all_nodes);
  g_assert(key);

  g_tree_remove (all_nodes, key);
}

/* finds a node */
node_t *nodes_catalog_find(const node_id_t *key)
{
  g_assert(all_nodes);
  g_assert(key);

  return g_tree_lookup (all_nodes, key);
}

/* returns the current number of nodes in catalog */
gint nodes_catalog_size(void)
{
  g_assert(all_nodes);

  return g_tree_nnodes (all_nodes);
}

 /* calls the func for every node */
void nodes_catalog_foreach(GTraverseFunc func, gpointer data)
{
  g_assert(all_nodes);

  return g_tree_foreach(all_nodes, func, data);
}

/* gfunc called by g_list_foreach to remove the node */
static void
gfunc_remove_node(gpointer data, gpointer user_data)
{
  nodes_catalog_remove( (const node_id_t *) data);
}

/* Calls update_node for every node. This is actually a function that
 shouldn't be called often, because it might take a very long time 
 to complete */
void
nodes_catalog_update_all(void)
{
  GList *delete_list = NULL;

  /* we can't delete nodes while traversing the catalog, so while updating we 
   * fill a list with the node_id's to remove */
  nodes_catalog_foreach((GTraverseFunc) node_update, &delete_list);

  /* after, remove all nodes on the list from catalog 
   * WARNING: after this call, the list items are also destroyed */
  g_list_foreach(delete_list, gfunc_remove_node, NULL);
  
  /* free the list - list items are already destroyed */
  g_list_free(delete_list);

  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
         _("Updated nodes. Active nodes %d"), nodes_catalog_size());
}				/* update_nodes */

/***************************************************************************
 *
 * name_t implementation
 *
 **************************************************************************/
name_t * node_name_create(const node_id_t *node_id)
{
  name_t *name;
  
  g_assert(node_id);
  name = g_malloc (sizeof (name_t));
  name->node_id = *node_id;
  name->n_packets = 0;
  name->accumulated = 0;
  name->numeric_name = NULL;
  name->name = NULL;
  return name;
}

void node_name_delete(name_t * name)
{
  if (name)
  {
    g_string_free (name->name, TRUE);
    g_string_free (name->numeric_name, TRUE);
    g_free (name);
  }
}

void node_name_assign(name_t * name, const gchar *nm, const gchar *num_nm, 
                 gboolean slv, gdouble sz)
{
  g_assert(name);
  if (!name->numeric_name)
    name->numeric_name = g_string_new (num_nm);
  else
    g_string_assign (name->numeric_name, num_nm);

  if (!name->name)
    name->name = g_string_new (nm);
  else
    g_string_assign (name->name, nm);
  
  name->solved = slv;
  name->n_packets++;
  name->accumulated += sz;
}

/* compares by node id */
gint 
node_name_id_compare(const name_t *a, const name_t *b)
{
  g_assert (a != NULL);
  g_assert (b != NULL);
  return node_id_compare(&a->node_id, &b->node_id);
}

/* Comparison function to sort protocols by their accumulated traffic */
gint
node_name_freq_compare (gconstpointer a, gconstpointer b)
{
  const name_t *name_a, *name_b;

  g_assert (a != NULL);
  g_assert (b != NULL);

  name_a = (const name_t *) a;
  name_b = (const name_t *) b;

  if (name_a->accumulated > name_b->accumulated)
    return -1;
  if (name_a->accumulated < name_b->accumulated)
    return 1;
  return 0;
}
