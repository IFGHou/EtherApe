



#include <gtk/gtk.h>


void on_file1_activate (GtkMenuItem * menuitem, gpointer user_data);

void on_new_file1_activate (GtkMenuItem * menuitem, gpointer user_data);

void on_open1_activate (GtkMenuItem * menuitem, gpointer user_data);

void on_save1_activate (GtkMenuItem * menuitem, gpointer user_data);

void on_save_as1_activate (GtkMenuItem * menuitem, gpointer user_data);

void on_exit1_activate (GtkMenuItem * menuitem, gpointer user_data);

void on_cut1_activate (GtkMenuItem * menuitem, gpointer user_data);

void on_copy1_activate (GtkMenuItem * menuitem, gpointer user_data);

void on_paste1_activate (GtkMenuItem * menuitem, gpointer user_data);

void on_clear1_activate (GtkMenuItem * menuitem, gpointer user_data);

void on_properties1_activate (GtkMenuItem * menuitem, gpointer user_data);

void on_preferences1_activate (GtkMenuItem * menuitem, gpointer user_data);

void on_about1_activate (GtkMenuItem * menuitem, gpointer user_data);

gboolean
on_app1_destroy_event (GtkWidget * widget,
		       GdkEvent * event, gpointer user_data);

gboolean
on_app1_delete_event (GtkWidget * widget,
		      GdkEvent * event, gpointer user_data);

void
  on_canvas1_size_allocate (GtkWidget * widget,
			    GtkAllocation * allocation, gpointer user_data);

void on_averaging_spin_adjustment_changed (GtkAdjustment * adj);
void on_refresh_spin_adjustment_changed (GtkAdjustment * adj,
					 GtkWidget * canvas);
void on_node_radius_slider_adjustment_changed (GtkAdjustment * adj);
void on_link_width_slider_adjustment_changed (GtkAdjustment * adj);
void on_node_to_spin_adjustment_changed (GtkAdjustment * adj);
void on_link_to_spin_adjustment_changed (GtkAdjustment * adj);

gboolean
on_node_popup_motion_notify_event (GtkWidget * widget,
				   GdkEventMotion * event,
				   gpointer user_data);

gboolean
on_node_popup_motion_notify_event (GtkWidget * widget,
				   GdkEventMotion * event,
				   gpointer user_data);

gboolean
on_node_popup_motion_notify_event (GtkWidget * widget,
				   GdkEventMotion * event,
				   gpointer user_data);

gboolean
on_name_motion_notify_event (GtkWidget * widget,
			     GdkEventMotion * event, gpointer user_data);

gboolean
on_node_popup_motion_notify_event (GtkWidget * widget,
				   GdkEventMotion * event,
				   gpointer user_data);

gboolean
on_name_motion_notify_event (GtkWidget * widget,
			     GdkEventMotion * event, gpointer user_data);

void
  on_toolbar_check_activate (GtkMenuItem * menuitem,
			     gpointer user_data);

void
  on_legend_check_activate (GtkMenuItem * menuitem,
			    gpointer user_data);

void
  on_status_bar_check_activate (GtkMenuItem * menuitem,
				gpointer user_data);

void
  on_font_button_clicked (GtkButton * button,
			  gpointer user_data);

void
  on_ok_button1_clicked (GtkButton * button,
			 gpointer user_data);

void
  on_cancel_button1_clicked (GtkButton * button,
			     gpointer user_data);

void
  on_apply_button1_clicked (GtkButton * button,
			    gpointer user_data);
void
  on_size_mode_menu_selected (GtkMenuShell * menu_shell,
			      gpointer data);
void
  on_stack_level_menu_selected (GtkMenuShell * menu_shell,
				gpointer data);
void
  on_save_pref_button_clicked (GtkButton * button,
			       gpointer user_data);

void
  on_save_pref_button_clicked (GtkButton * button,
			       gpointer user_data);

void
  on_diagram_only_toggle_toggled (GtkToggleButton * togglebutton,
				  gpointer user_data);

void
  on_diagram_only_toggle_toggled (GtkToggleButton * togglebutton,
				  gpointer user_data);

void
  on_filter_entry_changed (GtkEditable * editable,
			   gpointer user_data);

void
  on_filter_entry_changed (GtkEditable * editable,
			   gpointer user_data);

void
  on_apply_pref_button_clicked (GtkButton * button,
				gpointer user_data);

void
  on_ok_pref_button_clicked (GtkButton * button,
			     gpointer user_data);

void
  on_ok_pref_button_clicked (GtkButton * button,
			     gpointer user_data);

void
  on_cancel_pref_button_clicked (GtkButton * button,
				 gpointer user_data);

void
  on_ok_pref_button_clicked (GtkButton * button,
			     gpointer user_data);

void
  on_cancel_pref_button_clicked (GtkButton * button,
				 gpointer user_data);

void
  on_apply_pref_button_clicked (GtkButton * button,
				gpointer user_data);
