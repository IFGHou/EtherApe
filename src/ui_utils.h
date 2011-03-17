/* Etherape
 * Copyright (C) 2000-2009 Juan Toledo, Riccardo Ghetta
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
 *
 * ui_utils: helper gui routines
 */

#ifndef UI_UTILS_H
#define UI_UTILS_H

#include "appdata.h"
#include <gtk/gtkbutton.h>
#include <gtk/gtkclist.h>
#include <gtk/gtkcheckmenuitem.h>


#define MAX_C(a,b) ((a) > (b) ? (a) : (b))
#define MIN_C(a,b) ((a) > (b) ? (a) : (b))
#define LUMINANCE(r,g,b) ((MAX_C( (double)(r)/0xFFFF, MAX_C( (double)(g)/0xFFFF, \
                                  (double)(b)/0xFFFF)) + \
                          MIN_C( (double)(r)/0xFFFF, MIN_C( (double)(g)/0xFFFF, \
                                  (double)(b)/0xFFFF))) / 2.0)


/* returns a newly allocated string with a timeval in human readable form */
gchar *timeval_to_str (struct timeval last_heard);

/* returns a newly allocated string with a formatted traffic  */
gchar *traffic_to_str (gdouble traffic, gboolean is_speed);

/* registers the named glade widget on the specified object */
void register_glade_widget(GladeXML *xm, GObject *tgt, const gchar *widgetName);

/* changes text of label lblname on window wnd */
void update_gtklabel(GtkWidget *wnd, const gchar *lblname, const gchar *value);

/* show hide widget named lblname on window wnd */
void show_widget(GtkWidget *wnd, const gchar *lblname);
void hide_widget(GtkWidget *wnd, const gchar *lblname);

/* creates a new text column with a specific title, column number colno and
 * adds it to treeview gv.  If r_just true the column is right justified */
void create_add_text_column(GtkTreeView *gv, const gchar *title, int colno, 
                            gboolean r_just);

/* register/get a treeview to/from a window */
void register_treeview(GtkWidget *window, GtkTreeView *gv);
GtkTreeView *retrieve_treeview(GtkWidget *window);

#endif
