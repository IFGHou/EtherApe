#include <gtk/gtk.h>


void
on_file1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

gboolean
on_drawingarea1_configure_event        (GtkWidget       *widget,
                                        GdkEventConfigure *event,
                                        gpointer         user_data);

gboolean
on_drawingarea1_expose_event           (GtkWidget       *widget,
                                        GdkEventExpose  *event,
                                        gpointer         user_data);
