#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "diagram.h"
#include "math.h"

extern GTree *canvas_nodes;	/* Defined in diagram.c */
extern double averaging_time;
extern double node_radius_multiplier;
extern double node_radius_multiplier_control;
extern double link_width_multiplier;
extern double link_width_multiplier_control;
extern double link_timeout_time;
extern double node_timeout_time;
extern guint32 refresh_period;
extern gint diagram_timeout;

void
on_file1_activate (GtkMenuItem * menuitem,
		   gpointer user_data)
{
  GtkWidget *messagebox;
  messagebox = create_messagebox1 ();
  gtk_widget_show (messagebox);
}


void
on_new_file1_activate (GtkMenuItem * menuitem,
		       gpointer user_data)
{
  GtkWidget *messagebox;
  messagebox = create_messagebox1 ();
  gtk_widget_show (messagebox);
}


void
on_open1_activate (GtkMenuItem * menuitem,
		   gpointer user_data)
{
  GtkWidget *messagebox;
  messagebox = create_messagebox1 ();
  gtk_widget_show (messagebox);
}


void
on_save1_activate (GtkMenuItem * menuitem,
		   gpointer user_data)
{
  GtkWidget *messagebox;
  messagebox = create_messagebox1 ();
  gtk_widget_show (messagebox);
}


void
on_save_as1_activate (GtkMenuItem * menuitem,
		      gpointer user_data)
{
  GtkWidget *messagebox;
  messagebox = create_messagebox1 ();
  gtk_widget_show (messagebox);
}


void
on_exit1_activate (GtkMenuItem * menuitem,
		   gpointer user_data)
{
  gtk_exit (0);
}


void
on_cut1_activate (GtkMenuItem * menuitem,
		  gpointer user_data)
{
  GtkWidget *messagebox;
  messagebox = create_messagebox1 ();
  gtk_widget_show (messagebox);

}


void
on_copy1_activate (GtkMenuItem * menuitem,
		   gpointer user_data)
{
  GtkWidget *messagebox;
  messagebox = create_messagebox1 ();
  gtk_widget_show (messagebox);

}


void
on_paste1_activate (GtkMenuItem * menuitem,
		    gpointer user_data)
{
  GtkWidget *messagebox;
  messagebox = create_messagebox1 ();
  gtk_widget_show (messagebox);

}


void
on_clear1_activate (GtkMenuItem * menuitem,
		    gpointer user_data)
{
  GtkWidget *messagebox;
  messagebox = create_messagebox1 ();
  gtk_widget_show (messagebox);

}


void
on_properties1_activate (GtkMenuItem * menuitem,
			 gpointer user_data)
{
  GtkWidget *messagebox;
  messagebox = create_messagebox1 ();
  gtk_widget_show (messagebox);

}


void
on_preferences1_activate (GtkMenuItem * menuitem,
			  gpointer user_data)
{
  GtkWidget *messagebox;
  messagebox = create_messagebox1 ();
  gtk_widget_show (messagebox);

}


void
on_about1_activate (GtkMenuItem * menuitem,
		    gpointer user_data)
{
  GtkWidget *about;
  about = create_about2 ();
  gtk_widget_show (about);

}


gboolean
on_app1_delete_event (GtkWidget * widget,
		      GdkEvent * event,
		      gpointer user_data)
{
  gtk_exit (0);
  return FALSE;
}


void
on_canvas1_size_allocate (GtkWidget * widget,
			  GtkAllocation * allocation,
			  gpointer user_data)
{

  gnome_canvas_set_scroll_region (GNOME_CANVAS (widget),
				  -widget->allocation.width / 2,
				  -widget->allocation.height / 2,
				  widget->allocation.width / 2,
				  widget->allocation.height / 2);

  /* We have to make sure we put all nodes on it's place now */
  g_tree_traverse (canvas_nodes,
		   (GTraverseFunc) reposition_canvas_nodes,
		   G_IN_ORDER,
		   GNOME_CANVAS (widget));

}

void
on_node_radius_slider_adjustment_changed (GtkAdjustment * adj)
{
  node_radius_multiplier_control = adj->value;
  node_radius_multiplier = exp ((double) adj->value * log (10));
  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	 _ ("Adjustment value: %g. Radius multiplier %g"),
	 adj->value,
	 node_radius_multiplier);

}
void
on_link_width_slider_adjustment_changed (GtkAdjustment * adj)
{
  link_width_multiplier_control = adj->value;
  link_width_multiplier = exp ((double) adj->value * log (10));
  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	 _ ("Adjustment value: %g. Radius multiplier %g"),
	 adj->value,
	 link_width_multiplier);

}
void 
on_averaging_spin_adjustment_changed (GtkAdjustment * adj)
{
  averaging_time = adj->value * 1000;	/* Control in ms, value in us */
}
void 
on_refresh_spin_adjustment_changed (GtkAdjustment * adj, GtkWidget * canvas)
{
  gtk_timeout_remove (diagram_timeout);
  refresh_period = adj->value;
  diagram_timeout = gtk_timeout_add (refresh_period /* ms */ ,
				     (GtkFunction) update_diagram,
				     canvas);
}
void 
on_node_to_spin_adjustment_changed (GtkAdjustment * adj)
{
  node_timeout_time = adj->value * 1000;	/* Control in ms, value in us */
}
void 
on_link_to_spin_adjustment_changed (GtkAdjustment * adj)
{
  link_timeout_time = adj->value * 1000;	/* Control in ms, value in us */
}
