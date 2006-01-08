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
#include "capture.h"

static GTree *all_nodes = NULL;	/* Has all the nodes heard on the network */

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
  node_t *node;
  guint i = STACK_SIZE;

  node = g_malloc (sizeof (node_t));

  node->node_id = *node_id;

  node->name = NULL;
  node->numeric_name = NULL;

  node->name = g_string_new(node_id_str);
  node->numeric_name = g_string_new(node_id_str);

  while (i + 1)
    {
      node->main_prot[i] = NULL;
      i--;
    }

  traffic_stats_init(&node->node_stats);

  return node;
}				/* create_node */

/* destroys a node */
void node_delete(node_t *node)
{
  guint i;

  if (!node)
    return; /* nothing to do */

  g_string_free (node->name, TRUE);
  node->name = NULL;
  g_string_free (node->numeric_name, TRUE);
  node->numeric_name = NULL;

  for (i = 0; i <= STACK_SIZE; ++i)
    if (node->main_prot[i])
      {
        g_free (node->main_prot[i]);
        node->main_prot[i] = NULL;
      }

  traffic_stats_reset(&node->node_stats);

  g_free (node);
}

void
node_dump(const node_t * node)
{
  GList *protocol_item = NULL, *name_item = NULL;
  protocol_t *protocol = NULL;
  name_t *name = NULL;
  guint i;

  if (!node)
    return;

  if (node->name)
    g_my_info ("NODE %s INFORMATION", node->name->str);

  /* we skip level 0 */
  for (i = 1; i <= STACK_SIZE; i++)
    {
      protocol_item = node->node_stats.stats_protos.protostack[i];
      if (protocol_item)
        g_my_info ("Protocol level %d information", i);
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

/* This function is called to discard packets from the list 
 * of packets beloging to a node or a link, and to calculate
 * the average traffic for that node or link */
gboolean
node_update(node_id_t * node_id, node_t *node, gpointer delete_list_ptr)
{
  double pkt_expire_time, proto_expire_time;
  struct timeval diff;

  g_assert(delete_list_ptr);

  if (pref.node_timeout_time && pref.node_timeout_time < pref.averaging_time)
    pkt_expire_time = pref.node_timeout_time;
  else
    pkt_expire_time = pref.averaging_time;

  if (pref.proto_node_timeout_time && pref.proto_node_timeout_time < pref.node_timeout_time)
    proto_expire_time = pref.proto_node_timeout_time;
  else
    proto_expire_time = pref.node_timeout_time;

  if (traffic_stats_update(&node->node_stats, pkt_expire_time, proto_expire_time))
    {
      /* packet(s) active, update the most used protocols for this link */
      guint i = STACK_SIZE;
      while (i + 1)
        {
          if (node->main_prot[i])
            g_free (node->main_prot[i]);
          node->main_prot[i] = protocol_stack_sort_most_used(&node->node_stats.stats_protos, i);
          i--;
        }
      update_node_names (node);
    }
  else
    {
      /* no packet remaining on node */
      diff = substract_times (now, node->node_stats.last_time);

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
          *delete_list = g_list_prepend( *delete_list, node_id);
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
