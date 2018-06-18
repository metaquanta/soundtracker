
/*
 * The Real SoundTracker - instrument editor
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

#include <config.h>

#include <glib/gi18n.h>
#include <stdlib.h>

#include "instrument-editor.h"
#include "envelope-box.h"
#include "xm.h"
#include "st-subs.h"
#include "gui.h"
#include "gui-subs.h"
#include "keys.h"
#include "track-editor.h"
#include "clavier.h"
#include "errors.h"
#include "sample-editor.h"
#include "tracker.h"
#include "gui-settings.h"
#include "module-info.h"
#include "file-operations.h"

static GtkWidget *volenv, *panenv, *disableboxes[4];
static GtkWidget *instrument_editor_vibtype_w[4];
static GtkWidget *clavier;
static GtkWidget *curnote_label;

static STInstrument *current_instrument, *tmp_instrument = NULL;
static gint current_instrument_number = 0;

static void
instrument_page_volfade_changed (int value)
{
    current_instrument->volfade = value;
    xm_set_modified(1);
}

static void
instrument_page_vibspeed_changed (int value)
{
    current_instrument->vibrate = value;
    xm_set_modified(1);
}

static void
instrument_page_vibdepth_changed (int value)
{
    current_instrument->vibdepth = value;
    xm_set_modified(1);
}

static void
instrument_page_vibsweep_changed (int value)
{
    current_instrument->vibsweep = value;
    xm_set_modified(1);
}

static gui_subs_slider instrument_page_sliders[] = {
    { N_("VolFade"), 0, 0xfff, instrument_page_volfade_changed },
    { N_("VibSpeed"), 0, 0x3f, instrument_page_vibspeed_changed },
    { N_("VibDepth"), 0, 0xf, instrument_page_vibdepth_changed },
    { N_("VibSweep"), 0, 0xff, instrument_page_vibsweep_changed },
};

static void
instrument_editor_vibtype_changed (void)
{
    current_instrument->vibtype = find_current_toggle(instrument_editor_vibtype_w, 4);
    xm_set_modified(1);
}

static gint
instrument_editor_clavierkey_press_event (GtkWidget *widget,
					  gint key,
					  gpointer data)
{
    current_instrument->samplemap[key] = gui_get_current_sample();
    clavier_press(CLAVIER(clavier), key);
    return FALSE;
}

static void
instrument_editor_init_samplemap (void)
{
    int key;
    int sample = gui_get_current_sample();

    for(key = 0; key < sizeof(current_instrument->samplemap) / sizeof(current_instrument->samplemap[0]); key++) {
	current_instrument->samplemap[key] = sample;
    }

    clavier_set_key_labels(CLAVIER(clavier), current_instrument->samplemap);
}

static gint
instrument_editor_clavierkey_release_event (GtkWidget *widget,
					    gint key,
					    gpointer data)
{
    clavier_release(CLAVIER(clavier), key);
    return FALSE;
}

static gint
instrument_editor_clavierkey_enter_event (GtkWidget *widget,
					  gint key,
					  gpointer data)
{
    int index = (gui_settings.sharp ? 0 : 1) + (gui_settings.bh ? 2 : 0);
    gtk_label_set_text(GTK_LABEL(curnote_label), notenames[index][key]);
    return FALSE;
}

static gint
instrument_editor_clavierkey_leave_event (GtkWidget *widget,
					  gint key,
					  gpointer data)
{
    gtk_label_set_text(GTK_LABEL(curnote_label), NULL);
    return FALSE;
}

static void
instrument_editor_load_instrument (gchar *fn)
{
	gchar *localname = gui_filename_from_utf8(fn);
    STInstrument *instr = current_instrument;
    FILE *f;

    g_assert(instr != NULL);
    if(!localname)
		return;

	file_selection_save_path(fn, &gui_settings.loadinstr_path);
    // Instead of locking the instrument and samples, we simply stop playing.
    gui_play_stop();

    f = fopen(localname, "rb");
    if(f) {
        statusbar_update(STATUS_LOADING_INSTRUMENT, TRUE);
        xm_load_xi(instr, f);
       statusbar_update(STATUS_INSTRUMENT_LOADED, FALSE);
	fclose(f);
    } else {
	static GtkWidget *dialog = NULL;
	gui_error_dialog(&dialog, N_("Can't open file."), FALSE);
    }

	instrument_editor_update(TRUE);
    sample_editor_set_sample(&instr->samples[0]);
    xm_set_modified(1);
}

static void
instrument_editor_save_instrument (gchar *fn)
{
	gchar *localname = gui_filename_from_utf8(fn);
    STInstrument *instr = current_instrument;
    FILE *f;

    g_assert(instr != NULL);
    if(!localname)
		return;

	file_selection_save_path(fn, &gui_settings.saveinstr_path);
    f = fopen(localname, "wb");
    g_free(localname);
	if(f) {
		statusbar_update(STATUS_SAVING_INSTRUMENT, TRUE);
		if(xm_save_xi(instr, f)) {
			static GtkWidget *dialog = NULL;

			gui_error_dialog(&dialog, N_("Saving instrument failed."), FALSE);
			statusbar_update(STATUS_IDLE, FALSE);
		} else
			statusbar_update(STATUS_INSTRUMENT_SAVED, FALSE);
		fclose(f);
	} else {
		static GtkWidget *dialog = NULL;
		gui_error_dialog(&dialog, N_("Can't open file."), FALSE);
	}
}

void
instrument_editor_clear_current_instrument (void)
{
    gui_play_stop();

    st_clean_instrument(current_instrument, NULL);

    instrument_editor_update(TRUE);
    sample_editor_update();
    xm_set_modified(TRUE);
}

void
instrument_page_create (GtkNotebook *nb)
{
    GtkWidget *mainbox, *vbox, *thing, *box, *box2, *box3, *box4, *frame;
    static const char *vibtypelabels[] = { N_("Sine"), N_("Square"), N_("Saw Down"), N_("Saw Up"), NULL };

	static const gchar *xi_f[] = {N_("FastTracker instrumets (*.xi)"), "*.[xX][iI]", NULL};
	static const gchar **formats[] = {xi_f, NULL};

    mainbox = gtk_vbox_new(FALSE, 4);
    gtk_container_border_width(GTK_CONTAINER(mainbox), 10);
    gtk_notebook_append_page(nb, mainbox, gtk_label_new(_("Instrument Editor")));
    gtk_widget_show(mainbox);

    add_empty_vbox(mainbox);

    disableboxes[0] = vbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(mainbox), vbox, FALSE, TRUE, 0);
    gtk_widget_show(vbox);

    volenv = envelope_box_new(_("Volume envelope"));
    gtk_box_pack_start(GTK_BOX(vbox), volenv, TRUE, TRUE, 0);
    gtk_widget_show(volenv);

    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    panenv = envelope_box_new(_("Panning envelope"));
    gtk_box_pack_start(GTK_BOX(vbox), panenv, TRUE, TRUE, 0);
    gtk_widget_show(panenv);

    thing = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);


    box = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(mainbox), box, FALSE, TRUE, 0);
    gtk_widget_show(box);

    box2 = gtk_vbox_new(TRUE, 2);
    gtk_box_pack_start(GTK_BOX(box), box2, TRUE, TRUE, 0);
    gtk_widget_show(box2);

    file_selection_create(DIALOG_LOAD_INSTRUMENT, _("Load Instrument"), gui_settings.loadinstr_path,
                          instrument_editor_load_instrument, 5, TRUE, FALSE, formats,
                          N_("Load instrument in the current instrument slot"));
    file_selection_create(DIALOG_SAVE_INSTRUMENT, _("Save Instrument"), gui_settings.saveinstr_path,
                          instrument_editor_save_instrument, 6, FALSE, TRUE, formats,
                          N_("Save the current instrument"));

    thing = gtk_button_new_with_label(_("Load XI"));
    gtk_box_pack_start(GTK_BOX(box2), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    g_signal_connect(thing, "clicked",
		       G_CALLBACK(fileops_open_dialog), (gpointer)DIALOG_LOAD_INSTRUMENT);

    disableboxes[3] = thing = gtk_button_new_with_label(_("Save XI"));
    gtk_box_pack_start(GTK_BOX(box2), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    g_signal_connect(thing, "clicked",
		       G_CALLBACK(fileops_open_dialog), (gpointer)DIALOG_SAVE_INSTRUMENT);

    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(box), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    disableboxes[1] = box2 = gtk_vbox_new(TRUE, 2);
    gtk_box_pack_start(GTK_BOX(box), box2, TRUE, TRUE, 0);
    gtk_widget_show(box2);

    box3 = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box2), box3, FALSE, TRUE, 0);
    gtk_widget_show(box3);

    add_empty_hbox(box3);
    thing = make_labelled_radio_group_box(_("Vibrato Type:"), vibtypelabels, instrument_editor_vibtype_w, instrument_editor_vibtype_changed);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box3), thing, FALSE, TRUE, 0);
    thing = gui_subs_create_slider(&instrument_page_sliders[0], TRUE);
    gtk_box_pack_start(GTK_BOX(box3), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    add_empty_hbox(box3);

    box3 = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box2), box3, FALSE, TRUE, 0);
    gtk_widget_show(box3);

    add_empty_hbox(box3);
    thing = gui_subs_create_slider(&instrument_page_sliders[1], TRUE);
    gtk_box_pack_start(GTK_BOX(box3), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    thing = gui_subs_create_slider(&instrument_page_sliders[2], TRUE);
    gtk_box_pack_start(GTK_BOX(box3), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    thing = gui_subs_create_slider(&instrument_page_sliders[3], TRUE);
    gtk_box_pack_start(GTK_BOX(box3), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    add_empty_hbox(box3);

    thing = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    // Sample map editor coming up
    disableboxes[2] = box2 = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(mainbox), box2, FALSE, TRUE, 0);
    gtk_widget_show(box2);

    box3 = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(box2), box3, TRUE, TRUE, 0);
    gtk_widget_show(box3);

    box = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(box), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_show(box);
    gtk_box_pack_start(GTK_BOX(box3), box, TRUE, TRUE, 0);

    clavier = clavier_new();
    gtk_widget_show(clavier);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(box), clavier);

    gtk_drawing_area_size (GTK_DRAWING_AREA (clavier), 96 * 7, 50);

    clavier_set_clavier_type(CLAVIER(clavier), CLAVIER_TYPE_SEQUENCER);
    clavier_set_range(CLAVIER(clavier), 0, 95);
    clavier_set_show_middle_c(CLAVIER(clavier), FALSE);
    clavier_set_show_transpose(CLAVIER(clavier), FALSE);

    g_signal_connect(clavier, "clavierkey_press",
			G_CALLBACK(instrument_editor_clavierkey_press_event),
			NULL);
    g_signal_connect(clavier, "clavierkey_release",
			G_CALLBACK(instrument_editor_clavierkey_release_event),
			NULL);
    g_signal_connect(clavier, "clavierkey_enter",
			G_CALLBACK(instrument_editor_clavierkey_enter_event),
			NULL);
    g_signal_connect(clavier, "clavierkey_leave",
			G_CALLBACK(instrument_editor_clavierkey_leave_event),
			NULL);

    box3 = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(box2), box3, FALSE, TRUE, 0);
    gtk_widget_show(box3);

    add_empty_vbox(box3);

    thing = gtk_label_new(_("Note:"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box3), thing, FALSE, TRUE, 0);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
    gtk_box_pack_start(GTK_BOX(box3), frame, FALSE, TRUE, 0);
    gtk_widget_show(frame);

    box4 = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(box4);
    gtk_container_add (GTK_CONTAINER(frame), box4);
    gtk_container_border_width(GTK_CONTAINER(box4), 4);

    curnote_label = thing = gtk_label_new("");
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box4), thing, FALSE, TRUE, 0);

    thing = gtk_button_new_with_label(_("Initialize"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box3), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "clicked",
			G_CALLBACK(instrument_editor_init_samplemap), NULL);

    add_empty_vbox(box3);

    add_empty_vbox(mainbox);
}

void
instrument_editor_copy_instrument (STInstrument *instr)
{
    if (tmp_instrument == NULL)
	tmp_instrument = calloc(1, sizeof(STInstrument));
    st_copy_instrument(instr, tmp_instrument);
}

void
instrument_editor_cut_instrument (STInstrument *instr)
{
    instrument_editor_copy_instrument(instr);
    st_clean_instrument(instr, NULL);
}

void
instrument_editor_paste_instrument (STInstrument *instr)
{
    if (tmp_instrument == NULL)
	return;
    st_copy_instrument(tmp_instrument, instr);
}

void
instrument_editor_set_instrument (STInstrument *i, const gint ins)
{
    current_instrument = i;
    current_instrument_number = ins;

    instrument_editor_update(FALSE);
}

STInstrument*
instrument_editor_get_instrument (void)
{
    return current_instrument;
}

gboolean
instrument_editor_handle_keys (int shift,
			       int ctrl,
			       int alt,
			       guint32 keyval,
			       gboolean pressed)
{
    int i;

    i = keys_get_key_meaning(keyval, ENCODE_MODIFIERS(shift, ctrl, alt));
    if(i != -1 && KEYS_MEANING_TYPE(i) == KEYS_MEANING_NOTE) {
	track_editor_do_the_note_key(i, pressed, keyval, ENCODE_MODIFIERS(shift, ctrl, alt));
	return TRUE;
    }

    return FALSE;
}

void
instrument_editor_update (gboolean full)
{
    int o, n, m = xm_get_modified();

    o = current_instrument != NULL && st_instrument_num_samples(current_instrument) > 0;
    for(n = 0; n < 4; n++) {
	gtk_widget_set_sensitive(disableboxes[n], o);
    }

    if(current_instrument){
	current_instrument->no_cb = TRUE; /* Preventing callback when changing instrument name entry */
	gtk_entry_set_text(GTK_ENTRY(gui_curins_name), current_instrument->utf_name);
	current_instrument->no_cb = FALSE;
	}

    if(!o) {
	envelope_box_set_envelope(ENVELOPE_BOX(volenv), NULL);
	envelope_box_set_envelope(ENVELOPE_BOX(panenv), NULL);
	clavier_set_key_labels(CLAVIER(clavier), NULL);
    } else {

	envelope_box_set_envelope(ENVELOPE_BOX(volenv), &current_instrument->vol_env);
	envelope_box_set_envelope(ENVELOPE_BOX(panenv), &current_instrument->pan_env);

	gui_subs_set_slider_value(&instrument_page_sliders[0], current_instrument->volfade);
	gui_subs_set_slider_value(&instrument_page_sliders[1], current_instrument->vibrate);
	gui_subs_set_slider_value(&instrument_page_sliders[2], current_instrument->vibdepth);
	gui_subs_set_slider_value(&instrument_page_sliders[3], current_instrument->vibsweep);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(instrument_editor_vibtype_w[current_instrument->vibtype]), TRUE);

	clavier_set_key_labels(CLAVIER(clavier), current_instrument->samplemap);

	xm_set_modified(m);
    }

    if(full)
	modinfo_update_instrument(current_instrument_number);
}
