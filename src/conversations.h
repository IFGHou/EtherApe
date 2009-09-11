/* Etherape
 * Copyright (C) 2000 Juan Toledo
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


typedef struct
{
  guint32 src_address;
  guint32 dst_address;
  guint16 src_port;
  guint16 dst_port;
  gchar *data;
}
conversation_t;

void add_conversation (guint32 src_address, guint32 dst_address,
		       guint16 src_port, guint16 dst_port, const gchar * data);
const gchar *find_conversation (guint32 src_address, guint32 dst_address,
			  guint16 src_port, guint16 dst_port);
void delete_conversation_link(guint32 src_address, guint32 dst_address);
void delete_conversations (void);
