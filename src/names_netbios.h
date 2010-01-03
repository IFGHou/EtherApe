
/* These are all functions from ethereal, just as they are there */

/* packet-nbns.c
 * Routines for NetBIOS-over-TCP packet disassembly (the name dates back
 * to when it had only NBNS)
 * Gilbert Ramirez <gram@verdict.uthscsa.edu>
 * Much stuff added by Guy Harris <guy@netapp.com>
 *
 * $Id$
 *
 * Ethereal - Network traffic analyzer
 * By Gerald Combs <gerald@zing.org>
 * Copyright 1998 Gerald Combs
 *
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "globals.h"
#include <glib.h>


#define NETBIOS_NAME_LEN  16



int ethereal_nbns_name (const gchar * pd, int offset,
                        int pd_len,
			char *outname, size_t outname_size,
                        int *name_type_ret);


gchar *get_netbios_host_type (int type);
