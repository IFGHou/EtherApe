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

#include "preferences.h"
#include "datastructs.h"

static gboolean get_version_levels (const gchar * version_string,
				    guint * major, guint * minor,
				    guint * patch);
static gint version_compare (const gchar * a, const gchar * b);

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

