
/*
 * The Real SoundTracker - GTK+ envelope editor box (header)
 *
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

#ifndef _ENVELOPE_BOX_H
#define _ENVELOPE_BOX_H

#include <config.h>

#include <gtk/gtk.h>

#ifdef USE_CANVAS
#include <goocanvas.h>
#endif

#include "xm.h"

#define ENVELOPE_BOX(obj)          GTK_CHECK_CAST (obj, envelope_box_get_type (), EnvelopeBox)
#define ENVELOPE_BOX_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, envelope_box_get_type (), EnvelopeBoxClass)
#define IS_ENVELOPE_BOX(obj)       GTK_CHECK_TYPE (obj, envelope_box_get_type ())

typedef struct _EnvelopeBox       EnvelopeBox;
typedef struct _EnvelopeBoxClass  EnvelopeBoxClass;

struct _EnvelopeBox
{
    GtkVBox vbox;

    STEnvelope *current;

    GtkSpinButton *spin_length;
    GtkSpinButton *spin_pos;
    GtkSpinButton *spin_offset;
    GtkSpinButton *spin_value;
    GtkSpinButton *spin_sustain;
    GtkSpinButton *spin_loop_start;
    GtkSpinButton *spin_loop_end;

    GtkToggleButton *enable;
    GtkToggleButton *sustain;
    GtkToggleButton *loop;

    gboolean length_set_modified;

#ifdef USE_CANVAS
    GooCanvas *canvas;
    GooCanvasItem *group;
    GooCanvasItem *points[ST_MAX_ENVELOPE_POINTS], *cur_point;
    GooCanvasItem *lines[ST_MAX_ENVELOPE_POINTS - 1];
    GtkAdjustment *hadj, *vadj;

    int canvas_max_x;
    gdouble zoomfactor_base;
    gdouble zoomfactor_mult;

    gdouble dragging_item_from_x, dragging_item_from_y;     /* world coordinates */
    gdouble dragfromx, dragfromy;                        /* screen pixel coordinates */
    gdouble zooming_canvas_from_val;

    int dragpoint;
    gboolean dragging_canvas;
    gboolean zooming_canvas;
    guint prev_current;
#endif
};

struct _EnvelopeBoxClass
{
    GtkVBoxClass parent_class;
};

guint          envelope_box_get_type           (void);
GtkWidget*     envelope_box_new                (const gchar *label);

void           envelope_box_set_envelope       (EnvelopeBox *e, STEnvelope *env);

#endif /* _ENVELOPE_BOX_H */
