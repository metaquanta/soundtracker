
/*
 * The Real SoundTracker - File (output) driver.
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

#include <config.h>

#if USE_SNDFILE || AUDIOFILE_VERSION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <glib/gi18n.h>

#include <sys/types.h>
#include <unistd.h>

#if USE_SNDFILE
#include <sndfile.h>
#else
#include <audiofile.h>
#endif

#include "driver-inout.h"
#include "mixer.h"
#include "errors.h"
#include "gui-subs.h"
#include "preferences.h"

typedef struct file_driver {
    gchar *filename; /* must be the first entry. is altered by audio.c (hack, hack) */

#if USE_SNDFILE
    SNDFILE *outfile;
    SF_INFO sfinfo;
#else
    AFfilehandle outfile;
#endif

    int pipe[2];
    gpointer polltag;
    int firstpoll;

    gint16 *sndbuf;
    int sndbuf_size;
    double playtime;

    int p_resolution;
    int p_channels;
    int p_mixfreq;

    GtkWidget *configwidget, *prefs_channels_w[2], *prefs_mixfreq;
    GtkTreeModel *model;
} file_driver;

static const int mixfreqs[] = { 8000, 11025, 16000, 22050, 32000, 44100, 48000, 64000, 88200, 96000 };
#define NUM_FREQS sizeof(mixfreqs) / sizeof(mixfreqs[0])

static void
file_poll_ready_playing (gpointer data,
			 gint source,
			 GdkInputCondition condition)
{
    file_driver * const d = data;

    if(!d->firstpoll) {
#if USE_SNDFILE
	sf_writef_short (d->outfile, d->sndbuf, d->sndbuf_size >> d->p_channels);
#else
	afWriteFrames(d->outfile, AF_DEFAULT_TRACK, d->sndbuf, d->sndbuf_size >> d->p_channels);
#endif
	d->playtime += (double)((d->sndbuf_size) >> d->p_channels) / d->p_mixfreq;
    }

    d->firstpoll = FALSE;
#ifdef WORDS_BIGENDIAN
    audio_mix(d->sndbuf, d->sndbuf_size >> d->p_channels, d->p_mixfreq, ST_MIXER_FORMAT_S16_BE | (d->p_channels == 2 ? ST_MIXER_FORMAT_STEREO : 0));
#else
    audio_mix(d->sndbuf, d->sndbuf_size >> d->p_channels, d->p_mixfreq, ST_MIXER_FORMAT_S16_LE | (d->p_channels == 2 ? ST_MIXER_FORMAT_STEREO : 0));
#endif

}

static void
prefs_channels_changed (GtkWidget *w, file_driver *d)
{
    gint curr;

    if((curr = find_current_toggle(d->prefs_channels_w,
				   sizeof(d->prefs_channels_w) / sizeof(d->prefs_channels_w[0]))) < 0)
	return;
    d->p_channels = ++curr;
}

static void
prefs_mixfreq_changed (GtkWidget *w, file_driver *d)
{
    GtkTreeIter iter;

    if(!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(d->prefs_mixfreq), &iter))
	return;
    gtk_tree_model_get(d->model, &iter, 0, &d->p_mixfreq, -1);
}

static void
prefs_init_from_structure (file_driver *d)
{
    gui_set_radio_active(d->prefs_channels_w, d->p_channels - 1);
    gui_set_active_combo_item(d->prefs_mixfreq, d->model, d->p_mixfreq);
}

static void
file_make_config_widgets (file_driver *d)
{
    GtkWidget *thing, *mainbox, *box2;
    GtkListStore *ls;
    GtkCellRenderer *cell;
    GtkTreeIter iter;
    guint i;

    static const char *channelslabels[] = { N_("Mono"), N_("Stereo"), NULL };

    d->configwidget = mainbox = gtk_vbox_new(FALSE, 2);

    box2 = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(mainbox), box2, FALSE, TRUE, 0);

    thing = gtk_label_new(_("Channels:"));
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    add_empty_hbox(box2);
    make_radio_group_full(channelslabels, box2, d->prefs_channels_w, FALSE, TRUE, (void(*)())prefs_channels_changed, d);

    box2 = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(mainbox), box2, FALSE, TRUE, 0);

    thing = gtk_label_new(_("Frequency [Hz]:"));
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    ls = gtk_list_store_new(1, G_TYPE_UINT);
    d->model = GTK_TREE_MODEL(ls);
    thing = d->prefs_mixfreq = gtk_combo_box_new_with_model(GTK_TREE_MODEL(ls));
    g_object_unref(ls);
    for(i = 0; i < NUM_FREQS; i++) {
	gtk_list_store_append(GTK_LIST_STORE(d->model), &iter);
	gtk_list_store_set(GTK_LIST_STORE(d->model), &iter, 0, mixfreqs[i], -1);
    }
    cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(thing), cell, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(thing), cell, "text", 0, NULL);
    gtk_box_pack_end(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "changed",
		     G_CALLBACK(prefs_mixfreq_changed), d);

    gtk_widget_show_all(mainbox);
}

static GtkWidget *
file_getwidget (void *dp)
{
    file_driver * const d = dp;

    prefs_init_from_structure(d);
    return d->configwidget;
}

static void *
file_new (void)
{
    file_driver *d = g_new(file_driver, 1);

    d->p_mixfreq = 44100;
    d->p_channels = 2;
    d->p_resolution = 16;
    d->sndbuf = NULL;
    d->polltag = NULL;

    file_make_config_widgets(d);

	if(pipe(d->pipe) == -1)
		perror("File output: pipe()");

    return d;
}

static void
file_destroy (void *dp)
{
    file_driver * const d = dp;

    close(d->pipe[0]);
    close(d->pipe[1]);

    gtk_widget_destroy(d->configwidget);

    g_free(dp);
}

static void
file_release (void *dp)
{
    file_driver * const d = dp;

    free(d->sndbuf);
    d->sndbuf = NULL;

    audio_poll_remove(d->polltag);
    d->polltag = NULL;

    if(d->outfile != NULL) {
#if USE_SNDFILE
	sf_close(d->outfile);
#else
	afCloseFile(d->outfile);
#endif
	d->outfile = NULL;
    }
}

static gboolean
file_open (void *dp)
{
    file_driver * const d = dp;

#if USE_SNDFILE
    d->sfinfo.channels = d->p_channels;
    d->sfinfo.samplerate = d->p_mixfreq;
    d->sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16 ;

    d->outfile = sf_open (d->filename, SFM_WRITE, &d->sfinfo);
#else
	AFfilesetup outfilesetup;

    outfilesetup = afNewFileSetup();
    afInitFileFormat(outfilesetup, AF_FILE_WAVE);
    afInitChannels(outfilesetup, AF_DEFAULT_TRACK, d->p_channels);
    afInitRate(outfilesetup, AF_DEFAULT_TRACK, d->p_mixfreq);
    afInitSampleFormat(outfilesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    d->outfile = afOpenFile(d->filename, "w", outfilesetup);
    afFreeFileSetup(outfilesetup);
#endif

    if(!d->outfile) {
	error_error(_("Can't open file for writing."));
	goto out;
    }

    /* In case we're running setuid root... */
	if(chown(d->filename, getuid(), getgid()) == -1)
		error_warning(_("Can't change file ownership."));

    d->sndbuf_size = 16384;
    d->sndbuf = malloc(d->sndbuf_size);
    if(!d->sndbuf) {
	error_error(_("Can't allocate mix buffer."));
	goto out;
    }

    d->polltag = audio_poll_add(d->pipe[1], GDK_INPUT_WRITE, file_poll_ready_playing, d);
    d->firstpoll = TRUE;
    d->playtime = 0.0;

    return TRUE;

  out:
    file_release(dp);
    return FALSE;
}

static double
file_get_play_time (void *dp)
{
    file_driver * const d = dp;

    return d->playtime;
}

static gboolean
file_loadsettings (void *dp,
		   const gchar *f)
{
    file_driver * const d = dp;

    d->p_channels = prefs_get_int(f, "file-channels", d->p_channels);
    d->p_mixfreq = prefs_get_int(f, "file-mixfreq", d->p_mixfreq);
    prefs_init_from_structure(d);

    return TRUE;
}

static gboolean
file_savesettings (void *dp,
		   const gchar *f)
{
    file_driver * const d = dp;

    prefs_put_int(f, "file-channels", d->p_channels);
    prefs_put_int(f, "file-mixfreq", d->p_mixfreq);

    return TRUE;
}

st_driver driver_out_file = {
    "WAV Rendering Output using libfile",

    file_new,
    file_destroy,

    file_open,
    file_release,

    file_getwidget,
    file_loadsettings,
    file_savesettings,

    NULL,
    NULL,

    file_get_play_time,
    NULL,
};

#endif