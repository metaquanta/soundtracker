
/*
 * The Real SoundTracker - Preferences handling (header)
 *
 * Copyright (C) 2013 Yury Alyaev
 * Copyright (C) 1998-2001 Michael Krause
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _ST_PREFERENCES_H
#define _ST_PREFERENCES_H

#include <glib.h>
#include <gdk/gdkcolor.h>

void           prefs_init                     (void);
void           prefs_save                     (void);
void           prefs_close                    (void);

// Returns the file location of a configuration node
gchar *        prefs_get_prefsdir             (void);
gchar *        prefs_get_filename             (const gchar *name);

gboolean
               prefs_remove_key               (const gchar *section,
					       const gchar *key);

void           prefs_put_int                  (const gchar *section,
					       const gchar *key,
					       const gint value);
void           prefs_put_bool                 (const gchar *section,
					       const gchar *key,
					       const gboolean value);
void           prefs_put_color                (const gchar *section,
					       const gchar *key,
					       const GdkColor value);
void           prefs_put_string               (const gchar *section,
					       const gchar *key,
					       const gchar *value);
/* Currently is not needed
void           prefs_put_str_array            (const gchar *section,
					       const gchar *key,
					       const gchar * const *value,
					       gsize length);
*/
void           prefs_put_int_array            (const gchar *section,
					       const gchar *key,
					       gint *value,
					       gsize length);
void           prefs_put_bool_array           (const gchar *section,
					       const gchar *key,
					       gboolean *value,
					       gsize length);
gint           prefs_get_int                  (const gchar *section,
					       const gchar *key,
					       const gint deflt);
gboolean       prefs_get_bool                 (const gchar *section,
					       const gchar *key,
					       const gboolean deflt);
GdkColor       prefs_get_color                (const gchar *section,
					       const gchar *key,
					       const GdkColor deflt);
gchar*         prefs_get_string               (const gchar *section,
					       const gchar *key,
					       const gchar *deflt);

/* No default value, return NULL on failure */
gchar**        prefs_get_str_array            (const gchar *section,
					       const gchar *key,
					       gsize *length);
gint*          prefs_get_int_array            (const gchar *section,
					       const gchar *key,
					       gsize *length);
gboolean*      prefs_get_bool_array           (const gchar *section,
					       const gchar *key,
					       gsize *length);
/* No default value, return 0 on failure */
gsize          prefs_get_pairs                (const gchar *section,
					       gchar ***keys,
					       gchar ***values);

#endif /* _ST_PREFERENCES_H */
