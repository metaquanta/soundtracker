
/*
 * The Real SoundTracker - GTK+ envelope editor box
 *
 * Copyright (C) 1998-2001 Michael Krause
 * Copytight (C) 2006 Yury Aliaev (Gtk+-2 porting)
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

#include <config.h>

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "gui-subs.h"
#include "envelope-box.h"
#include "xm.h"
#include "gui-settings.h"

static STEnvelope dummy_envelope = {
    { { 0, 0 } },
    1,
    0,
    0,
    0,
    0
};

static void
spin_length_changed (GtkSpinButton *spin,
		     EnvelopeBox *e);

static gboolean
envelope_box_clip_point_movement (EnvelopeBox *e,
				  int p,
				  int *dx,
				  int *dy)
{
    gboolean corrected = FALSE;
    int bound;
    int curx = e->current->points[p].pos;
    int cury = e->current->points[p].val;

    if(dx != NULL) {
	if(p == 0 && *dx != 0) {
	    *dx = 0;
	    corrected = TRUE;
	} else if(*dx < 0) {
	    if(p > 0)
		bound = e->current->points[p-1].pos + 1;
	    else
		bound = 0;
	    if(*dx < bound - curx) {
		*dx = bound - curx;
		corrected = TRUE;
	    }
	} else {
	    if(p < e->current->num_points - 1)
		bound = e->current->points[p+1].pos - 1;
	    else
		bound = 65535;
	    if(*dx > bound - curx) {
		*dx = bound - curx;
		corrected = TRUE;
	    }
	}
    }

    if(dy != NULL) {
	if(*dy < 0) {
	    bound = 0;
	    if(*dy < bound - cury) {
		*dy = bound - cury;
		corrected = TRUE;
	    }
	} else {
	    bound = 64;
	    if(*dy > bound - cury) {
		*dy = bound - cury;
		corrected = TRUE;
	    }
	}
    }

    return corrected;
}

#ifdef USE_CANVAS

#define POINT_ACTIVE "#cccc00"
#define POINT_CURRENT "#ff7777"
#define POINT_NORMAL "#cc0000"

static void
envelope_box_initialize_point_dragging (EnvelopeBox *e,
					GooCanvasItem *eventitem,
					GdkEvent *event,
					GooCanvasItem *point);

static void
envelope_box_stop_point_dragging (EnvelopeBox *e,
				  GdkEvent *event);

static void
envelope_box_move_point (EnvelopeBox *e,
			 int n,
			 int dpos,
			 int dval);

static void
envelope_box_canvas_set_max_x (EnvelopeBox *e,
			       int x);

static gboolean
envelope_box_point_enter (GooCanvasItem *item, GooCanvasItem *target_item,
			  GdkEventCrossing *event,
			  gpointer data)
{
	g_object_set(G_OBJECT(item), "fill-color", POINT_ACTIVE, NULL);
	return FALSE;
}

static gboolean
envelope_box_point_leave (GooCanvasItem *item, GooCanvasItem *target_item,
			  GdkEventCrossing *event,
			  EnvelopeBox *e)
{
	g_object_set(G_OBJECT(item), "fill-color", item == e->cur_point ? POINT_CURRENT : POINT_NORMAL, NULL);
	return FALSE;
}

static gboolean
envelope_box_point_press (GooCanvasItem *item, GooCanvasItem *target_item,
			  GdkEventButton *event,
			  gpointer data)
{
	EnvelopeBox *e = ENVELOPE_BOX(data);

	if(event->button == 1) {
		envelope_box_initialize_point_dragging(e, item, (GdkEvent*)event, item);
		return TRUE;
	}
	return FALSE;
}

static gboolean
envelope_box_point_release (GooCanvasItem *item, GooCanvasItem *target_item,
			  GdkEventButton *event,
			  gpointer data)
{
	EnvelopeBox *e = ENVELOPE_BOX(data);

	if(event->button == 1) {
		envelope_box_stop_point_dragging(e, (GdkEvent*)event);
		return TRUE;
	}
	return FALSE;
}

static gboolean
envelope_box_point_motion (GooCanvasItem *item, GooCanvasItem *target_item,
			  GdkEventMotion *event,
			  gpointer data)
{
	EnvelopeBox *e = ENVELOPE_BOX(data);

	if(e->dragpoint != -1 && event->state & GDK_BUTTON1_MASK) {
		/* Snap movement to integer grid */
		int dx, dy;
		gdouble ex = event->x, ey = event->y;

		goo_canvas_convert_from_item_space(e->canvas, item, &ex, &ey);
		dx = ex - e->dragfromx;
		dy = e->dragfromy - ey;

		if(dx || dy) {
			envelope_box_clip_point_movement(e, e->dragpoint, &dx, &dy);

			envelope_box_move_point(e, e->dragpoint, dx, dy);

			e->dragfromx += dx;
			e->dragfromy -= dy;

			/* Expand scrolling region horizontally, if necessary */
			if(e->dragpoint == e->current->num_points - 1 && e->current->points[e->dragpoint].pos > e->canvas_max_x) {
				envelope_box_canvas_set_max_x(e, e->current->points[e->dragpoint].pos);
			}
		}
	}
	return TRUE;
}

static void
envelope_box_canvas_size_allocate (GooCanvas *c,
				   void *dummy,
				   EnvelopeBox *e)
{
    double newval = (double)GTK_WIDGET(c)->allocation.height / (64 + 10);
    if(newval != e->zoomfactor_base) {
	goo_canvas_set_scale(c, newval * e->zoomfactor_mult);
	e->zoomfactor_base = newval;
    }
}

static void
envelope_box_canvas_set_max_x (EnvelopeBox *e,
			       int x)
{
    e->canvas_max_x = x;
    goo_canvas_set_bounds(e->canvas, -2 - 10 - 10, -4, x + 2 + 10, 66);
}

static void
envelope_box_canvas_paint_grid (EnvelopeBox *e)
{
    GooCanvasItem *item;
    GooCanvasGroup *group;
    int lines[] = {
	0, 0, 0, 64,
	-6, 0, 0, 0,
	-4, 16, 0, 16,
	-6, 32, 0, 32,
	-4, 48, 0, 48,
	-6, 64, 0, 64,
	2, 0, 16384, 0,
	2, 64, 16384, 64,
    };
    int i;

    group = GOO_CANVAS_GROUP (goo_canvas_group_new (goo_canvas_get_root_item(e->canvas),
						       "x", 0.0,
						       "y", 0.0,
						       NULL));

    for(i = 0; i < sizeof(lines) / 4 / sizeof(int); i++) {
	item = goo_canvas_polyline_new_line(GOO_CANVAS_ITEM(group),
	                                    lines[4*i+0],
	                                    lines[4*i+1],
	                                    lines[4*i+2],
	                                    lines[4*i+3],
	                                    "fill-color", "#000088",
	                                    "line-width", 0.5,
	                                     NULL);
	goo_canvas_item_lower(item, NULL);
    }
}

static void
envelope_box_canvas_add_point (EnvelopeBox *e,
			       int n, gboolean current)
{
    // Create new point
    e->points[n] = goo_canvas_rect_new (e->group,
					  (double)e->current->points[n].pos - 1.5,
					  (double)(64-e->current->points[n].val) - 1.5,
					  3.0, 3.0, 
					  "fill-color", current ? POINT_CURRENT : POINT_NORMAL,
					  "stroke-color", "#ff0000",
					  "line-width", 0.0,
					  NULL);
    g_signal_connect (e->points[n], "enter-notify-event",
			G_CALLBACK(envelope_box_point_enter),
			e);
    g_signal_connect (e->points[n], "leave-notify-event",
			G_CALLBACK(envelope_box_point_leave),
			e);
    g_signal_connect (e->points[n], "button-press-event",
			G_CALLBACK(envelope_box_point_press),
			e);
    g_signal_connect (e->points[n], "button-release-event",
			G_CALLBACK(envelope_box_point_release),
			e);
    g_signal_connect (e->points[n], "motion-notify-event",
			G_CALLBACK(envelope_box_point_motion),
			e);

    // Adjust / Create line connecting to the previous point
    if(n > 0) {
	if(e->lines[n - 1]) {
		GooCanvasPoints *points;

		g_object_get(G_OBJECT(e->lines[n - 1]), "points", &points, NULL);
	    points->coords[2] = (double)e->current->points[n].pos;
	    points->coords[3] = (double)(64 - e->current->points[n].val);
	    g_object_set(G_OBJECT(e->lines[n - 1]), "points", points, NULL);
	} else {
	    e->lines[n-1] = goo_canvas_polyline_new_line (e->group,
	                        (double)e->current->points[n-1].pos,
	                        (double)(64 - e->current->points[n-1].val),
	                        (double)e->current->points[n].pos,
	                        (double)(64 - e->current->points[n].val),
						   "fill-color", "black",
						   "line-width", 1.0,
						   NULL);
	    goo_canvas_item_lower(e->lines[n-1], NULL);
	}
    }

    // Adjust / Create line connecting to the next point
    if(n < e->current->num_points - 1) {
	if(e->lines[n]) {
	    printf("muh.\n");
	} else {
	    e->lines[n] = goo_canvas_polyline_new_line (e->group,
	                        (double)e->current->points[n].pos,
	                        (double)(64 - e->current->points[n].val),
	                        (double)e->current->points[n + 1].pos,
	                        (double)(64 - e->current->points[n + 1].val),
						   "fill-color", "black",
						   "line-width", 1.0,
						 NULL);
	    goo_canvas_item_lower(e->lines[n], NULL);
	}
    }
}
#endif

static void
envelope_box_block_loop_spins (EnvelopeBox *e,
			       int block)
{
    guint (*func) (gpointer,
           GSignalMatchType    mask,
		 guint          signal_id,
		 GQuark         detail,
		 GClosure           *closure,
		 gpointer            func,
		 gpointer            data);

    func = block ? g_signal_handlers_block_matched : g_signal_handlers_unblock_matched;

    func(G_OBJECT(e->spin_length), 
	    (GSignalMatchType) G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, e);
    func(G_OBJECT(e->spin_pos),
	    (GSignalMatchType) G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, e);
    func(G_OBJECT(e->spin_offset),
	    (GSignalMatchType) G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, e);
    func(G_OBJECT(e->spin_value),
	    (GSignalMatchType) G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, e);
    func(G_OBJECT(e->spin_sustain),
	    (GSignalMatchType) G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, e);
    func(G_OBJECT(e->spin_loop_start),
	    (GSignalMatchType) G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, e);
    func(G_OBJECT(e->spin_loop_end),
	    (GSignalMatchType) G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, e);
}

static int
envelope_box_insert_point (EnvelopeBox *e,
			   int before,
			   int pos,
			   int val)
{
    /* Check if there's enough room */
    if(e->current->num_points == ST_MAX_ENVELOPE_POINTS)
	return FALSE;
    if(!(before >= 1 && before <= e->current->num_points))
	return FALSE;
    if(pos <= e->current->points[before - 1].pos)
	return FALSE;
    if(before < e->current->num_points && pos >= e->current->points[before].pos)
	return FALSE;

#ifdef USE_CANVAS
	/* Unhighlight previous current point */
	g_object_set(G_OBJECT(e->cur_point), "fill-color", POINT_NORMAL, NULL);
#endif

    // Update envelope structure
    memmove(&e->current->points[before + 1], &e->current->points[before],
	    (ST_MAX_ENVELOPE_POINTS - 1 - before) * sizeof(e->current->points[0]));
    e->current->points[before].pos = pos;
    e->current->points[before].val = val;
    e->current->num_points++;

    // Update GUI
    gtk_spin_button_set_value(e->spin_length, e->current->num_points);
    envelope_box_block_loop_spins(e, TRUE);
    gtk_spin_button_set_value(e->spin_pos, before);
    gtk_spin_button_set_value(e->spin_offset, e->current->points[before].pos);
    gtk_spin_button_set_value(e->spin_value, e->current->points[before].val);
    envelope_box_block_loop_spins(e, FALSE);
    xm_set_modified(1);

#ifdef USE_CANVAS
    // Update Canvas
    memmove(&e->points[before + 1], &e->points[before], (ST_MAX_ENVELOPE_POINTS - 1 - before) * sizeof(e->points[0]));
    memmove(&e->lines[before + 1], &e->lines[before], (ST_MAX_ENVELOPE_POINTS - 2 - before) * sizeof(e->lines[0]));
    e->lines[before] = NULL;
    envelope_box_canvas_add_point(e, before, TRUE);
    /* New current point, already painted highlit */
    e->cur_point = e->points[before];
    e->prev_current = before;
#endif

    return TRUE;
}

static void
envelope_box_delete_point (EnvelopeBox *e,
			   int n)
{
    if(!(n >= 1 && n < e->current->num_points))
	return;

    // Update envelope structure
    memmove(&e->current->points[n], &e->current->points[n + 1],
	    (ST_MAX_ENVELOPE_POINTS - 1 - n) * sizeof(e->current->points[0]));
    e->current->num_points--;

    // Update GUI
    gtk_spin_button_set_value(e->spin_length, e->current->num_points);
    envelope_box_block_loop_spins(e, TRUE);
    gtk_spin_button_set_value(e->spin_pos, n);
    gtk_spin_button_set_value(e->spin_offset, e->current->points[n].pos);
    gtk_spin_button_set_value(e->spin_value, e->current->points[n].val);
    envelope_box_block_loop_spins(e, FALSE);
    xm_set_modified(1);

#ifdef USE_CANVAS
    // Update Canvas
    goo_canvas_item_remove(e->points[n]);
    goo_canvas_item_remove(e->lines[n - 1]);
    memmove(&e->points[n], &e->points[n + 1], (ST_MAX_ENVELOPE_POINTS - 1 - n) * sizeof(e->points[0]));
	memmove(&e->lines[n - 1], &e->lines[n], (ST_MAX_ENVELOPE_POINTS - 1 - n) * sizeof(e->lines[0]));
    e->lines[e->current->num_points - 1] = NULL;
    if(e->lines[n-1]) {
		GooCanvasPoints *points;

		g_object_get(G_OBJECT(e->lines[n - 1]), "points", &points, NULL);
		points->coords[0] = (double)e->current->points[n - 1].pos;
		points->coords[1] = (double)(64 - e->current->points[n - 1].val);
		g_object_set(G_OBJECT(e->lines[n - 1]), "points", points, NULL);
    }
    envelope_box_canvas_set_max_x(e, e->current->points[e->current->num_points - 1].pos);

	/* New current point highlighting */
	if(n < e->current->num_points) { /* If the current point is last, it will be highlit automatically */
		e->cur_point = e->points[n];
		e->prev_current = n;
		g_object_set(G_OBJECT(e->cur_point), "fill-color", POINT_CURRENT, NULL);
	}
#endif
}

#ifdef USE_CANVAS
static void
envelope_box_canvas_point_out_of_sight (EnvelopeBox *e,
					STEnvelopePoint point, gint *dragx, gint *dragy)
{
    double xposwindow = point.pos, y = point.val;
    double bottom = gtk_adjustment_get_upper(e->vadj) - gtk_adjustment_get_page_size(e->vadj)
                  - gtk_adjustment_get_value(e->vadj) + 2.0 * (e->zoomfactor_base * e->zoomfactor_mult); /* :-P */

	goo_canvas_convert_to_pixels(e->canvas, &xposwindow, &y);
	xposwindow -= gtk_adjustment_get_value(e->hadj);

	if(xposwindow < 0)
		*dragx = -1;
	if(xposwindow >= GTK_WIDGET(e->canvas)->allocation.width)
		*dragx = 1;

	if(y < bottom)
		*dragy = -1;
	if(y >= bottom + GTK_WIDGET(e->canvas)->allocation.height)
		*dragy = 1;
}
#endif

/* We assume here that the movement is valid! */
static void
envelope_box_move_point (EnvelopeBox *e,
			 int n,
			 int dpos,
			 int dval)
{
#ifdef USE_CANVAS
	GooCanvasPoints *points;
#endif

    // Update envelope structure
    e->current->points[n].pos += dpos;
    e->current->points[n].val += dval;

    // Update GUI
    envelope_box_block_loop_spins(e, TRUE);
    gtk_spin_button_set_value(e->spin_offset, e->current->points[n].pos);
    gtk_spin_button_set_value(e->spin_value, e->current->points[n].val);
    envelope_box_block_loop_spins(e, FALSE);
    xm_set_modified(1);

#ifdef USE_CANVAS
    // Update Canvas
    goo_canvas_item_translate(e->points[n], dpos, -dval);
    if(n < e->current->num_points - 1) {
	g_object_get(G_OBJECT(e->lines[n]), "points", &points, NULL);

	points->coords[0] += dpos;
	points->coords[1] -= dval;
	g_object_set(G_OBJECT(e->lines[n]), "points", points, NULL);
    }
    if(n > 0) {
	g_object_get(G_OBJECT(e->lines[n - 1]), "points", &points, NULL);

	points->coords[2] += dpos;
	points->coords[3] -= dval;
	g_object_set(G_OBJECT(e->lines[n - 1]), "points", points, NULL);
    }
#endif
}

#ifdef USE_CANVAS

/* This function returns world coordinates for a click on an item, if
   it is given, or else (if item == NULL), assumes the click was on
   the root canvas (event->button.x/y contain screen pixel coords in
   that case). A little confusing, I admit. */

static void
envelope_box_get_world_coords (GooCanvasItem *item,
			       GdkEvent *event,
			       EnvelopeBox *e,
			       double *worldx,
			       double *worldy)
{
	*worldx = event->button.x;
	*worldy = event->button.y;
	if(item == NULL) {
		goo_canvas_convert_from_pixels(e->canvas, worldx, worldy);
	} else
		goo_canvas_convert_from_item_space(e->canvas, item, worldx, worldy);
}

static void
envelope_box_initialize_point_dragging (EnvelopeBox *e,
					GooCanvasItem *eventitem,
					GdkEvent *event,
					GooCanvasItem *point)
{
    GdkCursor *cursor;
    int i;
    double x, y;

    envelope_box_get_world_coords(eventitem, event, e, &x, &y);

    cursor = gdk_cursor_new (GDK_FLEUR);
    goo_canvas_pointer_grab (e->canvas, point,
			    GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
			    cursor,
			    event->button.time);
    gdk_cursor_unref (cursor);

    e->dragfromx = x;
    e->dragfromy = y;

    e->dragpoint = -1;
    for(i = 0; i < 12; i++) {
	if(e->points[i] == point) {
	    e->dragpoint = i;
	    break;
	}
    }
    g_assert(e->dragpoint != -1);

    e->dragging_item_from_x = e->current->points[e->dragpoint].pos;
    e->dragging_item_from_y = e->current->points[e->dragpoint].val;

    envelope_box_block_loop_spins(e, TRUE);
    gtk_spin_button_set_value(e->spin_pos, e->dragpoint);
    gtk_spin_button_set_value(e->spin_offset, e->current->points[e->dragpoint].pos);
    gtk_spin_button_set_value(e->spin_value, e->current->points[e->dragpoint].val);
    envelope_box_block_loop_spins(e, FALSE);

	g_object_set(G_OBJECT(e->cur_point), "fill-color", POINT_NORMAL, NULL);
	e->prev_current = e->dragpoint;
	e->cur_point = point;
}

static void
envelope_box_stop_point_dragging (EnvelopeBox *e,
				  GdkEvent *event)
{
	gint dragx = 0, dragy = 0;
	gdouble dx = 0.0, dy = 0.0, zoom = e->zoomfactor_base * e->zoomfactor_mult;

    if(e->dragpoint == -1)
	return;

    goo_canvas_pointer_ungrab(e->canvas, e->points[e->dragpoint], event->button.time);

    /* Shrink scrolling region horizontally, if necessary */
    if(e->dragpoint == e->current->num_points - 1 && e->current->points[e->dragpoint].pos < e->canvas_max_x) {
	envelope_box_canvas_set_max_x(e, e->current->points[e->dragpoint].pos);
    }

    /* If new location is out of sight, jump there */
	envelope_box_canvas_point_out_of_sight(e, e->current->points[e->dragpoint], &dragx, &dragy);
	if(dragx < 0)
		dx = e->current->points[e->dragpoint].pos - 10.0 / zoom;
	else if(dragx > 0)
		dx = e->current->points[e->dragpoint].pos - (GTK_WIDGET(e->canvas)->allocation.width - 10.0) / zoom;
	else if(dragy)
		dx = gtk_adjustment_get_value(e->hadj) / zoom - 22.0;

	if(dragy < 0)
		dy = 64.0 - e->current->points[e->dragpoint].val - (GTK_WIDGET(e->canvas)->allocation.height - 10.0 )/ zoom;
	else if(dragy > 0)
		dy = 64.0 - e->current->points[e->dragpoint].val - 10.0 / zoom;
	else if(dragx)
		dy = gtk_adjustment_get_value(e->vadj) / zoom - 4.0;

	if(dragx || dragy)
		goo_canvas_scroll_to(e->canvas, dx, dy);
}

static gboolean
scrolled_window_press (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
    GdkCursor *cursor;
    EnvelopeBox *e = ENVELOPE_BOX(data);

	if(event->button == 2) {
	    /* Middle button */
	    if(event->state & GDK_SHIFT_MASK) {
		/* Zoom in/out */
		cursor = gdk_cursor_new (GDK_SIZING);
		goo_canvas_pointer_grab (e->canvas, e->group,
					GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
					cursor,
					event->time);
		gdk_cursor_unref (cursor);

		e->zooming_canvas = TRUE;
		e->dragfromy = event->y - gtk_adjustment_get_value(e->vadj);
		e->zooming_canvas_from_val = e->zoomfactor_mult;

		return TRUE;
	    } else {
		/* Drag canvas */
		cursor = gdk_cursor_new (GDK_FLEUR);
		goo_canvas_pointer_grab (e->canvas, e->group,
					GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
					cursor,
					event->time);
		gdk_cursor_unref (cursor);

		e->dragging_canvas = TRUE;
		e->dragfromx = event->x;
		e->dragfromy = event->y;

	    return TRUE;
	    }
	}

	return FALSE;
}

static gboolean
scrolled_window_release (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
    EnvelopeBox *e = ENVELOPE_BOX(data);

	if(event->button == 2) {
	    goo_canvas_pointer_ungrab (e->canvas, e->group, event->time);
	    e->dragging_canvas = FALSE;
	    e->zooming_canvas = FALSE;
	    return TRUE;
	}

	return FALSE;
}

static gboolean
scrolled_window_motion (GtkScrolledWindow *widget, GdkEventMotion *event, gpointer data)
{
	EnvelopeBox *e = ENVELOPE_BOX(data);
	gdouble lower, upper, delta, new, step;

	if(e->dragging_canvas) {
		lower = gtk_adjustment_get_lower(e->hadj);
		upper = gtk_adjustment_get_upper(e->hadj) - gtk_adjustment_get_page_size(e->hadj);
		delta = event->x;
		step = event->x - e->dragfromx;
		new = gtk_adjustment_get_value(e->hadj) - step;

		e->dragfromx = event->x - step;
		if(new < lower)
			new = lower;
		if(new > upper)
			new = upper;

		gtk_adjustment_set_value(e->hadj, new);

		lower = gtk_adjustment_get_lower(e->vadj);
		upper = gtk_adjustment_get_upper(e->vadj) - gtk_adjustment_get_page_size(e->vadj);
		delta = event->y;
		step = event->y - e->dragfromy;
		new = gtk_adjustment_get_value(e->vadj) - step;

		e->dragfromy = event->y - step;
		if(new < lower)
			new = lower;
		if(new > upper)
			new = upper;

		gtk_adjustment_set_value(e->vadj, new);

		return TRUE;
	} else if(e->zooming_canvas) {
	    gdouble dy = event->y - e->dragfromy - gtk_adjustment_get_value(e->vadj);

	    e->zoomfactor_mult = e->zooming_canvas_from_val - (dy / 20);
	    if(e->zoomfactor_mult < 1.0) {
		e->zoomfactor_mult = 1.0;
	    }
	    goo_canvas_set_scale(e->canvas, e->zoomfactor_base * e->zoomfactor_mult);
	    return TRUE;
	}

	return FALSE;
}

static gboolean
envelope_box_canvas_event (GooCanvas *canvas,
			   GdkEvent *event,
			   gpointer data)
{
    EnvelopeBox *e = ENVELOPE_BOX(data);
    double x, y;
    GooCanvasItem *item;

    int i, insert_after = -1;

    switch (event->type) {
    case GDK_BUTTON_PRESS:
	if(event->button.button == 1) {
	    /* Find out where to insert new point */
	    x = event->button.x;
	    y = event->button.y;
	    goo_canvas_convert_from_pixels(canvas, &x, &y);
	    item = goo_canvas_get_item_at(canvas, x, y, FALSE);
		/* GooCanvas probably has a bug here. The event is send to canvas first, then
		   to the item. */
	    for(i = 0, insert_after = -1; i < e->current->num_points && e->points[i]; i++) {
		if(e->points[i] == item) {
		    /* An already existing point has been selected. Will
		       be handled by envelope_box_point_event(). */
		    return FALSE;
		}
		if(e->current->points[i].pos < (int)x) {
		    insert_after = i;
		}
	    }

	    if(insert_after != -1 && y >= 0 && y < 64) {
		/* Insert new point and start dragging */
		envelope_box_insert_point(e, insert_after + 1, (int)x, 64 - y);
		envelope_box_initialize_point_dragging(e, NULL, event, e->points[insert_after + 1]);
		return TRUE;
	    }
	}
	break;

    case GDK_BUTTON_RELEASE:
	if(event->button.button == 1) {
		x = event->button.x;
		y = event->button.y;
		goo_canvas_convert_from_pixels(canvas, &x, &y);
		item = goo_canvas_get_item_at(canvas, x, y, FALSE);

		for(i = 0, insert_after = -1; i < e->current->num_points && e->points[i]; i++)
			if(e->points[i] == item) {
		    /* An already existing point has been selected. Will
		       be handled by envelope_box_point_event(). */
		    return FALSE;
			}
	    envelope_box_stop_point_dragging(e, event);
	    return TRUE;
	}
	break;

    default:
	break;
    }

    return FALSE;
}
#endif

void envelope_box_set_envelope(EnvelopeBox *e, STEnvelope *env)
{
    int i;
    int m = xm_get_modified();

    g_return_if_fail(e != NULL);

    if(env == NULL) {
	env = &dummy_envelope;
    }

    e->current = env;

    // Some preliminary Paranoia...
    g_assert(env->num_points >= 1 && env->num_points <= ST_MAX_ENVELOPE_POINTS);
    for(i = 0; i < env->num_points; i++) {
	int h = env->points[i].val;
	g_assert(h >= 0 && h <= 64);
    }

    // Update GUI
    e->length_set_modified = FALSE;
    gtk_spin_button_set_value(e->spin_length, env->num_points);
    e->length_set_modified = TRUE;
    envelope_box_block_loop_spins(e, TRUE);
    gtk_spin_button_set_value(e->spin_pos, 0);
    gtk_spin_button_set_value(e->spin_offset, env->points[0].pos);
    gtk_spin_button_set_value(e->spin_value, env->points[0].val);
    gtk_spin_button_set_value(e->spin_sustain, env->sustain_point);
    gtk_spin_button_set_value(e->spin_loop_start, env->loop_start);
    gtk_spin_button_set_value(e->spin_loop_end, env->loop_end);
    envelope_box_block_loop_spins(e, FALSE);

    gtk_toggle_button_set_state(e->enable, env->flags & EF_ON);
    gtk_toggle_button_set_state(e->sustain, env->flags & EF_SUSTAIN);
    gtk_toggle_button_set_state(e->loop, env->flags & EF_LOOP);

#ifdef USE_CANVAS
    // Update Canvas
    for(i = 0; i < (sizeof(e->points) / sizeof(e->points[0])) && e->points[i]; i++) {
	goo_canvas_item_remove(e->points[i]);
	e->points[i] = NULL;
    }
    for(i = 0; i < (sizeof(e->lines) / sizeof(e->lines[0])) && e->lines[i]; i++) {
	goo_canvas_item_remove(e->lines[i]);
	e->lines[i] = NULL;
    }
    for(i = 0; i < env->num_points; i++) {
	envelope_box_canvas_add_point(e, i, !i);
    }
    envelope_box_canvas_set_max_x(e, env->points[env->num_points - 1].pos);
    e->prev_current = 0;
    e->cur_point = e->points[0];
#endif

    xm_set_modified(m);
}

static void handle_toggle_button(GtkToggleButton *t, EnvelopeBox *e)
{
    int flag = 0;

    if(t == e->enable)
	flag = EF_ON;
    else if(t == e->sustain)
	flag = EF_SUSTAIN;
    else if(t == e->loop)
	flag = EF_LOOP;

    g_return_if_fail(flag != 0);

    if(t->active)
	e->current->flags |= flag;
    else
	e->current->flags &= ~flag;

    xm_set_modified(1);
}

static void handle_spin_button(GtkSpinButton *s, EnvelopeBox *e)
{
    unsigned char *p = NULL;

    if(s == e->spin_sustain)
	p = &e->current->sustain_point;
    else if(s == e->spin_loop_start)
	p = &e->current->loop_start;
    else if(s == e->spin_loop_end)
	p = &e->current->loop_end;

    g_return_if_fail(p != NULL);

    *p = gtk_spin_button_get_value_as_int(s);

    xm_set_modified(1);
}

static void
spin_length_changed (GtkSpinButton *spin,
		     EnvelopeBox *e)
{
    int newval = gtk_spin_button_get_value_as_int(spin);

#ifdef USE_CANVAS
	int i;

	if(newval < e->current->num_points)
		for(i = e->current->num_points - 1; i >= newval; i--) {
			goo_canvas_item_remove(e->points[i]);
			goo_canvas_item_remove(e->lines[i - 1]);
			e->lines[i - 1] = NULL;
		}

	if(newval > e->current->num_points)
		for(i = e->current->num_points; i < newval; i++) {
			if(e->current->points[i].pos <= e->current->points[i - 1].pos)
				e->current->points[i].pos = e->current->points[i - 1].pos + 10;
			envelope_box_canvas_add_point(e, i, FALSE);
		}
#endif

	e->current->num_points = newval;
#ifdef USE_CANVAS
	envelope_box_canvas_set_max_x(e, e->current->points[e->current->num_points - 1].pos);
#endif

	gtk_spin_button_set_range(GTK_SPIN_BUTTON(e->spin_pos), 0, newval - 1);
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(e->spin_sustain), 0, newval - 1);
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(e->spin_loop_start), 0, newval - 1);
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(e->spin_loop_end), 0, newval - 1);

	if(e->length_set_modified)
		xm_set_modified(TRUE);
}

static void
spin_pos_changed (GtkSpinButton *spin,
		  EnvelopeBox *e)
{
    int m = xm_get_modified();
    int p = gtk_spin_button_get_value_as_int(e->spin_pos);

    g_assert(p >= 0 && p < e->current->num_points);

    envelope_box_block_loop_spins(e, TRUE);
    gtk_spin_button_set_value(e->spin_offset, e->current->points[p].pos);
    gtk_spin_button_set_value(e->spin_value, e->current->points[p].val);
    envelope_box_block_loop_spins(e, FALSE);
    xm_set_modified(m);

#ifdef USE_CANVAS
	if(e->prev_current < e->current->num_points)
		g_object_set(G_OBJECT(e->cur_point), "fill-color", POINT_NORMAL, NULL);
	e->prev_current = p;
	e->cur_point = e->points[p];
	g_object_set(G_OBJECT(e->cur_point), "fill-color", POINT_CURRENT, NULL);
#endif
}

static void
spin_offset_changed (GtkSpinButton *spin,
		     EnvelopeBox *e)
{
    int p = gtk_spin_button_get_value_as_int(e->spin_pos);
    int dx;

    g_assert(p >= 0 && p < e->current->num_points);

    dx = gtk_spin_button_get_value_as_int(spin) - e->current->points[p].pos;

    envelope_box_clip_point_movement(e, p, &dx, NULL);
    envelope_box_move_point(e, p, dx, 0);

#ifdef USE_CANVAS
    // Horizontal adjustment of scrolling region
    if(p == e->current->num_points - 1) {
	envelope_box_canvas_set_max_x(e, e->current->points[p].pos);
    }
#endif
}

static void
spin_value_changed (GtkSpinButton *spin,
		    EnvelopeBox *e)
{
    int p = gtk_spin_button_get_value_as_int(e->spin_pos);
    int dy;

    g_assert(p >= 0 && p < e->current->num_points);

    dy = gtk_spin_button_get_value_as_int(spin) - e->current->points[p].val;

    envelope_box_clip_point_movement(e, p, NULL, &dy);
    envelope_box_move_point(e, p, 0, dy);
}

static void
insert_clicked (GtkWidget *w,
		EnvelopeBox *e)
{
    int pos = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(e->spin_pos));

	/* We cannot insert point before the beginning */
	if(!pos)
		return;

    envelope_box_insert_point(e, pos, (e->current->points[pos - 1].pos + e->current->points[pos].pos) >> 1,
                                      (e->current->points[pos - 1].val + e->current->points[pos].val) >> 1);
}

static void
delete_clicked (GtkWidget *w,
		EnvelopeBox *e)
{
    envelope_box_delete_point(e, gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(e->spin_pos)));
}

GtkWidget* envelope_box_new(const gchar *label)
{
    EnvelopeBox *e;
    GtkWidget *box2, *thing, *box3, *box4;
#ifdef USE_CANVAS
    GtkWidget *canvas;
#else
	GtkWidget *frame;
#endif

    e = g_object_new(envelope_box_get_type(), NULL);
    GTK_BOX(e)->spacing = 2;
    GTK_BOX(e)->homogeneous = FALSE;

    box2 = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(e), box2, FALSE, TRUE, 0);
    gtk_widget_show(box2);

    thing = gtk_check_button_new_with_label(label);
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing), 0);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    g_signal_connect(thing, "toggled",
		      G_CALLBACK(handle_toggle_button), e);
    e->enable = GTK_TOGGLE_BUTTON(thing);

    add_empty_hbox(box2);

    box2 = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(e), box2, FALSE, TRUE, 0);
    gtk_widget_show(box2);

    /* Numerical list editing fields */
    box3 = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(box2), box3, FALSE, TRUE, 0);
    gtk_widget_show(box3);

    gui_put_labelled_spin_button(box3, _("Length"), 1, 12, (GtkWidget**)&e->spin_length, spin_length_changed, e);
    gui_put_labelled_spin_button(box3, _("Current"), 0, 11, (GtkWidget**)&e->spin_pos, spin_pos_changed, e);
    gui_put_labelled_spin_button(box3, _("Offset"), 0, 65535, (GtkWidget**)&e->spin_offset, spin_offset_changed, e);
    gui_put_labelled_spin_button(box3, _("Value"), 0, 64, (GtkWidget**)&e->spin_value, spin_value_changed, e);

    box4 = gtk_hbox_new(TRUE, 4);
    gtk_box_pack_start(GTK_BOX(box3), box4, FALSE, TRUE, 0);
    gtk_widget_show(box4);

    thing = gtk_button_new_with_label(_("Insert"));
    gtk_box_pack_start(GTK_BOX(box4), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    g_signal_connect(thing, "clicked",
		       G_CALLBACK(insert_clicked), e);
    
    thing = gtk_button_new_with_label(_("Delete"));
    gtk_box_pack_start(GTK_BOX(box4), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    g_signal_connect(thing, "clicked",
		       G_CALLBACK(delete_clicked), e);

    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    // Here comes the graphical stuff
#ifdef USE_CANVAS
	canvas = goo_canvas_new ();
    e->canvas = GOO_CANVAS(canvas);
    g_object_set(G_OBJECT(canvas), "background-color", "#ffffff", NULL); /* GooCanvas allows to set background simply! */

    memset(e->points, 0, sizeof(e->points));
    memset(e->lines, 0, sizeof(e->lines));
    e->zoomfactor_base = 0.0;
    e->zoomfactor_mult = 1.0;
    e->dragpoint = -1;
    e->prev_current = 0;
    e->length_set_modified = TRUE;

    envelope_box_canvas_paint_grid(e);

    e->group = goo_canvas_group_new (goo_canvas_get_root_item(e->canvas),
							  "x", 0.0,
							  "y", 0.0,
							  NULL);

    g_signal_connect_after(canvas, "event",
			      G_CALLBACK(envelope_box_canvas_event),
			      e);

    g_signal_connect_after(canvas, "size_allocate",
			     G_CALLBACK(envelope_box_canvas_size_allocate), e);

	thing = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(thing), GTK_POLICY_ALWAYS, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(thing), GTK_SHADOW_ETCHED_IN);
	g_signal_connect(thing, "button-press-event", G_CALLBACK(scrolled_window_press), e);
	g_signal_connect(thing, "button-release-event", G_CALLBACK(scrolled_window_release), e);
	g_signal_connect(thing, "motion-notify-event", G_CALLBACK(scrolled_window_motion), e);

	e->hadj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(thing));
	e->vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(thing));

    gtk_box_pack_start (GTK_BOX (box2), thing, TRUE, TRUE, 0);
    gtk_widget_set_size_request(canvas, 30, 64);
    gtk_container_add (GTK_CONTAINER (thing), canvas);
    gtk_widget_show_all(thing);

#else /* !USE_CANVAS */

    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
    gtk_widget_show (frame);

    gtk_box_pack_start (GTK_BOX (box2), frame, TRUE, TRUE, 0);

    thing = gtk_label_new(_("Graphical\nEnvelope\nEditor\nonly with\nGooCanvas"));
    gtk_widget_show(thing);
    gtk_container_add (GTK_CONTAINER (frame), thing);

#endif /* defined(USE_CANVAS) */

    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    /* Sustain / Loop widgets */
    box3 = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(box2), box3, FALSE, TRUE, 0);
    gtk_widget_show(box3);

    thing = gtk_check_button_new_with_label(_("Sustain"));
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing), 0);
    gtk_box_pack_start(GTK_BOX(box3), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    g_signal_connect(thing, "toggled",
		       G_CALLBACK(handle_toggle_button), e);
    e->sustain = GTK_TOGGLE_BUTTON(thing);

    gui_put_labelled_spin_button(box3, _("Point"), 0, 11, (GtkWidget**)&e->spin_sustain, handle_spin_button, e);

    thing = gtk_check_button_new_with_label(_("Loop"));
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing), 0);
    gtk_box_pack_start(GTK_BOX(box3), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    g_signal_connect(thing, "toggled",
		       G_CALLBACK(handle_toggle_button), e);
    e->loop = GTK_TOGGLE_BUTTON(thing);

    gui_put_labelled_spin_button(box3, _("Start"), 0, 11, (GtkWidget**)&e->spin_loop_start, handle_spin_button, e);
    gui_put_labelled_spin_button(box3, _("End"), 0, 11, (GtkWidget**)&e->spin_loop_end, handle_spin_button, e);

    return GTK_WIDGET(e);
}

guint envelope_box_get_type()
{
    static guint envelope_box_type = 0;
    
    if (!envelope_box_type) {
	GTypeInfo envelope_box_info =
	{
	    sizeof(EnvelopeBoxClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
	    (GClassInitFunc) NULL,
	    (GClassFinalizeFunc) NULL,
	    NULL,
	    sizeof(EnvelopeBox),
	    0,
	    (GInstanceInitFunc) NULL,
	};
	
	envelope_box_type = g_type_register_static(gtk_vbox_get_type (),"EnvelopeBox",
			&envelope_box_info, (GTypeFlags)0);
    }
    
    return envelope_box_type;
}
