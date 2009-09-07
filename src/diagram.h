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



gboolean already_updating;	/* True while an instance of update_diagram is running */

guint update_diagram (GtkWidget * canvas);
void init_diagram (GladeXML *xml);
void destroying_timeout (gpointer data);
void destroying_idle (gpointer data);
void set_statusbar_msg (gchar * str);
void delete_gui_protocols (void);
gchar *traffic_to_str (gdouble traffic, gboolean is_speed);
void ask_reposition(void); /* request diagram relayout */
void change_refresh_period(guint32 newperiod); 
