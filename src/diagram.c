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

#include <gnome.h>

#include "math.h"

#include "globals.h"
#include "diagram.h"
#include "preferences.h"

GdkColor black_color;

/* It updates controls from values of variables, and connects control
 * signals to callback functions */
void
init_diagram ()
{
  GtkWidget *widget;
  GtkSpinButton *spin;
  GtkStyle *style;
  GtkWidget *canvas;

  /* Creates trees */
  canvas_nodes = g_tree_new (node_id_compare);
  canvas_links = g_tree_new (link_id_compare);


  /* Updates controls from values of variables */
  widget = glade_xml_get_widget (xml, "node_radius_slider");
  gtk_adjustment_set_value (GTK_RANGE (widget)->adjustment,
			    log (pref.node_radius_multiplier) / log (10));
  gtk_signal_emit_by_name (GTK_OBJECT (GTK_RANGE (widget)->adjustment),
			   "changed");
  widget = glade_xml_get_widget (xml, "link_width_slider");
  gtk_adjustment_set_value (GTK_RANGE (widget)->adjustment,
			    log (pref.link_width_multiplier) / log (10));
  gtk_signal_emit_by_name (GTK_OBJECT (GTK_RANGE (widget)->adjustment),
			   "changed");
  spin = GTK_SPIN_BUTTON (glade_xml_get_widget (xml, "averaging_spin"));
  gtk_spin_button_set_value (spin, pref.averaging_time);
  spin = GTK_SPIN_BUTTON (glade_xml_get_widget (xml, "refresh_spin"));
  gtk_spin_button_set_value (spin, pref.refresh_period);
  spin = GTK_SPIN_BUTTON (glade_xml_get_widget (xml, "gui_node_to_spin"));
  gtk_spin_button_set_value (spin, pref.gui_node_timeout_time);
  spin = GTK_SPIN_BUTTON (glade_xml_get_widget (xml, "node_to_spin"));
  gtk_spin_button_set_value (spin, pref.node_timeout_time);
  spin = GTK_SPIN_BUTTON (glade_xml_get_widget (xml, "link_to_spin"));
  gtk_spin_button_set_value (spin, pref.link_timeout_time);

  widget = glade_xml_get_widget (xml, "diagram_only_toggle");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
				pref.diagram_only);
  widget = glade_xml_get_widget (xml, "group_unk_check");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), pref.group_unk);
  widget = glade_xml_get_widget (xml, "fade_toggle");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), !pref.nofade);
  widget = glade_xml_get_widget (xml, "cycle_toggle");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), pref.cycle);

  widget = glade_xml_get_widget (xml, "size_mode_menu");
  gtk_option_menu_set_history (GTK_OPTION_MENU (widget), pref.size_mode);
  widget = glade_xml_get_widget (xml, "node_size_optionmenu");
  gtk_option_menu_set_history (GTK_OPTION_MENU (widget),
			       pref.node_size_variable);
  widget = glade_xml_get_widget (xml, "stack_level_menu");
  gtk_option_menu_set_history (GTK_OPTION_MENU (widget), pref.stack_level);
  widget = glade_xml_get_widget (xml, "filter_gnome_entry");
  widget = glade_xml_get_widget (xml, "file_filter_entry");
  widget = glade_xml_get_widget (xml, "fileentry");
  widget = gnome_file_entry_gnome_entry (GNOME_FILE_ENTRY (widget));

  load_color_list ();		/* Updates the color preferences table with pref.colors */

  /* Connects signals */
  widget = glade_xml_get_widget (xml, "node_radius_slider");
  gtk_signal_connect (GTK_OBJECT (GTK_RANGE (widget)->adjustment),
		      "value_changed",
		      GTK_SIGNAL_FUNC
		      (on_node_radius_slider_adjustment_changed), NULL);
  widget = glade_xml_get_widget (xml, "link_width_slider");
  gtk_signal_connect (GTK_OBJECT (GTK_RANGE (widget)->adjustment),
		      "value_changed",
		      GTK_SIGNAL_FUNC
		      (on_link_width_slider_adjustment_changed), NULL);
  widget = glade_xml_get_widget (xml, "averaging_spin");
  gtk_signal_connect (GTK_OBJECT (GTK_SPIN_BUTTON (widget)->adjustment),
		      "value_changed",
		      GTK_SIGNAL_FUNC
		      (on_averaging_spin_adjustment_changed), NULL);
  widget = glade_xml_get_widget (xml, "refresh_spin");
  gtk_signal_connect (GTK_OBJECT (GTK_SPIN_BUTTON (widget)->adjustment),
		      "value_changed",
		      GTK_SIGNAL_FUNC
		      (on_refresh_spin_adjustment_changed),
		      glade_xml_get_widget (xml, "canvas1"));
  widget = glade_xml_get_widget (xml, "node_to_spin");
  gtk_signal_connect (GTK_OBJECT (GTK_SPIN_BUTTON (widget)->adjustment),
		      "value_changed",
		      GTK_SIGNAL_FUNC
		      (on_node_to_spin_adjustment_changed), NULL);
  widget = glade_xml_get_widget (xml, "gui_node_to_spin");
  gtk_signal_connect (GTK_OBJECT (GTK_SPIN_BUTTON (widget)->adjustment),
		      "value_changed",
		      GTK_SIGNAL_FUNC
		      (on_gui_node_to_spin_adjustment_changed), NULL);
  widget = glade_xml_get_widget (xml, "link_to_spin");
  gtk_signal_connect (GTK_OBJECT (GTK_SPIN_BUTTON (widget)->adjustment),
		      "value_changed",
		      GTK_SIGNAL_FUNC
		      (on_link_to_spin_adjustment_changed), NULL);
  widget = glade_xml_get_widget (xml, "size_mode_menu");
  gtk_signal_connect (GTK_OBJECT (GTK_OPTION_MENU (widget)->menu),
		      "deactivate",
		      GTK_SIGNAL_FUNC (on_size_mode_menu_selected), NULL);
  widget = glade_xml_get_widget (xml, "node_size_optionmenu");
  gtk_signal_connect (GTK_OBJECT (GTK_OPTION_MENU (widget)->menu),
		      "deactivate",
		      GTK_SIGNAL_FUNC (on_node_size_optionmenu_selected),
		      NULL);
  widget = glade_xml_get_widget (xml, "stack_level_menu");
  gtk_signal_connect (GTK_OBJECT (GTK_OPTION_MENU (widget)->menu),
		      "deactivate",
		      GTK_SIGNAL_FUNC (on_stack_level_menu_selected), NULL);

  /* Sets canvas background to black */
  canvas = glade_xml_get_widget (xml, "canvas1");
//r.g.  gc = GNOME_CANVAS(canvas);  

  gdk_color_parse ("black", &black_color);
  gdk_colormap_alloc_color (gdk_colormap_get_system (), &black_color,
			    TRUE, TRUE);
  style = gtk_style_new ();
  style->bg[GTK_STATE_NORMAL] = black_color;
  style->base[GTK_STATE_NORMAL] = black_color;

  gtk_widget_set_style (canvas, style);
  gtk_style_set_background (canvas->style, canvas->window, GTK_STATE_NORMAL);

  /* Initialize the known_protocols table */
  delete_gui_protocols ();

  /* Set the already_updating global flag */
  already_updating = FALSE;
}				/* init_diagram */


void
destroying_timeout (gpointer data)
{
/* g_message ("A timeout function has been destroyed"); */
  diagram_timeout = g_idle_add_full (G_PRIORITY_DEFAULT,
				     (GtkFunction) update_diagram,
				     data, (GDestroyNotify) destroying_idle);
  is_idle = TRUE;
}

void
destroying_idle (gpointer data)
{
/*   g_message ("An idle function has been destroyed"); */
  diagram_timeout = g_timeout_add_full (G_PRIORITY_DEFAULT,
					pref.refresh_period,
					(GtkFunction) update_diagram,
					data,
					(GDestroyNotify) destroying_timeout);
  is_idle = FALSE;
}


/* Refreshes the diagram. Called each refresh_period ms
 * 1. Checks for new protocols and displays them
 * 2. Updates nodes looks
 * 3. Updates links looks
 */
guint
update_diagram (GtkWidget * canvas)
{
  GString *status_string = NULL;
  guint n_links = 0, n_links_new = 1;
  guint n_nodes_before = 0, n_nodes_after = 1;
  static struct timeval last_time = { 0, 0 }, diff;
  guint32 diff_msecs;
  node_t *new_node = NULL;

  if (status == PAUSE)
    return FALSE;

  if (end_of_file && status != STOP)
    {
      g_my_debug ("End of file and status != STOP. Pausing.");
      gui_pause_capture ();
    }

  /* 
   * It could happen that during an intensive calculation, in order
   * to update the GUI and make the application responsive gtk_main_iteration
   * is called. But that could also trigger this very function's timeout.
   * If we let it run twice many problems could come up. Thus,
   * we are preventing it with the already_updating variable
   */

  if (already_updating)
    {
      g_my_debug ("update_diagram called while already updating");
      return FALSE;
    }

  already_updating = TRUE;


  gettimeofday (&now, NULL);


  /* We search for new protocols */
  while (known_protocols[pref.stack_level] !=
	 g_list_length (protocols[pref.stack_level]))
    g_list_foreach (protocols[pref.stack_level], (GFunc) check_new_protocol,
		    canvas);

  /* Deletes all nodes and updates traffic values */
  /* TODO To reduce CPU usage, I could just as well update each specific
   * node in update_canvas_nodes and create a new timeout function that would
   * make sure that old nodes get deleted by calling update_nodes, but
   * not as often as with diagram_refresh_period */
  update_nodes ();

  /* Check if there are any new nodes */
#if 0
  g_tree_traverse (nodes, (GTraverseFunc) check_new_node, G_IN_ORDER, canvas);
#endif
  while ((new_node = ape_get_new_node ()))
    check_new_node (new_node->node_id, new_node, canvas);

  /* Update nodes look and delete outdated canvas_nodes */
  do
    {
      n_nodes_before = g_tree_nnodes (canvas_nodes);
      g_tree_traverse (canvas_nodes,
		       (GTraverseFunc) update_canvas_nodes,
		       G_IN_ORDER, canvas);
      n_nodes_after = g_tree_nnodes (canvas_nodes);
    }
  while (n_nodes_before != n_nodes_after);

  /* Limit the number of nodes displayed, if a limit has been set */
  /* TODO check whether this is the right function to use, now that we have a more
   * general display_node called in update_canvas_nodes */
  limit_nodes ();

  /* Reposition canvas_nodes */

  if (need_reposition)
    {
      g_tree_traverse (canvas_nodes,
		       (GTraverseFunc) reposition_canvas_nodes,
		       G_IN_ORDER, canvas);
      need_reposition = 0;
    }


  /* Delete old capture links and update capture link variables */
  update_links ();

  /* Check if there are any new links */
  g_tree_traverse (links, (GTraverseFunc) check_new_link, G_IN_ORDER, canvas);


  /* Update links look 
   * We also delete timedout links, and when we do that we stop
   * traversing, so we need to go on until we have finished updating */

  do
    {
      n_links = g_tree_nnodes (canvas_links);
      g_tree_traverse (canvas_links,
		       (GTraverseFunc) update_canvas_links,
		       G_IN_ORDER, canvas);
      n_links_new = g_tree_nnodes (canvas_links);
    }
  while (n_links != n_links_new);

  /* Update protocol information */
  update_protocols ();

  /* With this we make sure that we don't overload the
   * CPU with redraws */

  if ((last_time.tv_sec == 0) && (last_time.tv_usec == 0))
    last_time = now;

  /* Force redraw */
  while (gtk_events_pending ())
    gtk_main_iteration ();

  gettimeofday (&now, NULL);
  diff = substract_times (now, last_time);
  diff_msecs = diff.tv_sec * 1000 + diff.tv_usec / 1000;
  last_time = now;

  status_string = g_string_new (_("Number of nodes: "));
  g_string_sprintfa (status_string, "%d", n_nodes_after);

  g_string_sprintfa (status_string,
		     _(". Refresh Period: %d"), (int) diff_msecs);

  g_string_sprintfa (status_string,
		     ". Total Packets %g, packets in memory: %g", n_packets,
		     n_mem_packets);
  if (is_idle)
    status_string = g_string_append (status_string, _(". IDLE."));
  else
    status_string = g_string_append (status_string, _(". TIMEOUT."));
  g_my_debug (status_string->str);
  g_string_free (status_string, TRUE);
  status_string = NULL;

#if 0
  g_message ("Total Packets %g, packets in memory: %g", n_packets,
	     n_mem_packets);
#endif

  already_updating = FALSE;

  if (!is_idle)
    {
      if (diff_msecs > pref.refresh_period * 1.2)
	{
/* 	  g_message ("Timeout about to be removed"); */
	  return FALSE;		/* Removes the timeout */
	}
    }
  else
    {
      if (diff_msecs < pref.refresh_period)
	{
/*	  g_message ("Idle about to be removed"); */
	  return FALSE;		/* removes the idle */
	}
    }

  return TRUE;			/* Keep on calling this function */

}				/* update_diagram */


/* Checks whether there is already a legend entry for each known 
 * protocol. If not, create it */
static void
check_new_protocol (protocol_t * protocol, GtkWidget * canvas)
{
  GList *protocol_item = NULL;
  protocol_t *legend_protocol = NULL;
  GtkWidget *prot_table;
  GtkWidget *label;
  GtkStyle *style;
  gchar *color_string;
  guint n_rows = 1, n_columns = 1;
  static gboolean first = TRUE;

  /* First, we check whether the diagram already knows about this protocol,
   * checking whether it is shown on the legend. */
  /*  g_message ("Looking for %s", protocol->name); */
  if ((protocol_item = g_list_find_custom (legend_protocols,
					   protocol->name, protocol_compare)))
    {
      g_my_debug ("Protocol %s found in legend protocols list",
		  protocol->name);
      legend_protocol = (protocol_t *) (protocol_item->data);
      protocol->color = legend_protocol->color;
      return;
    }

  g_my_debug ("Protocol not found. Creating legend item");

  /* It's not, so we build a new entry on the legend */
  /* First, we add a new row to the table */
  prot_table = glade_xml_get_widget (xml, "prot_table");
  g_object_get (G_OBJECT (prot_table), "n_rows", &n_rows, "n_columns",
		&n_columns, NULL);

  /* Glade won't let me define a 0 row table
   * I feel this is ugly, but it's late and I don't feel like
   * cleaning this up :-) */
  if (!first)
    n_rows++;

  first = FALSE;

  /* Then we add the new label widgets */
  label = gtk_label_new (protocol->name);
  gtk_widget_ref (label);
  /* I'm not really sure what this exactly does. I just copied it from 
   * interface.c, but what I'm trying to do is set the name of the widget */
  gtk_object_set_data_full (GTK_OBJECT (app1), protocol->name, label,
			    (GtkDestroyNotify) gtk_widget_unref);


  gtk_widget_show (label);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
  gtk_table_attach (GTK_TABLE (prot_table), label,
		    0, 1, n_rows - 1, n_rows,
		    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_table_resize (GTK_TABLE (prot_table), n_rows + 1, n_columns - 1);
  gtk_widget_queue_resize (GTK_WIDGET (app1));


  color_string = get_prot_color (protocol->name);
  g_my_debug ("Protocol %s in color %s", protocol->name, color_string);
  if (!gdk_color_parse (color_string, &(protocol->color)))
    g_warning (_("Unable to parse color string %s for new protocol %s"),
	       color_string, protocol->name);
  if (!gdk_colormap_alloc_color
      (gdk_colormap_get_system (), &protocol->color, FALSE, TRUE))
    g_warning (_("Unable to allocate color for new protocol %s"),
	       protocol->name);

  style = gtk_style_new ();
  style->fg[GTK_STATE_NORMAL] = protocol->color;
  gtk_widget_set_style (label, style);
  gtk_style_unref (style);

  /* We create a legend protocol and add it to the list, to keep count */
  legend_protocol = g_malloc (sizeof (protocol_t));
  legend_protocol->name = g_strdup (protocol->name);
  legend_protocol->color = protocol->color;
  legend_protocols = g_list_prepend (legend_protocols, legend_protocol);

  known_protocols[pref.stack_level]++;

}				/* check_new_protocol */

/* Frees the legend_protocols list and empties the table of protocols */
void
delete_gui_protocols (void)
{
  GList *item = NULL;
  protocol_t *protocol = NULL;
  GtkWidget *prot_table = NULL;
  guint i = 0;

  item = legend_protocols;

  while (item)
    {
      protocol = item->data;
      g_free (protocol->name);
      g_free (protocol);
      item = item->next;
    }

  g_list_free (legend_protocols);
  legend_protocols = NULL;
  prot_color_index = 0;
  for (; i <= STACK_SIZE; i++)
    known_protocols[i] = 0;

  prot_table = glade_xml_get_widget (xml, "prot_table");

  item = gtk_container_children (GTK_CONTAINER (prot_table));

  while (item)
    {
      gtk_container_remove (GTK_CONTAINER (prot_table),
			    (GtkWidget *) item->data);
      item = item->next;
    }

}				/* delete_gui_protocols */

/* 
 * For a given protocol, returns the color string that should be used
 */
static gchar *
get_prot_color (gchar * name)
{
  gchar **color_protocol = NULL;
  static gchar *color = NULL;
  static gchar *protocol = NULL;
  gint i = 0;

  /* Default is to assign the next color in cycle as long
   * as cycling assigned protocols is set.
   * If it's not, then search for a not assigned color. */

  do
    {
      if (color)
	g_free (color);
      if (protocol)
	g_free (protocol);
      color_protocol = g_strsplit (pref.colors[prot_color_index], ";", 0);
      color = g_strdup (color_protocol[0]);
      protocol = g_strdup (color_protocol[1]);
      g_strfreev (color_protocol);
      prot_color_index++;
      if (prot_color_index >= pref.n_colors)
	prot_color_index = 0;
      i++;
    }
  while ((!pref.cycle) && protocol && strcmp (protocol, "")
	 && (i < pref.n_colors));

  /* But if we find that a particular protocol has a particular color
   * assigned, we override the default */

  for (i = 0; i < pref.n_colors; i++)
    {
      color_protocol = g_strsplit (pref.colors[i], ";", 0);
      if (color_protocol[1] && !strcmp (name, color_protocol[1]))
	{
	  g_free (color);
	  color = g_strdup (color_protocol[0]);
	  if (!prot_color_index)
	    prot_color_index = pref.n_colors - 1;
	  else
	    prot_color_index--;
	}
      g_strfreev (color_protocol);
    }

  return color;

}				/* get_prot_color */


/* Checks if there is a canvas_node per each node. If not, one canvas_node
 * must be created and initiated */
static gint
check_new_node (guint8 * node_id, node_t * node, GtkWidget * canvas)
{
  canvas_node_t *new_canvas_node;
  GnomeCanvasGroup *group;

  if (!node || !node->node_id)
    return FALSE;

  if (display_node (node) && !g_tree_lookup (canvas_nodes, node_id))
    {
      group = gnome_canvas_root (GNOME_CANVAS (canvas));

      new_canvas_node = g_malloc (sizeof (canvas_node_t));
      new_canvas_node->canvas_node_id = g_memdup (node_id, node_id_length);
      new_canvas_node->node = node;

      group = GNOME_CANVAS_GROUP (gnome_canvas_item_new (group,
							 gnome_canvas_group_get_type
							 (), "x", 100.0, "y",
							 100.0, NULL));
      gtk_object_ref (GTK_OBJECT (group));

      new_canvas_node->node_item
	= gnome_canvas_item_new (group,
				 GNOME_TYPE_CANVAS_ELLIPSE,
				 "x1", 0.0,
				 "x2", 0.0,
				 "y1", 0.0,
				 "y2", 0.0,
				 "fill_color", pref.node_color,
				 "outline_color", "black",
				 "width_pixels", 0, NULL);
      gtk_object_ref (GTK_OBJECT (new_canvas_node->node_item));
      new_canvas_node->text_item =
	gnome_canvas_item_new (group, GNOME_TYPE_CANVAS_TEXT,
			       "text", node->name->str,
			       "x", 0.0,
			       "y", 0.0,
			       "anchor", GTK_ANCHOR_CENTER,
			       "font", pref.fontname,
			       "fill_color", pref.text_color, NULL);
      gtk_object_ref (GTK_OBJECT (new_canvas_node->text_item));
      new_canvas_node->group_item = group;

      gnome_canvas_item_raise_to_top (GNOME_CANVAS_ITEM
				      (new_canvas_node->text_item));
      gtk_signal_connect (GTK_OBJECT (new_canvas_node->group_item), "event",
			  (GtkSignalFunc) node_item_event, new_canvas_node);

      g_tree_insert (canvas_nodes,
		     new_canvas_node->canvas_node_id, new_canvas_node);
      g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	     _("Creating canvas_node: %s. Number of nodes %d"),
	     new_canvas_node->node->name->str, g_tree_nnodes (canvas_nodes));

      /*
       * We hide them until we are sure that they will get a proper position
       * in reposition_nodes
       */
      gnome_canvas_item_hide (new_canvas_node->node_item);
      gnome_canvas_item_hide (new_canvas_node->text_item);


      new_canvas_node->is_new = TRUE;
      new_canvas_node->shown = TRUE;
      need_reposition = TRUE;
    }

  return FALSE;			/* False to keep on traversing */
}				/* check_new_node */


/* - updates sizes, names, etc */
static gint
update_canvas_nodes (guint8 * node_id, canvas_node_t * canvas_node,
		     GtkWidget * canvas)
{
  node_t *node;
  gdouble node_size;
  GList *protocol_item;
  protocol_t *protocol = NULL;
  static clock_t start = 0;
  clock_t end;
  gdouble cpu_time_used;
  char *nametmp = NULL;

  /* We don't need this anymore since now update_nodes is called in update_diagram */
#if 0
  node = canvas_node->node;

  /* First we check whether the link has timed out */
  node = update_node (node);
#endif
#if 1
  node = g_tree_lookup (nodes, node_id);
#endif
  /* Remove node if node is too old or if capture is stopped */
  if (!node || !display_node (node))
    {
      if (canvas_node->group_item)
	{
	  gtk_object_destroy (GTK_OBJECT (canvas_node->group_item));
	  gtk_object_unref (GTK_OBJECT (canvas_node->group_item));
	  canvas_node->group_item = NULL;
	}
      if (canvas_node->node_item)
	{
	  gtk_object_destroy (GTK_OBJECT (canvas_node->node_item));
	  gtk_object_unref (GTK_OBJECT (canvas_node->node_item));
	  canvas_node->node_item = NULL;
	}
      if (canvas_node->text_item)
	{
	  gtk_object_destroy (GTK_OBJECT (canvas_node->text_item));
	  gtk_object_unref (GTK_OBJECT (canvas_node->text_item));
	  canvas_node->text_item = NULL;
	}

      g_tree_remove (canvas_nodes, node_id);
      g_my_debug ("Removing canvas_node. Number of nodes %d",
		  g_tree_nnodes (canvas_nodes));
      g_free (node_id);
      g_free (canvas_node);
      need_reposition = TRUE;
      return TRUE;		/* I've checked it's not safe to traverse 
				 * while deleting, so we return TRUE to stop
				 * the traversion (Does that word exist? :-) */
    }

  if (node->main_prot[pref.stack_level])
    {
      protocol_item = g_list_find_custom (protocols[pref.stack_level],
					  node->main_prot[pref.stack_level],
					  protocol_compare);
      if (protocol_item)
	protocol = protocol_item->data;
      else
	g_warning (_("Main node protocol not found in update_canvas_nodes"));
    }

  switch (pref.node_size_variable)
    {
    case INST_TOTAL:
      node_size = get_node_size (node->average);
      break;
    case INST_INBOUND:
      node_size = get_node_size (node->average_in);
      break;
    case INST_OUTBOUND:
      node_size = get_node_size (node->average_out);
      break;
    case ACCU_TOTAL:
      node_size = get_node_size (node->accumulated);
      break;
    case ACCU_INBOUND:
      node_size = get_node_size (node->accumulated_in);
      break;
    case ACCU_OUTBOUND:
      node_size = get_node_size (node->accumulated_out);
      break;
    default:
      node_size = get_node_size (node->average_out);
      g_warning (_("Unknown value or node_size_variable"));
    }


  if (protocol)
    {
      canvas_node->color = protocol->color;

      gnome_canvas_item_set (canvas_node->node_item,
			     "x1", -node_size / 2,
			     "x2", node_size / 2,
			     "y1", -node_size / 2,
			     "y2", node_size / 2,
			     "fill_color_gdk", &(canvas_node->color), NULL);
    }
  else
    {
      guint32 black = 0x000000ff;
      gnome_canvas_item_set (canvas_node->node_item,
			     "x1", -node_size / 2,
			     "x2", node_size / 2,
			     "y1", -node_size / 2,
			     "y2", node_size / 2,
			     "fill_color_rgba", black, NULL);
    }

  /* We check the name of the node, and update the canvas node name
   * if it has changed (useful for non blocking dns resolving) */
  /*TODO why is it exactly that sometimes it is NULL? */
  if (canvas_node->text_item)
    {
      g_object_get (G_OBJECT (canvas_node->text_item), "text", &nametmp,
		    NULL);
      if (strcmp (nametmp, node->name->str))
	{
	  gnome_canvas_item_set (canvas_node->text_item,
				 "text", node->name->str, NULL);
	  gnome_canvas_item_request_update (canvas_node->text_item);
	}

      /* Memprof is telling us that we have to free the string */
      g_free (nametmp);
    }

  /* Processor time check. If too much time has passed, update the GUI */
  end = clock ();
  cpu_time_used = ((gdouble) (end - start)) / CLOCKS_PER_SEC;
  if (cpu_time_used > 0.05)
    {
      /* Force redraw */
      while (gtk_events_pending ())
	gtk_main_iteration ();
      start = end;
    }
  return FALSE;			/* False means keep on calling the function */

}				/* update_canvas_nodes */


/* Returns whether the node in question should be displayed in the
 * diagram or not */
static gboolean
display_node (node_t * node)
{
  struct timeval diff;

  if (!node)
    return FALSE;

  diff = substract_times (now, node->last_time);

  /* There are problems if a canvas_node is deleted if it still
   * has packets, so we have to check that as well */

  /* Remove canvas_node if node is too old */
  if (IS_OLDER (diff, pref.gui_node_timeout_time)
      && pref.gui_node_timeout_time && !node->n_packets)
    return FALSE;

#if 1
  if ((pref.gui_node_timeout_time == 1) && !node->n_packets)
    g_my_critical ("Impossible situation in display node");
#endif

  return TRUE;
}				/* display_node */


/* Sorts canvas nodes with the criterium set in preferences and sets
 * which will be displayed in the diagram */
static void
limit_nodes (void)
{
  static GTree *ordered_nodes = NULL;
  static guint limit;
  displayed_nodes = 0;		/* We'll increment for each node we don't
				 * limit */

  if (pref.node_limit < 0)
    {
      displayed_nodes = g_tree_nnodes (canvas_nodes);
      return;
    }

  limit = pref.node_limit;

  ordered_nodes = g_tree_new (traffic_compare);

  g_tree_traverse (canvas_nodes, (GTraverseFunc) add_ordered_node, G_IN_ORDER,
		   ordered_nodes);
  g_tree_traverse (ordered_nodes, (GTraverseFunc) check_ordered_node,
		   G_IN_ORDER, &limit);
  g_tree_destroy (ordered_nodes);
  ordered_nodes = NULL;
}				/* limit_nodes */

static gint
add_ordered_node (guint8 * node_id, canvas_node_t * node,
		  GTree * ordered_nodes)
{
  g_tree_insert (ordered_nodes, node->node, node);
  g_my_debug ("Adding ordered node. Number of nodes: %d",
	      g_tree_nnodes (ordered_nodes));
  return FALSE;			/* keep on traversing */
}				/* add_ordered_node */

static gint
check_ordered_node (gdouble * traffic, canvas_node_t * node, guint * count)
{
  /* TODO We can probably optimize this by stopping the traversion once
   * the limit has been reached */
  if (*count)
    {
      if (!node->shown)
	need_reposition = TRUE;
      node->shown = TRUE;
      displayed_nodes++;
      (*count)--;
    }
  else
    {
      if (node->shown)
	need_reposition = TRUE;
      node->shown = FALSE;
    }
  return FALSE;			/* keep on traversing */
}				/* check_ordered_node */

/* Comparison function used to order the (GTree *) nodes
 * and canvas_nodes heard on the network */
static gint
traffic_compare (gconstpointer a, gconstpointer b)
{
  node_t *node_a, *node_b;

  g_assert (a != NULL);
  g_assert (b != NULL);

  node_a = (node_t *) a;
  node_b = (node_t *) b;

  if (node_a->average < node_b->average)
    return 1;
  if (node_a->average > node_b->average)
    return -1;

  /* If two nodes have the same traffic, we still have
   * to distinguish them somehow. We use the node_id */

  return (node_id_compare (node_a->node_id, node_b->node_id));

}				/* traffic_compare */


/* Called from update_diagram if the global need_reposition
 * is set. It rearranges the nodes*/
/* TODO I think I should update all links as well, so as not having
 * stale graphics if the diagram has been resized */
static gint
reposition_canvas_nodes (guint8 * ether_addr, canvas_node_t * canvas_node,
			 GtkWidget * canvas)
{
  static gfloat angle = 0.0;
  static guint node_i = 0, n_nodes = 0;
  gdouble x = 0, y = 0, xmin, ymin, xmax, ymax, text_compensation = 50;
  gdouble x_rad_max, y_rad_max;
  gdouble oddAngle = angle;

  if (!canvas_node->shown)
    {
      gnome_canvas_item_hide (canvas_node->node_item);
      gnome_canvas_item_hide (canvas_node->text_item);
      return FALSE;
    }

  gnome_canvas_get_scroll_region (GNOME_CANVAS (canvas),
				  &xmin, &ymin, &xmax, &ymax);
  if (!n_nodes)
    {
      n_nodes = node_i = displayed_nodes;
      g_my_debug ("Displayed nodes = %d", displayed_nodes);
    }

  xmin += text_compensation;
  xmax -= text_compensation;	/* Reduce the drawable area so that
				 * the node name is not lost
				 * TODO: Need a function to calculate
				 * text_compensation depending on font size */
  x_rad_max = 0.9 * (xmax - xmin) / 2;
  y_rad_max = 0.9 * (ymax - ymin) / 2;

  /* TODO I've done all the stationary changes in a hurry
   * I should review it an tidy up all this stuff */
  if (pref.stationary)
    {
      if (canvas_node->is_new)
	{
	  static guint count = 0, base = 1;
	  gdouble angle = 0;

	  if (count == 0)
	    {
	      angle = M_PI * 2.0f;
	      count++;
	    }
	  else
	    {

	      if (count > 2 * base)
		{
		  base *= 2;
		  count = 1;
		}
	      angle = M_PI * (gdouble) count / ((gdouble) base);
	      count += 2;
	    }
	  x = x_rad_max * cos (angle);
	  y = y_rad_max * sin (angle);
	}

    }
  else
    {

      if (n_nodes % 2 == 0)	/* spacing is better when n_nodes is odd and Y is linear */
	oddAngle = (angle * n_nodes) / (n_nodes + 1);
      if (n_nodes > 7)
	{
	  x = x_rad_max * cos (oddAngle);
	  y = y_rad_max * asin (sin (oddAngle)) / (M_PI / 2);
	}
      else
	{
	  x = x_rad_max * cos (angle);
	  y = y_rad_max * sin (angle);
	}
    }

  if (!pref.stationary || canvas_node->is_new)
    {
      /* g_message ("%g %g", x, y); */
      gnome_canvas_item_set (GNOME_CANVAS_ITEM (canvas_node->group_item),
			     "x", x, "y", y, NULL);
      canvas_node->is_new = FALSE;
    }

  /* We update the text font */
  gnome_canvas_item_set (canvas_node->text_item, "font", pref.fontname, NULL);

  if (pref.diagram_only)
    {
      gnome_canvas_item_hide (canvas_node->text_item);
    }
  else
    {
      gnome_canvas_item_show (canvas_node->text_item);
      gnome_canvas_item_request_update (canvas_node->text_item);
    }

  gnome_canvas_item_show (canvas_node->node_item);
  gnome_canvas_item_request_update (canvas_node->node_item);


  node_i--;

  if (node_i)
    angle += 2 * M_PI / n_nodes;
  else
    {
      angle = 0.0;
      n_nodes = 0;
    }

  return FALSE;
}				/* reposition_canvas_nodes */


/* Goes through all known links and checks whether there already exists
 * a corresponding canvas_link. If not, create it.*/
static gint
check_new_link (guint8 * link_id, link_t * link, GtkWidget * canvas)
{
  canvas_link_t *new_canvas_link;
  GnomeCanvasGroup *group;
  GnomeCanvasPoints *points;
  guint i = 0;

  GtkArg args[2];
  args[0].name = "x";
  args[1].name = "y";



  if (!g_tree_lookup (canvas_links, link_id))
    {
      group = gnome_canvas_root (GNOME_CANVAS (canvas));

      new_canvas_link = g_malloc (sizeof (canvas_link_t));
      new_canvas_link->canvas_link_id = g_memdup (link_id,
						  2 * node_id_length);
      new_canvas_link->link = link;

      /* We set the lines position using groups positions */
      points = gnome_canvas_points_new (3);

      for (; i <= 5; i++)
	points->coords[i] = 0.0;

      new_canvas_link->link_item
	= gnome_canvas_item_new (group,
				 gnome_canvas_polygon_get_type (),
				 "points", points, "fill_color", "tan", NULL);
      gtk_object_ref (GTK_OBJECT (new_canvas_link->link_item));


      g_tree_insert (canvas_links,
		     new_canvas_link->canvas_link_id, new_canvas_link);
      gnome_canvas_item_lower_to_bottom (new_canvas_link->link_item);

      gnome_canvas_points_unref (points);

      gtk_signal_connect (GTK_OBJECT (new_canvas_link->link_item), "event",
			  (GtkSignalFunc) link_item_event, new_canvas_link);


      g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	     _("Creating canvas_link: %s-%s. Number of links %d"),
	     link->src_name, link->dst_name, g_tree_nnodes (canvas_links));

    }

  return FALSE;
}				/* check_new_link */


/* - calls update_links, so that the related link updates its average
 *   traffic and main protocol, and old links are deleted
 * - caculates link size and color fading */
static gint
update_canvas_links (guint8 * link_id, canvas_link_t * canvas_link,
		     GtkWidget * canvas)
{
  link_t *link;
  GnomeCanvasPoints *points;
  canvas_node_t *canvas_node;
  GList *protocol_item;
  protocol_t *protocol = NULL;
  gdouble link_size, versorx, versory, modulus;
  struct timeval diff;
  guint32 scaledColor;
  gdouble scale;
  double dx, dy;		/* temporary */

/* We used to run update_link here, but that was a major performance penalty, and now it is done in update_diagram */
  link = g_tree_lookup (links, link_id);

  if (!link)
    {
      /* Right now I'm not very sure in which cases there could be a canvas_link but not a link_item, but
       * I had a not in update_canvas_nodes that if the test is not done it can lead to corruption */
      if (canvas_link->link_item)
	{
	  gtk_object_destroy (GTK_OBJECT (canvas_link->link_item));
	  gtk_object_unref (GTK_OBJECT (canvas_link->link_item));
	  canvas_link->link_item = NULL;
	}

      g_tree_remove (canvas_links, link_id);
      g_my_debug ("Removing canvas link. Number of links %d",
		  g_tree_nnodes (canvas_links));
      g_free (link_id);
      g_free (canvas_link);
      return TRUE;		/* I've checked it's not safe to traverse 
				 * while deleting, so we return TRUE to stop
				 * the traversion (Does that word exist? :-) */
    }

  diff = substract_times (now, link->last_time);

  if (link->main_prot[pref.stack_level])
    {
      protocol_item = g_list_find_custom (protocols[pref.stack_level],
					  link->main_prot[pref.stack_level],
					  protocol_compare);
      if (protocol_item)
	protocol = protocol_item->data;
      else
	g_warning (_("Main link protocol not found in update_canvas_links"));
    }


  points = gnome_canvas_points_new (3);

  /* If either source or destination has disappeared, we hide the link
   * until it can be show again */
  /* TODO: This is a dirty hack. Redo this again later by properly 
   * deleting the link */

  /* We get coords for the destination node */
  canvas_node = g_tree_lookup (canvas_nodes, link_id + node_id_length);
  if (!canvas_node || !canvas_node->shown)
    {
      gnome_canvas_item_hide (canvas_link->link_item);
      gnome_canvas_points_unref (points);
      return FALSE;
    }
  g_object_get (G_OBJECT (canvas_node->group_item), "x", &points->coords[0],
		"y", &points->coords[1], NULL);

  /* We get coords from source node */
  canvas_node = g_tree_lookup (canvas_nodes, link_id);
  if (!canvas_node || !canvas_node->shown)
    {
      gnome_canvas_item_hide (canvas_link->link_item);
      gnome_canvas_points_unref (points);
      return FALSE;
    }

  g_object_get (G_OBJECT (canvas_node->group_item), "x", &dx, "y", &dy, NULL);
  versorx = -(points->coords[1] - dy);
  versory = points->coords[0] - dx;

  modulus = sqrt (pow (versorx, 2) + pow (versory, 2));
  link_size = get_link_size (link->average) / 2;

  points->coords[2] = dx + (versorx / modulus) * link_size;
  points->coords[3] = dy + (versory / modulus) * link_size;
  points->coords[4] = dx - (versorx / modulus) * link_size;
  points->coords[5] = dy - (versory / modulus) * link_size;

  /* TODO What if there never is a protocol?
   * I have to initialize canvas_link->color to a known value */
  if (protocol)
    {
      canvas_link->color = protocol->color;

      if (pref.nofade)
	{
	  guint32 color;
	  color = (((int) (canvas_link->color.red) & 0xFF00) << 16) |
	    (((int) (canvas_link->color.green) & 0xFF00) << 8) |
	    ((int) (canvas_link->color.blue) & 0xFF00) | 0xFF;
	  gnome_canvas_item_set (canvas_link->link_item,
				 "points", points,
				 "fill_color_rgba", color, NULL);
	}
      else
	{
	  /* scale color down to 10% at link timeout */
	  scale =
	    pow (0.10,
		 (diff.tv_sec * 1000.0 +
		  diff.tv_usec / 1000) / pref.link_timeout_time);
	  scaledColor =
	    (((int) (scale * canvas_link->color.red) & 0xFF00) << 16) |
	    (((int) (scale * canvas_link->color.green) & 0xFF00) << 8) |
	    ((int) (scale * canvas_link->color.blue) & 0xFF00) | 0xFF;
	  gnome_canvas_item_set (canvas_link->link_item, "points", points,
				 "fill_color_rgba", scaledColor, NULL);
	}
    }

  else
    {
      guint32 black = 0x000000ff;
      gnome_canvas_item_set (canvas_link->link_item, "points", points,
			     "fill_color_rgba", black, NULL);
    }





  /* If we got this far, the link can be shown. Make sure it is */
  gnome_canvas_item_show (canvas_link->link_item);


  gnome_canvas_points_unref (points);

  return FALSE;

}				/* update_canvas_links */


/* Returs the radius in pixels given average traffic and size mode */
static gdouble
get_node_size (gdouble average)
{
  gdouble result = 0.0;
  switch (pref.size_mode)
    {
    case LINEAR:
      result = average + 1;
      break;
    case LOG:
      result = log (average + 1);
      break;
    case SQRT:
      result = sqrt (average + 1);
      break;
    }
  return (double) (5 + pref.node_radius_multiplier * result);
}

/* Returs the width in pixels given average traffic and size mode */
static gdouble
get_link_size (gdouble average)
{
  gdouble result = 0.0;
  switch (pref.size_mode)
    {
    case LINEAR:
      result = average + 1;
      break;
    case LOG:
      result = log (average + 1);
      break;
    case SQRT:
      result = sqrt (average + 1);
      break;
    }
  return (double) (1 + pref.link_width_multiplier * result);
}				/* get_link_size */



/* Called for every event a link receives. Right now it's used to 
 * set a message in the appbar */
static gint
link_item_event (GnomeCanvasItem * item, GdkEvent * event,
		 canvas_link_t * canvas_link)
{
  static GnomeAppBar *appbar;
  gchar *str;

  if (!appbar)
    appbar = GNOME_APPBAR (glade_xml_get_widget (xml, "appbar1"));

  switch (event->type)
    {

    case GDK_ENTER_NOTIFY:
      if (canvas_link && canvas_link->link
	  && canvas_link->link->main_prot[pref.stack_level])
	str =
	  g_strdup_printf (_("Link main protocol: %s"),
			   canvas_link->link->main_prot[pref.stack_level]);
      else
	str = g_strdup_printf (_("Link main protocol unknown"));
      gnome_appbar_push (appbar, str);
      g_free (str);
      break;
    case GDK_LEAVE_NOTIFY:
      gnome_appbar_pop (appbar);
      break;
    default:
      break;
    }

  return FALSE;
}				/* link_item_event */


/* Called for every event a node receives. Right now it's used to 
 * set a message in the appbar and launch the popup timeout */
static gint
node_item_event (GnomeCanvasItem * item, GdkEvent * event,
		 canvas_node_t * canvas_node)
{

  gdouble item_x, item_y;
  static GnomeAppBar *appbar;
#if 0
  gchar *str;
  static gint popup = 0;
  static struct popup_data pd = { NULL, NULL };
#endif

  if (!appbar)
    appbar = GNOME_APPBAR (glade_xml_get_widget (xml, "appbar1"));

  /* This is not used yet, but it will be. */
  item_x = event->button.x;
  item_y = event->button.y;
  gnome_canvas_item_w2i (item->parent, &item_x, &item_y);

  switch (event->type)
    {

      /* I am finally going to get rid of the hideous popup! */
#if 0
    case GDK_ENTER_NOTIFY:
      pd.canvas_node = canvas_node;
      popup = gtk_timeout_add (1000, (GtkFunction) popup_to, &pd);
      str = g_strdup_printf ("%s (%s)",
			     canvas_node->node->name->str,
			     canvas_node->node->numeric_name->str);
      gnome_appbar_push (appbar, str);
      g_free (str);
      break;
    case GDK_LEAVE_NOTIFY:
      if (popup)
	{
	  gtk_timeout_remove (popup);
	  popup = 0;
	  pd.canvas_node = NULL;
	  pd.node_popup = NULL;
	}
      gnome_appbar_pop (appbar);
      break;
#endif
    case GDK_2BUTTON_PRESS:
      if (!canvas_node || !canvas_node->node)
	return FALSE;
      create_node_info_window (canvas_node);
      break;
    default:
      break;
    }

  return FALSE;

}				/* node_item_event */



#if 0
/* This function is the one that sets ups and displays the node
 * pop up windows */
static guint
popup_to (struct popup_data *pd)
{

  GladeXML *xml_popup;
  GtkLabel *label;
  gchar *str;

  xml_popup =
    glade_xml_new (GLADEDIR "/" ETHERAPE_GLADE_FILE, "node_popup", NULL);
  glade_xml_signal_autoconnect (xml);
  pd->node_popup = glade_xml_get_widget (xml_popup, "node_popup");

  /* TODO Why is not the signal connection being set up automatically?
   * I don't know, and so I have to do it on my own while I investigate
   * the problem */
  gtk_widget_set_events (pd->node_popup, GDK_POINTER_MOTION_MASK);
  gtk_signal_connect (GTK_OBJECT (pd->node_popup), "motion_notify_event",
		      GTK_SIGNAL_FUNC (gtk_widget_destroy), NULL);

  label = (GtkLabel *) glade_xml_get_widget (xml_popup, "name");

  /* This function may be called even before the node has a name
   * If that happens, return */
  if (!(pd->canvas_node) || !(pd->canvas_node->node)
      || !(pd->canvas_node->node->name)
      || !(pd->canvas_node->node->numeric_name))
    return FALSE;

  if (mode == ETHERNET && pd->canvas_node->node->ip_address
      && pd->canvas_node->node->numeric_ip)
    {
      str = g_strdup_printf ("%s (%s, %s)",
			     pd->canvas_node->node->name->str,
			     pd->canvas_node->node->numeric_ip->str,
			     pd->canvas_node->node->numeric_name->str);
      gtk_label_set_text (label, str);
      g_free (str);
    }

  else
    {
      str = g_strdup_printf ("%s (%s)",
			     pd->canvas_node->node->name->str,
			     pd->canvas_node->node->numeric_name->str);
      gtk_label_set_text (label, str);
      g_free (str);
    }


  label = (GtkLabel *) glade_xml_get_widget (xml_popup, "accumulated");
  str =
    g_strdup_printf ("Acummulated bytes: %g",
		     pd->canvas_node->node->accumulated);
  gtk_label_set_text (label, str);
  g_free (str);

  label = (GtkLabel *) glade_xml_get_widget (xml_popup, "average");
  str = g_strdup_printf ("Average bps: %g", pd->canvas_node->node->average);
  gtk_label_set_text (label, str);
  g_free (str);

  gtk_widget_show (GTK_WIDGET (pd->node_popup));

/*  gtk_object_unref(GTK_OBJECT(xml_popup)); */
  return FALSE;			/* Only called once */

}				/* popup_to */
#endif

/* Pushes a string into the appbar status area */

void
set_appbar_status (gchar * str)
{
  static GnomeAppBar *appbar = NULL;
  static gchar *status_string = NULL;

  if (status_string)
    g_free (status_string);

  status_string = g_strdup (str);

  if (!appbar)
    appbar = GNOME_APPBAR (glade_xml_get_widget (xml, "appbar1"));

  gnome_appbar_pop (appbar);
  gnome_appbar_push (appbar, status_string);

}				/* set_appbar_status */

gchar *
traffic_to_str (gdouble traffic, gboolean is_speed)
{
  static gchar *str = NULL;

  if (str)
    g_free (str);

  if (is_speed)
    {
      if (traffic > 1000000)
	str = g_strdup_printf ("%.3f Mbps", traffic / 1000000);
      else if (traffic > 1000)
	str = g_strdup_printf ("%.3f Kbps", traffic / 1000);
      else
	str = g_strdup_printf ("%.0f bps", traffic);
    }
  else
    {
      /* Debug code for sanity check */
      if (traffic && traffic < 1)
	g_warning ("Ill traffic value in traffic_to_str");

      if (traffic > 1024 * 1024)
	str = g_strdup_printf ("%.3f Mbytes", traffic / 1024 / 1024);
      else if (traffic > 1024)
	str = g_strdup_printf ("%.3f Kbytes", traffic / 1024);
      else
	str = g_strdup_printf ("%.0f bytes", traffic);
    }

  return str;
}				/* traffic_to_str */
