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

#include "globals.h"
#include "dns.h"
#include "eth_resolv.h"

typedef void (p_func_t) (void);

typedef struct
{
  gchar *prot;
  p_func_t *function;
}
prot_function_t;

#define KNOWN_PROTS 4


static const guint8 *p;
static const guint8 *id;
static guint16 offset;
static guint16 packet_size;
static guint8 level;
static guint id_length;
static packet_direction dir;
static GList **prot_list;
static GList *protocol_item;
static protocol_t *protocol;
static GList *name_item;
static name_t *name;
static gchar **tokens = NULL;
static GTree *prot_functions = NULL;
static prot_function_t *next_func = NULL;


void get_eth_name (void);
void get_ip_name (void);
void add_name (gchar * numeric, gchar * resolved);
gint id_compare (gconstpointer a, gconstpointer b);

static prot_function_t prot_functions_table[KNOWN_PROTS + 1] = {
  {"ETH_II", get_eth_name},
  {"802.2", get_eth_name},
  {"802.3", get_eth_name},
  {"ISL", get_eth_name},
  {"IP", get_ip_name}
};
