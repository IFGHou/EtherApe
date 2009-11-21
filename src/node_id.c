/* EtherApe
 * Copyright (C) 2001-2009 Juan Toledo, Riccardo Ghetta
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
#include "node_id.h"
#include "util.h"

/***************************************************************************
 *
 * node_id_t implementation
 *
 **************************************************************************/

void node_id_clear(node_id_t *a)
{
  memset(&a->addr, 0, sizeof(node_addr_t));
  a->node_type = APEMODE_DEFAULT;
}

/* Comparison function used to order the (GTree *) nodes
 * and canvas_nodes heard on the network */
gint
node_id_compare (const node_id_t * na, const node_id_t * nb)
{
  const guint8 *ga;
  const guint8 *gb;
  int i;

  g_assert (na != NULL);
  g_assert (nb != NULL);
  if (na->node_type < nb->node_type)
    return -1;
  else if (na->node_type > nb->node_type)
    return 1;

  /* same node type, compare */
  switch (na->node_type)
    {
    case APEMODE_DEFAULT:
      return 0; /* default has only one value */
    case LINK6:
      ga = na->addr.eth;
      gb = nb->addr.eth;
      i = sizeof(na->addr.eth);
      break;
    case IP:
      ga = na->addr.ip4;
      gb = nb->addr.ip4;
      i = sizeof(na->addr.ip4);
      break;
    case TCP:
      ga = na->addr.tcp4.host;
      gb = nb->addr.tcp4.host;
      i = sizeof(na->addr.tcp4.host)+sizeof(na->addr.tcp4.port); 
      break;
    default:
      g_error (_("Unsopported ape mode in node_id_compare"));
      ga = na->addr.eth;
      gb = nb->addr.eth;
      i = sizeof(node_addr_t);
      break;
    }

  i = memcmp(ga, gb, i);
  if (i>1)
    i=1;
  else if (i<-1)
    i=-1;
  return i;
}				/* node_id_compare */

/* returns a newly allocated string with a human-readable id */
gchar *node_id_str(const node_id_t *id)
{
  gchar *msg;
  g_assert(id);
  
  switch (id->node_type)
    {
    case APEMODE_DEFAULT:      
      msg = g_strdup("00:00:00:00:00:00");
      break;
    case LINK6:      
      msg = g_strdup(ether_to_str(id->addr.eth));
      break;
    case IP:
      msg = g_strdup(ip_to_str (id->addr.ip4));
      break;
    case TCP:
      msg = g_strdup_printf("%s:%d", ip_to_str (id->addr.tcp4.host), 
                            id->addr.tcp4.port);
      break;
    default:
      g_error("node_id_type %d unknown", (int) (id->node_type));
      break;
    }

  return msg;
}


gchar *node_id_dump(const node_id_t *id)
{
  gchar *msg;
  g_assert(id);
  switch (id->node_type)
    {
    case APEMODE_DEFAULT:
      msg = g_strdup_printf("NONE: 00:00:00:00:00:00");
      break;
    case LINK6:
      msg = g_strdup_printf("LINK: %s", ether_to_str(id->addr.eth));
      break;
    case IP:
      msg = g_strdup_printf("IP: %s", ip_to_str (id->addr.ip4));
      break;
    case TCP:
      msg = g_strdup_printf("TCP/UDP: %s:%d", ip_to_str (id->addr.tcp4.host), 
                            id->addr.tcp4.port);
      break;
    default:
      msg = g_strdup_printf("node_id_type %d unknown", (int)(id->node_type));
      break;
    }

  return msg;
}

/***************************************************************************
 *
 * name_t implementation
 *
 **************************************************************************/
static long node_name_count = 0;

long active_names(void)
{
  return node_name_count;
}

name_t * node_name_create(const node_id_t *node_id)
{
  name_t *name;
  
  g_assert(node_id);

  name = g_malloc (sizeof (name_t));
  g_assert(name);
  
  name->node_id = *node_id;
  name->accumulated = 0;
  name->numeric_name = NULL;
  name->res_name = NULL;
  ++node_name_count;
    {
      gchar *gg = node_id_dump(node_id);
      g_my_debug("node name created (%p): >%s<, total %ld", name, 
                 gg, node_name_count);
      g_free(gg);
    }
  return name;
}

void node_name_delete(name_t * name)
{
  if (name)
  {
    {
      gchar *gg = node_id_dump(&name->node_id);
      g_my_debug("node name delete (%p): >%s<, total %ld", name, 
                 gg, node_name_count-1);
      g_free(gg);
    }
	if (name->res_name)
      g_string_free (name->res_name, TRUE);
    if (name->numeric_name)
      g_string_free (name->numeric_name, TRUE);
    g_free (name);
    --node_name_count;
  }
}

void node_name_assign(name_t * name, const gchar *nm, const gchar *num_nm, 
                 gdouble sz)
{
  if (DEBUG_ENABLED)
    {
      gchar *msgid = node_id_dump(&name->node_id);
      g_my_debug(" node_name_assign: id %s, name %s, num.name %s\n", 
             msgid, (nm) ? nm : "<none>", num_nm);
      g_free(msgid);
    }
  g_assert(name);
  if (!name->numeric_name)
    name->numeric_name = g_string_new (num_nm);
  else
    g_string_assign (name->numeric_name, num_nm);

  if (nm)
    {
      if (!name->res_name)
        name->res_name = g_string_new (nm);
      else
        g_string_assign (name->res_name, nm);
    }
  name->accumulated += sz;
}

gchar *node_name_dump(const name_t *name)
{
  gchar *msg;
  gchar *nid;
  if (!name)
    return g_strdup("name_t NULL");
  
  nid = node_id_dump(&name->node_id);
  msg = g_strdup_printf("node id: %s, name: %s, numeric_name: %s, solved: %d, "
                        "accumulated %f",
                        nid, 
                        (name->res_name) ? name->res_name->str : "<none>", 
                        name->numeric_name->str,
                        name->res_name != NULL, 
                        name->accumulated);
  g_free(nid);
  return msg;
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
