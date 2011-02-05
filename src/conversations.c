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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <netinet/in.h>
#include "globals.h"
#include "conversations.h"
#include "dns.h"
#include "util.h"

/* Some protocols add an item here to help identify further packets of the 
 * same protocol */
static GList *conversations = NULL;
static long n_conversations = 0;

long active_conversations(void)
{
  return n_conversations;
}

/* Returns the item ptr if there is any matching conversation in any of the
 * two directions */
/* A zero in any of the ports matches any port number */
static GList
  * find_conversation_ptr(address_t * src_address, address_t * dst_address,
		       guint16 src_port, guint16 dst_port)
{
  GList *item = conversations;
  conversation_t *conv = NULL;

  while (item)
    {
      conv = item->data;
      if ((is_addr_eq(src_address, &conv->src_address)
	   && is_addr_eq(dst_address, &conv->dst_address)
	   && (!src_port || !conv->src_port || (src_port == conv->src_port))
	   && (!dst_port || !conv->dst_port || (dst_port == conv->dst_port))
          )
	  || (is_addr_eq(src_address, &conv->dst_address)
	      && is_addr_eq(dst_address, &conv->src_address)
	      && (!src_port || !conv->dst_port || (src_port == conv->dst_port)) 
              && (!dst_port || !conv->src_port || (dst_port == conv->src_port))
              )
          )
	{
          /* found */
	  break;
	}
      item = item->next;
    }

  return item;
}				/* find_conversation_ptr */


void
add_conversation (address_t * src_address, address_t * dst_address,
		  guint16 src_port, guint16 dst_port, const gchar * data)
{
  conversation_t *conv = NULL;
  const gchar *old_data = NULL;

  /* Make sure there is not one such conversation */
  if ((old_data = find_conversation (src_address, dst_address,
				     src_port, dst_port)))
    {
      if (!strcmp (old_data, data))
	g_my_critical
	  ("Conflicting conversations %s:%d-%s:%d in add_conversation",
	   address_to_str (src_address), src_port,
	   address_to_str (dst_address), dst_port);
      else
	g_my_debug
	  ("Conversation %s:%d-%s:%d %s already exists in add_conversation",
	   address_to_str (src_address), src_port,
	   address_to_str (dst_address), dst_port, data);
      return;
    }

  g_my_debug ("Adding new conversation %s:%d-%s:%d %s",
	      address_to_str (src_address), src_port,
	      address_to_str (dst_address), dst_port, data);

  conv = g_malloc (sizeof (conversation_t));
  g_assert(conv);
  
  address_copy(&conv->src_address, src_address);
  address_copy(&conv->dst_address, dst_address);
  conv->src_port = src_port;
  conv->dst_port = dst_port;
  conv->data = g_strdup (data);

  conversations = g_list_prepend (conversations, conv);
  n_conversations++;
}				/* add_conversation */


/* Returns the data if there is any matching conversation in any of the
 * two directions */
/* A zero in any of the ports matches any port number */
const gchar* 
find_conversation (address_t * src_address, address_t * dst_address,
		      guint16 src_port, guint16 dst_port)
{
  GList *item;
  item = find_conversation_ptr(src_address, dst_address, src_port, dst_port);
  if (item)
    {
      /* found */
      conversation_t *conv = item->data;
      return conv->data;
    }
  else
    return NULL;
}

/* removes all conversations with the specified addresses */
void
delete_conversation_link(address_t * src_address, address_t * dst_address)
{
  GList *item;

  while ( (item = find_conversation_ptr(src_address, dst_address, 0, 0)) )
    {
      conversation_t *conv = NULL;
      conv = item->data;
      g_my_debug ("Removing conversation %s:%d-%s:%d %s",
		  address_to_str (&conv->src_address), conv->src_port,
		  address_to_str (&conv->dst_address), conv->dst_port, conv->data);
      g_free (conv->data);
      g_free (conv);
      conversations = g_list_delete_link(conversations, item);
      n_conversations--;
    }
}

void
delete_conversations (void)
{
  GList *item = conversations;
  conversation_t *conv = NULL;

  while (item)
    {
      conv = item->data;
      g_my_debug ("Removing conversation %s:%d-%s:%d %s",
		  address_to_str (&conv->src_address), conv->src_port,
		  address_to_str (&conv->dst_address), conv->dst_port, conv->data);
      g_free (conv->data);
      g_free (conv);
      item = item->next;
      n_conversations--;
    }

  g_list_free (conversations);
  conversations = NULL;
}				/* delete_conversations */
