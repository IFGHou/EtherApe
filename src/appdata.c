/* EtherApe
 * Copyright (C) 2001 Juan Toledo
 * Copyright (C) 2011 Riccardo Ghetta
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

#define ETHERAPE_GLADE_FILE "etherape.glade"	/* glade 3 file */

void appdata_init(struct appdata_struct *p)
{
  gettimeofday (&p->now, NULL);

  p->xml = NULL;
  p->app1 = NULL;
  p->statusbar = NULL;

  p->glade_file = NULL;
  p->input_file = NULL;
  p->export_file = NULL;
  p->export_file_final = NULL;
  p->export_file_signal = NULL;
  p->interface = NULL;

  p->mode = IP;
  p->node_limit = -1;
  p->debug_mask = (G_LOG_LEVEL_MASK & ~(G_LOG_LEVEL_DEBUG | G_LOG_LEVEL_INFO));
  p->min_delay = 0;
  p->max_delay = G_MAXULONG;

  p->n_packets = 0;
  p->total_mem_packets = 0;
  p->request_dump = FALSE;
}

gboolean appdata_init_glade(gchar *new_glade_file)
{
  if (new_glade_file)
    appdata.glade_file = g_strdup(new_glade_file);
  else
    appdata.glade_file = g_strdup(GLADEDIR "/" ETHERAPE_GLADE_FILE);

  appdata.xml = glade_xml_new (appdata.glade_file, NULL, NULL);
  if (!appdata.xml)
    {
      g_error (_("Could not load glade interface file '%s'!"),
	       appdata.glade_file);
      return FALSE;
    }
  glade_xml_signal_autoconnect (appdata.xml);

  appdata.app1 = glade_xml_get_widget (appdata.xml, "app1");
  appdata.statusbar = GTK_STATUSBAR(glade_xml_get_widget (appdata.xml, "statusbar1"));
  return TRUE;
}

/* releases all memory allocated for internal fields */
void appdata_free(struct appdata_struct *p)
{
  g_free(p->glade_file);
  p->glade_file = NULL;

  g_free(p->input_file);
  p->input_file = NULL;

  g_free(p->export_file);
  p->export_file = NULL;

  g_free(p->export_file_final);
  p->export_file_final = NULL;

  g_free(p->export_file_signal);
  p->export_file_signal = NULL;

  g_free(p->interface);
  p->interface=NULL;

  /* no need to free glade widgets ... */
}

