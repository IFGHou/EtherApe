#include "capture.h"


/* Diagram structures */

typedef struct
  {
    guint8 *canvas_node_id;
    node_t *node;
    GnomeCanvasItem *node_item;
    GnomeCanvasItem *text_item;
    GnomeCanvasItem *accu_item;
    gchar *accu_str;
    GnomeCanvasGroup *group_item;
  }
canvas_node_t;

typedef struct
  {
    guint8 *canvas_link_id;
    link_t *link;
    GnomeCanvasItem *link_item;
  }
canvas_link_t;


/* Function definitions */

gdouble get_node_size (gdouble average);
gdouble get_link_size (gdouble average);
gint reposition_canvas_nodes (guint8 * ether_addr, canvas_node_t * canvas_node, GtkWidget * canvas);
gint update_canvas_links (guint8 * ether_link, canvas_link_t * canvas_link, GtkWidget * canvas);
gint update_canvas_nodes (guint8 * ether_addr, canvas_node_t * canvas_node, GtkWidget * canvas);
gint check_new_link (guint8 * ether_link, link_t * link, GtkWidget * canvas);
gint check_new_node (guint8 * ether_addr, node_t * node, GtkWidget * canvas);
guint update_diagram (GtkWidget * canvas);
void init_diagram ();
