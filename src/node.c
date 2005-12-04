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

static GTree *all_nodes = NULL;		/* Has all the nodes heard on the network */

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
  g_free(data);
}

/* initializes counters */
void packet_stats_initialize(packet_stats_t *pkt_stat)
{
  g_assert(pkt_stat);

  pkt_stat->cur_pkt_list = NULL;
  pkt_stat->n_packets = 0;
  pkt_stat->last_time = now;

  traffic_stats_reset(&pkt_stat->stats);
}

/* releases memory */
void packet_stats_release(packet_stats_t *pkt_stat)
{
  g_assert(pkt_stat);

  /* release items and free list */
  g_list_foreach(pkt_stat->cur_pkt_list, packet_stats_list_item_delete, NULL);
  g_list_free(pkt_stat->cur_pkt_list);

  /* resets everything */
  packet_stats_initialize(pkt_stat);
}

/* adds a packet */
void packet_stats_add_packet(packet_stats_t *pkt_stat, packet_info_t *new_pkt)
{
  g_assert(pkt_stat);
  g_assert(new_pkt);

  new_pkt->ref_count++;
  
  pkt_stat->cur_pkt_list = g_list_prepend (pkt_stat->cur_pkt_list, new_pkt);

  pkt_stat->n_packets++;
  pkt_stat->last_time = now;

  pkt_stat->stats.accumulated += new_pkt->size;
  pkt_stat->stats.aver_accu += new_pkt->size;

  /* note: averages are calculated later, by update_packet_list */
}

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

}				/* dump_node_info */

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

  g_free (node);
}

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
}				/* names_freq_compare */


/***************************************************************************
 *
 * protocol_t implementation
 *
 **************************************************************************/
protocol_t *protocol_t_create(const gchar *protocol_name)
{
  protocol_t *pr = NULL;

  pr = g_malloc (sizeof (protocol_t));
  pr->name = g_strdup (protocol_name);
  pr->accumulated = 0;
  pr->aver_accu = 0;
  pr->average = 0;
  pr->n_packets = 0;
  pr->node_names = NULL;
  pr->packets = NULL;
  pr->color.pixel = 0;
  pr->color.red = 0;
  pr->color.green = 0;
  pr->color.blue = 0;
  pr->last_heard.tv_sec = 0;
  pr->last_heard.tv_usec = 0;

  return pr;
}

void protocol_t_delete(protocol_t *prot)
{
  g_assert(prot);

  g_free (prot->name);
  prot->name = NULL;

  while (prot->node_names)
    {
      GList *name_item = prot->node_names;
      name_t *name = name_item->data;
      node_name_delete(name);
      prot->node_names =
        g_list_delete_link (prot->node_names,
                            name_item);
    }

  g_free (prot);
}
