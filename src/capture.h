/* Etherape
 * Copyright (C) 2000 Juan Toledo
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
#ifndef ETHERAPE_CAPTURE_H
#define ETHERAPE_CAPTURE_H

/* Possible states of capture status */
enum status_t
{
  STOP = 0, 
  PLAY = 1, 
  PAUSE = 2,
  CAP_EOF = 3 /* end-of-file */
};

enum status_t get_capture_status(void);

gchar *init_capture (void);
gboolean start_capture (void);
gboolean pause_capture (void);
gboolean stop_capture (void);
void cleanup_capture (void);
gint set_filter (gchar * filter, gchar * device);

#endif
