
/*
 * The Real SoundTracker - GUI configuration dialog
 *
 * Copyright (C) 1999-2001 Michael Krause
 * Copyright (C) 2006 Yury Aliaev (GTK+-2 porting)
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#include "extspinbutton.h"
#include "gui-settings.h"
#include "gui-subs.h"
#include "gui.h"
#include "preferences.h"
#include "scope-group.h"
#include "track-editor.h"
#include "tracker-settings.h"

#define SECTION "settings"
#define SECTION_ALWAYS "settings-always"

gui_prefs gui_settings;

static GtkWidget* col_samples[TRACKERCOL_LAST];
static GtkWidget* colorsel;
static guint active_col_item = 0;

static GtkWidget* ts_box = NULL;

static void prefs_scopesfreq_changed(int value);
static void prefs_trackerfreq_changed(int value);

static gui_subs_slider prefs_scopesfreq_slider = {
    N_("Scopes Frequency"), 1, 80, prefs_scopesfreq_changed
};
static gui_subs_slider prefs_trackerfreq_slider = {
    N_("Tracker Frequency"), 1, 80, prefs_trackerfreq_changed
};

static void
gui_settings_close(GtkWidget* configwindow)
{
    gtk_widget_hide(configwindow);
}

static void
prefs_scopesfreq_changed(int value)
{
    extern ScopeGroup* scopegroup;
    gui_settings.scopes_update_freq = value;
    scope_group_set_update_freq(scopegroup, value);
}

static void
prefs_trackerfreq_changed(int value)
{
    gui_settings.tracker_update_freq = value;
    tracker_set_update_freq(value);
}

static void
gui_settings_hexmode_toggled(GtkWidget* widget)
{
    int o = gui_settings.tracker_hexmode;
    if (o != (gui_settings.tracker_hexmode = GTK_TOGGLE_BUTTON(widget)->active)) {
        gtk_widget_queue_resize(GTK_WIDGET(tracker));
    }
}

static void
gui_settings_upcase_toggled(GtkWidget* widget)
{
    int o = gui_settings.tracker_upcase;
    if (o != (gui_settings.tracker_upcase = GTK_TOGGLE_BUTTON(widget)->active)) {
        gtk_widget_queue_resize(GTK_WIDGET(tracker));
    }
}

static void
gui_settings_asyncedit_toggled(GtkWidget* widget)
{
    gui_play_stop();
    gui_settings.asynchronous_editing = GTK_TOGGLE_BUTTON(widget)->active;
}

static void
gui_settings_trypoly_toggled(GtkWidget* widget)
{
    gui_play_stop();
    gui_settings.try_polyphony = GTK_TOGGLE_BUTTON(widget)->active;
}

static void
gui_settings_tempo_bpm_update_toggled(GtkWidget* widget)
{
    int o = gui_settings.tempo_bpm_update;
    if (o != (gui_settings.tempo_bpm_update = GTK_TOGGLE_BUTTON(widget)->active)) {
        gtk_widget_queue_resize(GTK_WIDGET(tracker));
    }
}

static void
gui_settings_auto_switch_toggled(GtkWidget* widget)
{
    int o = gui_settings.auto_switch;
    if (o != (gui_settings.auto_switch = GTK_TOGGLE_BUTTON(widget)->active)) {
        gtk_widget_queue_resize(GTK_WIDGET(tracker));
    }
}

static void
gui_settings_save_geometry_toggled(GtkWidget* widget)
{
    int o = gui_settings.save_geometry;
    if (o != (gui_settings.save_geometry = GTK_TOGGLE_BUTTON(widget)->active)) {
        gtk_widget_queue_resize(GTK_WIDGET(tracker));
    }
}

static void
gui_settings_scopebufsize_changed(GtkSpinButton* spin)
{
    double n = gtk_spin_button_get_value_as_float(spin);

    gui_settings.scopes_buffer_size = n * 1000000;
}

static void
gui_settings_bh_toggled(GtkWidget* widget)
{
    gui_settings.bh = GTK_TOGGLE_BUTTON(widget)->active;
    tracker_redraw(tracker);
}

static void
gui_settings_perm_toggled(GtkWidget* widget)
{
    gui_settings.store_perm = GTK_TOGGLE_BUTTON(widget)->active;
}

void gui_settings_highlight_rows_changed(GtkSpinButton* spin)
{
    int n = gtk_spin_button_get_value_as_int(spin);

    gui_settings.highlight_rows_n = n;
    if (gui_settings.highlight_rows)
        tracker_redraw(tracker);
}

void gui_settings_highlight_rows_minor_changed(GtkSpinButton* spin)
{
    int n = gtk_spin_button_get_value_as_int(spin);

    gui_settings.highlight_rows_minor_n = n;
    if (gui_settings.highlight_rows)
        tracker_redraw(tracker);
}

static void
gui_settings_tracker_line_note_modified(GtkEntry* entry)
{
    gchar* text = g_strdup(gtk_entry_get_text(entry));
    int i;

    for (i = 0; i < 3; i++) {
        if (!text[i]) {
            text[i] = ' ';
            text[i + 1] = 0;
        }
    }
    text[3] = 0;
    if (strncmp(gui_settings.tracker_line_format, text, 3)) {
        strncpy(gui_settings.tracker_line_format, text, 3);
        tracker_redraw(tracker);
    }
    g_free(text);
}

static void
gui_settings_tracker_line_ins_modified(GtkEntry* entry)
{
    gchar* text = g_strdup(gtk_entry_get_text(entry));
    int i;

    for (i = 0; i < 2; i++) {
        if (!text[i]) {
            text[i] = ' ';
            text[i + 1] = 0;
        }
    }
    text[2] = 0;
    if (strncmp(gui_settings.tracker_line_format + 3, text, 2)) {
        strncpy(gui_settings.tracker_line_format + 3, text, 2);
        tracker_redraw(tracker);
    }
    g_free(text);
}

static void
gui_settings_tracker_line_vol_modified(GtkEntry* entry)
{
    gchar* text = g_strdup(gtk_entry_get_text(entry));
    int i;

    for (i = 0; i < 2; i++) {
        if (!text[i]) {
            text[i] = ' ';
            text[i + 1] = 0;
        }
    }
    text[2] = 0;
    if (strncmp(gui_settings.tracker_line_format + 5, text, 2)) {
        strncpy(gui_settings.tracker_line_format + 5, text, 2);
        tracker_redraw(tracker);
    }
    g_free(text);
}

static void
gui_settings_tracker_line_effect_modified(GtkEntry* entry, GdkEventKey* event)
{
    gchar* text = g_strdup(gtk_entry_get_text(entry));
    int i;

    for (i = 0; i < 3; i++) {
        if (!text[i]) {
            text[i] = ' ';
            text[i + 1] = 0;
        }
    }
    text[3] = 0;
    if (strncmp(gui_settings.tracker_line_format + 7, text, 3)) {
        strncpy(gui_settings.tracker_line_format + 7, text, 3);
        tracker_redraw(tracker);
    }
    g_free(text);
}

static gboolean
col_sample_paint(GtkWidget* widget, GdkEvent* event, guint n)
{
    static GdkGC* gc = NULL;

    if (!gc)
        gc = gdk_gc_new(widget->window);

    gdk_gc_set_foreground(gc, &tracker->colors[n]);
    gdk_draw_rectangle(widget->window, gc, TRUE, 0, 0, widget->allocation.width, widget->allocation.height);
    return TRUE;
}

static void
colors_dialog_response(GtkWidget* dialog, gint response)
{
    guint i;

    switch (response) {
    case GTK_RESPONSE_APPLY:
        tracker_apply_colors(tracker);
        tracker_redraw(tracker);
        break;
    case GTK_RESPONSE_REJECT:
        tracker_init_colors(tracker);
        tracker_redraw(tracker);
        for (i = 0; i < TRACKERCOL_LAST; i++)
            col_sample_paint(col_samples[i], NULL, i);
        gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(colorsel), &tracker->colors[active_col_item]);
        break;
    default:
        gtk_widget_hide(dialog);
        break;
    }
}

static void
col_item_toggled(GtkToggleButton* butt, guint n)
{
    if (gtk_toggle_button_get_active(butt)) {
        active_col_item = n;
        gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(colorsel), &tracker->colors[n]);
    }
}

static void
color_changed(void)
{
    gtk_color_selection_get_current_color(GTK_COLOR_SELECTION(colorsel), &tracker->colors[active_col_item]);
    gdk_color_alloc(gtk_widget_get_colormap(colorsel), &tracker->colors[active_col_item]);
    col_sample_paint(col_samples[active_col_item], NULL, active_col_item);
}

static void
gui_settings_tracker_colors_dialog(GtkWindow* window)
{
    static GtkWidget* dialog = NULL;
    GtkWidget *thing, *table, *radio = NULL, *hbox, *vbox;
    intptr_t i;

    if (dialog) {
        gtk_window_present(GTK_WINDOW(dialog));
        return;
    }

    dialog = gtk_dialog_new_with_buttons(_("Tracker colors configuration"), window,
        GTK_DIALOG_DESTROY_WITH_PARENT,
        _("Reset"), GTK_RESPONSE_REJECT,
        GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
        GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
    gui_dialog_adjust(dialog, GTK_RESPONSE_APPLY);
    gtk_widget_set_tooltip_text(gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT), _("Reset tracker colors to standard ST"));

    g_signal_connect(dialog, "response",
        G_CALLBACK(colors_dialog_response), NULL);
    g_signal_connect(dialog, "delete-event",
        G_CALLBACK(gui_delete_noop), NULL);

    table = gtk_table_new(TRACKERCOL_LAST, 5, FALSE);
    for (i = 0; i < TRACKERCOL_LAST; i++) {
        radio = i ? gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(radio))
                  : gtk_radio_button_new(NULL);
        gtk_table_attach_defaults(GTK_TABLE(table), radio, 0, 1, i, i + 1);
        g_signal_connect(radio, "toggled",
            G_CALLBACK(col_item_toggled), (gpointer)i);

        col_samples[i] = gtk_drawing_area_new();
        gtk_table_attach_defaults(GTK_TABLE(table), col_samples[i], 1, 2, i, i + 1);
        g_signal_connect(col_samples[i], "expose_event",
            G_CALLBACK(col_sample_paint), (gpointer)i);

        hbox = gtk_hbox_new(FALSE, 0);
        thing = gtk_label_new(_(color_meanings[i]));
        gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
        gtk_table_attach_defaults(GTK_TABLE(table), hbox, 2, 3, i, i + 1);
        gtk_table_set_row_spacing(GTK_TABLE(table), i, 2);
    }

    thing = gtk_vseparator_new();
    gtk_table_attach_defaults(GTK_TABLE(table), thing, 3, 4, 0, TRACKERCOL_LAST);
    gtk_table_set_col_spacing(GTK_TABLE(table), 1, 4);
    gtk_table_set_col_spacing(GTK_TABLE(table), 2, 4);
    gtk_table_set_col_spacing(GTK_TABLE(table), 3, 4);

    colorsel = gtk_color_selection_new();
    gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(colorsel), &tracker->colors[0]);
    g_signal_connect(colorsel, "color_changed",
        G_CALLBACK(color_changed), NULL);
    gtk_table_attach_defaults(GTK_TABLE(table), colorsel, 4, 5, 0, TRACKERCOL_LAST);

    gtk_widget_show_all(table);
    vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);

    thing = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, FALSE, 4);
    gtk_widget_show_all(thing);

    gtk_widget_realize(dialog);

    for (i = 0; i < TRACKERCOL_LAST; i++)
        gtk_widget_set_size_request(col_samples[i], radio->allocation.width * 2, radio->allocation.height);

    gtk_widget_show(dialog);

    for (i = 0; i < TRACKERCOL_LAST; i++)
        col_sample_paint(col_samples[i], NULL, i);
}

void gui_settings_dialog(void)
{
    static GtkWidget* configwindow = NULL;

    GtkWidget *mainbox, *mainhbox, *thing, *box1, *vbox1;
    gchar stmp[5];

    if (configwindow != NULL) {
        gtk_window_present(GTK_WINDOW(configwindow));
        return;
    }

    configwindow = gtk_dialog_new_with_buttons(_("GUI Configuration"), GTK_WINDOW(mainwindow), 0,
        GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
    g_signal_connect(configwindow, "delete_event",
        G_CALLBACK(gui_delete_noop), NULL);
    g_signal_connect(configwindow, "response",
        G_CALLBACK(gui_settings_close), NULL);
    gui_dialog_adjust(configwindow, GTK_RESPONSE_CLOSE);
    mainbox = gtk_dialog_get_content_area(GTK_DIALOG(configwindow));

    mainhbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(mainbox), mainhbox, TRUE, TRUE, 0);

    vbox1 = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(mainhbox), vbox1, FALSE, TRUE, 0);

    thing = gui_subs_create_slider(&prefs_scopesfreq_slider, FALSE);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);

    thing = gui_subs_create_slider(&prefs_trackerfreq_slider, FALSE);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);

    thing = gtk_check_button_new_with_label(_("Hexadecimal row numbers"));
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing), gui_settings.tracker_hexmode);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_settings_hexmode_toggled), NULL);

    thing = gtk_check_button_new_with_label(_("Use upper case letters for hex numbers"));
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing), gui_settings.tracker_upcase);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_settings_upcase_toggled), NULL);

    thing = gtk_check_button_new_with_label(_("Asynchronous (IT-style) pattern editing"));
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing), gui_settings.asynchronous_editing);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_settings_asyncedit_toggled), NULL);

    thing = gtk_check_button_new_with_label(_("Polyphonic try (non-editing) mode"));
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing), gui_settings.try_polyphony);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_settings_trypoly_toggled), NULL);

    thing = gtk_check_button_new_with_label(_("Fxx command updates Tempo/BPM sliders"));
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing), gui_settings.tempo_bpm_update);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_settings_tempo_bpm_update_toggled), NULL);

    thing = gtk_check_button_new_with_label(_("Switch to tracker after loading/saving"));
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing), gui_settings.auto_switch);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_settings_auto_switch_toggled), NULL);

    thing = gtk_check_button_new_with_label(_("Save window geometry on exit"));
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing), gui_settings.save_geometry);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_settings_save_geometry_toggled), NULL);

    thing = gtk_check_button_new_with_label(_("Use note name B instead of H"));
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing), gui_settings.bh);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_settings_bh_toggled), NULL);

    thing = gtk_check_button_new_with_label(_("Save and restore permanent channels"));
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing), gui_settings.store_perm);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(G_OBJECT(thing), "toggled",
        G_CALLBACK(gui_settings_perm_toggled), NULL);

    gui_subs_set_slider_value(&prefs_scopesfreq_slider, gui_settings.scopes_update_freq);
    gui_subs_set_slider_value(&prefs_trackerfreq_slider, gui_settings.tracker_update_freq);

    box1 = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox1), box1, FALSE, TRUE, 0);

    thing = gtk_label_new(_("Scopes buffer size [MB]"));
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
    add_empty_hbox(box1);
    thing = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new((double)gui_settings.scopes_buffer_size / 1000000, 0.5, 5.0, 0.1, 1.0, 0.0)), 0, 0, FALSE);
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(thing), 1);
    g_signal_connect(thing, "value-changed",
        G_CALLBACK(gui_settings_scopebufsize_changed), NULL);

    thing = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);

    /* Track line format */
    box1 = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox1), box1, FALSE, TRUE, 0);
    thing = gtk_label_new(_("Track line format:"));
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);

    thing = gtk_entry_new();
    gtk_widget_set_usize(thing, 13 * 3, thing->requisition.height);
    gtk_entry_set_max_length((GtkEntry*)thing, 3);
    strncpy(stmp, gui_settings.tracker_line_format, 3);
    stmp[3] = 0;
    gtk_entry_set_text((GtkEntry*)thing, stmp);
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "changed",
        G_CALLBACK(gui_settings_tracker_line_note_modified), 0);

    thing = gtk_entry_new();
    gtk_widget_set_usize(thing, 13 * 2, thing->requisition.height);
    gtk_entry_set_max_length((GtkEntry*)thing, 2);
    strncpy(stmp, gui_settings.tracker_line_format + 3, 2);
    stmp[2] = 0;
    gtk_entry_set_text((GtkEntry*)thing, stmp);
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "changed",
        G_CALLBACK(gui_settings_tracker_line_ins_modified), 0);

    thing = gtk_entry_new();
    gtk_widget_set_usize(thing, 13 * 2, thing->requisition.height);
    gtk_entry_set_max_length((GtkEntry*)thing, 2);
    strncpy(stmp, gui_settings.tracker_line_format + 5, 2);
    stmp[2] = 0;
    gtk_entry_set_text((GtkEntry*)thing, stmp);
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "changed",
        G_CALLBACK(gui_settings_tracker_line_vol_modified), 0);

    thing = gtk_entry_new();
    gtk_widget_set_usize(thing, 13 * 3, thing->requisition.height);
    gtk_entry_set_max_length((GtkEntry*)thing, 3);
    strncpy(stmp, gui_settings.tracker_line_format + 7, 3);
    stmp[3] = 0;
    gtk_entry_set_text((GtkEntry*)thing, stmp);
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "changed",
        G_CALLBACK(gui_settings_tracker_line_effect_modified), 0);

    /* Tracker colors configuration dialog */
    thing = gtk_button_new_with_label(_("Color scheme"));
    gtk_widget_set_tooltip_text(thing, _("Tracker colors configuration"));
    gtk_box_pack_end(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect_swapped(thing, "clicked",
        G_CALLBACK(gui_settings_tracker_colors_dialog), GTK_WINDOW(configwindow));

    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(mainhbox), thing, FALSE, TRUE, 0);

    /* The tracker widget settings */
    ts_box = vbox1 = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(mainhbox), vbox1, TRUE, TRUE, 0);

    g_object_ref(G_OBJECT(trackersettings));
    gtk_box_pack_start(GTK_BOX(vbox1), trackersettings, TRUE, TRUE, 0);

    thing = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, FALSE, 4);

    gtk_widget_show_all(configwindow);
}

void gui_settings_load_config(void)
{
    gui_settings.st_window_x = prefs_get_int(SECTION, "st-window-x", -666);
    gui_settings.st_window_y = prefs_get_int(SECTION, "st-window-y", 0);
    gui_settings.st_window_w = prefs_get_int(SECTION, "st-window-w", 0);
    gui_settings.st_window_h = prefs_get_int(SECTION, "st-window-h", 0);

    gui_settings.tracker_hexmode = prefs_get_bool(SECTION, "gui-use-hexadecimal-numbers", TRUE);
    gui_settings.tracker_upcase = prefs_get_bool(SECTION, "gui-use-upper-case", FALSE);
    gui_settings.advance_cursor_in_fx_columns = prefs_get_bool(SECTION, "gui-advance-cursor-in-fx-columns", FALSE);
    gui_settings.asynchronous_editing = prefs_get_bool(SECTION, "gui-asynchronous-editing", FALSE);
    gui_settings.try_polyphony = prefs_get_bool(SECTION, "gui-try-polyphony", TRUE);
    gui_settings.tracker_line_format = prefs_get_string(SECTION, "gui-tracker-line-format", "---0000000");
    gui_settings.tracker_font = prefs_get_string(SECTION, "tracker-font", "fixed");
    gui_settings.tempo_bpm_update = prefs_get_bool(SECTION, "gui-tempo-bpm-update", TRUE);
    gui_settings.auto_switch = prefs_get_bool(SECTION, "gui-auto-switch", FALSE);
    gui_settings.gui_display_scopes = prefs_get_bool(SECTION, "gui-display-scopes", TRUE);
    gui_settings.gui_use_backing_store = prefs_get_bool(SECTION, "gui-use-backing-store", TRUE);
    gui_settings.highlight_rows = prefs_get_bool(SECTION, "tracker-highlight-rows", TRUE);
    gui_settings.highlight_rows_n = prefs_get_int(SECTION, "tracker-highlight-rows-n", 16);
    gui_settings.highlight_rows_minor_n = prefs_get_int(SECTION, "tracker-highlight-rows-minor-n", 8);

    gui_settings.save_geometry = prefs_get_bool(SECTION, "save-geometry", TRUE);
    gui_settings.save_settings_on_exit = prefs_get_bool(SECTION, "save-settings-on-exit", TRUE);
    gui_settings.tracker_update_freq = prefs_get_int(SECTION, "tracker-update-frequency", 50);
    gui_settings.scopes_update_freq = prefs_get_int(SECTION, "scopes-update-frequency", 40);
    gui_settings.scopes_buffer_size = prefs_get_int(SECTION, "scopes-buffer-size", 500000);
    gui_settings.sharp = prefs_get_bool(SECTION, "sharp", TRUE);
    gui_settings.bh = prefs_get_bool(SECTION, "bh", FALSE);
    gui_settings.store_perm = prefs_get_bool(SECTION, "store-permanent", TRUE);

    if (gui_settings.store_perm)
        gui_settings.permanent_channels = prefs_get_int(SECTION, "permanent-channels", 0);

    gui_settings.loadmod_path = prefs_get_string(SECTION_ALWAYS, "loadmod-path", "~");
    gui_settings.savemod_path = prefs_get_string(SECTION_ALWAYS, "savemod-path", "~");
    gui_settings.savemodaswav_path = prefs_get_string(SECTION_ALWAYS, "savemodaswav-path", "~");
    gui_settings.savesongasxm_path = prefs_get_string(SECTION_ALWAYS, "savesongasxm-path", "~");
    gui_settings.loadsmpl_path = prefs_get_string(SECTION_ALWAYS, "loadsmpl-path", "~");
    gui_settings.savesmpl_path = prefs_get_string(SECTION_ALWAYS, "savesmpl-path", "~");
    gui_settings.loadinstr_path = prefs_get_string(SECTION_ALWAYS, "loadinstr-path", "~");
    gui_settings.saveinstr_path = prefs_get_string(SECTION_ALWAYS, "saveinstr-path", "~");
    gui_settings.loadpat_path = prefs_get_string(SECTION_ALWAYS, "loadpat-path", "~");
    gui_settings.savepat_path = prefs_get_string(SECTION_ALWAYS, "savepat-path", "~");

    gui_settings.rm_path = prefs_get_string(SECTION_ALWAYS, "rm-path", "rm");
    gui_settings.unzip_path = prefs_get_string(SECTION_ALWAYS, "unzip-path", "unzip");
    gui_settings.lha_path = prefs_get_string(SECTION_ALWAYS, "lha-path", "lha");
    gui_settings.gz_path = prefs_get_string(SECTION_ALWAYS, "gz-path", "zcat");
    gui_settings.bz2_path = prefs_get_string(SECTION_ALWAYS, "bz2-path", "bunzip2");

    gui_settings.gui_disable_splash = prefs_get_bool(SECTION_ALWAYS, "gui-disable-splash", FALSE);
}

void gui_settings_save_config(void)
{
    guint i;

    if (gui_settings.save_geometry) {
        gdk_window_get_size(mainwindow->window,
            &gui_settings.st_window_w,
            &gui_settings.st_window_h);
        gdk_window_get_root_origin(mainwindow->window,
            &gui_settings.st_window_x,
            &gui_settings.st_window_y);
    }

    prefs_put_int(SECTION, "st-window-x", gui_settings.st_window_x);
    prefs_put_int(SECTION, "st-window-y", gui_settings.st_window_y);
    prefs_put_int(SECTION, "st-window-w", gui_settings.st_window_w);
    prefs_put_int(SECTION, "st-window-h", gui_settings.st_window_h);

    prefs_put_bool(SECTION, "gui-use-hexadecimal-numbers", gui_settings.tracker_hexmode);
    prefs_put_bool(SECTION, "gui-use-upper-case", gui_settings.tracker_upcase);
    prefs_put_bool(SECTION, "gui-advance-cursor-in-fx-columns", gui_settings.advance_cursor_in_fx_columns);
    prefs_put_bool(SECTION, "gui-asynchronous-editing", gui_settings.asynchronous_editing);
    prefs_put_bool(SECTION, "gui-try-polyphony", gui_settings.try_polyphony);
    prefs_put_string(SECTION, "gui-tracker-line-format", gui_settings.tracker_line_format);
    prefs_put_string(SECTION, "tracker-font", gui_settings.tracker_font);
    prefs_put_bool(SECTION, "gui-tempo-bpm-update", gui_settings.tempo_bpm_update);
    prefs_put_bool(SECTION, "gui-auto-switch", gui_settings.auto_switch);
    prefs_put_bool(SECTION, "gui-display-scopes", gui_settings.gui_display_scopes);
    prefs_put_bool(SECTION, "gui-use-backing-store", gui_settings.gui_use_backing_store);
    prefs_put_bool(SECTION, "tracker-highlight-rows", gui_settings.highlight_rows);
    prefs_put_int(SECTION, "tracker-highlight-rows-n", gui_settings.highlight_rows_n);
    prefs_put_int(SECTION, "tracker-highlight-rows-minor-n", gui_settings.highlight_rows_minor_n);
    for (i = 0; i < TRACKERCOL_LAST; i++)
        prefs_put_color(SECTION, color_meanings[i], tracker->colors[i]);
    prefs_put_bool(SECTION, "save-geometry", gui_settings.save_geometry);
    prefs_put_bool(SECTION, "save-settings-on-exit", gui_settings.save_settings_on_exit);
    prefs_put_int(SECTION, "tracker-update-frequency", gui_settings.tracker_update_freq);
    prefs_put_int(SECTION, "scopes-update-frequency", gui_settings.scopes_update_freq);
    prefs_put_int(SECTION, "scopes-buffer-size", gui_settings.scopes_buffer_size);
    prefs_put_bool(SECTION, "sharp", gui_settings.sharp);
    prefs_put_bool(SECTION, "bh", gui_settings.bh);
    prefs_put_bool(SECTION, "store-permanent", gui_settings.store_perm);

    if (gui_settings.store_perm)
        prefs_put_int(SECTION, "permanent-channels", gui_settings.permanent_channels);
}

void gui_settings_save_config_always(void)
{
    prefs_put_string(SECTION_ALWAYS, "loadmod-path", gui_settings.loadmod_path);
    prefs_put_string(SECTION_ALWAYS, "savemod-path", gui_settings.savemod_path);
    prefs_put_string(SECTION_ALWAYS, "savemodaswav-path", gui_settings.savemodaswav_path);
    prefs_put_string(SECTION_ALWAYS, "savesongasxm-path", gui_settings.savesongasxm_path);
    prefs_put_string(SECTION_ALWAYS, "loadsmpl-path", gui_settings.loadsmpl_path);
    prefs_put_string(SECTION_ALWAYS, "savesmpl-path", gui_settings.savesmpl_path);
    prefs_put_string(SECTION_ALWAYS, "loadinstr-path", gui_settings.loadinstr_path);
    prefs_put_string(SECTION_ALWAYS, "saveinstr-path", gui_settings.saveinstr_path);
    prefs_put_string(SECTION_ALWAYS, "loadpat-path", gui_settings.loadpat_path);
    prefs_put_string(SECTION_ALWAYS, "savepat-path", gui_settings.savepat_path);

    prefs_put_string(SECTION_ALWAYS, "rm-path", gui_settings.rm_path);
    prefs_put_string(SECTION_ALWAYS, "unzip-path", gui_settings.unzip_path);
    prefs_put_string(SECTION_ALWAYS, "lha-path", gui_settings.lha_path);
    prefs_put_string(SECTION_ALWAYS, "gz-path", gui_settings.gz_path);
    prefs_put_string(SECTION_ALWAYS, "bz2-path", gui_settings.bz2_path);

    prefs_put_bool(SECTION_ALWAYS, "gui-disable-splash", gui_settings.gui_disable_splash);
}
