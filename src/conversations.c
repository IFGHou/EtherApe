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

static GList *conversations = NULL;	/* Some protocols add an item here to help identify
				 * further packets of the same protocol */

/* Returns the item ptr if there is any matching conversation in any of the
 * two directions */
/* A zero in any of the ports matches any port number */
static GList
  * find_conversation_ptr(guint32 src_address, guint32 dst_address,
		       guint16 src_port, guint16 dst_port)
{
  GList *item = conversations;
  conversation_t *conv = NULL;

  while (item)
    {
      conv = item->data;
      if (((src_address == conv->src_address)
	   && (dst_address == conv->dst_address)
	   && (!src_port || !conv->src_port || (src_port == conv->src_port))
	   && (!dst_port || !conv->dst_port || (dst_port == conv->dst_port))
          )
	  || ((src_address == conv->dst_address)
	      && (dst_address == conv->src_address)
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
add_conversation (guint32 src_address, guint32 dst_address,
		  guint16 src_port, guint16 dst_port, gchar * data)
{
  conversation_t *conv = NULL;
  gchar *old_data = NULL;
  guint32 src, dst;

  /* Because that is the way that ip_to_str works */
  src = htonl (src_address);
  dst = htonl (dst_address);

  /* Make sure there is not one such conversation */
  if ((old_data = find_conversation (src_address, dst_address,
				     src_port, dst_port)))
    {
      if (!strcmp (old_data, data))
	g_my_critical
	  ("Conflicting conversations %s:%d-%s:%d in add_conversation",
	   ip_to_str ((guint8 *) & src), src_port,
	   ip_to_str ((guint8 *) & dst), dst_port);
      else
	g_my_debug
	  ("Conversation %s:%d-%s:%d %s already exists in add_conversation",
	   ip_to_str ((guint8 *) & src), src_port,
	   ip_to_str ((guint8 *) & dst), dst_port, data);
      return;
    }

  g_my_debug ("Adding new conversation %s:%d-%s:%d %s",
	      ip_to_str ((guint8 *) & src), src_port,
	      ip_to_str ((guint8 *) & dst), dst_port, data);

  conv = g_malloc (sizeof (conversation_t));
  conv->src_address = src_address;
  conv->dst_address = dst_address;
  conv->src_port = src_port;
  conv->dst_port = dst_port;
  conv->data = g_strdup (data);

  conversations = g_list_prepend (conversations, conv);

}				/* add_conversation */


/* Returns the data if there is any matching conversation in any of the
 * two directions */
/* A zero in any of the ports matches any port number */
gchar* 
find_conversation (guint32 src_address, guint32 dst_address,
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
  
void
delete_conversations (void)
{
  GList *item = conversations;
  conversation_t *conv = NULL;
  guint32 src, dst;

  while (item)
    {
      conv = item->data;
      /* Because that is the way that ip_to_str works */
      src = htonl (conv->src_address);
      dst = htonl (conv->dst_address);
      g_my_debug ("Removing conversation %s:%d-%s:%d %s",
		  ip_to_str ((guint8 *) & src), conv->src_port,
		  ip_to_str ((guint8 *) & dst), conv->dst_port, conv->data);
      g_free (conv->data);
      item = item->next;
    }

  g_list_free (conversations);
  conversations = NULL;
}				/* delete_conversations */
