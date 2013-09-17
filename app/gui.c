
/*
 * The Real SoundTracker - main user interface handling
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <unistd.h>

#include "poll.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>
#ifndef NO_GDK_PIXBUF
#include <gdk-pixbuf/gdk-pixbuf.h>
#endif

#include "gui.h"
#include "gui-subs.h"
#include "gui-settings.h"
#include "xm.h"
#include "st-subs.h"
#include "audio.h"
#include "xm-player.h"
#include "tracker.h"
#include "main.h"
#include "keys.h"
#include "instrument-editor.h"
#include "sample-editor.h"
#include "track-editor.h"
#include "scope-group.h"
#include "menubar.h"
#include "module-info.h"
#include "preferences.h"
#include "time-buffer.h"
#include "tips-dialog.h"
#include "gui-settings.h"
#include "file-operations.h"
#include "playlist.h"
#include "extspinbutton.h"
#include "clock.h"

#define XML_FILE PREFIX"/share/soundtracker/soundtracker.xml"

int gui_playing_mode = 0;
int notebook_current_page = NOTEBOOK_PAGE_FILE;
GtkWidget *editing_toggle;
GtkWidget *gui_curins_name, *gui_cursmpl_name;
GtkWidget *mainwindow = NULL;
GtkWidget *alt[2], *arrow[2];
ScopeGroup *scopegroup;

static GtkWidget *gui_splash_window = NULL;
#ifndef NO_GDK_PIXBUF
static GdkPixbuf *gui_splash_logo = NULL;
static GtkWidget *gui_splash_logo_area;
#endif
static GtkWidget *gui_splash_label;
static GtkWidget *gui_splash_close_button;

static gint pipetag = -1;
static gchar *current_filename = NULL;
static GtkWidget *mainwindow_upper_hbox, *mainwindow_second_hbox;
static GtkWidget *notebook;
static GtkWidget *spin_editpat, *spin_patlen, *spin_numchans;
static GtkWidget *cursmpl_spin;
static GtkWidget *pbutton;
static GtkAdjustment *adj_amplification, *adj_pitchbend;
static GtkWidget *spin_jump, *curins_spin, *spin_octave;
static GtkWidget *toggle_lock_editpat;
static Playlist *playlist;
static GtkBuilder *builder;

GtkWidget *status_bar;
GtkWidget *st_clock;

struct measure
{
    const gchar *title;
    gint major;
    gint minor;
};

static struct measure measure_msr[] = {
    {"2/2", 16, 8},
    {"3/2", 24, 8},
    {"4/2", 32, 8},
    {"2/4", 8,  4},
    {"3/4", 12, 4},
    {"4/4", 16, 4},
    {"5/4", 20, 4},
    {"6/4", 24, 4},
    {"7/4", 28, 4},
    {"3/8", 6, 2},
    {"4/8", 8, 2},
    {"5/8", 10, 2},
    {"6/8", 12, 2},
    {"9/8", 18, 2},
    {"12/8", 24, 2},
    {NULL}
};

static GtkWidget *measurewindow = NULL;
//static gint measure_chosen;
    
static void gui_tempo_changed (int value);
static void gui_bpm_changed (int value);

gui_subs_slider tempo_slider = {
    N_("Tempo"), 1, 31, gui_tempo_changed, GUI_SUBS_SLIDER_SPIN_ONLY
};
gui_subs_slider bpm_slider = {
    "BPM", 32, 255, gui_bpm_changed, GUI_SUBS_SLIDER_SPIN_ONLY
};

static GdkColor gui_clipping_led_on, gui_clipping_led_off;
static GtkWidget *gui_clipping_led;
static gboolean gui_clipping_led_status;

static int editing_pat = 0;

static int gui_ewc_startstop = 0;

/* gui event handlers */
static void current_instrument_changed(GtkSpinButton *spin);
static void current_instrument_name_changed(void);
static void current_sample_changed(GtkSpinButton *spin);
static void current_sample_name_changed(void);
static int keyevent(GtkWidget *widget, GdkEventKey *event, gpointer data);
static void gui_editpat_changed(GtkSpinButton *spin);
static void gui_patlen_changed(GtkSpinButton *spin);
static void gui_numchans_changed(GtkSpinButton *spin);
static void notebook_page_switched(GtkNotebook *notebook, GtkNotebookPage *page, int page_num);
static void gui_adj_amplification_changed(GtkAdjustment *adj);
static void gui_adj_pitchbend_changed(GtkAdjustment *adj);

/* mixer / player communication */
static void read_mixer_pipe(gpointer data, gint source, GdkInputCondition condition);
static void wait_for_player(void);
static void play_pattern(void);
static void play_current_pattern_row(void);

/* gui initialization / helpers */
static void gui_enable(int enable);
static void offset_current_pattern(int offset);
static void offset_current_instrument(int offset);
static void offset_current_sample(int offset);

static void gui_auto_switch_page (void);
static void gui_load_xm (const char *filename);

static void
editing_toggled (GtkToggleButton *button, gpointer data)
{
    tracker_redraw(tracker);
    if (button->active)
	show_editmode_status();
    else
	statusbar_update(STATUS_IDLE, FALSE);
}

static void
gui_highlight_rows_toggled (GtkWidget *widget)
{
    gui_settings.highlight_rows = GTK_TOGGLE_BUTTON(widget)->active;

    tracker_redraw(tracker);
}

void
gui_accidentals_clicked (GtkWidget *widget, gpointer data)
{
	GtkWidget *focus_widget = GTK_WINDOW(mainwindow)->focus_widget;

	if(GTK_IS_ENTRY(focus_widget)) { /* Emulate Ctrl + A if the cursor is in an entry */
		g_signal_emit_by_name(focus_widget, "move-cursor", GTK_MOVEMENT_DISPLAY_LINE_ENDS, -1, FALSE, NULL);
		g_signal_emit_by_name(focus_widget, "move-cursor", GTK_MOVEMENT_DISPLAY_LINE_ENDS, 1, TRUE, NULL);
	}
    gui_settings.sharp = !gui_settings.sharp;
    gtk_widget_hide(alt[gui_settings.sharp ? 1 : 0]);
    gtk_widget_show(alt[gui_settings.sharp ? 0 : 1]);
    tracker_redraw(tracker);
}

void
gui_direction_clicked (GtkWidget *widget, gpointer data)
{
    gui_settings.advance_cursor_in_fx_columns = !gui_settings.advance_cursor_in_fx_columns;
    gtk_widget_hide(arrow[gui_settings.advance_cursor_in_fx_columns ? 0 : 1]);
    gtk_widget_show(arrow[gui_settings.advance_cursor_in_fx_columns ? 1 : 0]);
}

static gboolean
measure_close_requested (void)
{
    gtk_widget_hide(measurewindow);
/* to make keyboard working immediately after closing the dialog */
    gtk_widget_grab_focus(pbutton);
    return TRUE;
}

static void
measure_dialog ()
{
    GtkObject *adj;
    GtkWidget *mainbox, *thing, *vbox;
    static GtkWidget *majspin;

    if(measurewindow != NULL) {
	gtk_window_set_position(GTK_WINDOW(measurewindow), GTK_WIN_POS_MOUSE);
	gtk_window_present(GTK_WINDOW(measurewindow));
	gtk_widget_grab_focus(majspin);
	return;
    }
    
	measurewindow = gtk_dialog_new_with_buttons(_("Row highlighting configuration"), GTK_WINDOW(mainwindow),
	                                            GTK_DIALOG_MODAL, GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
	gui_dialog_adjust(measurewindow, GTK_RESPONSE_CLOSE);
    g_signal_connect(measurewindow, "response",
			G_CALLBACK(measure_close_requested), NULL);
    vbox = gtk_dialog_get_content_area(GTK_DIALOG(measurewindow));

    g_signal_connect(measurewindow, "delete_event",
			G_CALLBACK(measure_close_requested), NULL);
	gtk_window_set_position(GTK_WINDOW(measurewindow), GTK_WIN_POS_MOUSE);

    mainbox = gtk_hbox_new(FALSE, 2);
    
    thing = gtk_label_new(_("Highlight rows (major / minor):"));
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);

    add_empty_hbox(mainbox);
    adj = gtk_adjustment_new((double)gui_settings.highlight_rows_n, 1, 32, 1, 2, 0.0);
    majspin = extspinbutton_new(GTK_ADJUSTMENT(adj), 0, 0);
    gtk_box_pack_start(GTK_BOX(mainbox), majspin, FALSE, TRUE, 0);

    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(majspin), 0);
    g_signal_connect(majspin, "value-changed",
		       G_CALLBACK(gui_settings_highlight_rows_changed), NULL);
    adj = gtk_adjustment_new((double)gui_settings.highlight_rows_minor_n, 1, 16, 1, 2, 0.0);
    thing = extspinbutton_new(GTK_ADJUSTMENT(adj), 0, 0);
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);

    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(thing), 0);
    g_signal_connect(thing, "value-changed",
		       G_CALLBACK(gui_settings_highlight_rows_minor_changed), NULL);
    
    gtk_box_pack_start(GTK_BOX(vbox), mainbox, TRUE, TRUE, 0);

    thing = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, FALSE, 4);

    gtk_widget_show_all(measurewindow);
    gtk_widget_grab_focus(majspin);
}    

static void
measure_changed (GtkWidget *widget, gpointer data)
{
	gint measure_chosen;
	guint maxmeasure = GPOINTER_TO_INT(data);

	if((measure_chosen = gtk_combo_box_get_active(GTK_COMBO_BOX(widget))) <= (maxmeasure - 1)) {
		if(measurewindow && gtk_widget_get_visible(measurewindow))
			gtk_widget_hide(measurewindow);
		if((gui_settings.highlight_rows_n != measure_msr[measure_chosen].major) ||
		   (gui_settings.highlight_rows_minor_n != measure_msr[measure_chosen].minor)) {
			gui_settings.highlight_rows_n = measure_msr[measure_chosen].major;
			gui_settings.highlight_rows_minor_n = measure_msr[measure_chosen].minor;
			tracker_redraw(tracker);
/* to make keyboard working immediately after chosing the measure */
			gtk_widget_grab_focus(pbutton);
		}
/* Gtk+ stupidity: when combo box list is popped down, */
	} else if (measure_chosen == maxmeasure + 1)
		measure_dialog();
}

static void
popwin_hide (GtkWidget *widget, GParamSpec *ps, gpointer data)
{
	gboolean shown;
	guint maxmeasure = GPOINTER_TO_INT(data);

	g_object_get(G_OBJECT(widget), "popup-shown", &shown, NULL);
	if(!shown && gtk_combo_box_get_active(GTK_COMBO_BOX(widget)) == maxmeasure + 1) /* Popup is hidden by clicking on "Other..." */
		measure_dialog();
}

void
gui_update_title (const gchar *filename)
{
    gchar *title;
	if(filename && g_strcmp0(filename, current_filename)) {
		if(current_filename){
			g_free(current_filename);
		}
		current_filename = g_strdup(filename);
	}

    title = g_strdup_printf("SoundTracker "VERSION": %s%s", xm_get_modified() ? "*" : "", current_filename ? g_basename(current_filename) : "");
    gtk_window_set_title(GTK_WINDOW(mainwindow), title);
    g_free(title);
}

static void
gui_mixer_play_pattern (int pattern,
			int row,
			int stop_after_row)
{
	audio_ctlpipe_id i = AUDIO_CTLPIPE_PLAY_PATTERN;

	if(write(audio_ctlpipe, &i, sizeof(i)) != sizeof(i) ||
	   write(audio_ctlpipe, &pattern, sizeof(pattern)) != sizeof(pattern) ||
	   write(audio_ctlpipe, &row, sizeof(row)) != sizeof(row) ||
	   write(audio_ctlpipe, &stop_after_row, sizeof(stop_after_row)) != sizeof(stop_after_row))
		gui_error_dialog(N_("Connection with audio thread failed!"));
}

static void
gui_mixer_stop_playing (void)
{
	audio_ctlpipe_id i = AUDIO_CTLPIPE_STOP_PLAYING;

	if(write(audio_ctlpipe, &i, sizeof(i)) != sizeof(i))
		gui_error_dialog(N_("Connection with audio thread failed!"));
}

static void
gui_mixer_set_songpos (int songpos)
{
	audio_ctlpipe_id i = AUDIO_CTLPIPE_SET_SONGPOS;

	if(write(audio_ctlpipe, &i, sizeof(i)) != sizeof(i) ||
	   write(audio_ctlpipe, &songpos, sizeof(songpos)) != sizeof(songpos))
		gui_error_dialog(N_("Connection with audio thread failed!"));
}

static void
gui_mixer_set_pattern (int pattern)
{
	audio_ctlpipe_id i = AUDIO_CTLPIPE_SET_PATTERN;

	if(write(audio_ctlpipe, &i, sizeof(i)) != sizeof(i) ||
	   write(audio_ctlpipe, &pattern, sizeof(pattern)) !=sizeof(pattern))
		gui_error_dialog(N_("Connection with audio thread failed!"));
}

static void
gui_save (const gchar *data, gboolean save_smpls)
{
	gchar *localname = gui_filename_from_utf8(data);

	if(!localname)
		return;

	statusbar_update(STATUS_SAVING_MODULE, TRUE);
	if(XM_Save(xm, localname, save_smpls)) {
		gui_error_dialog(N_("Saving module failed"));
	    statusbar_update(STATUS_IDLE, FALSE);
	} else {
	    xm_set_modified(0);
	    gui_auto_switch_page();
	    statusbar_update(STATUS_MODULE_SAVED, FALSE);
	    gui_update_title (data);
	}

	g_free(localname);
}

void
gui_save_current (void)
{
	if(current_filename)
		gui_save(current_filename, TRUE);
	else
		fileops_open_dialog(NULL, (gpointer)1);
}

static void
save_wav (gchar *fn)
{
	int l;
	gchar *path = gui_filename_from_utf8(fn);

	if(!path)
		return;

	l = strlen(path);

	file_selection_save_path(fn, &gui_settings.savemodaswav_path);
	audio_ctlpipe_id i = AUDIO_CTLPIPE_RENDER_SONG_TO_FILE;

	gui_play_stop();

	if(write(audio_ctlpipe, &i, sizeof(i)) != sizeof(i) ||
	   write(audio_ctlpipe, &l, sizeof(l)) != sizeof(l) ||
	   write(audio_ctlpipe, path, l + 1) != l + 1)
		gui_error_dialog(N_("Connection with audio thread failed!"));
	wait_for_player();
	g_free(path);
}

static void
gui_shrink_callback (XMPattern *data)
{
	st_shrink_pattern(data);
	gui_update_pattern_data();
	tracker_set_pattern(tracker, NULL);
	tracker_set_pattern(tracker, data);
	xm_set_modified(1);
}

void
gui_shrink_pattern ()
{
	XMPattern *patt = tracker->curpattern;

	if(st_check_if_odd_are_not_empty(patt)) {
		if(gui_ok_cancel_modal(mainwindow,
		                       _("Odd pattern rows contain data which will be lost after shrinking.\n"
		                         "Do you want to continue anyway?")))
			gui_shrink_callback(patt);
	} else {
		gui_shrink_callback(patt);
	}
}

static void
gui_expand_callback (XMPattern *data)
{
	st_expand_pattern(data);
	gui_update_pattern_data();
	tracker_set_pattern(tracker, NULL);
	tracker_set_pattern(tracker, data);
	xm_set_modified(1);
}

void
gui_expand_pattern ()
{
	XMPattern *patt = tracker->curpattern;

	if(patt->length > 128) {
		if(gui_ok_cancel_modal(mainwindow,
		                       _("The pattern is too long for expanding.\n"
		                       "Some data at the end of the pattern will be lost.\n"
		                       "Do you want to continue anyway?")))
			gui_expand_callback(patt);
	} else {
		gui_expand_callback(patt);
	}
}

static void
gui_pattern_length_correct (FILE *f, int length, gint reply)
{
    XMPattern *patt = tracker->curpattern;

    switch (reply) {
    case GTK_RESPONSE_YES: /* Yes! */
	st_set_pattern_length (patt, length);
	gui_update_pattern_data ();/* Falling through */
    case GTK_RESPONSE_NO: /* No! */
	if (xm_xp_load (f, length, patt, xm)) {
	    tracker_set_pattern (tracker, NULL);
	    tracker_set_pattern (tracker, patt);
	    xm_set_modified(1);
	}
    case 2: /* Cancel, do nothing */
    default:
	break;
    }
}

static void
load_xm (const gchar *fn)
{
	static GtkWidget *dialog = NULL;

	file_selection_save_path(fn, &gui_settings.loadmod_path);
	if(xm_get_modified()) {
		gint response;

		if(!dialog)
			dialog = gtk_message_dialog_new(GTK_WINDOW(mainwindow), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
			                                _("Are you sure you want to free the current project?\nAll changes will be lost!"));

		response = gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_hide(dialog);
		if(response == GTK_RESPONSE_OK) {
			gui_load_xm(fn);
			gui_auto_switch_page();
		}
	} else {
		gui_load_xm(fn);
		gui_auto_switch_page();
	}
}

static void
save_song (gchar *fn)
{
	file_selection_save_path(fn, &gui_settings.savemod_path);
	gui_save(fn, TRUE); /* with samples */
}

static void
save_xm (gchar *fn)
{
	file_selection_save_path(fn, &gui_settings.savesongasxm_path);
	gui_save(fn, FALSE); /* without samples */
}

static void
save_pat (gchar *fn)
{
	gchar *localname = gui_filename_from_utf8(fn);

	if(!localname)
		return;

	file_selection_save_path(fn, &gui_settings.savepat_path);
	xm_xp_save (localname, tracker->curpattern, xm);
	g_free(localname);
}

static void
load_pat (const gchar *fn)
{
	int length;
	FILE *f;
	static GtkWidget *dialog = NULL, *dialog1 = NULL;

	XMPattern *patt = tracker->curpattern;
	gchar *localname = gui_filename_from_utf8(fn);

	if(!localname)
		return;

	f = fopen(localname, "r");
	g_free(localname);

	file_selection_save_path(fn, &gui_settings.loadpat_path);

	if (!f){
		gint response;

		if(!dialog)
			dialog = gtk_message_dialog_new(GTK_WINDOW(mainwindow), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
			                                _("Error when opening pattern file %s!"), fn);

		response = gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_hide(dialog);

		return;
	}
	if (xm_xp_load_header (f, &length)) {
		if (length == patt->length) {
			if (xm_xp_load (f, length, patt, xm)) {
				tracker_set_pattern (tracker, NULL);
				tracker_set_pattern (tracker, patt);
				xm_set_modified(1);
			}
		} else {
			gint response;

			if(!dialog1) {
				dialog1 = gtk_message_dialog_new(GTK_WINDOW(mainwindow), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
				                                 _("The length of the pattern being loaded doesn't match with that of current pattern in module.\n"
				                                   "Do you want to change the current pattern length?"));
				gtk_dialog_add_buttons(GTK_DIALOG(dialog1), GTK_STOCK_YES, GTK_RESPONSE_YES, GTK_STOCK_NO, GTK_RESPONSE_NO,
				                                           GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);
			}

			response = gtk_dialog_run(GTK_DIALOG(dialog1));
			gtk_widget_hide(dialog1);
			gui_pattern_length_correct(f, length, response);
		}
	}
	fclose (f);
}

static void
current_instrument_changed (GtkSpinButton *spin)
{
    int ins;
    
    int m = xm_get_modified();
    STInstrument *i = &xm->instruments[ins = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin))-1];
    STSample *s = &i->samples[gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cursmpl_spin))];

    instrument_editor_set_instrument(i);
    sample_editor_set_sample(s);
    modinfo_set_current_instrument(ins);
    xm_set_modified(m);
}

static void
current_instrument_name_changed (void)
{
	gchar *term;
    STInstrument *i = &xm->instruments[gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin))-1];

	if(i->no_cb) /* Instrument name is not modified, only current instrument is changed */
		return;

    g_utf8_strncpy(i->utf_name, gtk_entry_get_text(GTK_ENTRY(gui_curins_name)), 22);
    term = g_utf8_offset_to_pointer(i->utf_name, 23);
    term[0] = 0;
    i->needs_conversion = TRUE;
    modinfo_update_instrument(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin))-1);
    xm_set_modified(1);
}

static void
current_sample_changed (GtkSpinButton *spin)
{
    int smpl;

    int m = xm_get_modified();
    STInstrument *i = &xm->instruments[gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin))-1];
    STSample *s = &i->samples[smpl = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cursmpl_spin))];

    gtk_entry_set_text(GTK_ENTRY(gui_cursmpl_name), s->utf_name);
    sample_editor_set_sample(s);
    modinfo_set_current_sample(smpl);
    xm_set_modified(m);
}

static void
current_sample_name_changed (void)
{
	gchar *term;
    STInstrument *i = &xm->instruments[gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin))-1];
    STSample *s = &i->samples[gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cursmpl_spin))];

	if(s->no_cb) /* The sample name is not modified; only current sample is changed */
		return;

    g_utf8_strncpy(s->utf_name, gtk_entry_get_text(GTK_ENTRY(gui_cursmpl_name)), 22);
    term = g_utf8_offset_to_pointer(i->utf_name, 23);
    term[0] = 0;
    s->needs_conversion = TRUE;
    modinfo_update_sample(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cursmpl_spin)));
    xm_set_modified(1);
}

static gboolean
gui_handle_standard_keys (int shift,
			  int ctrl,
			  int alt,
			  guint32 keyval)
{
    gboolean handled = FALSE, b;
    int currpos;

    switch (keyval) {
    case GDK_F1 ... GDK_F7:
	if(!shift && !ctrl && !alt) {
	    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_octave), keyval - GDK_F1);
	    handled = TRUE;
	}
	break;
    case '1' ... '8':
	if(ctrl) {
	    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_jump), keyval - '0');
	    handled = TRUE;
        }
        if(alt) {
            switch(keyval)
            {
	    case '1' ... '5':
                gtk_notebook_set_page(GTK_NOTEBOOK(notebook), keyval - '1');
                break;
	    default:
		break;
            }
	    handled = TRUE;
        }
	break;
    case GDK_Left:
	if(ctrl) {
	    /* previous instrument */
	    offset_current_instrument(shift ? -5 : -1);
	    handled = TRUE;
	} else if(alt) {
	    /* previous pattern */
	    offset_current_pattern(shift ? -10 : -1);
	    handled = TRUE;
	} else if(shift){
	    /* previous position */
	    currpos = playlist_get_position (playlist);
	    if((--currpos) >= 0 ) {
		playlist_set_position (playlist, currpos);
		handled = TRUE;
	    }
	}
	break;
    case GDK_Right:
	if(ctrl) {
	    /* next instrument */
	    offset_current_instrument(shift ? 5 : 1);
	    handled = TRUE;
	} else if(alt) {
	    /* next pattern */
	    offset_current_pattern(shift ? 10 : 1);
	    handled = TRUE;
	} else if(shift){
	    /* next position */
	    currpos = playlist_get_position (playlist);
	    if ((++currpos) < xm->song_length ) {
		playlist_set_position (playlist, currpos);
		handled = TRUE;
	    }
        }
	break;
    case GDK_Up:
	if(ctrl) {
	    /* next sample */
	    offset_current_sample(shift ? 4 : 1);
	    handled = TRUE;
	}
	break;
    case GDK_Down:
	if(ctrl) {
	    /* previous sample */
	    offset_current_sample(shift ? -4 : -1);
	    handled = TRUE;
	}
	break;
    case GDK_Alt_R:
    case GDK_Meta_R:
    case GDK_Super_R:
    case GDK_Hyper_R:
    case GDK_Mode_switch: /* well... this is X :D */
    case GDK_Multi_key:
    case GDK_ISO_Level3_Shift:
	play_pattern();
	if(shift)
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(editing_toggle), TRUE);
	handled = TRUE;
	break;
    case GDK_Control_R:
	play_song();
	if(shift)
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(editing_toggle), TRUE);
	handled = TRUE;
	break;
    case GDK_Menu:
	play_current_pattern_row();
	break;
    case ' ':
        if(ctrl || alt || shift)
            break;
	b = GUI_ENABLED;
	if (!b)
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(editing_toggle), FALSE);

	gui_play_stop();
	if(notebook_current_page != NOTEBOOK_PAGE_SAMPLE_EDITOR
	   && notebook_current_page != NOTEBOOK_PAGE_INSTRUMENT_EDITOR
	   && notebook_current_page != NOTEBOOK_PAGE_FILE
	   && notebook_current_page != NOTEBOOK_PAGE_MODULE_INFO) {
	    if(b) {
		/* toggle editing mode (only if we haven't been in playing mode) */
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(editing_toggle), !GUI_EDITING);
	    }
	}
	handled = TRUE;
	break;
    case GDK_Escape:
        if(ctrl || alt || shift)
            break;
	/* toggle editing mode, even if we're in playing mode */
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(editing_toggle), !GUI_EDITING);
	handled = TRUE;
	break;
    }

    return handled;
}

static int
keyevent (GtkWidget *widget,
	  GdkEventKey *event,
	  gpointer data)
{
    static gboolean (*handle_page_keys[])(int,int,int,guint32,gboolean) = {
	fileops_page_handle_keys,
	track_editor_handle_keys,
	instrument_editor_handle_keys,
	sample_editor_handle_keys,
	modinfo_page_handle_keys,
    };
    gboolean pressed = (gboolean)GPOINTER_TO_INT(data);
    gboolean handled = FALSE;
    gboolean entry_focus = GTK_IS_ENTRY(GTK_WINDOW(mainwindow)->focus_widget);

    if(!entry_focus && GTK_WIDGET_VISIBLE(notebook)) {
	int shift = event->state & GDK_SHIFT_MASK;
	int ctrl = event->state & GDK_CONTROL_MASK;
	int alt = event->state & GDK_MOD1_MASK;

	if(pressed)
	    handled = gui_handle_standard_keys(shift, ctrl, alt, event->keyval);
	handled = handled || handle_page_keys[notebook_current_page](shift, ctrl, alt, event->keyval, pressed);

	if(!handled) switch(event->keyval) {
	    /* from gtk+-1.2.8's gtkwindow.c. These keypresses need to
	       be stopped in any case. */
	case GDK_Up:
	case GDK_Down:
	case GDK_Left:
	case GDK_Right:
	case GDK_KP_Up:
	case GDK_KP_Down:
	case GDK_KP_Left:
	case GDK_KP_Right:
	case GDK_Tab:
	case GDK_ISO_Left_Tab:
	    handled = TRUE;
	}

	if(handled) {
	    if(pressed) {
		gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "key-press-event");
	    } else {
		gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "key-release-event");
	    }
	}
    } else {
	if(pressed) {
	    switch(event->keyval) {
	    case GDK_Return:
	    if(notebook_current_page == NOTEBOOK_PAGE_FILE) /* Saving file by enter pressing in filename entry */
			break;
	    case GDK_Tab:
		gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "key-press-event");
		gtk_window_set_focus(GTK_WINDOW(mainwindow), NULL);
		break;
	    }
	}
    }

    return handled;
}

static void
gui_playlist_position_changed (Playlist *p,
			       int newpos)
{
    if(gui_playing_mode == PLAYING_SONG) {
	// This will only be executed when the user changes the song position manually
	event_waiter_start(audio_songpos_ew);
	gui_mixer_set_songpos(newpos);
    } else {
	if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle_lock_editpat))) {
	    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_editpat),
				      playlist_get_nth_pattern(p, newpos));
	}
    }
}

static void
gui_playlist_restart_position_changed (Playlist *p,
				       int pos)
{
    xm->restart_position = pos;
    xm_set_modified(1);
}

static void
gui_playlist_entry_changed (Playlist *p,
			    int pos,
			    int pat)
{
    int i;

    if(pos != -1) {
	xm->pattern_order_table[pos] = pat;
	if(pos == playlist_get_position(p)
	    && !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle_lock_editpat))) {
	    gui_set_current_pattern(pat, TRUE);
	}
    } else {
	for(i = 0; i < xm->song_length; i++) {
	    xm->pattern_order_table[i] = playlist_get_nth_pattern(p, i);
	}
    }

    xm_set_modified(1);
}

static void
gui_playlist_song_length_changed (Playlist *p,
				  int length)
{
    xm->song_length = length;
    gui_playlist_entry_changed(p, -1, -1);
}

static void
gui_editpat_changed (GtkSpinButton *spin)
{
    int n = gtk_spin_button_get_value_as_int(spin);

    if(n != editing_pat) {
	gui_set_current_pattern(n, FALSE);

	/* If we are in 'playing pattern' mode and asynchronous
	 * editing is disabled, make the audio thread jump to the new
	 * pattern, too. I think it would be cool to have this for
	 * 'playing song' mode, too, but then modifications in
	 * gui_update_player_pos() will be necessary. */
	if(gui_playing_mode == PLAYING_PATTERN && !ASYNCEDIT) {
	    gui_mixer_set_pattern(n);
	}
    }
}

static void
gui_patlen_changed (GtkSpinButton *spin)
{
    int n = gtk_spin_button_get_value_as_int(spin);
    XMPattern *pat = &xm->patterns[editing_pat];

    if(n != pat->length) {
	st_set_pattern_length(pat, n);
	tracker_set_pattern(tracker, NULL);
	tracker_set_pattern(tracker, pat);
	xm_set_modified(1);
    }
}

static void
gui_numchans_changed (GtkSpinButton *spin)
{
    int n = gtk_spin_button_get_value_as_int(spin);

    if(n & 1) {
	gtk_spin_button_set_value(spin, --n);
	return;
    }

    if(xm->num_channels != n) {
	gui_play_stop();
	tracker_set_pattern(tracker, NULL);
	st_set_num_channels(xm, n);
	gui_init_xm(0, FALSE);
	xm_set_modified(1);
    }
}

static void
gui_tempo_changed (int value)
{
    audio_ctlpipe_id i;
    xm->tempo = value;
    xm_set_modified(1);
    if(gui_playing_mode) {
	event_waiter_start(audio_tempo_ew);
    }
    i = AUDIO_CTLPIPE_SET_TEMPO;
	if(write(audio_ctlpipe, &i, sizeof(i)) != sizeof(i) ||
	   write(audio_ctlpipe, &value, sizeof(value)) != sizeof(value))
		gui_error_dialog(N_("Connection with audio thread failed!"));
}

static void
gui_bpm_changed (int value)
{
    audio_ctlpipe_id i;
    xm->bpm = value;
    xm_set_modified(1);
    if(gui_playing_mode) {
	event_waiter_start(audio_bpm_ew);
    }
    i = AUDIO_CTLPIPE_SET_BPM;
	if(write(audio_ctlpipe, &i, sizeof(i)) != sizeof(i) ||
	   write(audio_ctlpipe, &value, sizeof(value)) != sizeof(value))
		gui_error_dialog(N_("Connection with audio thread failed!"));
}

static void
gui_adj_amplification_changed (GtkAdjustment *adj)
{
    audio_ctlpipe_id a = AUDIO_CTLPIPE_SET_AMPLIFICATION;
    float b = 8.0 - adj->value;

	if(write(audio_ctlpipe, &a, sizeof(a)) != sizeof(a) ||
	   write(audio_ctlpipe, &b, sizeof(b)) != sizeof(b))
		gui_error_dialog(N_("Connection with audio thread failed!"));
}

static void
gui_adj_pitchbend_changed (GtkAdjustment *adj)
{
    audio_ctlpipe_id a = AUDIO_CTLPIPE_SET_PITCHBEND;
    float b = adj->value;

	if(write(audio_ctlpipe, &a, sizeof(a)) != sizeof(a) ||
	   write(audio_ctlpipe, &b, sizeof(b)) != sizeof(b))
		gui_error_dialog(N_("Connection with audio thread failed!"));
}

static void
gui_reset_pitch_bender (void)
{
    gtk_adjustment_set_value(adj_pitchbend, 0.0);
}

static void
notebook_page_switched (GtkNotebook *notebook,
			GtkNotebookPage *page,
			int page_num)
{
    notebook_current_page = page_num;
    if(page_num != NOTEBOOK_PAGE_FILE)
		fileops_restore_subpage();
	else
		fileops_focus_entry();
}

/* gui_update_player_pos() is responsible for updating various GUI
   features according to the current player position. ProTracker and
   FastTracker scroll the patterns while the song is playing, and we
   need the time-buffer and event-waiter interfaces here for correct
   synchronization (we're called from
   track-editor.c::tracker_timeout() which hands us time-correct data)

   We have an ImpulseTracker-like editing mode as well ("asynchronous
   editing"), which disables the scrolling, but still updates the
   current song position spin buttons, for example. */

void
gui_update_player_pos (const audio_player_pos *p)
{
    int m = xm_get_modified();

    if(gui_playing_mode == PLAYING_NOTE)
	return;

    if(gui_playing_mode == PLAYING_SONG) {
	if(event_waiter_ready(audio_songpos_ew, p->time)) {
	    /* The following check prevents excessive calls of set_position() */
	    if(p->songpos != playlist_get_position(playlist)) {
		playlist_freeze_signals(playlist);
		playlist_set_position(playlist, p->songpos);
		playlist_thaw_signals(playlist);
	    }
	}
	if(!ASYNCEDIT) {
	    /* The following is a no-op if we're already in the right pattern */
	    gui_set_current_pattern(xm->pattern_order_table[p->songpos], TRUE);
	}
    }

    if(!ASYNCEDIT) {
	tracker_set_patpos(tracker, p->patpos);

	if(notebook_current_page == 0) {
	    /* Prevent accumulation of X drawing primitives */
	    gdk_flush();
	}
    }

    if(gui_settings.tempo_bpm_update) {
	if(event_waiter_ready(audio_tempo_ew, p->time)) {
	    tempo_slider.update_without_signal = TRUE;
	    gui_subs_set_slider_value(&tempo_slider, p->tempo);
	    tempo_slider.update_without_signal = FALSE;
	}

	if(event_waiter_ready(audio_bpm_ew, p->time)) {
	    bpm_slider.update_without_signal = TRUE;
	    gui_subs_set_slider_value(&bpm_slider, p->bpm);
	    bpm_slider.update_without_signal = FALSE;
	}
    }

    xm_set_modified(m);
}

void
gui_clipping_indicator_update (double songtime)
{
    if(songtime < 0.0) {
	gui_clipping_led_status = 0;
    } else {
	audio_clipping_indicator *c = time_buffer_get(audio_clipping_indicator_tb, songtime);
	gui_clipping_led_status = c && c->clipping;
    }
    gtk_widget_draw(gui_clipping_led, NULL);
}

static void
read_mixer_pipe (gpointer data,
		 gint source,
		 GdkInputCondition condition)
{
    audio_backpipe_id a;
    struct pollfd pfd = { source, POLLIN, 0 };
    int x;

    static char *msgbuf = NULL;
    static int msgbuflen = 0;

  loop:
    if(poll(&pfd, 1, 0) <= 0)
	return;

	if(read(source, &a, sizeof(a)) != sizeof(a))
		gui_error_dialog(N_("Connection with audio thread failed!"));
//    printf("read %d\n", a);

    switch(a) {
    case AUDIO_BACKPIPE_PLAYING_STOPPED:
        statusbar_update(STATUS_IDLE, FALSE);
        clock_stop(CLOCK(st_clock));

        if(gui_ewc_startstop > 0) {
	    /* can be equal to zero when the audio subsystem decides to stop playing on its own. */
	    gui_ewc_startstop--;
	}
	gui_playing_mode = 0;
	scope_group_stop_updating(scopegroup);
	tracker_stop_updating();
	sample_editor_stop_updating();
	gui_enable(1);
	break;

    case AUDIO_BACKPIPE_PLAYING_STARTED:
        statusbar_update(STATUS_PLAYING_SONG, FALSE);
	/* fall through */

    case AUDIO_BACKPIPE_PLAYING_PATTERN_STARTED:
        if(a == AUDIO_BACKPIPE_PLAYING_PATTERN_STARTED)
	    statusbar_update(STATUS_PLAYING_PATTERN, FALSE);
        clock_set_seconds(CLOCK(st_clock), 0);
        clock_start(CLOCK(st_clock));

        gui_ewc_startstop--;
	gui_playing_mode = (a == AUDIO_BACKPIPE_PLAYING_STARTED) ? PLAYING_SONG : PLAYING_PATTERN;
	if(!ASYNCEDIT) {
	    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(editing_toggle), FALSE);
	}
	gui_enable(0);
	scope_group_start_updating(scopegroup);
	tracker_start_updating();
	sample_editor_start_updating();
	break;

    case AUDIO_BACKPIPE_PLAYING_NOTE_STARTED:
	gui_ewc_startstop--;
	if(!gui_playing_mode) {
	    gui_playing_mode = PLAYING_NOTE;
	    scope_group_start_updating(scopegroup);
	    tracker_start_updating();
	    sample_editor_start_updating();
	}
	break;

    case AUDIO_BACKPIPE_DRIVER_OPEN_FAILED:
	gui_ewc_startstop--;
        break;

    case AUDIO_BACKPIPE_ERROR_MESSAGE:
    case AUDIO_BACKPIPE_WARNING_MESSAGE:
        statusbar_update(STATUS_IDLE, FALSE);
        readpipe(source, &x, sizeof(x));
	if(msgbuflen < x + 1) {
	    g_free(msgbuf);
	    msgbuf = g_new(char, x + 1);
	    msgbuflen = x + 1;
	}
	readpipe(source, msgbuf, x + 1);
	if(a == AUDIO_BACKPIPE_ERROR_MESSAGE)
            gui_error_dialog(msgbuf);
	else
            gui_warning_dialog(msgbuf);
	break;

    default:
	fprintf(stderr, "\n\n*** read_mixer_pipe: unexpected backpipe id %d\n\n\n", a);
	g_assert_not_reached();
	break;
    }

    goto loop;
}

static void
wait_for_player (void)
{
    struct pollfd pfd = { audio_backpipe, POLLIN, 0 };

    gui_ewc_startstop++;
    while(gui_ewc_startstop != 0) {
	g_return_if_fail(poll(&pfd, 1, -1) > 0);
	read_mixer_pipe(NULL, audio_backpipe, 0);
    }
}

void
play_song (void)
{
    int sp = playlist_get_position(playlist);
    int pp = 0;
    audio_ctlpipe_id i = AUDIO_CTLPIPE_PLAY_SONG;

    g_assert(xm != NULL);

    gui_play_stop();

	if(write(audio_ctlpipe, &i, sizeof(i)) != sizeof(i) ||
	   write(audio_ctlpipe, &sp, sizeof(sp)) != sizeof(sp) ||
	   write(audio_ctlpipe, &pp, sizeof(pp)) != sizeof(pp))
		gui_error_dialog(N_("Connection with audio thread failed!"));
    wait_for_player();
}

static void
play_pattern (void)
{
    gui_play_stop();
    gui_mixer_play_pattern(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_editpat)), 0, 0);
    wait_for_player();
}

static void
play_current_pattern_row (void)
{
    gui_play_stop();
    gui_mixer_play_pattern(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_editpat)), tracker->patpos, 1);
    gui_ewc_startstop++;
}

void
gui_play_stop (void)
{
    gui_mixer_stop_playing();
    wait_for_player();
}

void
gui_init_xm (int new_xm, gboolean updatechspin)
{
    int m = xm_get_modified();
    audio_ctlpipe_id i;

    i = AUDIO_CTLPIPE_INIT_PLAYER;
	if(write(audio_ctlpipe, &i, sizeof(i)) != sizeof(i))
		gui_error_dialog(N_("Connection with audio thread failed!"));
    tracker_reset(tracker);
    if(new_xm) {
	gui_playlist_initialize();
	editing_pat = -1;
	gui_set_current_pattern(xm->pattern_order_table[0], TRUE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(curins_spin), 1);
	current_instrument_changed(GTK_SPIN_BUTTON(curins_spin));
	modinfo_set_current_instrument(0);
	modinfo_set_current_sample(0);
	modinfo_update_all();
    } else {
	i = editing_pat;
	editing_pat = -1;
	gui_set_current_pattern(i, TRUE);
    }
    gui_subs_set_slider_value(&tempo_slider, xm->tempo);
    gui_subs_set_slider_value(&bpm_slider, xm->bpm);
    track_editor_set_num_channels(xm->num_channels);
    if(updatechspin)
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_numchans), xm->num_channels);
    scope_group_set_num_channels(scopegroup, xm->num_channels);
    xm_set_modified(m);
}

void
gui_free_xm (void)
{
    gui_play_stop();
    instrument_editor_set_instrument(NULL);
    sample_editor_set_sample(NULL);
    tracker_set_pattern(tracker, NULL);
    XM_Free(xm);
    xm = NULL;
}

void
gui_new_xm (void)
{
    xm = XM_New();

    if(!xm) {
	fprintf(stderr, "Whooops, having memory problems?\n");
	exit(1);
    }
    gui_init_xm(1, TRUE);
}

static void
gui_load_xm (const char *filename)
{
	gchar *newname;
    statusbar_update(STATUS_LOADING_MODULE, TRUE);

    gui_free_xm();
    newname = gui_filename_from_utf8(filename);
    if(newname) {
		xm = File_Load(newname);
		g_free(newname);
	}

    if(!xm) {
	gui_new_xm();
	statusbar_update(STATUS_IDLE, FALSE);
    } else {
	gui_init_xm(1, TRUE);
	statusbar_update(STATUS_MODULE_LOADED, FALSE);
	gui_update_title (filename);
    }
}

void
gui_play_note (int channel,
	       int note)
{
    int instrument = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin));
    audio_ctlpipe_id a = AUDIO_CTLPIPE_PLAY_NOTE;

	if(write(audio_ctlpipe, &a, sizeof(a)) != sizeof(a) ||
	   write(audio_ctlpipe, &channel, sizeof(channel)) != sizeof(channel) ||
	   write(audio_ctlpipe, &note, sizeof(note)) != sizeof(note) ||
	   write(audio_ctlpipe, &instrument, sizeof(instrument)) != sizeof(instrument))
		gui_error_dialog(N_("Connection with audio thread failed!"));
    gui_ewc_startstop++;
}

void
gui_play_note_full (unsigned channel,
		    unsigned note,
		    STSample *sample,
		    guint32 offset,
		    guint32 count)
{
    int x;
    gboolean result;
    audio_ctlpipe_id a = AUDIO_CTLPIPE_PLAY_NOTE_FULL;

    g_assert(sizeof(int) == sizeof(unsigned));

	result = write(audio_ctlpipe, &a, sizeof(a)) != sizeof(a) ||
	         write(audio_ctlpipe, &channel, sizeof(channel)) != sizeof(channel) ||
	         write(audio_ctlpipe, &note, sizeof(note)) != sizeof(note) ||
	         write(audio_ctlpipe, &sample, sizeof(sample)) != sizeof(sample);
    x = offset; result |= write(audio_ctlpipe, &x, sizeof(x)) != sizeof(x);
    x = count; result |= write(audio_ctlpipe, &x, sizeof(x)) != sizeof(x);
	if(result)
		gui_error_dialog(N_("Connection with audio thread failed!"));
    gui_ewc_startstop++;
}

void
gui_play_note_keyoff (int channel)
{
    audio_ctlpipe_id a = AUDIO_CTLPIPE_PLAY_NOTE_KEYOFF;

	if(write(audio_ctlpipe, &a, sizeof(a)) != sizeof(a) ||
	   write(audio_ctlpipe, &channel, sizeof(channel)) != sizeof(channel))
		gui_error_dialog(N_("Connection with audio thread failed!"));
}

static void
gui_enable (int enable)
{
    if(!ASYNCEDIT) {
	gtk_widget_set_sensitive(vscrollbar, enable);
	gtk_widget_set_sensitive(spin_patlen, enable);
    }
    playlist_enable(playlist, enable);
}

void
gui_set_current_pattern (int p, gboolean updatespin)
{
    int m;

    if(editing_pat == p)
	return;

    m = xm_get_modified();

    editing_pat = p;
    tracker_set_pattern(tracker, &xm->patterns[p]);
    if(updatespin)
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_editpat), p);
    gui_update_pattern_data();

    xm_set_modified(m);
}

void
gui_update_pattern_data (void)
{
    int p = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_editpat));

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_patlen), xm->patterns[p].length);
}

int
gui_get_current_pattern (void)
{
    return editing_pat;
}

static void
offset_current_pattern (int offset)
{
    int nv;

    nv = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_editpat)) + offset;
    if(nv < 0)
	nv = 0;
    else if(nv > 255)
	nv = 255;

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_editpat), nv);
}

void
gui_set_current_instrument (int n)
{
    int m = xm_get_modified();

    g_return_if_fail(n >= 1 && n <= 128);
    if(n != gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin))) {
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(curins_spin), n);
    }
    xm_set_modified(m);
}

void
gui_set_current_sample (int n)
{
    int m = xm_get_modified();
    g_return_if_fail(n >= 0 && n <= 127);
    if(n != gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cursmpl_spin))) {
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(cursmpl_spin), n);
    }
    xm_set_modified(m);
}

int
gui_get_current_sample (void)
{
    return gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cursmpl_spin));
}

int
gui_get_current_octave_value (void)
{
    return gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_octave));
}

int
gui_get_current_jump_value (void)
{
    if(!GUI_ENABLED && !ASYNCEDIT)
	return 0;
    else
	return gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_jump));
}

void
gui_set_jump_value (int value)
{
    if(GUI_ENABLED || ASYNCEDIT){
 	g_return_if_fail (value >= 0 && value <= 16);
 	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_jump), value);
    }
}

int
gui_get_current_instrument (void)
{
    return gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin));
}

static void
offset_current_instrument (int offset)
{
    int nv, v;

    v = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin));
    nv = v + offset;

    if(nv < 1)
	nv = 1;
    else if(nv > 128)
	nv = 128;

    gui_set_current_instrument(nv);
}

static void
offset_current_sample (int offset)
{
    int nv, v;

    v = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cursmpl_spin));
    nv = v + offset;

    if(nv < 0)
	nv = 0;
    else if(nv > 127)
	nv = 127;

    gui_set_current_sample(nv);
}

static void
gui_add_free_pattern (GtkWidget *w, Playlist *p)
{
    int pos = playlist_get_position(p) + 1;

    if(p->length < p->max_length) {
	int n = st_find_first_unused_and_empty_pattern(xm);

	if(n != -1) {
	    playlist_insert_pattern(p, pos, n);
	    playlist_set_position(p, pos);
	}
    }
}

static void
gui_add_free_pattern_and_copy (GtkWidget *w, Playlist *p)
{
    int pos = playlist_get_position(p) + 1;

    if(p->length < p->max_length) {
	int n = st_find_first_unused_and_empty_pattern(xm);
	int c = gui_get_current_pattern();

	if(n != -1) {
	    st_copy_pattern(&xm->patterns[n], &xm->patterns[c]);
	    playlist_insert_pattern(p, pos, n);
	    playlist_set_position(p, pos);
	}
    }
}

void
gui_get_text_entry (int length,
		    void(*changedfunc)(),
		    GtkWidget **widget)
{
    GtkWidget *thing;

    thing = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(thing), length);

    g_signal_connect(thing, "changed",
                     G_CALLBACK(changedfunc), NULL);

    *widget = thing;
}

void
gui_playlist_initialize (void)
{
    int i;

    playlist_freeze_signals(playlist);
    playlist_freeze(playlist);
    playlist_set_position(playlist, 0);
    playlist_set_length(playlist, xm->song_length);
    for(i = 0; i < xm->song_length; i++) {
	playlist_set_nth_pattern(playlist, i, xm->pattern_order_table[i]);
    }
    playlist_set_restart_position(playlist, xm->restart_position);
    playlist_thaw(playlist);
    playlist_thaw_signals(playlist);
}

static gint
gui_clipping_led_event (GtkWidget *thing,
			GdkEvent *event)
{
    static GdkGC *clipping_led_gc = NULL;

    if(!clipping_led_gc)
	clipping_led_gc = gdk_gc_new(thing->window);

    switch (event->type) {
    case GDK_MAP:
    case GDK_EXPOSE:
	gdk_gc_set_foreground(clipping_led_gc, gui_clipping_led_status ? &gui_clipping_led_on : &gui_clipping_led_off);
	gdk_draw_rectangle(thing->window, clipping_led_gc, 1, 0, 0, -1, -1);
    default:
	break;
    }
    
    return 0;
}

void
gui_go_to_fileops_page (void)
{
    gtk_notebook_set_page(GTK_NOTEBOOK(notebook),
			  0);
}

void
gui_go_to_page (gint page)
{
	gtk_notebook_set_page(GTK_NOTEBOOK(notebook),
                         page);
}

void
gui_auto_switch_page (void)
{
    if(gui_settings.auto_switch)
        gtk_notebook_set_page(GTK_NOTEBOOK(notebook),
                              1);
}

#ifndef NO_GDK_PIXBUF
static gint
gui_splash_logo_expose (GtkWidget *widget,
			GdkEventExpose *event,
			gpointer data)
{
    gdk_pixbuf_render_to_drawable (gui_splash_logo,
				   widget->window,
				   widget->style->black_gc,
				   event->area.x, event->area.y,
				   event->area.x, event->area.y,
				   event->area.width, event->area.height,
				   GDK_RGB_DITHER_NORMAL,
				   0, 0);

    return TRUE;
}
#endif

static void
gui_splash_close (void)
{
    gtk_widget_destroy(gui_splash_window);
    gui_splash_window = NULL;
}

static void
gui_splash_set_label (const gchar *text,
		      gboolean update)
{
    char buf[256];

    strcpy(buf, "SoundTracker v" VERSION " - ");
    strncat(buf, text, 255 - strlen(buf));

    gtk_label_set_text(GTK_LABEL(gui_splash_label), buf);

    while(update && gtk_events_pending ()) {
	gtk_main_iteration ();
    }
}

int
gui_splash (void)
{
    GtkWidget *vbox, *thing;
#ifndef NO_GDK_PIXBUF
    GtkWidget *hbox, *logo_area, *frame;
#endif

    gdk_rgb_init();

    gui_splash_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    gtk_window_set_title (GTK_WINDOW(gui_splash_window), _("SoundTracker Startup"));
    gtk_window_set_position (GTK_WINDOW (gui_splash_window), GTK_WIN_POS_CENTER);
    gtk_window_set_policy (GTK_WINDOW (gui_splash_window), FALSE, FALSE, FALSE);
    gtk_window_set_modal(GTK_WINDOW(gui_splash_window), TRUE);

    g_signal_connect(gui_splash_window, "delete_event",
		       G_CALLBACK(gui_splash_close), NULL);

    vbox = gtk_vbox_new (FALSE, 4);
    gtk_container_add (GTK_CONTAINER (gui_splash_window), vbox);

    gtk_container_border_width(GTK_CONTAINER(vbox), 4);

    /* Show splash screen if enabled and image available. */

#ifndef NO_GDK_PIXBUF
    GError *error = NULL;
    gui_splash_logo = gdk_pixbuf_new_from_file(PREFIX"/share/soundtracker/soundtracker_splash.png", &error);
    if(gui_splash_logo) {
	thing = gtk_hseparator_new();
	gtk_widget_show(thing);
	gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, TRUE, 0);

	hbox = gtk_hbox_new(FALSE, 4);
	gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	add_empty_vbox(hbox);

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(hbox), frame, FALSE, TRUE, 0);
	gtk_widget_show(frame);

	gui_splash_logo_area = logo_area = gtk_drawing_area_new ();
	gtk_container_add (GTK_CONTAINER(frame), logo_area);
	gtk_widget_show(logo_area);

	add_empty_vbox(hbox);

	g_signal_connect(logo_area, "expose_event",
			    G_CALLBACK(gui_splash_logo_expose),
			    NULL);

	gtk_drawing_area_size (GTK_DRAWING_AREA (logo_area),
			       gdk_pixbuf_get_width(gui_splash_logo),
			       gdk_pixbuf_get_height(gui_splash_logo));
    }
#endif

    /* Show version number. */

    thing = gtk_hseparator_new();
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, TRUE, 0);

    gui_splash_label = thing = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gui_splash_set_label(_("Loading..."), FALSE);

    /* Show tips if enabled. */

    if(tips_dialog_show_tips) {
	    thing = gtk_hseparator_new();
	    gtk_widget_show(thing);
	    gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, TRUE, 0);

	    tips_box_populate(vbox, FALSE);
    }

    thing = gtk_hseparator_new();
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, TRUE, 0);

    if(
#ifndef NO_GDK_PIXBUF
       gui_splash_logo ||
#endif
       tips_dialog_show_tips) {
	gui_splash_close_button = thing = gtk_button_new_with_label(_("Use SoundTracker!"));
	gtk_widget_show(thing);
	gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, TRUE, 0);
	g_signal_connect(thing, "clicked",
			   (gui_splash_close), NULL);
	gtk_widget_set_sensitive(thing, FALSE);
    }

    gtk_widget_show(vbox);
    gtk_widget_show(gui_splash_window);

    gui_splash_set_label(_("Loading..."), TRUE);

    return 1;
}

GtkStyle*
gui_get_style(void)
{
    static GtkStyle *style = NULL;
    
    if(!style) {
	if(!GTK_WIDGET_REALIZED(mainwindow))
	    gtk_widget_realize(mainwindow); /* to produce the correct style... */
	style = gtk_widget_get_style(mainwindow);
    }
    return style;
}

gboolean
quit_requested (void)
{
	if(xm_get_modified()) {
		if(gui_ok_cancel_modal(mainwindow,
		                       _("Are you sure you want to quit?\nAll changes will be lost!")))
			gtk_main_quit();
	} else {
		gtk_main_quit();
	}
	return TRUE;
}

static gboolean
is_sep (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	GtkTreePath *path = gtk_tree_model_get_path(model, iter);
	gint index = GPOINTER_TO_INT(data);
	gint *indices = gtk_tree_path_get_indices(path);
	gint curindex = indices[0];

	gtk_tree_path_free(path);
	return curindex == index;
}

int
gui_final (int argc,
	   char *argv[])
{
    GtkWidget *thing, *mainvbox, *table, *hbox, *frame, *mainvbox0, *pmw, *vbox;
    GdkColormap *colormap;
    GtkStyle *style;
    gint i, selected;
    GError *error = NULL;
    GtkListStore *ls;
    GtkTreeIter iter;

	struct menu_callback cb[] = {
		{"file_open", fileops_open_dialog, (gpointer)DIALOG_LOAD_MOD},
		{"file_save_as", fileops_open_dialog, (gpointer)DIALOG_SAVE_MOD},
		{"file_save_wav", fileops_open_dialog, (gpointer)DIALOG_SAVE_MOD_AS_WAV},
		{"file_save_xm", fileops_open_dialog, (gpointer)DIALOG_SAVE_SONG_AS_XM},
		{"module_clear_all", menubar_clear_clicked, (gpointer)1},
		{"module_clear_patterns", menubar_clear_clicked, (gpointer)0},
		{"edit_cut", menubar_handle_cutcopypaste, (gpointer)0},
		{"edit_copy", menubar_handle_cutcopypaste, (gpointer)1},
		{"edit_paste", menubar_handle_cutcopypaste, (gpointer)2},
		{"edit_track_increment_cmd", menubar_handle_edit_menu, (gpointer)0},
		{"edit_track_decrement_cmd", menubar_handle_edit_menu, (gpointer)1},
		{"popup_increment_cmd", menubar_handle_edit_menu, (gpointer)0},
		{"popup_decrement_cmd", menubar_handle_edit_menu, (gpointer)1},
		{"edit_selection_transpose_up", menubar_handle_edit_menu, (gpointer)2},
		{"edit_selection_transpose_down", menubar_handle_edit_menu, (gpointer)3},
		{"edit_selection_transpose_12up", menubar_handle_edit_menu, (gpointer)4},
		{"edit_selection_transpose_12down", menubar_handle_edit_menu, (gpointer)5},
		{"pattern_load", fileops_open_dialog, (gpointer)DIALOG_LOAD_PATTERN},
		{"pattern_save", fileops_open_dialog, (gpointer)DIALOG_SAVE_PATTERN},
		{"track_current_permanent", menubar_toggle_perm_wrapper, (gpointer)0},
		{"track_all_permanent", menubar_toggle_perm_wrapper, (gpointer)1},
		{"instrument_load", fileops_open_dialog, (gpointer)DIALOG_LOAD_INSTRUMENT},
		{"instrument_save", fileops_open_dialog, (gpointer)DIALOG_SAVE_INSTRUMENT},
		{NULL}
	};

	static const gchar *xm_f[] = {N_("FastTracker modules (*.xm)"), "*.[xX][mM]", NULL};
	static const gchar *mod_f[] = {N_("Original SoundTracker modules (*.mod)"), "*.[mM][oO][dD]", NULL};
	static const gchar **mod_formats[] = {xm_f, mod_f, NULL};
	static const gchar *save_mod_f[] = {N_("FastTracker modules (*.xm)"), "*.[xX][mM]", NULL};
	static const gchar **save_mod_formats[] = {save_mod_f, NULL};
	static const gchar *wav_f[] = {N_("Microsoft RIFF (*.wav)"), "*.[wW][aA][wW]", NULL};
	static const gchar **wav_formats[] = {wav_f, NULL};
	static const gchar *xp_f[] = {N_("Extended pattern (*.xp)"), "*.[xX][pP]", NULL};
	static const gchar **xp_formats[] = {xp_f, NULL};

    pipetag = gdk_input_add(audio_backpipe, GDK_INPUT_READ, read_mixer_pipe, NULL);

	builder = gtk_builder_new();
	if(!gtk_builder_add_from_file(builder, XML_FILE, &error)) {
		g_critical(_("%s.\n"PACKAGE" startup is aborted\nFailed GUI description file: %s\n"),
		           error->message, XML_FILE);
		g_error_free(error);
		return 0;
	}

	mainwindow = gui_get_widget("mainwindow");
	if(!mainwindow)
		return 0;
    gtk_window_set_title (GTK_WINDOW (mainwindow), "SoundTracker " VERSION);

    if(gui_splash_window) {
	gtk_window_set_transient_for(GTK_WINDOW(gui_splash_window),
				     GTK_WINDOW(mainwindow));
    }

    if(gui_settings.st_window_x != -666) {
	gtk_window_set_default_size (GTK_WINDOW (mainwindow),
				     gui_settings.st_window_w,
				     gui_settings.st_window_h);
	gtk_widget_set_uposition (GTK_WIDGET (mainwindow),
				  gui_settings.st_window_x,
				  gui_settings.st_window_y);
    }
    gtk_window_set_icon_from_file(GTK_WINDOW(mainwindow), PREFIX"/share/soundtracker/soundtracker-icon.png", NULL);

//!!! TODO pop-up tooltip hints: Render module as WAV etc.
	file_selection_create(DIALOG_LOAD_MOD, _("Load Module"), gui_settings.loadmod_path, load_xm, 0, TRUE, FALSE, FALSE, mod_formats);
	file_selection_create(DIALOG_SAVE_MOD, _("Save Module"), gui_settings.savemod_path, save_song, 1, FALSE, TRUE, FALSE, save_mod_formats);
#if USE_SNDFILE || !defined (NO_AUDIOFILE)
	file_selection_create(DIALOG_SAVE_MOD_AS_WAV, _("Render WAV"), gui_settings.savemodaswav_path, save_wav, 2, FALSE, TRUE, TRUE, wav_formats);
#endif
	file_selection_create(DIALOG_SAVE_SONG_AS_XM, _("Save XM without samples..."), gui_settings.savesongasxm_path, save_xm, -1, FALSE, TRUE, FALSE, save_mod_formats);
	file_selection_create(DIALOG_LOAD_PATTERN, _("Load current pattern..."), gui_settings.loadpat_path, load_pat, -1, FALSE, FALSE, FALSE, xp_formats);
	file_selection_create(DIALOG_SAVE_PATTERN, _("Save current pattern..."), gui_settings.savepat_path, save_pat, -1, FALSE, TRUE, FALSE, xp_formats);

	mainvbox0 = gui_get_widget("mainvbox0");
	if(!mainvbox0)
		return 0;

    mainvbox = gtk_vbox_new(FALSE, 4);
    gtk_container_border_width(GTK_CONTAINER(mainvbox), 4);
    gtk_box_pack_start(GTK_BOX(mainvbox0), mainvbox, TRUE, TRUE, 0);
    gtk_widget_show(mainvbox);

    /* The upper part of the window */
    mainwindow_upper_hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(mainvbox), mainwindow_upper_hbox, FALSE, TRUE, 0);
    gtk_widget_show(mainwindow_upper_hbox);

    /* Program List */
    thing = playlist_new();
    gtk_box_pack_start(GTK_BOX(mainwindow_upper_hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    playlist = PLAYLIST(thing);
    g_signal_connect(playlist, "current_position_changed",
			G_CALLBACK(gui_playlist_position_changed), NULL);
    g_signal_connect(playlist, "restart_position_changed",
			G_CALLBACK(gui_playlist_restart_position_changed), NULL);
    g_signal_connect(playlist, "song_length_changed",
			G_CALLBACK(gui_playlist_song_length_changed), NULL);
    g_signal_connect(playlist, "entry_changed",
			G_CALLBACK(gui_playlist_entry_changed), NULL);
    g_signal_connect(playlist->ifbutton, "clicked",
			G_CALLBACK(gui_add_free_pattern), playlist);
    g_signal_connect(playlist->icbutton, "clicked",
			G_CALLBACK(gui_add_free_pattern_and_copy), playlist);
    
    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(mainwindow_upper_hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    /* Basic editing commands and properties */
    
    table = gtk_table_new(5, 2, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_box_pack_start(GTK_BOX(mainwindow_upper_hbox), table, FALSE, TRUE, 0);
    gtk_widget_show(table);

    style = gui_get_style();
    hbox = gtk_hbox_new(FALSE, 4);
    gtk_table_attach_defaults(GTK_TABLE(table), hbox, 0, 2, 0, 1);
    gtk_widget_show(hbox);

    pmw = gtk_image_new_from_file(PREFIX"/share/soundtracker/play.xpm");
    pbutton = thing = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(thing), pmw);
    g_signal_connect(thing, "clicked",
			G_CALLBACK(play_song), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE,0);
    gtk_widget_set_tooltip_text(thing, _("Play Song"));
    gtk_widget_show_all(thing);

    pmw = gtk_image_new_from_file(PREFIX"/share/soundtracker/play_cur.xpm");
    thing = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(thing), pmw);
    g_signal_connect(thing, "clicked",
			G_CALLBACK(play_pattern), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE,0);
    gtk_widget_set_tooltip_text(thing, _("Play Pattern"));
    gtk_widget_show_all(thing);

    pmw = gtk_image_new_from_file(PREFIX"/share/soundtracker/stop.xpm");
    thing = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(thing), pmw);
    g_signal_connect(thing, "clicked",
			G_CALLBACK(gui_play_stop), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE,0);		
    gtk_widget_set_tooltip_text(thing, _("Stop"));
    gtk_widget_show_all(thing);
    
    add_empty_hbox(hbox);

    thing = gtk_label_new(_("Pat"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    spin_editpat = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 255, 1.0, 10.0, 0.0)), 1, 0);
    extspinbutton_disable_size_hack(EXTSPINBUTTON(spin_editpat));
    gtk_widget_set_tooltip_text(spin_editpat, _("Edited pattern"));
    gtk_box_pack_start(GTK_BOX(hbox), spin_editpat, FALSE, TRUE, 0);
    gtk_widget_show(spin_editpat);
    g_signal_connect(spin_editpat, "value-changed",
		     G_CALLBACK(gui_editpat_changed), NULL);
		       
    pmw = gtk_image_new_from_file(PREFIX"/share/soundtracker/lock.xpm");
    toggle_lock_editpat = thing = gtk_toggle_button_new();
    gtk_container_add(GTK_CONTAINER(thing), pmw);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE,0);		
    gtk_widget_set_tooltip_text(thing, _("When enabled, browsing the playlist does not change the edited pattern."));
    gtk_widget_show_all(thing);

    thing = gui_subs_create_slider(&tempo_slider);
    gtk_table_attach_defaults(GTK_TABLE(table), thing, 0, 1, 3, 4);

    gtk_widget_show(thing);

    thing = gui_subs_create_slider(&bpm_slider);
    gtk_table_attach_defaults(GTK_TABLE(table), thing, 1, 2, 3, 4);
    gtk_widget_show(thing);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_table_attach_defaults(GTK_TABLE(table), hbox, 0, 2, 1, 2);
    gtk_widget_show(hbox);

    thing = gtk_label_new(_("Number of Channels:"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    add_empty_hbox(hbox);

    spin_numchans = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new(8, 2, 32, 2.0, 8.0, 0.0)), 1, 0);
    extspinbutton_disable_size_hack(EXTSPINBUTTON(spin_numchans));
    gtk_box_pack_start(GTK_BOX(hbox), spin_numchans, FALSE, TRUE, 0);
    g_signal_connect(spin_numchans, "value-changed",
		       G_CALLBACK(gui_numchans_changed), NULL);
    gtk_widget_show(spin_numchans);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_table_attach_defaults(GTK_TABLE(table), hbox, 0, 2, 2, 3);
    gtk_widget_show(hbox);

    thing = gtk_label_new(_("Pattern Length"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    add_empty_hbox(hbox);

    spin_patlen = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new(64, 1, 256, 1.0, 16.0, 0.0)), 1, 0);
    extspinbutton_disable_size_hack(EXTSPINBUTTON(spin_patlen));
    gtk_box_pack_start(GTK_BOX(hbox), spin_patlen, FALSE, TRUE, 0);
    g_signal_connect(spin_patlen, "value-changed",
		       G_CALLBACK(gui_patlen_changed), NULL);
    gtk_widget_show(spin_patlen);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_table_attach_defaults(GTK_TABLE(table), hbox, 0, 2, 4, 5);
    gtk_widget_show(hbox);
    
    vbox = gtk_vbox_new(FALSE, 0);
    alt[0] = gtk_image_new_from_file(PREFIX"/share/soundtracker/sharp.xpm");
    gtk_box_pack_start(GTK_BOX(vbox), alt[0], FALSE, FALSE, 0);
    
    alt[1] = gtk_image_new_from_file(PREFIX"/share/soundtracker/flat.xpm");
    gtk_widget_show(alt[gui_settings.sharp ? 0 : 1]);
    gtk_box_pack_start(GTK_BOX(vbox), alt[1], FALSE, FALSE, 0);
    gtk_widget_show(vbox);
    
    thing = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(thing), vbox);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(thing, _("Set preferred accidental type"));
    gtk_widget_show(thing);
    g_signal_connect(thing, "clicked",
		       G_CALLBACK(gui_accidentals_clicked), NULL);

    add_empty_hbox(hbox);
    thing = gtk_toggle_button_new_with_label(_("Measure"));
    gtk_widget_set_tooltip_text(thing, _("Enable row highlighting"));
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing),gui_settings.highlight_rows);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    g_signal_connect(thing, "toggled",
		       G_CALLBACK(gui_highlight_rows_toggled), NULL);
    gtk_widget_show(thing);

	selected = -1;
	ls = gtk_list_store_new(1, G_TYPE_STRING);

	for(i = 0; measure_msr[i].title != NULL; i++) {
		gtk_list_store_append(ls, &iter);
		gtk_list_store_set(ls, &iter, 0, measure_msr[i].title, -1);
		if ((measure_msr[i].major == gui_settings.highlight_rows_n) &&
		    (measure_msr[i].minor == gui_settings.highlight_rows_minor_n))
			selected = i;
	}
	if (selected == -1) selected = i + 1;
	gtk_list_store_append(ls, &iter); /* separator */
	gtk_list_store_set(ls, &iter, 0, "", -1);
	gtk_list_store_append(ls, &iter);
	gtk_list_store_set(ls, &iter, 0, _("Other..."), -1);

	thing = gui_combo_new(ls);
	gtk_combo_box_set_row_separator_func(GTK_COMBO_BOX(thing), is_sep, (gpointer)i, NULL);
	gtk_combo_box_set_active(GTK_COMBO_BOX(thing), selected);

	gtk_widget_set_tooltip_text(thing, _("Row highlighting configuration"));
	gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
	gtk_widget_show(thing);

	g_signal_connect(thing, "changed",
	                 G_CALLBACK(measure_changed), (gpointer)i);
	g_signal_connect(thing, "notify::popup-shown",
	                 G_CALLBACK(popwin_hide), (gpointer)i);
    
    add_empty_hbox(hbox);
    
    vbox = gtk_vbox_new(FALSE, 0);
    arrow[0] = gtk_image_new_from_file(PREFIX"/share/soundtracker/downarrow.xpm");
    gtk_box_pack_start(GTK_BOX(vbox), arrow[0], FALSE, FALSE, 0);
    
    arrow[1] = gtk_image_new_from_file(PREFIX"/share/soundtracker/rightarrow.xpm");
    gtk_box_pack_start(GTK_BOX(vbox), arrow[1], FALSE, FALSE, 0);
    gtk_widget_show(arrow[gui_settings.advance_cursor_in_fx_columns ? 1 : 0]);
    gtk_widget_show(vbox);
    
    thing = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(thing), vbox);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(thing, _("Change effect column editing direction"));
    gtk_widget_show(thing);
    g_signal_connect(thing, "clicked",
		       G_CALLBACK(gui_direction_clicked), NULL);

    /* Scopes Group or Instrument / Sample Listing */

    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(mainwindow_upper_hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

#ifndef NO_GDK_PIXBUF
    scopegroup = SCOPE_GROUP(scope_group_new(gdk_pixbuf_new_from_file(PREFIX"/share/soundtracker/muted.png", &error)));
#else
    scopegroup = SCOPE_GROUP(scope_group_new());
#endif
    gtk_widget_show(GTK_WIDGET(scopegroup));
    gtk_box_pack_start(GTK_BOX(mainwindow_upper_hbox), GTK_WIDGET(scopegroup), TRUE, TRUE, 0);

    /* Amplification and Pitchbender */

    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(mainwindow_upper_hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    
    hbox = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(mainwindow_upper_hbox), hbox, FALSE, TRUE, 0);

    adj_amplification = GTK_ADJUSTMENT(gtk_adjustment_new(7.0, 0, 8.0, 0.1, 0.1, 0.1));
    thing = gtk_vscale_new(adj_amplification);
    gtk_widget_set_tooltip_text(thing, _("Global amplification"));
    gtk_scale_set_draw_value(GTK_SCALE(thing), FALSE);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(hbox), thing, TRUE, TRUE, 0);
    g_signal_connect(adj_amplification, "value_changed",
			G_CALLBACK(gui_adj_amplification_changed), NULL);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
    gtk_box_pack_start(GTK_BOX(hbox), frame, FALSE, TRUE, 0);
    gtk_widget_show(frame);

    gui_clipping_led = thing = gtk_drawing_area_new();
    gtk_drawing_area_size(GTK_DRAWING_AREA(thing), 15, 15);
    gtk_widget_set_events(thing, GDK_EXPOSURE_MASK);
    gtk_container_add (GTK_CONTAINER(frame), thing);
    colormap = gtk_widget_get_colormap(thing);
    gui_clipping_led_on.red = 0xffff;
    gui_clipping_led_on.green = 0;
    gui_clipping_led_on.blue = 0;
    gui_clipping_led_on.pixel = 0;
    gui_clipping_led_off.red = 0;
    gui_clipping_led_off.green = 0;
    gui_clipping_led_off.blue = 0;
    gui_clipping_led_off.pixel = 0;
    gdk_color_alloc(colormap, &gui_clipping_led_on);
    gdk_color_alloc(colormap, &gui_clipping_led_off);
    g_signal_connect(thing, "event", G_CALLBACK(gui_clipping_led_event), thing);
    gtk_widget_show (thing);

    hbox = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(mainwindow_upper_hbox), hbox, FALSE, TRUE, 0);

    adj_pitchbend = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, -20.0, +20.0, 1, 1, 1));
    thing = gtk_vscale_new(adj_pitchbend);
    gtk_widget_set_tooltip_text(thing, _("Pitchbend"));
    gtk_scale_set_draw_value(GTK_SCALE(thing), FALSE);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(hbox), thing, TRUE, TRUE, 0);
    g_signal_connect(adj_pitchbend, "value_changed",
			G_CALLBACK(gui_adj_pitchbend_changed), NULL);

    thing = gtk_button_new_with_label("R");
    gtk_widget_set_tooltip_text(thing, _("Reset pitchbend to its normal value"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "clicked",
			G_CALLBACK(gui_reset_pitch_bender), NULL);

    /* Instrument, sample, editing status */

    mainwindow_second_hbox = hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(mainvbox), hbox, FALSE, TRUE, 0);
    gtk_widget_show(hbox);

    editing_toggle = thing = gtk_check_button_new_with_label(_("Editing"));
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(thing), 0);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gtk_signal_connect (GTK_OBJECT(thing), "toggled",
			GTK_SIGNAL_FUNC(editing_toggled), NULL);

    thing = gtk_label_new(_("Octave"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    spin_octave = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new(3.0, 0.0, 6.0, 1.0, 1.0, 0.0)), 0, 0);
    extspinbutton_disable_size_hack(EXTSPINBUTTON(spin_octave));
    gtk_box_pack_start(GTK_BOX(hbox), spin_octave, FALSE, TRUE, 0);
    gtk_widget_show(spin_octave);

    thing = gtk_label_new(_("Jump"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    spin_jump = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new(1.0, 0.0, 16.0, 1.0, 1.0, 0.0)), 0, 0);
    extspinbutton_disable_size_hack(EXTSPINBUTTON(spin_jump));
    gtk_box_pack_start(GTK_BOX(hbox), spin_jump, FALSE, TRUE, 0);
    gtk_widget_show(spin_jump);

    thing = gtk_label_new(_("Instr"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    curins_spin = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new(1.0, 1.0, 128.0, 1.0, 16.0, 0.0)), 0, 0);
    extspinbutton_disable_size_hack(EXTSPINBUTTON(curins_spin));
    gtk_box_pack_start(GTK_BOX(hbox), curins_spin, FALSE, TRUE, 0);
    gtk_widget_show(curins_spin);
    g_signal_connect(curins_spin, "value-changed",
			G_CALLBACK(current_instrument_changed), NULL);

    gui_get_text_entry(22, current_instrument_name_changed, &gui_curins_name);
    gtk_box_pack_start(GTK_BOX(hbox), gui_curins_name, TRUE, TRUE, 0);
    gtk_widget_show(gui_curins_name);
    gtk_widget_set_size_request(gui_curins_name, 100, -1);

    thing = gtk_label_new(_("Sample"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    cursmpl_spin = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 127.0, 1.0, 4.0, 0.0)), 0, 0);
    extspinbutton_disable_size_hack(EXTSPINBUTTON(cursmpl_spin));
    gtk_box_pack_start(GTK_BOX(hbox), cursmpl_spin, FALSE, TRUE, 0);
    gtk_widget_show(cursmpl_spin);
    g_signal_connect(cursmpl_spin, "value-changed",
			G_CALLBACK(current_sample_changed), NULL);

    gui_get_text_entry(22, current_sample_name_changed, &gui_cursmpl_name);
    gtk_box_pack_start(GTK_BOX(hbox), gui_cursmpl_name, TRUE, TRUE, 0);
    gtk_widget_show(gui_cursmpl_name);
    gtk_widget_set_size_request(gui_cursmpl_name, 100, -1);

    /* The notebook */

    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(mainvbox), notebook, TRUE, TRUE, 0);
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
    gtk_widget_show(notebook);
    gtk_container_border_width(GTK_CONTAINER(notebook), 0);
    g_signal_connect(notebook, "switch_page",
		       G_CALLBACK(notebook_page_switched), NULL);

    fileops_page_create(GTK_NOTEBOOK(notebook));
    tracker_page_create(GTK_NOTEBOOK(notebook));
    instrument_page_create(GTK_NOTEBOOK(notebook));
    sample_editor_page_create(GTK_NOTEBOOK(notebook));
    modinfo_page_create(GTK_NOTEBOOK(notebook));

    // Activate tracker page
    gtk_notebook_set_page(GTK_NOTEBOOK(notebook),
			  1);
    notebook_current_page = 1;

    /* Status Bar */

#define WELCOME_MESSAGE _("Welcome to SoundTracker!")

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_container_border_width(GTK_CONTAINER(hbox), 2);
    gtk_box_pack_start(GTK_BOX(mainvbox), hbox, FALSE, TRUE, 0);
    gtk_widget_show(hbox);

    thing = gtk_frame_new (NULL);
    gtk_widget_show (thing);
    gtk_box_pack_start (GTK_BOX (hbox), thing, TRUE, TRUE, 0);
    gtk_frame_set_shadow_type (GTK_FRAME (thing), GTK_SHADOW_IN);

    status_bar = gtk_label_new(WELCOME_MESSAGE);
    gtk_misc_set_alignment(GTK_MISC(status_bar), 0.0, 0.5);
    gtk_misc_set_padding(GTK_MISC(status_bar), 4, 0);
    gtk_widget_show (status_bar);
    gtk_container_add (GTK_CONTAINER (thing), status_bar);

    thing = gtk_frame_new (NULL);
    gtk_widget_show (thing);
    gtk_box_pack_start (GTK_BOX (hbox), thing, FALSE, FALSE, 0);
    gtk_frame_set_shadow_type (GTK_FRAME (thing), GTK_SHADOW_IN);

    st_clock = clock_new ();
    gtk_widget_show (st_clock);
    gtk_container_add (GTK_CONTAINER (thing), st_clock);
    gtk_widget_set_size_request (st_clock, 48, 20);
    clock_set_format (CLOCK (st_clock), _("%M:%S"));
    clock_set_seconds(CLOCK (st_clock), 0);

    /* capture all key presses */
    gtk_widget_add_events(GTK_WIDGET(mainwindow), GDK_KEY_RELEASE_MASK);
    g_signal_connect(mainwindow, "key-press-event", G_CALLBACK(keyevent), (gpointer)1);
    g_signal_connect(mainwindow, "key_release_event", G_CALLBACK(keyevent), (gpointer)0);

	/* All widgets and main data structures are created, now it's possible to connect signals */
	gtk_builder_connect_signals(builder, tracker);
	/* Connect handlers to "activate" signals to individual widgets got by names if the case is not standard (data is not 'tracker') */
	for(i = 0; cb[i].widget_name; i++) {
		GtkWidget *w = gui_get_widget(cb[i].widget_name);

		if(w)
			g_signal_connect(w, "activate", G_CALLBACK(cb[i].fn), cb[i].data);
	}

    if(argc == 2) {
	gui_load_xm(argv[1]);
    } else {
	gui_new_xm();
    }

    menubar_init_prefs();
    fileops_page_post_create();

    gtk_widget_show (mainwindow);

    if(!keys_init()) {
	return 0;
    }

    if(gui_splash_window) {
	if(!gui_settings.gui_disable_splash && (
#ifndef NO_GDK_PIXBUF
           gui_splash_logo ||
#endif
           tips_dialog_show_tips)) {
	    gdk_window_raise(gui_splash_window->window);
//	    gtk_window_set_transient_for(GTK_WINDOW(gui_splash_window), GTK_WINDOW(mainwindow));
// (doesn't do anything on WindowMaker)
	    gui_splash_set_label(_("Ready."), TRUE);
//	    gdk_window_hide(gui_splash_window->window);
//	    gdk_window_show(gui_splash_window->window);
#ifndef NO_GDK_PIXBUF
	    if(gui_splash_logo) {
		gtk_widget_add_events(gui_splash_logo_area,
				      GDK_BUTTON_PRESS_MASK);
		g_signal_connect(gui_splash_logo_area, "button_press_event",
				    G_CALLBACK(gui_splash_close),
				    NULL);
	    }
#endif
	    gtk_widget_set_sensitive(gui_splash_close_button, TRUE);
	} else {
	    gui_splash_close();
	}
    }

    gtk_widget_grab_focus(pbutton);
    return 1;
}

GtkWidget*
gui_get_widget (const gchar *name)
{
	GtkWidget *w = GTK_WIDGET(gtk_builder_get_object(builder, name));

	if(!w)
		fprintf(stderr, _("GUI creation error: Widget '%s' is not found in %s file.\n"), name, XML_FILE);

	return w;
}

static gboolean
call_menu(GtkWidget *widget, GdkEventButton *event, GtkMenu *menu)
{
	if(event->button == 3) {
		gtk_menu_popup(menu, NULL, NULL, NULL, NULL, event->button, event->time);
		return TRUE;
	}

	return FALSE;
}

void
gui_popup_menu_attach (GtkWidget *menu, GtkWidget *widget, gpointer *user_data)
{
	gtk_menu_attach_to_widget(GTK_MENU(menu), widget, NULL);
	g_signal_connect(menu, "deactivate", G_CALLBACK(gtk_widget_hide), NULL);
	g_signal_connect(widget, "button-press-event", G_CALLBACK(call_menu), menu);
}
