
/*
 * The Real SoundTracker - GUI (menu bar)
 *
 * Copyright (C) 1999-2003 Michael Krause
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
#include "X11/Xlib.h"
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>

#include "menubar.h"
#include "gui.h"
#include "gui-subs.h"
#include "main.h"
#include "st-subs.h"
#include "keys.h"
#include "module-info.h"
#include "preferences.h"
#include "scope-group.h"
#include "track-editor.h"
#include "audioconfig.h"
#include "gui-settings.h"
#include "tips-dialog.h"
#include "transposition.h"
#include "cheat-sheet.h"
#include "file-operations.h"
#include "instrument-editor.h"
#include "tracker-settings.h"
#include "midi-settings.h"
#include "sample-editor.h"

static GtkWidget *mark_mode;

static gboolean mark_mode_toggle_ignore = FALSE;

extern ScopeGroup *scopegroup;


void
about_dialog (void)
{
    const gchar *authors[] = {"Michael Krause <rawstyle@soundtracker.org>",
                              "Yury Alyaev <mutab0r@rambler.ru>", NULL};

    gtk_show_about_dialog(GTK_WINDOW(mainwindow), "name", "SoundTracker", "version", VERSION,
			  "authors", authors, "license", "GPL", "copyright",
			  "Copyright (C) 1998-2008 Michael Krause\n<rawstyle@soundtracker.org>\n"
			  "Copyright (C) 2008-2013 Yury Alyaev\n<mutab0r@rambler.ru>"
			  "\n\nIncludes OpenCP player from Niklas Beisert and Tammo Hinrichs.",
			  NULL);
}

static void
menubar_clear (gboolean all)
{
	if(all) {
		gui_free_xm();
		gui_new_xm();
		xm_set_modified(0);
	} else {
		gui_play_stop();
		st_clean_song(xm);
		gui_init_xm(1, TRUE);
		xm_set_modified(0);
	}
}

void
menubar_clear_clicked (gpointer b)
{
	if(xm_get_modified()) {
		if(gui_ok_cancel_modal((mainwindow),
		                       _("Are you sure you want to do this?\nAll changes will be lost!")))
			menubar_clear(GPOINTER_TO_INT(b));
	} else {
		menubar_clear(GPOINTER_TO_INT(b));
	}
}

void
menubar_backing_store_toggled (GtkCheckMenuItem *widget)
{
    gui_settings.gui_use_backing_store = gtk_check_menu_item_get_active(widget);
    tracker_set_backing_store(tracker, gui_settings.gui_use_backing_store);
}

void
menubar_scopes_toggled (GtkCheckMenuItem *widget)
{
    gui_settings.gui_display_scopes = gtk_check_menu_item_get_active(widget);
    scope_group_enable_scopes(scopegroup, gui_settings.gui_display_scopes);
}

void
menubar_splash_toggled (GtkCheckMenuItem *widget)
{
    gui_settings.gui_disable_splash = gtk_check_menu_item_get_active(widget);
}

void
menubar_mark_mode_toggled (GtkCheckMenuItem *widget)
{
	if(!mark_mode_toggle_ignore) {
		gboolean state = gtk_check_menu_item_get_active(widget);
		tracker_mark_selection(tracker, state);
	}
}

void
menubar_save_settings_on_exit_toggled (GtkCheckMenuItem *widget)
{
    gui_settings.save_settings_on_exit = gtk_check_menu_item_get_active(widget);
}

void
menubar_save_settings_now (void)
{
    gui_settings_save_config();
    keys_save_config();
    audioconfig_save_config();
    trackersettings_write_settings();
#if defined(DRIVER_ALSA_09x)
    midi_save_config();
#endif
    prefs_save();
}

void
menubar_handle_cutcopypaste (gpointer a)
{
	static const gchar *signals[] = {"cut-clipboard", "copy-clipboard", "paste-clipboard"};

    Tracker *t = tracker;

    STInstrument *curins = &xm->instruments[gui_get_current_instrument() - 1];
    GtkWidget *focus_widget = GTK_WINDOW(mainwindow)->focus_widget;
    gint i = GPOINTER_TO_INT(a);

	if(GTK_IS_ENTRY(focus_widget)) {
		g_signal_emit_by_name(focus_widget, signals[i], NULL);
		return;
	}

    switch(i){
    case 0:		//Cut
	switch(notebook_current_page) {
	case NOTEBOOK_PAGE_TRACKER:
	    track_editor_cut_selection(NULL, t);
	    break;
	case NOTEBOOK_PAGE_INSTRUMENT_EDITOR:
	case NOTEBOOK_PAGE_MODULE_INFO:
	    instrument_editor_cut_instrument(curins);
	    xm_set_modified(1);
	    instrument_editor_update(TRUE);
	    sample_editor_update();
	    break;
	case NOTEBOOK_PAGE_SAMPLE_EDITOR:
	    sample_editor_copy_cut_common(TRUE, TRUE);
	    xm_set_modified(1);
	    break;
	}
	break;
    case 1:		//Copy
	switch(notebook_current_page){
	case NOTEBOOK_PAGE_TRACKER:
	    track_editor_copy_selection(NULL, t);
	    break;
	case NOTEBOOK_PAGE_INSTRUMENT_EDITOR:
	case NOTEBOOK_PAGE_MODULE_INFO:
	    instrument_editor_copy_instrument(curins);
	    break;
	case NOTEBOOK_PAGE_SAMPLE_EDITOR:
	    sample_editor_copy_cut_common(TRUE, FALSE);
	    break;
	}
	break;
    case 2:		//Paste
	switch(notebook_current_page){
	case NOTEBOOK_PAGE_TRACKER:
	    track_editor_paste_selection(NULL, t);
	    break;
	case NOTEBOOK_PAGE_INSTRUMENT_EDITOR:
	case NOTEBOOK_PAGE_MODULE_INFO:
	    instrument_editor_paste_instrument(curins);
	    xm_set_modified(1);
	    instrument_editor_update(TRUE);
	    sample_editor_update();
	    break;
	case NOTEBOOK_PAGE_SAMPLE_EDITOR:
	    sample_editor_paste_clicked();
	    xm_set_modified(1);
	    break;
	}
	break;
    }
}

void
menubar_handle_edit_menu (gpointer a)
{
    Tracker *t = tracker;

    switch(GPOINTER_TO_INT(a)) {
    case 0:
        track_editor_cmd_mvalue(t, TRUE);   /* increment CMD value */
        break;
    case 1:
        track_editor_cmd_mvalue(t, FALSE);  /* decrement CMD value */
        break;
    case 2:
	transposition_transpose_selection(t, +1);
	break;
    case 3:
	transposition_transpose_selection(t, -1);
	break;
    case 4:
	transposition_transpose_selection(t, +12);
	break;
    case 5:
	transposition_transpose_selection(t, -12);
	break;
    default:
        break;
    }
}

void
menubar_settings_tracker_next_font (void)
{
    trackersettings_cycle_font_forward(TRACKERSETTINGS(trackersettings));
}

void
menubar_settings_tracker_prev_font (void)
{
    trackersettings_cycle_font_backward(TRACKERSETTINGS(trackersettings));
}

void
menubar_toggle_perm_wrapper (gpointer all)
{
    track_editor_toggle_permanentness(tracker, GPOINTER_TO_INT(all));
}

void
menubar_init_prefs ()
{
	GtkWidget *record_keyreleases = gui_get_widget("edit_record_keyreleases");
	GtkWidget *display_scopes = gui_get_widget("settings_display_scopes");
	GtkWidget *backing_store = gui_get_widget("settings_tracker_flicker_free");
#if ! defined(DRIVER_ALSA_09x)
	GtkWidget *settings_midi = gui_get_widget("settings_midi");
#endif
#if USE_SNDFILE == 0 && !defined (AUDIOFILE_VERSION)
	GtkWidget *savewav = gui_get_widget("file_save_wav");
#endif
#if !defined(USE_GTKHTML)
	GtkWidget *help_cheat = gui_get_widget("help_cheat");
#endif
	GtkWidget *disable_splash = gui_get_widget("settings_disable_splash");
	GtkWidget *save_onexit = gui_get_widget("settings_save_on_exit");

	mark_mode = gui_get_widget("edit_selection_mark_mode");

    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(display_scopes), gui_settings.gui_display_scopes);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(backing_store), gui_settings.gui_use_backing_store);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(disable_splash), gui_settings.gui_disable_splash);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(save_onexit), gui_settings.save_settings_on_exit);

    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(record_keyreleases), TRUE); // Record aftertouch

#if USE_SNDFILE == 0 && !defined (AUDIOFILE_VERSION)
    gtk_widget_set_sensitive(savewav, FALSE);
#endif
#if ! defined(DRIVER_ALSA_09x)
    gtk_widget_set_sensitive(settings_midi, FALSE);
#endif
#if !defined(USE_GTKHTML)
    gtk_widget_set_sensitive(help_cheat, FALSE);
#endif
}

void
menubar_block_mode_set(gboolean state)
{
    mark_mode_toggle_ignore = TRUE;
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mark_mode), state);
    mark_mode_toggle_ignore = FALSE;
}
