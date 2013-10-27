
/*
 * The Real SoundTracker - Common driver module definitions
 *
 * Copyright (C) 1999-2001 Michael Krause
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

#ifndef _ST_DRIVER_H
#define _ST_DRIVER_H

#include <gtk/gtk.h>

#include "preferences.h"

typedef struct st_driver {
    const char *name;

    // create new instance of this driver class
    void *      (*new)           (void);

    // destroy instance of this driver class
    void        (*destroy)       (void *d);

    // open the driver
    gboolean    (*open)          (void *d);

    // close the driver, release audio device
    void        (*release)       (void *d);

    // get pointer to configuration widget
    GtkWidget * (*getwidget)     (void *d);

    // load configuration from provided preferences section
    // can use get/put the functions from preferences.h
    gboolean    (*loadsettings)  (void *d, const gchar *section);

    // save configuration to specified preferences section
    // can use get/put the functions from preferences.h
    gboolean    (*savesettings)  (void *d, const gchar *section);

    // activate the driver when it becomes the current one
    void        (*activate)      (void *d);

    // deactivate the driver
    void        (*deactivate)    (void *d);

    // get time offset since first sound output
    double   (*get_play_time) (void *d);
    int      (*get_play_rate) (void *d);
} st_driver;

#endif /* _ST_DRIVER_H */

