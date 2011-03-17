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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "appdata.h"
#include "node.h"
#include "capture.h"
#include "preferences.h"
#include "util.h"

typedef struct
{
  const gchar *protocol;
  gboolean must_resolve; /* true if a name must be resolved to be used */
  
} name_decode_t;

static name_decode_t ethernet_sequence[] = { 
    {"ETH_II", TRUE},
    {"802.2",TRUE},
    {"803.3",TRUE},
    {"FDDI", TRUE},
    {"IEEE802", TRUE},
    {"NETBIOS-DGM",FALSE},
    {"NETBIOS-SSN",FALSE}, 
    {"IP",FALSE},
    {"IPV6",FALSE},
    {"IPX-SAP",FALSE},
    {"ARP",FALSE},
    {"ETH_II",FALSE},
    {"802.2",FALSE},
    {"802.3",FALSE},
    {"FDDI",FALSE},
    {"IEEE802",FALSE},
    {NULL, FALSE}
};

static name_decode_t ip_sequence[] = { 
    {"NETBIOS-DGM",FALSE},
    {"NETBIOS-SSN",FALSE}, 
    {"IP",FALSE},
    {"IPV6",FALSE},
    {NULL, FALSE}
};

static name_decode_t tcp_sequence[] = { 
    {"TCP",FALSE},
    {NULL, FALSE}
};

static GTree *all_nodes = NULL;	/* Has all the nodes heard on the network */
static gint nodes_num = 0;      /* nodes counter */

/***************************************************************************
 *
 * node_t implementation
 *
 **************************************************************************/
static void node_name_update(node_t * node);
static void set_node_name (node_t * node, const name_decode_t *sequence);

/* Allocates a new node structure */
node_t *
node_create(const node_id_t * node_id)
{
  node_t *node;
  gchar *name;
  guint i;

  g_assert(node_id);
  
  node = g_malloc (sizeof (node_t));
  g_assert(node);

  node->node_id = *node_id;

  name = node_id_str(node_id);
  node->name = g_string_new(name);
  node->numeric_name = g_string_new(name);
  g_free(name);

  for (i = 0 ; i <= STACK_SIZE; ++i)
      node->main_prot[i] = NULL;

  traffic_stats_init(&node->node_stats);

  ++nodes_num;

  if (INFO_ENABLED)
    {
      gchar *msg = node_id_dump(&node->node_id);
      g_my_info(_("New node: %s. Number of nodes %d"),
                msg, nodes_num);
      g_free(msg);
    }
  return node;
}				/* create_node */

/* destroys a node */
void node_delete(node_t *node)
{
  guint i;

  if (!node)
    return; /* nothing to do */

  if (node->name)
    g_string_free (node->name, TRUE);
  node->name = NULL;
  if (node->numeric_name)
    g_string_free (node->numeric_name, TRUE);
  node->numeric_name = NULL;

  for (i = 0; i <= STACK_SIZE; ++i)
    if (node->main_prot[i])
      {
        g_free (node->main_prot[i]);
        node->main_prot[i] = NULL;
      }

  traffic_stats_reset(&node->node_stats);

  if (INFO_ENABLED)
    {
      gchar *msg = node_id_dump(&node->node_id);
      g_my_info(_("delete node: %s. Number of nodes %d"),
                msg, nodes_num-1);
      g_free(msg);
    }
  g_free (node);
  --nodes_num;
}

gint node_count(void)
{
  return nodes_num;
}

gchar *node_dump(const node_t * node)
{
  gchar *msg;
  gchar *msg_id;
  gchar *msg_stats;
  gchar *msg_mprot;
  guint i;

  if (!node)
    return g_strdup("node_t NULL");

  msg_id = node_id_dump(&node->node_id);
  msg_stats = traffic_stats_dump(&node->node_stats);

  msg_mprot=g_strdup_printf("top: [%s], stack:", 
                            (node->main_prot[0]) ? 
                            node->main_prot[0] : "-none-");
  
  for (i = 1; i <= STACK_SIZE; i++)
    {
      gchar *tmp = msg_mprot;
      msg_mprot = g_strdup_printf("%s %d:>%s<", msg_mprot, i, 
                           (node->main_prot[i]) ? 
                           node->main_prot[i] : "-none-");
      g_free(tmp);
    }

  msg = g_strdup_printf("id: %s, name: %s, numeric_name: %s, main_prot: [%s], "
                        "stats [%s]",
                        msg_id, node->name->str, node->numeric_name->str,
                        msg_mprot, msg_stats);
  g_free(msg_id);
  g_free(msg_stats);
  g_free(msg_mprot);

  return msg;
}

/* returns a newly allocated string with an xml dump of node 
 * N.B.
 * ignores main_prot array (protocol names), because is already dumped
 * with protostack stats */
gchar *node_xml(const node_t * node)
{
  gchar *msg;
  gchar *msg_id;
  gchar *msg_stats;
  gchar *msg_resolved;
  gchar *msg_numeric;

  if (!node)
    return xmltag("node", "");

  msg_id = node_id_xml(&node->node_id);
  msg_stats = traffic_stats_xml(&node->node_stats);
  msg_resolved = xmltag("resolved_name", "%s", node->name->str);
  msg_numeric = xmltag("numeric_name", "%s", node->numeric_name->str);
  
  msg = xmltag("node","\n<name>\n%s%s%s</name>\n%s",
                        msg_id, msg_resolved, msg_numeric,
                        msg_stats);
  g_free(msg_id);
  g_free(msg_stats);
  g_free(msg_resolved);
  g_free(msg_numeric);

  return msg;
}

/* This function is called to discard packets from the list 
 * of packets beloging to a node or a link, and to calculate
 * the average traffic for that node or link */
gboolean
node_update(node_id_t * node_id, node_t *node, gpointer delete_list_ptr)
{
  double diffms;

  g_assert(delete_list_ptr);

  if (traffic_stats_update(&node->node_stats, pref.averaging_time, 
                            pref.proto_node_timeout_time))
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
      node_name_update (node);
    }
  else
    {
      /* no packets remaining on node - if node expiration active, see if the
       * node is expired */
      if (pref.node_timeout_time)
        {
          diffms = substract_times_ms(&appdata.now, &node->node_stats.stats.last_time);
          if (diffms >= pref.node_timeout_time)
            {
              /* node expired, remove */
              GList **delete_list = (GList **)delete_list_ptr;

              if (DEBUG_ENABLED)
                {
                  gchar *msg = node_id_dump(&node->node_id);
                  g_my_debug(_("Queuing node '%s' for remove"), msg);
                  g_free(msg);
                }
    
              /* First thing we do is delete the node from the list of new_nodes,
               * if it's there */
              new_nodes_remove(node);
    
              /* adds current to list of nodes to be delete */
              *delete_list = g_list_prepend( *delete_list, node_id);
            }
        }
    }

  return FALSE;
}

/* Sets the node->name and node->numeric_name to the most used of 
 * the default name for the current mode */
static void
node_name_update(node_t * node)
{
  GList *protocol_item;
  protocol_t *protocol;
  guint i = STACK_SIZE;

  /* for each level and each protocol at that level, sort names by traffic,
   * placing the busiest at front */
  for (i = 0 ; i <= STACK_SIZE ; ++i)
    {
      for ( protocol_item = node->node_stats.stats_protos.protostack[i]; 
            protocol_item; 
            protocol_item = protocol_item->next)
        {
          protocol = (protocol_t *) (protocol_item->data);
          protocol->node_names
            = g_list_sort (protocol->node_names, node_name_freq_compare);
        }
    }

  switch (appdata.mode)
    {
    case LINK6:
      set_node_name (node, ethernet_sequence);
      break;
    case IP:
      set_node_name (node, ip_sequence);
      break;
    case TCP:
      set_node_name (node, tcp_sequence);
      break;
    default:
      break;
    }
}				/* update_node_names */


static void
set_node_name (node_t * node, const name_decode_t *sequence)
{
  const name_decode_t *iter;
  gboolean cont;

  if (DEBUG_ENABLED)
    {
      gchar *msgid = node_id_dump(&node->node_id);
      g_my_debug("set_node_name: node id [%s]", msgid);
      g_free(msgid);
    }

  cont = TRUE;
  
  for (iter = sequence; iter->protocol && cont; ++iter)
    {
      const GList *name_item;
      const name_t *name;
      const protocol_t *protocol;
      guint j;

      /* We don't do level 0, which has the topmost prot */
      for (j = STACK_SIZE; j && cont; j--)
	{
          g_my_debug(" Searching %s at stack level %d", iter->protocol, j);
	  protocol = protocol_stack_find(&node->node_stats.stats_protos, 
                                         j, iter->protocol);
	  if (!protocol || strcmp (protocol->name, iter->protocol))
            continue;

          /* protocol found, we take the first name (i.e. the most used one) */
          name_item = protocol->node_names;
          if (!name_item)
            {
              g_my_debug("  found protocol without names, ignore");
              continue;
            }

          name = (const name_t *) (name_item->data);
          if (DEBUG_ENABLED)
            {
              gchar *msgname = node_name_dump(name);
              if (name->res_name || !iter->must_resolve || !pref.name_res)
                g_my_debug("  found protocol with name [%s]", msgname);
              else
                g_my_debug("  found protocol with UNRESOLVED name [%s], ignored", 
                           msgname);
              g_free(msgname);
            }

          /* If we require this protocol to be solved and it's not,
           * the we have to go on */
          if (name->res_name || !iter->must_resolve || !pref.name_res)
            {
              if (name->res_name)
                {
                  if (!node->name || 
                      strcmp (node->name->str, name->res_name->str))
                    {
                      g_my_debug ("  set node name from %s to %s",
                                  (node->name) ? node->name->str : "<none>",
                                  name->res_name->str);
                      g_string_assign (node->name, name->res_name->str);
                    }
                }
              if (!node->numeric_name || 
                  strcmp(node->numeric_name->str, name->numeric_name->str))
                  {
                    g_my_debug ("  set node numeric_name from %s to %s",
                                (node->numeric_name) ? 
                                      node->numeric_name->str : "none",
                                name->numeric_name->str);
                    g_string_assign (node->numeric_name,name->numeric_name->str);
                  }
              cont = FALSE;
            }
	}
    }
  g_my_debug("set_node_name END --");
}				/* set_node_name */


/***************************************************************************
 *
 * new nodes methods
 *
 **************************************************************************/

static GList *new_nodes = NULL;	/* List that contains ptrs to every new node 
				 * not yet acknowledged by the main app with
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
node_t *nodes_catalog_new(const node_id_t *node_id)
{
  node_t *new_node;
  
  g_assert(all_nodes);
  g_assert(node_id);

  new_node = node_create(node_id);
  if (!new_node)
    return NULL;
  
  g_tree_insert (all_nodes, &new_node->node_id, new_node);
  return new_node;
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
  g_assert(key);
  if (!all_nodes)
    return NULL;

  return g_tree_lookup (all_nodes, key);
}

/* returns the current number of nodes in catalog */
gint nodes_catalog_size(void)
{
  if (!all_nodes)
    return 0;

  return g_tree_nnodes (all_nodes);
}

 /* calls the func for every node */
void nodes_catalog_foreach(GTraverseFunc func, gpointer data)
{
  if (!all_nodes)
    return;

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

  if (!all_nodes)
    return;

  /* we can't delete nodes while traversing the catalog, so while updating we 
   * fill a list with the node_id's to remove */
  nodes_catalog_foreach((GTraverseFunc) node_update, &delete_list);

  /* after, remove all nodes on the list from catalog 
   * WARNING: after this call, the list items are also destroyed */
  g_list_foreach(delete_list, gfunc_remove_node, NULL);
  
  /* free the list - list items are already destroyed */
  g_list_free(delete_list);

  g_my_debug(_("Updated nodes. Active nodes %d"), nodes_catalog_size());
}				/* update_nodes */

static gboolean node_dump_tvs(gpointer key, gpointer value, gpointer data)
{
  gchar *msg_node;
  gchar *tmp;
  gchar **msg = (gchar **)data;
  const node_t *node = (const node_t *)value;
  
  msg_node = node_dump(node);
  tmp = *msg;
  *msg = g_strdup_printf("%snode %p:\n%s\n", tmp, node, msg_node);
  g_free(tmp);
  g_free(msg_node);
  return FALSE;
}

gchar *nodes_catalog_dump(void)
{
  gchar *msg;

  msg = g_strdup("");
  nodes_catalog_foreach(node_dump_tvs, &msg);
  return msg;
}

static gboolean node_xml_tvs(gpointer key, gpointer value, gpointer data)
{
  gchar *msg_node;
  gchar *tmp;
  gchar **msg = (gchar **)data;
  const node_t *node = (const node_t *)value;
  
  msg_node = node_xml(node);
  tmp = *msg;
  *msg = g_strdup_printf("%s%s", tmp, msg_node);
  g_free(tmp);
  g_free(msg_node);
  return FALSE;
}

gchar *nodes_catalog_xml(void)
{
  gchar *msg;
  gchar *xml;
  
  msg = g_strdup("");
  nodes_catalog_foreach(node_xml_tvs, &msg);
  xml = xmltag("nodes","\n%s", msg);
  g_free(msg);
  return xml;
}
