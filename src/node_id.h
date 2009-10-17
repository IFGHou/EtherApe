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

#ifndef ETHERAPE_NODE_ID_H
#define ETHERAPE_NODE_ID_H

/* address union */
typedef union __attribute__ ((packed))
{
  guint8 eth[6] ;                 /* ethernet address */
  guint8 ip4[4];                  /* ip address */
  struct __attribute__ ((packed))
  {
      guint8 host[4];            /* tcp/udp address */
      guint16 port;            /* port number */
  } tcp4;

} 
node_addr_t;

/* a node identification */
typedef struct
{
  apemode_t node_type;
  node_addr_t addr;
} node_id_t;
void node_id_clear(node_id_t *a);
gint node_id_compare (const node_id_t *a, const node_id_t *b);
/* returns a newly allocated string with a human-readable id */
gchar *node_id_str(const node_id_t *id); 
/* returns a newly allocated string with a dump of id */
gchar *node_id_dump(const node_id_t *id);

/* a node name */
typedef struct
{
  node_id_t node_id;
  GString *res_name; /* resolved name */
  GString *numeric_name; /* readable version of node_id */
  gboolean solved; /* true if the name was resolved */
  gdouble accumulated; /* total accumulated traffic */
}
name_t;

name_t * node_name_create(const node_id_t *node_id);
void node_name_delete(name_t * name);
void node_name_assign(name_t * name, const gchar *nm, const gchar *num_nm, 
                 gboolean slv, gdouble sz);
gint node_name_id_compare(const name_t *a, const name_t *b);
gint node_name_freq_compare (gconstpointer a, gconstpointer b);
gchar *node_name_dump(const name_t *name);
long active_names(void);

#endif
