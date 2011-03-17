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
#include "preferences.h"
#include "diagram.h"
#include "capture.h"
#include "datastructs.h"
#include "util.h"

#define MILLI   (1000.0)
#define COLSPACES   "        "

static void color_list_to_pref (void);
static void pref_to_color_list (void);
static gboolean get_version_levels (const gchar * version_string,
				    guint * major, guint * minor,
				    guint * patch);
static gint version_compare (const gchar * a, const gchar * b);
static void on_stack_level_changed(GtkComboBox * combo, gpointer data);
static void on_size_variable_changed(GtkComboBox * combo, gpointer data);
static void on_size_mode_changed(GtkComboBox * combo, gpointer data);
static void on_text_color_changed(GtkColorButton * wdg, gpointer data);
static void on_text_font_changed(GtkFontButton * wdg, gpointer data);
static void on_filter_entry_changed (GtkComboBoxEntry * cbox, gpointer user_data);
static void cbox_add_select(GtkComboBoxEntry *cbox, const gchar *str);


static gboolean colors_changed = FALSE;
static GtkWidget *diag_pref = NULL;		/* Pointer to the diagram configuration window */
static struct pref_struct *tmp_pref = NULL; /* tmp copy of pref data */
static const gchar *pref_group = "Diagram";

void init_config(struct pref_struct *p)
{
  p->input_file = NULL;
  p->export_file = NULL;
  p->export_file_final = NULL;
  p->export_file_signal = NULL;
  p->name_res = TRUE;
  p->mode = IP;
  p->filter = NULL;
  p->refresh_period = 800;	/* ms */
  p->text_color = g_strdup ("yellow");
  p->node_limit = -1;

  p->diagram_only = FALSE;
  p->group_unk = TRUE;
  p->stationary = FALSE;
  p->node_radius_multiplier = 0.0005;
  p->link_node_ratio = 1;
  p->size_mode = LINEAR;
  p->node_size_variable = INST_OUTBOUND;
  p->stack_level = 0;
  p->node_limit = -1;
  p->proto_timeout_time=0;
  p->gui_node_timeout_time=0;
  p->node_timeout_time=0;
  p->proto_node_timeout_time=0;
  p->gui_link_timeout_time=0;
  p->link_timeout_time=0;
  p->proto_link_timeout_time=0;
  p->refresh_period=0;	

  p->text_color=NULL;
  p->fontname=NULL;
  p->colors=NULL;
  
  p->averaging_time=3000;	
  p->interface=NULL;
  p->filter=NULL;
  p->debug_mask = (G_LOG_LEVEL_MASK & ~(G_LOG_LEVEL_DEBUG | G_LOG_LEVEL_INFO));
  p->min_delay = 0;
  p->max_delay = G_MAXULONG;
}

void set_default_config(struct pref_struct *p)
{
  p->mode = IP; /* not saved */
  p->filter = g_strdup("ip or ip6"); /* not saved */
  p->diagram_only = FALSE;
  p->group_unk = TRUE;
  p->stationary = FALSE;
  p->name_res = TRUE;
  p->node_timeout_time = 120000.0;
  p->gui_node_timeout_time = 60000.0;
  p->proto_node_timeout_time = 60000.0;
  p->link_timeout_time = 20000.0;
  p->gui_link_timeout_time = 20000.0;
  p->proto_link_timeout_time = 20000.0;
  p->proto_timeout_time = 600000.0;
  p->averaging_time = 2000.0;
  p->node_radius_multiplier = 0.0005;
  p->link_node_ratio = 1.0;
  p->refresh_period = 100;
  p->size_mode = LINEAR;
  p->node_size_variable = INST_OUTBOUND;
  p->stack_level = 0;

  g_free(p->fontname);
  p->fontname = g_strdup("Sans 8");

  g_free(p->text_color);
  p->text_color = g_strdup("#ffff00");

  g_strfreev(p->colors);
  p->colors = g_strsplit("#ff0000;WWW,HTTP #0000ff;DOMAIN #00ff00 #ffff00 "
                           "#ff00ff #00ffff #ffffff #ff7700 #ff0077 #ffaa77 "
                           "#7777ff #aaaa33",
                           " ", 0);
  p->colors = protohash_compact(p->colors);
  protohash_read_prefvect(p->colors);
}

static void read_string_config(gchar **item, GKeyFile *gkey, const char *key)
{
  gchar *tmp;
  tmp = g_key_file_get_string(gkey, pref_group, key, NULL);
  if (!tmp)
    return;

  /* frees previous value and sets to new pointer */
  g_free(*item);
  *item = tmp;
}

static gchar *config_file_name(void)
{
  return g_strdup_printf("%s/%s", g_get_user_config_dir(), "etherape");
}
static gchar *old_config_file_name(void)
{
  return g_strdup_printf("%s/.gnome2/Etherape", g_get_home_dir());
}

/* loads configuration from .gnome/Etherape */
void load_config(void)
{
  gchar *pref_file;
  gchar *tmpstr;
  gchar **colorarray;
  GKeyFile *gkey;

  /* first reset configurations to defaults */
  set_default_config(&pref);

  gkey = g_key_file_new();

  /* tries to read config from file (~/.config/etherape) */
  pref_file = config_file_name();
  if (!g_key_file_load_from_file(gkey, pref_file, G_KEY_FILE_NONE, NULL))
    {
      /* file not found, try old location (~/.gnome2/Etherape) */
      g_free(pref_file);
      pref_file = old_config_file_name();
      if (!g_key_file_load_from_file(gkey, pref_file, G_KEY_FILE_NONE, NULL))
        {
          g_free(pref_file);
          return; 
        }
    }
  g_free(pref_file);

  read_string_config(&pref.filter, gkey, "filter");
  read_string_config(&pref.fontname, gkey, "fontname");
  read_string_config(&pref.text_color, gkey, "text_color");

  pref.mode = g_key_file_get_integer(gkey, pref_group, "mode", NULL);
  pref.diagram_only = g_key_file_get_boolean(gkey, pref_group, 
                                             "diagram_only", NULL);
  pref.group_unk = g_key_file_get_boolean(gkey, pref_group, "group_unk", NULL);
  pref.stationary = g_key_file_get_boolean(gkey, pref_group, 
                                           "stationary", NULL);
  pref.name_res = g_key_file_get_boolean(gkey, pref_group, "name_res", NULL);
  pref.refresh_period = g_key_file_get_integer(gkey, pref_group, 
                                               "refresh_period", NULL);
  pref.size_mode = g_key_file_get_integer(gkey, pref_group, 
                                          "size_mode", NULL);
  pref.node_size_variable = g_key_file_get_integer(gkey, pref_group, 
                                                   "node_size_variable", NULL);
  pref.stack_level = g_key_file_get_integer(gkey, pref_group, 
                                            "stack_level", NULL);

  pref.node_timeout_time = g_key_file_get_double(gkey, pref_group, 
                                                 "node_timeout_time", NULL); 
  pref.gui_node_timeout_time = g_key_file_get_double(gkey, pref_group, 
                                                     "gui_node_timeout_time", 
                                                     NULL);
  pref.proto_node_timeout_time = g_key_file_get_double(gkey, pref_group, 
                                                       "proto_node_timeout_time", 
                                                       NULL);
  pref.link_timeout_time = g_key_file_get_double(gkey, pref_group, 
                                                 "link_timeout_time", NULL);
  pref.gui_link_timeout_time = g_key_file_get_double(gkey, pref_group, 
                                                     "gui_link_timeout_time", 
                                                     NULL);
  pref.proto_link_timeout_time = g_key_file_get_double(gkey, pref_group, 
                                                       "proto_link_timeout_time", 
                                                       NULL);
  pref.proto_timeout_time = g_key_file_get_double(gkey, pref_group, 
                                                  "proto_timeout_time", NULL);
  pref.averaging_time = g_key_file_get_double(gkey, pref_group, 
                                              "averaging_time", NULL);
  pref.node_radius_multiplier = g_key_file_get_double(gkey, pref_group, 
                                                      "node_radius_multiplier", 
                                                      NULL);
  pref.link_node_ratio = g_key_file_get_double(gkey, pref_group, 
                                               "link_node_ratio", NULL);

  tmpstr = g_key_file_get_string(gkey, pref_group, "colors", NULL);
  if (tmpstr)
    {
      colorarray = g_strsplit(tmpstr, " ", 0);
      if (colorarray)
        {
          g_strfreev(pref.colors);
          pref.colors = protohash_compact(colorarray);
          protohash_read_prefvect(pref.colors);
        }
      g_free(tmpstr);
    }

  /* if needed, read the config version 
  version = g_key_file_get_string(gkey, "General", "version", NULL);
  g_free(version);
  */

  g_key_file_free(gkey);
}				/* load_config */

/* saves configuration to .gnome/Etherape */
/* It's not static since it will be called from the GUI */
void save_config(void)
{
  gchar *pref_file;
  gchar *cfgdata;
  gchar *tmpstr;
  gboolean res;
  GError *error = NULL;
  GKeyFile *gkey;

  gkey = g_key_file_new();
  
  g_key_file_set_boolean(gkey, pref_group, "diagram_only", pref.diagram_only);
  g_key_file_set_boolean(gkey, pref_group, "group_unk", pref.group_unk);
  g_key_file_set_boolean(gkey, pref_group, "name_res", pref.name_res);
  g_key_file_set_double(gkey, pref_group, "node_timeout_time",
			  pref.node_timeout_time);
  g_key_file_set_double(gkey, pref_group, "gui_node_timeout_time",
			  pref.gui_node_timeout_time);
  g_key_file_set_double(gkey, pref_group, "proto_node_timeout_time",
			  pref.proto_node_timeout_time);
  g_key_file_set_double(gkey, pref_group, "link_timeout_time",
			  pref.link_timeout_time);
  g_key_file_set_double(gkey, pref_group, "gui_link_timeout_time",
			  pref.gui_link_timeout_time);
  g_key_file_set_double(gkey, pref_group, "proto_link_timeout_time",
			  pref.proto_link_timeout_time);
  g_key_file_set_double(gkey, pref_group, "proto_timeout_time",
			  pref.proto_timeout_time);
  g_key_file_set_double(gkey, pref_group, "averaging_time", pref.averaging_time);
  g_key_file_set_double(gkey, pref_group, "node_radius_multiplier",
			  pref.node_radius_multiplier);
  g_key_file_set_double(gkey, pref_group, "link_node_ratio",
			  pref.link_node_ratio);
  g_key_file_set_integer(gkey, pref_group, "refresh_period", pref.refresh_period);
  g_key_file_set_integer(gkey, pref_group, "size_mode", pref.size_mode);
  g_key_file_set_integer(gkey, pref_group, "node_size_variable",
			pref.node_size_variable);
  g_key_file_set_integer(gkey, pref_group, "stack_level", pref.stack_level);
  g_key_file_set_string(gkey, pref_group, "fontname", pref.fontname);
  g_key_file_set_string(gkey, pref_group, "text_color", pref.text_color);

  tmpstr = g_strjoinv(" ", pref.colors);
  g_key_file_set_string(gkey, pref_group, "colors", tmpstr);
  g_free(tmpstr);

  g_key_file_set_string(gkey, "General", "version", VERSION);

  /* write config to file */
  cfgdata = g_key_file_to_data(gkey, NULL, NULL);
  pref_file = config_file_name();
  res = g_file_set_contents(pref_file, cfgdata, -1, &error);
  g_free(cfgdata);

  if (res)
    g_my_info (_("Preferences saved to %s"), pref_file);
  else
    {
      GtkWidget *dialog = gtk_message_dialog_new (NULL,
                             GTK_DIALOG_DESTROY_WITH_PARENT,
                             GTK_MESSAGE_ERROR,
                             GTK_BUTTONS_CLOSE,
                             _("Error saving preferences to '%s': %s"),
                             pref_file, 
                             (error && error->message) ? error->message : "");
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
    }
  g_free(pref_file);
}				/* save_config */

/* duplicates a config */
struct pref_struct *
duplicate_config(const struct pref_struct *src)
{
  struct pref_struct *t;

  t = g_malloc(sizeof(struct pref_struct));
  g_assert(t);

  t->input_file = NULL;
  t->export_file = NULL;
  t->export_file_final = NULL;
  t->export_file_signal = NULL;
  t->text_color=NULL;
  t->fontname=NULL;
  t->colors = NULL;
  t->interface=NULL;
  t->filter=NULL;

  copy_config(t, src);

  return t;
}

/* releases all memory allocated for internal fields */
void 
free_config(struct pref_struct *t)
{
  g_free(t->input_file);
  t->input_file = NULL;
  g_free(t->export_file);
  t->export_file = NULL;
  g_free(t->export_file_final);
  t->export_file_final = NULL;
  g_free(t->export_file_signal);
  t->export_file_signal = NULL;
  g_free(t->text_color);
  t->text_color=NULL;
  g_free(t->fontname);
  t->fontname=NULL;

  g_strfreev(t->colors);
  t->colors = NULL;

  g_free(t->interface);
  t->interface=NULL;
  g_free(t->filter);
  t->filter=NULL;
}

/* copies a configuration from src to tgt */
void 
copy_config(struct pref_struct *tgt, const struct pref_struct *src)
{
  if (tgt == src)
	return;

  /* first, reset old data */
  free_config(tgt);

  /* then copy */
  tgt->input_file = g_strdup(src->input_file);
  tgt->export_file = g_strdup(src->export_file);
  tgt->export_file_final = g_strdup(src->export_file_final);
  tgt->export_file_signal = g_strdup(src->export_file_signal);
  tgt->name_res=src->name_res;
  tgt->mode=src->mode;
  tgt->diagram_only = src->diagram_only;
  tgt->group_unk = src->group_unk;
  tgt->stationary = src->stationary;
  tgt->node_radius_multiplier = src->node_radius_multiplier;
  tgt->link_node_ratio = src->link_node_ratio;
  tgt->size_mode = src->size_mode;
  tgt->node_size_variable = src->node_size_variable;
  tgt->text_color=g_strdup(src->text_color);
  tgt->fontname=g_strdup(src->fontname);
  tgt->stack_level = src->stack_level;
  tgt->node_limit = src->node_limit;
  tgt->colors = g_strdupv(src->colors);

  tgt->proto_timeout_time = src->proto_timeout_time;     
  tgt->gui_node_timeout_time = src->gui_node_timeout_time;
  tgt->node_timeout_time = src->node_timeout_time;
  tgt->proto_node_timeout_time = src->proto_node_timeout_time;
  tgt->gui_link_timeout_time = src->gui_link_timeout_time;
  tgt->link_timeout_time = src->link_timeout_time;
  tgt->proto_link_timeout_time = src->proto_link_timeout_time;

  tgt->refresh_period = src->refresh_period;
  tgt->averaging_time = src->averaging_time;

  tgt->interface = g_strdup(src->interface);
  tgt->filter = g_strdup(src->filter);

  tgt->debug_mask = src->debug_mask;
  tgt->min_delay = src->min_delay;
  tgt->max_delay = src->max_delay;
}

static void
confirm_changes(void)
{
  GtkWidget *widget = NULL;

  widget = glade_xml_get_widget (xml, "filter_combo");
  on_filter_entry_changed (GTK_COMBO_BOX_ENTRY(widget), NULL);

  if (colors_changed)
    {
      color_list_to_pref ();
      delete_gui_protocols ();
    }
}				/* confirm_changes */

static void
hide_pref_dialog(void)
{
  /* purge temporary */
  free_config(tmp_pref);
  g_free(tmp_pref);
  tmp_pref = NULL;

  /* reset flags */
  colors_changed = FALSE;

  /* hide dialog */
  gtk_widget_hide (diag_pref);
}

void
initialize_pref_controls(void)
{
  GtkWidget *widget;
  GtkSpinButton *spin;
  GdkColor color;
  GtkTreeModel *model;

  diag_pref = glade_xml_get_widget (xml, "diag_pref");

  /* Updates controls from values of variables */
  widget = glade_xml_get_widget (xml, "node_radius_slider");
  gtk_adjustment_set_value (GTK_RANGE (widget)->adjustment,
			    log (pref.node_radius_multiplier) / log (10));
  g_signal_emit_by_name (G_OBJECT (GTK_RANGE (widget)->adjustment),
			 "changed");
  widget = glade_xml_get_widget (xml, "link_width_slider");
  gtk_adjustment_set_value (GTK_RANGE (widget)->adjustment,
			    pref.link_node_ratio);
  g_signal_emit_by_name (GTK_OBJECT (GTK_RANGE (widget)->adjustment),
			 "changed");
  spin = GTK_SPIN_BUTTON (glade_xml_get_widget (xml, "averaging_spin"));
  gtk_spin_button_set_value (spin, pref.averaging_time);
  spin = GTK_SPIN_BUTTON (glade_xml_get_widget (xml, "refresh_spin"));
  gtk_spin_button_set_value (spin, pref.refresh_period);
  spin = GTK_SPIN_BUTTON (glade_xml_get_widget (xml, "node_to_spin"));
  gtk_spin_button_set_value (spin, pref.node_timeout_time/MILLI);
  spin = GTK_SPIN_BUTTON (glade_xml_get_widget (xml, "gui_node_to_spin"));
  gtk_spin_button_set_value (spin, pref.gui_node_timeout_time/MILLI);
  spin = GTK_SPIN_BUTTON (glade_xml_get_widget (xml, "proto_node_to_spin"));
  gtk_spin_button_set_value (spin, pref.proto_node_timeout_time/MILLI);

  spin = GTK_SPIN_BUTTON (glade_xml_get_widget (xml, "link_to_spin"));
  gtk_spin_button_set_value (spin, pref.link_timeout_time/MILLI);
  spin = GTK_SPIN_BUTTON (glade_xml_get_widget (xml, "gui_link_to_spin"));
  gtk_spin_button_set_value (spin, pref.gui_link_timeout_time/MILLI);
  spin = GTK_SPIN_BUTTON (glade_xml_get_widget (xml, "proto_link_to_spin"));
  gtk_spin_button_set_value (spin, pref.proto_link_timeout_time/MILLI);

  spin = GTK_SPIN_BUTTON (glade_xml_get_widget (xml, "proto_to_spin"));
  gtk_spin_button_set_value (spin, pref.proto_timeout_time/MILLI);

  widget = glade_xml_get_widget (xml, "diagram_only_toggle");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
				pref.diagram_only);
  widget = glade_xml_get_widget (xml, "group_unk_check");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), pref.group_unk);
  widget = glade_xml_get_widget (xml, "name_res_check");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), pref.name_res);
  widget = glade_xml_get_widget (xml, "stack_level");
  gtk_combo_box_set_active(GTK_COMBO_BOX(widget), pref.stack_level);
  widget = glade_xml_get_widget (xml, "size_variable");
  gtk_combo_box_set_active(GTK_COMBO_BOX(widget), pref.node_size_variable);
  widget = glade_xml_get_widget (xml, "size_mode");
  gtk_combo_box_set_active(GTK_COMBO_BOX(widget), pref.size_mode);
  widget = glade_xml_get_widget (xml, "text_font");
  gtk_font_button_set_font_name(GTK_FONT_BUTTON(widget), pref.fontname);
  widget = glade_xml_get_widget (xml, "text_color");
  if (!gdk_color_parse(pref.text_color, &color))
    gdk_color_parse("#ffff00", &color);
  gtk_color_button_set_color(GTK_COLOR_BUTTON(widget), &color);

  widget = glade_xml_get_widget (xml, "filter_combo");
  model = gtk_combo_box_get_model(GTK_COMBO_BOX(widget));
  if (!model)
    {
      GtkListStore *list_store;
      list_store=gtk_list_store_new (1, G_TYPE_STRING);
      gtk_combo_box_set_model(GTK_COMBO_BOX(widget), GTK_TREE_MODEL(list_store));
    }

  pref_to_color_list();		/* Updates the color preferences table with pref.colors */

  /* Connects signals */
  widget = glade_xml_get_widget (xml, "diag_pref");
  g_signal_connect (G_OBJECT (widget),
		    "delete_event",
		    GTK_SIGNAL_FUNC
		    (on_cancel_pref_button_clicked ), NULL);
  widget = glade_xml_get_widget (xml, "node_radius_slider");
  g_signal_connect (G_OBJECT (GTK_RANGE (widget)->adjustment),
		    "value_changed",
		    GTK_SIGNAL_FUNC
		    (on_node_radius_slider_adjustment_changed), NULL);
  widget = glade_xml_get_widget (xml, "link_width_slider");
  g_signal_connect (G_OBJECT (GTK_RANGE (widget)->adjustment),
		    "value_changed",
		    GTK_SIGNAL_FUNC
		    (on_link_width_slider_adjustment_changed), NULL);
  widget = glade_xml_get_widget (xml, "averaging_spin");
  g_signal_connect (G_OBJECT (GTK_SPIN_BUTTON (widget)->adjustment),
		    "value_changed",
		    GTK_SIGNAL_FUNC
		    (on_averaging_spin_adjustment_changed), NULL);
  widget = glade_xml_get_widget (xml, "refresh_spin");
  g_signal_connect (G_OBJECT (GTK_SPIN_BUTTON (widget)->adjustment),
		    "value_changed",
		    GTK_SIGNAL_FUNC
		    (on_refresh_spin_adjustment_changed),
		    glade_xml_get_widget (xml, "canvas1"));
  widget = glade_xml_get_widget (xml, "node_to_spin");
  g_signal_connect (G_OBJECT (GTK_SPIN_BUTTON (widget)->adjustment),
		    "value_changed",
		    GTK_SIGNAL_FUNC
		    (on_node_to_spin_adjustment_changed), NULL);
  widget = glade_xml_get_widget (xml, "gui_node_to_spin");
  g_signal_connect (G_OBJECT (GTK_SPIN_BUTTON (widget)->adjustment),
		    "value_changed",
		    GTK_SIGNAL_FUNC
		    (on_gui_node_to_spin_adjustment_changed), NULL);
  widget = glade_xml_get_widget (xml, "proto_node_to_spin");
  g_signal_connect (G_OBJECT (GTK_SPIN_BUTTON (widget)->adjustment),
		    "value_changed",
		    GTK_SIGNAL_FUNC
		    (on_proto_node_to_spin_adjustment_changed), NULL);
  widget = glade_xml_get_widget (xml, "link_to_spin");
  g_signal_connect (G_OBJECT (GTK_SPIN_BUTTON (widget)->adjustment),
		    "value_changed",
		    GTK_SIGNAL_FUNC
		    (on_link_to_spin_adjustment_changed), NULL);
  widget = glade_xml_get_widget (xml, "gui_link_to_spin");
  g_signal_connect (G_OBJECT (GTK_SPIN_BUTTON (widget)->adjustment),
		    "value_changed",
		    GTK_SIGNAL_FUNC
		    (on_gui_link_to_spin_adjustment_changed), NULL);
  widget = glade_xml_get_widget (xml, "proto_link_to_spin");
  g_signal_connect (G_OBJECT (GTK_SPIN_BUTTON (widget)->adjustment),
		    "value_changed",
		    GTK_SIGNAL_FUNC
		    (on_proto_link_to_spin_adjustment_changed), NULL);
  widget = glade_xml_get_widget (xml, "proto_to_spin");
  g_signal_connect (G_OBJECT (GTK_SPIN_BUTTON (widget)->adjustment),
		    "value_changed",
		    GTK_SIGNAL_FUNC
		    (on_proto_to_spin_adjustment_changed), NULL);
  widget = glade_xml_get_widget (xml, "stack_level");
  g_signal_connect (G_OBJECT (widget), 
                    "changed",
		    GTK_SIGNAL_FUNC (on_stack_level_changed), NULL);
  widget = glade_xml_get_widget (xml, "size_variable");
  g_signal_connect (G_OBJECT (widget), 
                    "changed",
		    GTK_SIGNAL_FUNC (on_size_variable_changed), NULL);
  widget = glade_xml_get_widget (xml, "size_mode");
  g_signal_connect (G_OBJECT (widget), 
                    "changed",
		    GTK_SIGNAL_FUNC (on_size_mode_changed), NULL);
  widget = glade_xml_get_widget (xml, "text_font");
  g_signal_connect (G_OBJECT (widget), 
                    "font_set",
		    GTK_SIGNAL_FUNC (on_text_font_changed), NULL);
  widget = glade_xml_get_widget (xml, "text_color");
  g_signal_connect (G_OBJECT (widget), 
                    "color_set",
		    GTK_SIGNAL_FUNC (on_text_color_changed), NULL);
}

void
on_preferences1_activate (GtkMenuItem * menuitem, gpointer user_data)
{
  GtkComboBoxEntry *cbox;

  /* saves current prefs to a temporary */
  tmp_pref = duplicate_config(&pref);

  initialize_pref_controls();
  
  cbox = GTK_COMBO_BOX_ENTRY(glade_xml_get_widget (xml, "filter_combo"));
  cbox_add_select(cbox, pref.filter);

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

  pref.link_node_ratio = adj->value;
  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
	 _("Adjustment value: %g. Link-node ratio %g"),
	 adj->value, pref.link_node_ratio);

}

void
on_averaging_spin_adjustment_changed (GtkAdjustment * adj)
{
  pref.averaging_time = adj->value;	/* Control and value in ms */
}

void
on_refresh_spin_adjustment_changed (GtkAdjustment * adj, GtkWidget * canvas)
{
  change_refresh_period(adj->value);
}

void
on_node_to_spin_adjustment_changed (GtkAdjustment * adj)
{
  pref.node_timeout_time = adj->value*MILLI;	/* value in ms */
}				/* on_node_to_spin_adjustment_changed */

void
on_gui_node_to_spin_adjustment_changed (GtkAdjustment * adj)
{
  pref.gui_node_timeout_time = adj->value*MILLI;	/* value in ms */
}

void
on_proto_node_to_spin_adjustment_changed (GtkAdjustment * adj)
{
  pref.proto_node_timeout_time = adj->value*MILLI;	/* value in ms */
}

void
on_link_to_spin_adjustment_changed (GtkAdjustment * adj)
{
  pref.link_timeout_time = adj->value*MILLI;	/* value in ms */
}

void
on_gui_link_to_spin_adjustment_changed (GtkAdjustment * adj)
{
  pref.gui_link_timeout_time = adj->value*MILLI;	/* value in ms */
}

void
on_proto_link_to_spin_adjustment_changed (GtkAdjustment * adj)
{
  pref.proto_link_timeout_time = adj->value*MILLI;	/* value in ms */
}

void
on_proto_to_spin_adjustment_changed (GtkAdjustment * adj)
{
  pref.proto_timeout_time = adj->value*MILLI;	/* value in ms */
}

static void on_size_mode_changed(GtkComboBox * combo, gpointer data)
{
  /* Beware! Size mode is an enumeration. The menu options
   * must much the enumaration values */
  pref.size_mode = (size_mode_t)gtk_combo_box_get_active (combo);
}

static void on_size_variable_changed(GtkComboBox * combo, gpointer data)
{
  /* Beware! Size variable is an enumeration. The menu options
   * must much the enumaration values */
  pref.node_size_variable = (node_size_variable_t)gtk_combo_box_get_active (combo);
}

static void on_stack_level_changed(GtkComboBox * combo, gpointer data)
{
  pref.stack_level = gtk_combo_box_get_active (combo);
  delete_gui_protocols();
}

static void on_text_font_changed(GtkFontButton * wdg, gpointer data)
{
  const gchar *str = gtk_font_button_get_font_name(wdg);
  if (str)
    {
      if (pref.fontname)
	g_free (pref.fontname);
      pref.fontname = g_strdup (str);
      ask_reposition(TRUE); /* reposition and update font */
    }
}

static void on_text_color_changed(GtkColorButton * wdg, gpointer data)
{
  GdkColor new_color;
  gtk_color_button_get_color(wdg, &new_color);
  g_free(pref.text_color);
  pref.text_color = gdk_color_to_string(&new_color);
  ask_reposition(TRUE);
}

void
on_diagram_only_toggle_toggled (GtkToggleButton * togglebutton,
				gpointer user_data)
{
  pref.diagram_only = gtk_toggle_button_get_active (togglebutton);
  ask_reposition(FALSE);
}				/* on_diagram_only_toggle_toggled */

void
on_group_unk_check_toggled (GtkToggleButton * togglebutton,
			    gpointer user_data)
{
  pref.group_unk = gtk_toggle_button_get_active (togglebutton);
}				/* on_group_unk_check_toggled */

/*
 * confirm changes
 */
void
on_ok_pref_button_clicked (GtkButton * button, gpointer user_data)
{
  confirm_changes();
  hide_pref_dialog();
}				/* on_ok_pref_button_clicked */


/* reset configuration to saved and close dialog */
void
on_cancel_pref_button_clicked (GtkButton * button, gpointer user_data)
{
  /* reset configuration to saved */
  copy_config(&pref, tmp_pref);

  ask_reposition(TRUE);
  if (colors_changed)
    {
      protohash_read_prefvect(pref.colors);
      delete_gui_protocols ();
    }

  hide_pref_dialog();
}				/* on_cancel_pref_button_clicked */

void
on_save_pref_button_clicked (GtkButton * button, gpointer user_data)
{
  confirm_changes();	/* to save we simulate confirmation */
  save_config ();
  hide_pref_dialog();
}				/* on_save_pref_button_clicked */


/* Makes a new filter */
static void
on_filter_entry_changed (GtkComboBoxEntry * cbox, gpointer user_data)
{
  gchar *str;
  /* TODO should make sure that for each mode the filter is set up
   * correctly */
  str = gtk_combo_box_get_active_text(GTK_COMBO_BOX(cbox));
  if (pref.filter)
    g_free (pref.filter);
  pref.filter = g_strdup (str);
  /* TODO We should look at the error code from set_filter and pop
   * up a window accordingly */
  set_filter (pref.filter, NULL);
  g_free (str);
  cbox_add_select(cbox, pref.filter);
}				/* on_filter_entry_changed */

void
on_numeric_toggle_toggled (GtkToggleButton * togglebutton, gpointer user_data)
{
  pref.name_res = gtk_toggle_button_get_active (togglebutton);
}				/* on_numeric_toggle_toggled */

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
  GtkWidget *dlg =
    glade_xml_get_widget (xml, "colorselectiondialog");
  g_object_set_data( G_OBJECT(dlg), "isadd", GINT_TO_POINTER(TRUE));
  gtk_widget_show (dlg);
}				/* on_color_add_button_clicked */

void
on_color_change_button_clicked (GtkButton * button, gpointer user_data)
{
  GtkTreePath *gpath;
  GtkTreeViewColumn *gcol;
  GtkTreeIter it;
  GdkColor *gdk_color;
  GtkColorSelectionDialog *dlg;
  GtkColorSelection *csel;
  EATreePos ep;

  if (!get_color_store (&ep))
    return;

  /* gets the row (path) at cursor */
  gtk_tree_view_get_cursor (ep.gv, &gpath, &gcol);
  if (!gpath)
    return;			/* no row selected */

  /* get iterator from path */
  if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (ep.gs), &it, gpath))
    return;

  gtk_tree_model_get (GTK_TREE_MODEL (ep.gs), &it, 1, &gdk_color, -1);

  dlg = GTK_COLOR_SELECTION_DIALOG
          (glade_xml_get_widget (xml, "colorselectiondialog"));

  csel = GTK_COLOR_SELECTION(dlg->colorsel);
  gtk_color_selection_set_current_color(csel, gdk_color);
  gtk_color_selection_set_previous_color(csel, gdk_color);
  
  g_object_set_data( G_OBJECT(dlg), "isadd", GINT_TO_POINTER(FALSE));
  gtk_widget_show (GTK_WIDGET(dlg));
}				/* on_color_change_button_clicked */

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

#if GTK_CHECK_VERSION(2,2,0)
  if (gtk_list_store_remove (ep.gs, &it))
    {
      /* iterator still valid, selects current pos */
      gpath = gtk_tree_model_get_path (GTK_TREE_MODEL (ep.gs), &it);
      gtk_tree_view_set_cursor (ep.gv, gpath, NULL, 0);
      gtk_tree_path_free (gpath);
    }
#else
  /* gtk < 2.2 had gtk_list_store_remove void */
  gtk_list_store_remove (ep.gs, &it);
#endif

  colors_changed = TRUE;
  color_list_to_pref ();
}				/* on_color_remove_button_clicked */

void
on_colordiag_ok_clicked (GtkButton * button, gpointer user_data)
{
  GtkWidget *colorsel, *colorseldiag;
  GdkColor gdk_color;
  GtkTreePath *gpath = NULL;
  GtkTreeViewColumn *gcol = NULL;
  GtkTreeIter it;
  gint isadd;
  EATreePos ep;
  if (!get_color_store (&ep))
    return;

  colorseldiag = glade_xml_get_widget (xml, "colorselectiondialog");
  isadd = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(colorseldiag), "isadd"));

  /* gets the row (path) at cursor */
  gtk_tree_view_get_cursor (ep.gv, &gpath, &gcol);
  if (isadd)
    {
      if (gpath)
        {
          /* row sel, add/change color */
          GtkTreeIter itsibling;
          if (!gtk_tree_model_get_iter
              (GTK_TREE_MODEL (ep.gs), &itsibling, gpath))
            return;			/* path not found */
            gtk_list_store_insert_before (ep.gs, &it, &itsibling);
        }
      else
        gtk_list_store_append (ep.gs, &it);	/* no row selected, append */
    }
  else
    {
      if (!gpath || 
          !gtk_tree_model_get_iter(GTK_TREE_MODEL (ep.gs), &it, gpath))
	return;			/* path not found */
    }

  /* get the selected color */
  colorsel = GTK_COLOR_SELECTION_DIALOG (colorseldiag)->colorsel;
  gtk_color_selection_get_current_color (GTK_COLOR_SELECTION (colorsel),
					 &gdk_color);

  /* Since we are only going to save 24bit precision, we might as well
   * make sure we don't display any more than that */
  gdk_color.red = (gdk_color.red >> 8) << 8;
  gdk_color.green = (gdk_color.green >> 8) << 8;
  gdk_color.blue = (gdk_color.blue >> 8) << 8;

  /* fill data */
  if (isadd)
    gtk_list_store_set (ep.gs, &it, 0, COLSPACES, 1, &gdk_color, 2, "", -1);
  else
    gtk_list_store_set (ep.gs, &it, 0, COLSPACES, 1, &gdk_color, -1);

  gtk_widget_hide (colorseldiag);

  color_list_to_pref ();
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
  GtkComboBoxEntry *cbox;
  EATreePos ep;

  if (!get_color_store (&ep))
    return;

  /* gets the row (path) at cursor */
  gtk_tree_view_get_cursor (ep.gv, &gpath, &gcol);
  if (!gpath)
    return;			/* no row selected */

  if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (ep.gs), &it, gpath))
    return;			/* path not found */

  gtk_tree_model_get (GTK_TREE_MODEL (ep.gs), &it, 2, &protocol_string, -1);

  cbox = GTK_COMBO_BOX_ENTRY(glade_xml_get_widget (xml, "protocol_entry"));
  cbox_add_select(cbox, protocol_string);

  g_free (protocol_string);
}


void
on_protocol_edit_ok_clicked (GtkButton * button, gpointer user_data)
{
  gchar *proto_string;
  GtkTreePath *gpath = NULL;
  GtkTreeViewColumn *gcol = NULL;
  GtkTreeIter it;
  GtkComboBoxEntry *cbox;
  EATreePos ep;
  if (!get_color_store (&ep))
    return;

  /* gets the row (path) at cursor */
  gtk_tree_view_get_cursor (ep.gv, &gpath, &gcol);
  if (!gpath)
    return;			/* no row selected */

  if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (ep.gs), &it, gpath))
    return;			/* path not found */

  cbox = GTK_COMBO_BOX_ENTRY(glade_xml_get_widget (xml, "protocol_entry"));
  proto_string = gtk_combo_box_get_active_text(GTK_COMBO_BOX(cbox));
  proto_string = g_utf8_strup (proto_string, -1);
  proto_string = remove_spaces(proto_string);
  
  cbox_add_select(cbox, proto_string);
  gtk_list_store_set (ep.gs, &it, 2, proto_string, -1);

  g_free (proto_string);
  gtk_widget_hide (glade_xml_get_widget (xml, "protocol_edit_dialog"));

  colors_changed = TRUE;
  color_list_to_pref ();
}				/* on_protocol_edit_ok_clicked */



static void
pref_to_color_list (void)
{
  gint i;
  EATreePos ep;
  if (!get_color_store (&ep))
    return;

  /* clear list */
  gtk_list_store_clear (ep.gs);

  for (i = 0; pref.colors[i]; ++i)
    {
      GdkColor gdk_color;
      gchar **colors_protocols = NULL;
      gchar *protocol = NULL;
      GtkTreeIter it;

      colors_protocols = g_strsplit (pref.colors[i], ";", 0);

      /* converting color */
      gdk_color_parse (colors_protocols[0], &gdk_color);
      
      /* converting proto name */
      if (!colors_protocols[1])
	protocol = "";
      else
	protocol = colors_protocols[1];

      /* adds a new row */
      gtk_list_store_append (ep.gs, &it);
      gtk_list_store_set (ep.gs, &it, 0, COLSPACES, 1, &gdk_color, 
                          2, protocol, -1);
      g_strfreev(colors_protocols);
    }
}

/* Called whenever preferences are applied or OKed. Copied whatever there is
 * in the color table to the color preferences in memory */
static void
color_list_to_pref (void)
{
  gint i;
  gint ncolors;
  GtkTreeIter it;

  EATreePos ep;
  if (!get_color_store (&ep))
    return;

  g_strfreev(pref.colors);
  pref.colors = NULL;

  ncolors = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (ep.gs), NULL);
  pref.colors = g_malloc (sizeof (gchar *) * (ncolors+1) );
  g_assert(pref.colors);

  gtk_tree_model_get_iter_first (GTK_TREE_MODEL (ep.gs), &it);
  for (i = 0; i < ncolors; i++)
    {
      gchar *protocol;
      GdkColor *gdk_color;

      /* reads the list */
      gtk_tree_model_get (GTK_TREE_MODEL (ep.gs), &it,
                          1, &gdk_color, 2, &protocol, -1);

      pref.colors[i] = g_strdup_printf ("#%02x%02x%02x;%s", 
                                        gdk_color->red >> 8, 
                                        gdk_color->green >> 8, 
                                        gdk_color->blue >> 8,
                                        protocol);
      g_free (protocol);

      gtk_tree_model_iter_next (GTK_TREE_MODEL (ep.gs), &it);
    }
  pref.colors[ncolors] = NULL;
  
  pref.colors = protohash_compact(pref.colors);
  protohash_read_prefvect(pref.colors);
}

static gint
version_compare (const gchar * a, const gchar * b)
{
  guint a_mj, a_mi, a_pl, b_mj, b_mi, b_pl;

  g_assert (a != NULL);
  g_assert (b != NULL);

  /* TODO What should we return if there was a problem? */
  g_return_val_if_fail ((get_version_levels (a, &a_mj, &a_mi, &a_pl)
			 && get_version_levels (b, &b_mj, &b_mi, &b_pl)), 0);
  if (a_mj < b_mj)
    return -1;
  else if (a_mj > b_mj)
    return 1;
  else if (a_mi < b_mi)
    return -1;
  else if (a_mi > b_mi)
    return 1;
  else if (a_pl < b_pl)
    return -1;
  else if (a_pl > b_pl)
    return 1;
  else
    return 0;
}

static gboolean
get_version_levels (const gchar * version_string,
		    guint * major, guint * minor, guint * patch)
{
  gchar **tokens;

  g_assert (version_string != NULL);

  tokens = g_strsplit (version_string, ".", 0);
  g_return_val_if_fail ((tokens
			 && tokens[0] && tokens[1] && tokens[2]
			 && sscanf (tokens[0], "%d", major)
			 && sscanf (tokens[1], "%d", minor)
			 && sscanf (tokens[2], "%d", patch)), FALSE);
  g_strfreev (tokens);
  return TRUE;
}

/* adds a string to the combo if not already present and loads it in the
   edit box. If str is "" simply clears the edit box */
static void cbox_add_select(GtkComboBoxEntry *cbox, const gchar *str)
{
  GtkTreeModel *model;
  GtkTreeIter iter,iter3;
  gboolean res;
  gchar *modelstr;
  GtkWidget *entry;

  if (!str)
     str = "";
  
  entry = gtk_bin_get_child (GTK_BIN (cbox));
  gtk_entry_set_text (GTK_ENTRY (entry), str);

  if (*str)
    {
      model = gtk_combo_box_get_model(GTK_COMBO_BOX(cbox));
      for (res=gtk_tree_model_get_iter_first (model, &iter);
           res;
           res=gtk_tree_model_iter_next (model, &iter))
        {
          gtk_tree_model_get (model, &iter, 0, &modelstr, -1);
          if (strcmp (str, modelstr) == 0)
            {
              /* already present */
              g_free(modelstr);
              return; 
            }
          g_free(modelstr);
        }

      gtk_list_store_insert_with_values(GTK_LIST_STORE(model), 
                                        &iter3, 0, 0, str, -1);
    }
}

