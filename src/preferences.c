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

#include "preferences.h"

gboolean colors_changed = FALSE;

void
on_preferences1_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  GtkEditable *entry;
  gint position = 0;

  entry = GTK_EDITABLE (glade_xml_get_widget (xml, "filter_entry"));
  gtk_editable_delete_text (entry, 0, -1);
  if (pref.filter)
    gtk_editable_insert_text (entry, pref.filter, strlen (pref.filter),
			      &position);
  else
    gtk_editable_insert_text (entry, "", 0, &position);
  gtk_widget_show (diag_pref);
  gdk_window_raise (diag_pref->window);
}

void
on_node_radius_slider_adjustment_changed (GtkAdjustment * adj)
{

  pref.node_radius_multiplier = exp ((double) adj->value * log (10));
  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	 _("Adjustment value: %g. Radius multiplier %g"),
	 adj->value, pref.node_radius_multiplier);

}

void
on_link_width_slider_adjustment_changed (GtkAdjustment * adj)
{

  pref.link_width_multiplier = exp ((double) adj->value * log (10));
  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	 _("Adjustment value: %g. Radius multiplier %g"),
	 adj->value, pref.link_width_multiplier);

}

void
on_averaging_spin_adjustment_changed (GtkAdjustment * adj)
{
  pref.averaging_time = adj->value;	/* Control and value in ms */
}

void
on_refresh_spin_adjustment_changed (GtkAdjustment * adj, GtkWidget * canvas)
{
  pref.refresh_period = adj->value;
  /* When removing the source (which could either be an idle or a timeout
   * function, I'm also forcing the callback for the corresponding 
   * destroying function, which in turn will install a timeout or idle
   * function using the new refresh_period. It might take a while for it
   * to settle down, but I think it works now */
  g_source_remove (diagram_timeout);
}

void
on_node_to_spin_adjustment_changed (GtkAdjustment * adj)
{
  pref.node_timeout_time = adj->value;	/* Control and value in ms */
}				/* on_node_to_spin_adjustment_changed */

void
on_gui_node_to_spin_adjustment_changed (GtkAdjustment * adj)
{
  pref.gui_node_timeout_time = adj->value;	/* Control and value in ms */
}				/* on_gui_node_to_spin_adjustment_changed */

void
on_link_to_spin_adjustment_changed (GtkAdjustment * adj)
{
  pref.link_timeout_time = adj->value;	/* Control and value in ms */
}

void
on_font_button_clicked (GtkButton * button, gpointer user_data)
{
  static GtkWidget *fontsel = NULL;
  if (!fontsel)
    fontsel = glade_xml_get_widget (xml, "fontselectiondialog1");
  gtk_font_selection_dialog_set_font_name (GTK_FONT_SELECTION_DIALOG
					   (fontsel), pref.fontname);
  gtk_widget_show (fontsel);
}


void
on_ok_button1_clicked (GtkButton * button, gpointer user_data)
{
  GtkWidget *fontsel;
  gchar *str;
  fontsel = glade_xml_get_widget (xml, "fontselectiondialog1");
  str =
    gtk_font_selection_dialog_get_font_name (GTK_FONT_SELECTION_DIALOG
					     (fontsel));
  if (str)
    {
      if (pref.fontname)
	g_free (pref.fontname);
      pref.fontname = g_strdup (str);
      g_free (str);
      need_reposition = TRUE;
    }

  gtk_widget_hide (fontsel);
}


void
on_cancel_button1_clicked (GtkButton * button, gpointer user_data)
{
  GtkWidget *fontsel;
  fontsel = glade_xml_get_widget (xml, "fontselectiondialog1");
  gtk_widget_hide (fontsel);
}				/* on_cancel_button1_clicked */


void
on_apply_button1_clicked (GtkButton * button, gpointer user_data)
{
  GtkWidget *fontsel;
  gchar *str;

  fontsel = glade_xml_get_widget (xml, "fontselectiondialog1");
  str =
    gtk_font_selection_dialog_get_font_name (GTK_FONT_SELECTION_DIALOG
					     (fontsel));
  if (str)
    {
      if (pref.fontname)
	g_free (pref.fontname);
      pref.fontname = g_strdup (str);
      g_free (str);
      need_reposition = TRUE;
    }
}				/* on_apply_button1_clicked */

void
on_size_mode_menu_selected (GtkMenuShell * menu_shell, gpointer data)
{
  GtkWidget *active_item;

  active_item = gtk_menu_get_active (GTK_MENU (menu_shell));
  /* Beware! Size mode is an enumeration. The menu options
   * must much the enumaration values */
  pref.size_mode = g_list_index (menu_shell->children, active_item);

}				/* on_size_mode_menu_selected */

void
on_node_size_optionmenu_selected (GtkMenuShell * menu_shell, gpointer data)
{
  GtkWidget *active_item;

  active_item = gtk_menu_get_active (GTK_MENU (menu_shell));
  /* Beware! Size mode is an enumeration. The menu options
   * must much the enumaration values */
  pref.node_size_variable = g_list_index (menu_shell->children, active_item);

}				/* on_node_size_optionmenu_selected */

void
on_stack_level_menu_selected (GtkMenuShell * menu_shell, gpointer data)
{
  GtkWidget *active_item;

  active_item = gtk_menu_get_active (GTK_MENU (menu_shell));
  pref.stack_level = g_list_index (menu_shell->children, active_item);

  delete_gui_protocols ();

}				/* on_stack_level_menu_selected */

void
on_diagram_only_toggle_toggled (GtkToggleButton * togglebutton,
				gpointer user_data)
{

  pref.diagram_only = gtk_toggle_button_get_active (togglebutton);
  need_reposition = TRUE;

}				/* on_diagram_only_toggle_toggled */

void
on_group_unk_check_toggled (GtkToggleButton * togglebutton,
			    gpointer user_data)
{
  enum status_t old_status = status;

  if ((status == PLAY) || (status == PAUSE))
    gui_stop_capture ();

  pref.group_unk = gtk_toggle_button_get_active (togglebutton);

  if (old_status == PLAY)
    gui_start_capture ();

}				/* on_group_unk_check_toggled */

void
on_aa_check_toggled (GtkToggleButton * togglebutton, gpointer user_data)
{
  enum status_t old_status = status;

  if ((status == PLAY) || (status == PAUSE))
    gui_stop_capture ();

  pref.antialias = gtk_toggle_button_get_active (togglebutton);

  if (old_status == PLAY)
    gui_start_capture ();
}				/* on_group_unk_check_toggled */

/*
 * TODO
 * I have to change the whole preferences workings, so that OK, apply and 
 * cancel have all the proper semantics
 */
void
on_ok_pref_button_clicked (GtkButton * button, gpointer user_data)
{
  on_apply_pref_button_clicked (button, NULL);
  gtk_widget_hide (diag_pref);

}				/* on_ok_pref_button_clicked */

void
on_apply_pref_button_clicked (GtkButton * button, gpointer user_data)
{
  GtkWidget *widget = NULL;

  widget = glade_xml_get_widget (xml, "filter_entry");
  on_filter_entry_changed (GTK_EDITABLE (widget), NULL);
  widget = glade_xml_get_widget (xml, "");

  /* add proto name to history */
  gnome_entry_append_history (GNOME_ENTRY
			      (glade_xml_get_widget
			       (xml, "filter_gnome_entry")), FALSE,
			      pref.filter);

  if (colors_changed)
    {
      color_list_to_pref ();
      delete_gui_protocols ();
      colors_changed = FALSE;
    }
}				/* on_apply_pref_button_clicked */

void
on_cancel_pref_button_clicked (GtkButton * button, gpointer user_data)
{
  gtk_widget_hide (diag_pref);

}				/* on_cancel_pref_button_clicked */

void
on_save_pref_button_clicked (GtkButton * button, gpointer user_data)
{
  on_apply_pref_button_clicked (button, user_data);	/* to save we simulate apply */
  save_config ("/Etherape/");
}				/* on_save_pref_button_clicked */


/* Makes a new filter */
void
on_filter_entry_changed (GtkEditable * editable, gpointer user_data)
{
  gchar *str;
  /* TODO should make sure that for each mode the filter is set up
   * correctly */
  str = gtk_editable_get_chars (editable, 0, -1);
  if (pref.filter)
    g_free (pref.filter);
  pref.filter = g_strdup (str);
  g_free (str);
  /* TODO We should look at the error code from set_filter and pop
   * up a window accordingly */
  set_filter (pref.filter, NULL);
}				/* on_filter_entry_changed */

void
on_fade_toggle_toggled (GtkToggleButton * togglebutton, gpointer user_data)
{

  pref.nofade = !gtk_toggle_button_get_active (togglebutton);

}				/* on_fade_toggle_toggled */

void
on_cycle_toggle_toggled (GtkToggleButton * togglebutton, gpointer user_data)
{

  pref.cycle = gtk_toggle_button_get_active (togglebutton);
  colors_changed = TRUE;

}				/* on_cycle_toggle_toggled */


/*
 * Names dialog related functions
 */


void
on_name_clist_select_row (GtkCList * clist,
			  gint row,
			  gint column, GdkEvent * event, gpointer user_data)
{
}				/* on_name_clist_select_row */

void
on_dns_toggle_toggled (GtkToggleButton * togglebutton, gpointer user_data)
{
}				/* on_dns_toggle_toggled */

void
on_numeric_toggle_toggled (GtkToggleButton * togglebutton, gpointer user_data)
{
}				/* on_numeric_toggle_toggled */


void
on_protocol_add_button_clicked (GtkButton * button, gpointer user_data)
{
}				/* on_protocol_add_button_clicked */

void
on_protocol_remove_button_clicked (GtkButton * button, gpointer user_data)
{
}				/* on_protocol_remove_button_clicked */

void
on_protocol_move_up_clicked (GtkButton * button, gpointer user_data)
{
}				/* on_protocol_move_up_clicked */

void
on_protocol_move_down_clicked (GtkButton * button, gpointer user_data)
{
}				/* on_protocol_move_down_clicked */

/* ----------------------------------------------------------

   Color-proto preferences handling

   ---------------------------------------------------------- */

/* helper struct used to move around trees data ... */
typedef struct _EATreePos
{
  GtkTreeView *gv;
  GtkListStore *gs;
} EATreePos;


/* fill ep with the listore for the color treeview, creating it if necessary
   Returns FALSE if something goes wrong
*/
static gboolean
get_color_store (EATreePos * ep)
{
  /* initializes the view ptr */
  ep->gs = NULL;
  ep->gv = GTK_TREE_VIEW (glade_xml_get_widget (xml, "color_list"));
  if (!ep->gv)
    return FALSE;		/* error */

  /* retrieve the model (store) */
  ep->gs = GTK_LIST_STORE (gtk_tree_view_get_model (ep->gv));
  if (ep->gs)
    return TRUE;		/* model already initialized, finished */

  /* store not found, must be created  - it uses 3 values: 
     First the color string, then the gdk color, lastly the protocol */
  ep->gs =
    gtk_list_store_new (3, G_TYPE_STRING, GDK_TYPE_COLOR, G_TYPE_STRING);
  gtk_tree_view_set_model (ep->gv, GTK_TREE_MODEL (ep->gs));

  /* the view columns and cell renderers must be also created ... 
     Note: the bkg color is linked to the second column of store
   */
  gtk_tree_view_append_column (ep->gv,
			       gtk_tree_view_column_new_with_attributes
			       ("Color", gtk_cell_renderer_text_new (),
				"text", 0, "background-gdk", 1, NULL));
  gtk_tree_view_append_column (ep->gv,
			       gtk_tree_view_column_new_with_attributes
			       ("Protocol", gtk_cell_renderer_text_new (),
				"text", 2, NULL));

  return TRUE;
}


void
on_color_add_button_clicked (GtkButton * button, gpointer user_data)
{
  GtkWidget *colorseldiag =
    glade_xml_get_widget (xml, "colorselectiondialog");
  gtk_widget_show (colorseldiag);
}				/* on_color_add_button_clicked */

void
on_color_remove_button_clicked (GtkButton * button, gpointer user_data)
{
  GtkTreePath *gpath = NULL;
  GtkTreeViewColumn *gcol = NULL;
  GtkTreeIter it;
  EATreePos ep;
  if (!get_color_store (&ep))
    return;

  /* gets the row (path) at cursor */
  gtk_tree_view_get_cursor (ep.gv, &gpath, &gcol);
  if (!gpath)
    return;			/* no row selected */

  /* get iterator from path  and removes from store */
  if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (ep.gs), &it, gpath))
    return;			/* path not found */
  if (gtk_list_store_remove (ep.gs, &it))
    {
      /* iterator still valid, selects current pos */
      gpath = gtk_tree_model_get_path (GTK_TREE_MODEL (ep.gs), &it);
      gtk_tree_view_set_cursor (ep.gv, gpath, NULL, 0);
      gtk_tree_path_free (gpath);
    }

  colors_changed = TRUE;
}				/* on_color_remove_button_clicked */

void
on_colordiag_ok_clicked (GtkButton * button, gpointer user_data)
{
  GtkWidget *colorsel, *colorseldiag;
  gdouble colors[4];
  GdkColor gdk_color;
  gchar tmp[64];
  GtkTreePath *gpath = NULL;
  GtkTreeViewColumn *gcol = NULL;
  GtkTreeIter it;
  EATreePos ep;
  if (!get_color_store (&ep))
    return;

  /* gets the row (path) at cursor */
  gtk_tree_view_get_cursor (ep.gv, &gpath, &gcol);
  if (gpath)
    {
      /* row sel, add before */
      GtkTreeIter itsibling;
      if (!gtk_tree_model_get_iter
	  (GTK_TREE_MODEL (ep.gs), &itsibling, gpath))
	return;			/* path not found */
      gtk_list_store_insert_before (ep.gs, &it, &itsibling);
    }
  else
    gtk_list_store_append (ep.gs, &it);	/* no row selected, append */

  /* get the selected color */
  colorseldiag = glade_xml_get_widget (xml, "colorselectiondialog");
  colorsel = GTK_COLOR_SELECTION_DIALOG (colorseldiag)->colorsel;
  gtk_color_selection_get_color (GTK_COLOR_SELECTION (colorsel), colors);

  gdk_color.red = (guint16) (colors[0] * 65535.0);
  gdk_color.green = (guint16) (colors[1] * 65535.0);
  gdk_color.blue = (guint16) (colors[2] * 65535.0);

  /* Since we are only going to save 24bit precision, we might as well
   * make sure we don't display any more than that */
  gdk_color.red = (gdk_color.red >> 8) << 8;
  gdk_color.green = (gdk_color.green >> 8) << 8;
  gdk_color.blue = (gdk_color.blue >> 8) << 8;

  /* converting color */
  g_snprintf (tmp, sizeof (tmp), "#%02x%02x%02x", gdk_color.red >> 8,
	      gdk_color.green >> 8, gdk_color.blue >> 8);

  /* fill data */
  gtk_list_store_set (ep.gs, &it, 0, tmp, 1, &gdk_color, 2, "", -1);

  gtk_widget_hide (colorseldiag);

  colors_changed = TRUE;
}				/* on_colordiag_ok_clicked */



void
on_protocol_edit_button_clicked (GtkButton * button, gpointer user_data)
{
  GtkWidget *protocol_edit_dialog = NULL;
  protocol_edit_dialog = glade_xml_get_widget (xml, "protocol_edit_dialog");
  gtk_widget_show (protocol_edit_dialog);
}				/* on_protocol_edit_button_clicked */

void
on_protocol_edit_dialog_show (GtkWidget * wdg, gpointer user_data)
{
  gchar *protocol_string;
  GtkTreePath *gpath = NULL;
  GtkTreeViewColumn *gcol = NULL;
  GtkTreeIter it;
  GtkWidget *protocol_entry = NULL;
  int pos = 0;
  EATreePos ep;
  if (!get_color_store (&ep))
    return;

  /* gets the row (path) at cursor */
  gtk_tree_view_get_cursor (ep.gv, &gpath, &gcol);
  if (!gpath)
    return;			/* no row selected */

  GtkTreeIter itsibling;
  if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (ep.gs), &it, gpath))
    return;			/* path not found */

  gtk_tree_model_get (GTK_TREE_MODEL (ep.gs), &it, 2, &protocol_string, -1);

  protocol_entry = glade_xml_get_widget (xml, "protocol_entry");

  gtk_editable_delete_text (GTK_EDITABLE (protocol_entry), 0, -1);
  gtk_editable_insert_text (GTK_EDITABLE (protocol_entry), protocol_string,
			    strlen (protocol_string), &pos);

  g_free (protocol_string);
}


void
on_protocol_edit_ok_clicked (GtkButton * button, gpointer user_data)
{
  gchar *proto_string;
  GtkTreePath *gpath = NULL;
  GtkTreeViewColumn *gcol = NULL;
  GtkTreeIter it;
  GtkWidget *protocol_entry = NULL;
  int pos = 0;
  EATreePos ep;
  if (!get_color_store (&ep))
    return;

  /* gets the row (path) at cursor */
  gtk_tree_view_get_cursor (ep.gv, &gpath, &gcol);
  if (!gpath)
    return;			/* no row selected */

  GtkTreeIter itsibling;
  if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (ep.gs), &it, gpath))
    return;			/* path not found */


  protocol_entry = glade_xml_get_widget (xml, "protocol_entry");
  proto_string =
    gtk_editable_get_chars (GTK_EDITABLE (protocol_entry), 0, -1);
  proto_string = g_utf8_strup (proto_string, -1);

  gtk_list_store_set (ep.gs, &it, 2, proto_string, -1);

  /* add proto name to history */
  gnome_entry_append_history (GNOME_ENTRY
			      (glade_xml_get_widget
			       (xml, "protocol_gnome_entry")), FALSE,
			      proto_string);

  g_free (proto_string);

  colors_changed = TRUE;
  gtk_widget_hide (glade_xml_get_widget (xml, "protocol_edit_dialog"));
}				/* on_protocol_edit_ok_clicked */



void
load_color_list (void)
{
  gint i;
  EATreePos ep;
  if (!get_color_store (&ep))
    return;

  /* clear list */
  gtk_list_store_clear (ep.gs);

  for (i = 0; i < pref.n_colors; i++)
    {
      GdkColor gdk_color;
      gchar **colors_protocols = NULL;
      gchar *protocol = NULL;
      gchar tmp[64];
      GtkTreeIter it;

      colors_protocols = g_strsplit (pref.colors[i], ";", 0);

      /* converting color */
      gdk_color_parse (colors_protocols[0], &gdk_color);
      g_snprintf (tmp, sizeof (tmp), "#%02x%02x%02x", gdk_color.red >> 8,
		  gdk_color.green >> 8, gdk_color.blue >> 8);

      /* converting proto name */
      if (!colors_protocols[1])
	protocol = "";
      else
	protocol = colors_protocols[1];

      /* adds a new row */
      gtk_list_store_append (ep.gs, &it);
      gtk_list_store_set (ep.gs, &it, 0, tmp, 1, &gdk_color, 2, protocol, -1);
    }

}

/* Called whenever preferences are applied or OKed. Copied whatever there is
 * in the color table to the color preferences in memory */
void
color_list_to_pref (void)
{
  gint i;
  GtkTreeIter it;

  EATreePos ep;
  if (!get_color_store (&ep))
    return;

  while (pref.colors && pref.n_colors)
    {
      g_free (pref.colors[pref.n_colors - 1]);
      pref.n_colors--;
    }
  g_free (pref.colors);
  pref.colors = NULL;

  pref.n_colors =
    gtk_tree_model_iter_n_children (GTK_TREE_MODEL (ep.gs), NULL);
  pref.colors = g_malloc (sizeof (gchar *) * pref.n_colors);

  gtk_tree_model_get_iter_first (GTK_TREE_MODEL (ep.gs), &it);
  for (i = 0; i < pref.n_colors; i++)
    {
      gchar *color, *protocol;

      /* reads the list */
      gtk_tree_model_get (GTK_TREE_MODEL (ep.gs), &it,
			  0, &color, 2, &protocol, -1);

      if (strcmp ("", protocol))
	pref.colors[i] = g_strdup_printf ("%s;%s", color, protocol);
      else
	pref.colors[i] = g_strdup (color);

      g_free (color);
      g_free (protocol);

      gtk_tree_model_iter_next (GTK_TREE_MODEL (ep.gs), &it);
    }
}				/* color_clist_to_pref */
