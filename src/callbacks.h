#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include "globals.h"

gboolean
on_app1_delete_event (GtkWidget * widget,
		      GdkEvent * event, gpointer user_data);
void
on_canvas1_size_allocate (GtkWidget * widget,
			  GtkAllocation * allocation, gpointer user_data);
gboolean
on_node_popup_motion_notify_event (GtkWidget * widget,
				   GdkEventMotion * event,
				   gpointer user_data);
gboolean
on_name_motion_notify_event (GtkWidget * widget,
			     GdkEventMotion * event, gpointer user_data);
