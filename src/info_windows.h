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
  guint8 *node_id;
  GtkWidget *window;
}
node_info_window_t;

typedef struct
{
  gchar *prot_name;
  GtkWidget *window;
}
prot_info_window_t;

GList *node_info_windows = NULL;
GList *prot_info_windows = NULL;

static GList *info_protocols = NULL;

static guint prot_clist_sort_column = 0;
static gboolean prot_clist_reverse_sort = FALSE;


static void update_prot_info_windows (void);

gboolean on_prot_table_button_press_event (GtkWidget * widget,
					   GdkEventButton * event,
					   gpointer user_data);
static void update_node_info_window (node_info_window_t * node_info_window);
static void update_prot_info_window (prot_info_window_t * prot_info_window);
static gint node_info_compare (gconstpointer a, gconstpointer b);
static gint prot_info_compare (gconstpointer a, gconstpointer b);

void on_node_info_delete_event (GtkWidget * node_info, gpointer user_data);
void on_prot_info_delete_event (GtkWidget * node_info, gpointer user_data);
static void prot_clist_button_clicked (GtkButton * button,
				       gpointer func_data);
static gint prot_window_compare (GtkCList * clist, gconstpointer p1,
				 gconstpointer p2);
static void create_prot_info_window (protocol_t * protocol);

static gchar *timeval_to_str (struct timeval tv);
