#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"

GdkPixmap *pixmap = NULL;
/* I need to make this global so that I can later draw unto it */
GtkWidget *drawing_area = NULL;

void
on_file1_activate (GtkMenuItem * menuitem,
		   gpointer user_data)
{

}

gboolean
on_drawingarea1_configure_event (GtkWidget * widget,
				 GdkEventConfigure * event,
				 gpointer user_data)
{
  if (pixmap)
    {
      gdk_pixmap_unref (pixmap);
    }

  pixmap = gdk_pixmap_new (widget->window,
			   widget->allocation.width,
			   widget->allocation.height,
			   -1);

  /* Exports the widget's address */
  drawing_area = widget;

  /* Fills the pixmap in white */
  gdk_draw_rectangle (pixmap,
		      widget->style->white_gc,
		      TRUE,
		      0, 0,
		      widget->allocation.width,
		      widget->allocation.height);

  return TRUE;
}

gboolean
on_drawingarea1_expose_event (GtkWidget * widget,
			      GdkEventExpose * event,
			      gpointer user_data)
{
  gdk_draw_pixmap (widget->window,
		   widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
		   pixmap,
		   event->area.x, event->area.y,
		   event->area.x, event->area.y,
		   event->area.width, event->area.height);

  return FALSE;
}
