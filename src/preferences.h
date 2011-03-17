/* Etherape
 * Copyright (C) 2000 Juan Toledo
 * Copyright (C) 2011 Riccardo Ghetta
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

#ifndef PREFERENCES_H
#define PREFERENCES_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include "globals.h"
#include "math.h"

void load_config(void);
void save_config(void);

void init_config(struct pref_struct *cfg);
void set_default_config(struct pref_struct *cfg);
struct pref_struct *duplicate_config(const struct pref_struct *src);
void free_config(struct pref_struct *t);
void copy_config(struct pref_struct *tgt, const struct pref_struct *src);

#endif 
