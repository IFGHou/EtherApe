#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "diagram.h"
#include "math.h"

extern GTree *canvas_nodes;		/* Defined in diagram.c */
extern double node_radius_multiplier;
extern double node_radius_multiplier_control;

void
on_file1_activate (GtkMenuItem * menuitem,
		   gpointer user_data)
{

}


void
on_new_file1_activate (GtkMenuItem * menuitem,
		       gpointer user_data)
{

}


void
on_open1_activate (GtkMenuItem * menuitem,
		   gpointer user_data)
{

}


void
on_save1_activate (GtkMenuItem * menuitem,
		   gpointer user_data)
{

}


void
on_save_as1_activate (GtkMenuItem * menuitem,
		      gpointer user_data)
{

}


void
on_exit1_activate (GtkMenuItem * menuitem,
		   gpointer user_data)
{

}


void
on_cut1_activate (GtkMenuItem * menuitem,
		  gpointer user_data)
{

}


void
on_copy1_activate (GtkMenuItem * menuitem,
		   gpointer user_data)
{

}


void
on_paste1_activate (GtkMenuItem * menuitem,
		    gpointer user_data)
{

}


void
on_clear1_activate (GtkMenuItem * menuitem,
		    gpointer user_data)
{

}


void
on_properties1_activate (GtkMenuItem * menuitem,
			 gpointer user_data)
{

}


void
on_preferences1_activate (GtkMenuItem * menuitem,
			  gpointer user_data)
{

}


void
on_about1_activate (GtkMenuItem * menuitem,
		    gpointer user_data)
{
  GtkWidget *about2;
  about2 = create_about2 ();
  gtk_widget_show (about2);

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

  g_tree_traverse (canvas_nodes, reposition_canvas_nodes, G_IN_ORDER, GNOME_CANVAS (widget));


}


void
on_scrolledwindow1_size_allocate (GtkWidget * widget,
				  GtkAllocation * allocation,
				  gpointer user_data)
{
  GtkWidget *canvas;
  canvas = lookup_widget (GTK_WIDGET (widget), "canvas1");
  gnome_canvas_set_scroll_region (GNOME_CANVAS (canvas),
				  -canvas->allocation.width / 2,
				  -canvas->allocation.height / 2,
				  canvas->allocation.width / 2,
				  canvas->allocation.height / 2);


}

void on_hscale6_adjustment_changed (GtkAdjustment *adj) 
{
   node_radius_multiplier_control=adj->value;
   node_radius_multiplier=exp ((double)adj->value * log (10));
   g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	 _ ("Adjustment value: %g. Radius multiplier %g"),
	 adj->value,
	 node_radius_multiplier);

}
				    
