
/*
 * The Real SoundTracker - sample editor
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
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <math.h>

#if USE_SNDFILE
#include <sndfile.h>
#elif AUDIOFILE_VERSION
#include <audiofile.h>
#endif

#include <glib.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include "sample-editor.h"
#include "xm.h"
#include "st-subs.h"
#include "gui.h"
#include "gui-subs.h"
#include "instrument-editor.h"
#include "sample-display.h"
#include "endian-conv.h"
#include "keys.h"
#include "track-editor.h"
#include "errors.h"
#include "time-buffer.h"
#include "audio.h"
#include "mixer.h"
#include "module-info.h"
#include "file-operations.h"
#include "gui-settings.h"
#include "clock.h"

// == GUI variables

static STSample *current_sample = NULL;

static GtkWidget *spin_volume, *spin_panning, *spin_finetune, *spin_relnote;
static GtkWidget *savebutton, *savebutton_rgn;
static SampleDisplay *sampledisplay;
static GtkWidget *sample_editor_hscrollbar;
static GtkWidget *loopradio[3], *resolution_radio[2], *load_radio[4], *record_radio[4];
static GtkWidget *spin_loopstart, *spin_loopend;

static struct SampleEditor { // simplifies future encapsulation of this into a Gtk+ widget
    GtkWidget *label_selection;
    GtkWidget *label_length;
    gchar label_selection_new_text[64];
    gchar label_length_new_text[64];
    guint idle_handler;
    GtkWidget *vertical_boxes[3];
} sample_editor;
static struct SampleEditor * const se = &sample_editor;

// = Volume ramping dialog

static GtkWidget *sample_editor_volramp_spin_w[2];
static int sample_editor_volramp_last_values[2] = { 100, 100 };

// = Trim dialog


// = Load sample dialog

enum {
    MODE_MONO = 0,
    MODE_STEREO_MIX,
    MODE_STEREO_LEFT,
    MODE_STEREO_RIGHT,
    MODE_STEREO_2
};

#if USE_SNDFILE || AUDIOFILE_VERSION
static GtkWidget *wavload_dialog;

struct wl {
	gboolean through_library;
	const char *samplename;

#if USE_SNDFILE
	SNDFILE *file;
	SF_INFO wavinfo;
	sf_count_t frameCount;
#else
	AFfilehandle file;
	AFframecount frameCount;
#endif

	int sampleWidth, channelCount, endianness, unsignedwords;
	long rate;
};

static GtkWidget *wavload_raw_resolution_w[2];
static GtkWidget *wavload_raw_channels_w[2];
static GtkWidget *wavload_raw_signed_w[2];
static GtkWidget *wavload_raw_endian_w[2];
#endif

// = Sampler variables

static SampleDisplay *monitorscope;
static GtkWidget *clearbutton, *okbutton, *sclock;

st_driver *sampling_driver = NULL;
void *sampling_driver_object = NULL;

static GtkWidget *samplingwindow = NULL;

struct recordbuf {
    struct recordbuf *next;
    guint length;
    void *data;
};

static struct recordbuf *recordbufs, *current;
static guint recordedlen, rate, toggled_id;
static gboolean sampling, monitoring, has_data;
static STMixerFormat format;

// = Editing operations variables

static void *copybuffer = NULL;
static int copybufferlen;
static STSample copybuffer_sampleinfo;

// = Realtime stuff

static int update_freq = 50;
static int gtktimer = -1;

static void sample_editor_ok_clicked(void);

static void sample_editor_spin_volume_changed(GtkSpinButton *spin);
static void sample_editor_spin_panning_changed(GtkSpinButton *spin);
static void sample_editor_spin_finetune_changed(GtkSpinButton *spin);
static void sample_editor_spin_relnote_changed(GtkSpinButton *spin);
static void sample_editor_selection_to_loop_clicked (void);
static void sample_editor_loop_changed(void);
static void sample_editor_display_loop_changed(SampleDisplay *, int start, int end);
static void sample_editor_display_selection_changed(SampleDisplay *, int start, int end);
static void sample_editor_display_window_changed(SampleDisplay *, int start, int end);
static void sample_editor_select_none_clicked(void);
static void sample_editor_select_all_clicked(void);
static void sample_editor_clear_clicked(void);
static void sample_editor_crop_clicked(void);
static void sample_editor_show_all_clicked(void);
static void sample_editor_zoom_in_clicked(void);
static void sample_editor_zoom_out_clicked(void);
static void sample_editor_loopradio_changed(void);
static void sample_editor_resolution_changed(void);
static void sample_editor_monitor_clicked(void);
static void sample_editor_cut_clicked(void);
static void sample_editor_remove_clicked(void);
static void sample_editor_copy_clicked(void);
void sample_editor_paste_clicked(void);
static void sample_editor_zoom_to_selection_clicked(void);

#if USE_SNDFILE || AUDIOFILE_VERSION
static void sample_editor_load_wav(const gchar *name);
static void sample_editor_save_wav(const gchar *name);
static void sample_editor_save_region_wav(const gchar *name);
#endif

static void sample_editor_open_volume_ramp_dialog(void);
static void sample_editor_perform_ramp(GtkWidget *w, gint response, gpointer data);

static void sample_editor_reverse_clicked(void);

static void sample_editor_trim_dialog(void);
static void sample_editor_trim(gboolean beg, gboolean end, gfloat threshold);
static void sample_editor_crop(void);
static void sample_editor_delete(STSample *sample,int start, int end);
    
static void
sample_editor_lock_sample (void)
{
    g_mutex_lock(current_sample->sample.lock);
}

static void
sample_editor_unlock_sample (void)
{
    if(gui_playing_mode) {
	mixer->updatesample(&current_sample->sample);
    }
    g_mutex_unlock(current_sample->sample.lock);
}

void
sample_editor_page_create (GtkNotebook *nb)
{
    GtkWidget *box, *thing, *hbox, *vbox, *vbox2, *frame, *box2;
    static const char *looplabels[] = {
	N_("No loop"),
	N_("Amiga"),
	N_("PingPong"),
	NULL
    };
    static const char *resolutionlabels[] = {
	N_("8 bits"),
	N_("16 bits"),
	NULL
    };

#if USE_SNDFILE || AUDIOFILE_VERSION
	static const gchar *aiff_f[] = {N_("Apple/SGI audio (*aif, *.aiff, *.aifc)"), "*.[aA][iI][fF]", "*.[aA][iI][fF][fFcC]", NULL};
	static const gchar *au_f[] = {N_("SUN/NeXT audio (*.au, *.snd)"), "*.[aA][uU]", "*.[sS][nN][dD]", NULL};
	static const gchar *avr_f[] = {N_("Audio Visual Research files (*.avr)"), "*.[aA][vV][rR]", NULL};
	static const gchar *caf_f[] = {N_("Apple Core Audio files (*.caf)"), "*.[cC][aA][fF]", NULL};
	static const gchar *iff_f[] = {N_("Amiga IFF/SV8/SV16 (*.iff)"), "*.[iI][fF][fF]", NULL};
	static const gchar *sf_f[] = {N_("Berkeley/IRCAM/CARL audio (*.sf)"), "*.[sS][fF]", NULL};
	static const gchar *voc_f[] = {N_("Creative Labs audio (*.voc)"), "*.[vV][oO][cC]", NULL};
	static const gchar *wavex_f[] = {N_("Microsoft RIFF/NIST Sphere (*.wav)"), "*.[wW][aA][vV]", NULL};
	static const gchar *flac_f[] = {N_("FLAC lossless audio (*.flac)"), "*.flac", NULL};
	static const gchar *wve_f[] = {N_("Psion audio (*.wve)"), "*.wve", NULL};
	static const gchar *ogg_f[] = {N_("OGG compressed audio (*.ogg, *.vorbis)"), "*.ogg", "*.vorbis", NULL};
	static const gchar *rf64_f[] = {N_("RIFF 64 files (*.rf64)"), "*.rf64", NULL};

	static const gchar *wav_f[] = {N_("Microsoft RIFF (*.wav)"), "*.[wW][aA][vV]", NULL};
	static const gchar **out_f[] = {wav_f, NULL};
#endif

#if USE_SNDFILE
	static const gchar *htk_f[] = {N_("HMM Tool KIt files (*.htk)"), "*.[hH][tT][kK]", NULL};
	static const gchar *mat_f[] = {N_("GNU Octave/Matlab files (*.mat)"), "*.[mM][aA][tT]", NULL};
	static const gchar *paf_f[] = {N_("Ensoniq PARIS files (*.paf)"), "*.[pP][aA][fF]", NULL};
	static const gchar *pvf_f[] = {N_("Portable Voice Format files (*.pvf)"), "*.[pP][vV][fF]", NULL};
	static const gchar *raw_f[] = {N_("Headerless raw data (*.raw, *.r8)"), "*.[rR][aA][wW]", "*.[rR]8", NULL};
	static const gchar *sd2_f[] = {N_("Sound Designer II files (*.sd2)"), "*.[sS][dD]2", NULL};
	static const gchar *sds_f[] = {N_("Midi Sample Dump Standard files (*.sds)"), "*.[sS][dD][sS]", NULL};
	static const gchar *w64_f[] = {N_("SoundFoundry WAVE 64 files (*.w64)"), "*.[wW]64", NULL};

	static const gchar **in_f[] = {aiff_f, au_f, avr_f, caf_f, htk_f, iff_f, mat_f, paf_f, pvf_f, raw_f, sd2_f, sds_f, sf_f,
	                               voc_f, w64_f, wavex_f, flac_f, wve_f, ogg_f, rf64_f, NULL};
#endif

#if !USE_SNDFILE && AUDIOFILE_VERSION
	static const gchar *smp_f[] = {N_("Sample Vision files (*.smp)"), "*.[sS][mM][pP]", NULL};

	static const gchar **in_f[] = {aiff_f, au_f, avr_f, caf_f, iff_f, sf_f, voc_f, wavex_f, smp_f, rf64_f, ogg_f,
	                               wve_f, flac_f, NULL};
#endif

    box = gtk_vbox_new(FALSE, 2);
    gtk_container_border_width(GTK_CONTAINER(box), 10);
    gtk_notebook_append_page(nb, box, gtk_label_new(_("Sample Editor")));
    gtk_widget_show(box);

    thing = sample_display_new(TRUE);
    gtk_box_pack_start(GTK_BOX(box), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    g_signal_connect(thing, "loop_changed",
		       G_CALLBACK(sample_editor_display_loop_changed), NULL);
    g_signal_connect(thing, "selection_changed",
		       G_CALLBACK(sample_editor_display_selection_changed), NULL);
    g_signal_connect(thing, "window_changed",
		       G_CALLBACK(sample_editor_display_window_changed), NULL);
    sampledisplay = SAMPLE_DISPLAY(thing);
    sample_display_enable_zero_line(SAMPLE_DISPLAY(thing), TRUE);

    sample_editor_hscrollbar = gtk_hscrollbar_new(NULL);
    gtk_widget_show(sample_editor_hscrollbar);
    gtk_box_pack_start(GTK_BOX(box), sample_editor_hscrollbar, FALSE, TRUE, 0);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, TRUE, 0);
    gtk_widget_show(hbox);

    se->vertical_boxes[0] = vbox = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(vbox);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, TRUE, 0);

    make_radio_group((const char**)looplabels, vbox, loopradio, FALSE, FALSE, sample_editor_loopradio_changed);
    gui_put_labelled_spin_button(vbox, _("Start"), 0, 0, &spin_loopstart, sample_editor_loop_changed, NULL, TRUE);
    gui_put_labelled_spin_button(vbox, _("End"), 0, 0, &spin_loopend, sample_editor_loop_changed, NULL, TRUE);
    sample_editor_loopradio_changed();

    thing = gtk_vseparator_new();
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);

    se->vertical_boxes[1] = vbox = gtk_vbox_new(TRUE, 2);
    gtk_widget_show(vbox);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, TRUE, 0);

    gui_put_labelled_spin_button(vbox, _("Volume"), 0, 64, &spin_volume, sample_editor_spin_volume_changed, NULL, TRUE);
    gui_put_labelled_spin_button(vbox, _("Panning"), -128, 127, &spin_panning, sample_editor_spin_panning_changed, NULL, TRUE);
    gui_put_labelled_spin_button(vbox, _("Finetune"), -128, 127, &spin_finetune, sample_editor_spin_finetune_changed, NULL, TRUE);
    make_radio_group((const char**)resolutionlabels, vbox, resolution_radio, FALSE, FALSE, sample_editor_resolution_changed);

    thing = gtk_vseparator_new();
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);

    se->vertical_boxes[2] = vbox = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(vbox);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, TRUE, 0);

    /* Sample selection */
    vbox2 = gtk_vbox_new(TRUE, 2);
    gtk_widget_show(vbox2);
    gtk_box_pack_start(GTK_BOX(vbox), vbox2, FALSE, TRUE, 0);

    box2 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box2);
    gtk_box_pack_start(GTK_BOX(vbox2), box2, FALSE, TRUE, 0);

    thing = gtk_label_new(_("Selection:"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);

    thing = gtk_button_new_with_label(_("None"));
    g_signal_connect(thing, "clicked",
		       G_CALLBACK(sample_editor_select_none_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(box2), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label(_("All"));
    g_signal_connect(thing, "clicked",
		       G_CALLBACK(sample_editor_select_all_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(box2), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
    gtk_box_pack_start (GTK_BOX(vbox2), frame, FALSE, TRUE, 0);
    gtk_widget_show (frame);

    se->label_selection = gtk_label_new ("");
    gtk_misc_set_alignment (GTK_MISC (se->label_selection), 0.5, 0.5);
    gtk_container_add (GTK_CONTAINER (frame), se->label_selection);
    gtk_widget_show (se->label_selection);

    /* Sample length */
    box2 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box2);
    gtk_box_pack_start(GTK_BOX(vbox2), box2, FALSE, TRUE, 0);

    thing = gtk_label_new(_("Length:"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);

    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
    gtk_box_pack_start (GTK_BOX(box2), frame, TRUE, TRUE, 0);
    gtk_widget_show (frame);

    se->label_length = gtk_label_new ("");
    gtk_misc_set_alignment (GTK_MISC (se->label_length), 0.5, 0.5);
    gtk_container_add (GTK_CONTAINER (frame), se->label_length);
    gtk_widget_show (se->label_length);

    add_empty_vbox(vbox);

    thing = gtk_button_new_with_label(_("Set as loop"));
    g_signal_connect(thing, "clicked",
		       G_CALLBACK(sample_editor_selection_to_loop_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    add_empty_vbox(vbox);

    gui_put_labelled_spin_button(vbox, _("RelNote"), -128, 127, &spin_relnote, sample_editor_spin_relnote_changed, NULL, TRUE);

    thing = gtk_vseparator_new();
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(vbox);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

#if USE_SNDFILE || AUDIOFILE_VERSION
    file_selection_create(DIALOG_LOAD_SAMPLE, _("Load Sample"), gui_settings.loadsmpl_path, sample_editor_load_wav, 3, TRUE, FALSE, in_f, N_("Load sample into the current sample slot"));
    file_selection_create(DIALOG_SAVE_SAMPLE, _("Save Sample"), gui_settings.savesmpl_path, sample_editor_save_wav, 4, FALSE, TRUE, out_f, N_("Save the current sample"));
    file_selection_create(DIALOG_SAVE_RGN_SAMPLE, _("Save region as WAV..."), gui_settings.savesmpl_path, sample_editor_save_region_wav, -1, FALSE, TRUE, out_f, NULL);
#endif

    thing = gtk_button_new_with_label(_("Load Sample"));
    g_signal_connect(thing, "clicked",
		       G_CALLBACK(fileops_open_dialog), (gpointer)DIALOG_LOAD_SAMPLE);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
#if USE_SNDFILE == 0 && !defined (AUDIOFILE_VERSION)
    gtk_widget_set_sensitive(thing, 0);
#endif

    thing = gtk_button_new_with_label(_("Save WAV"));
    g_signal_connect(thing, "clicked",
		       G_CALLBACK(fileops_open_dialog), (gpointer)DIALOG_SAVE_SAMPLE);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    savebutton = thing;
#if USE_SNDFILE == 0 && !defined (AUDIOFILE_VERSION)
    gtk_widget_set_sensitive(thing, 0);
#endif

    thing = gtk_button_new_with_label(_("Save Region"));
    g_signal_connect(thing, "clicked",
		       G_CALLBACK(fileops_open_dialog), (gpointer)DIALOG_SAVE_RGN_SAMPLE);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    savebutton_rgn = thing;
#if USE_SNDFILE == 0 && !defined (AUDIOFILE_VERSION)
      gtk_widget_set_sensitive(thing, 0);
#endif

    
    thing = gtk_button_new_with_label(_("Sampling"));
    g_signal_connect(thing, "clicked",
		       G_CALLBACK(sample_editor_monitor_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label(_("Volume Ramp"));
    g_signal_connect(thing, "clicked",
		       G_CALLBACK(sample_editor_open_volume_ramp_dialog), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label(_("Trim"));
    g_signal_connect(G_OBJECT(thing), "clicked",
		       G_CALLBACK(sample_editor_trim_dialog), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);


    vbox = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(vbox);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

    thing = gtk_button_new_with_label(_("Zoom to selection"));
    g_signal_connect(thing, "clicked",
		       G_CALLBACK(sample_editor_zoom_to_selection_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label(_("Show all"));
    g_signal_connect(thing, "clicked",
		       G_CALLBACK(sample_editor_show_all_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label(_("Zoom in (+50%)"));
    g_signal_connect(thing, "clicked",
		       G_CALLBACK(sample_editor_zoom_in_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label(_("Zoom out (-50%)"));
    g_signal_connect(thing, "clicked",
		       G_CALLBACK(sample_editor_zoom_out_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label(_("Reverse"));
    g_signal_connect(thing, "clicked",
		       G_CALLBACK(sample_editor_reverse_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(vbox);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

    thing = gtk_button_new_with_label(_("Cut"));
    g_signal_connect(thing, "clicked",
		       G_CALLBACK(sample_editor_cut_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label(_("Remove"));
    g_signal_connect(thing, "clicked",
		       G_CALLBACK(sample_editor_remove_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label(_("Copy"));
    g_signal_connect(thing, "clicked",
		       G_CALLBACK(sample_editor_copy_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label(_("Paste"));
    g_signal_connect(thing, "clicked",
		       G_CALLBACK(sample_editor_paste_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label(_("Clear Sample"));
    g_signal_connect(thing, "clicked",
		       G_CALLBACK(sample_editor_clear_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label(_("Crop"));
    g_signal_connect(G_OBJECT(thing), "clicked",
		       G_CALLBACK(sample_editor_crop_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
}

static void
sample_editor_block_loop_spins (int block)
{
	if(block){
		g_signal_handlers_block_by_func(G_OBJECT(spin_loopstart),
				sample_editor_loop_changed, NULL);
		g_signal_handlers_block_by_func(G_OBJECT(spin_loopend),
				sample_editor_loop_changed, NULL);
	} else {
		g_signal_handlers_unblock_by_func(G_OBJECT(spin_loopstart),
				sample_editor_loop_changed, NULL);
		g_signal_handlers_unblock_by_func(G_OBJECT(spin_loopend),
				sample_editor_loop_changed, NULL);
	}
}

static void
sample_editor_blocked_set_loop_spins (int start,
				      int end)
{
    sample_editor_block_loop_spins(1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_loopstart), start);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_loopend), end);
    sample_editor_block_loop_spins(0);
}

static gint
sample_editor_idle_draw_function (struct SampleEditor *se)
{
    gtk_label_set(GTK_LABEL(se->label_selection), se->label_selection_new_text);
    gtk_label_set(GTK_LABEL(se->label_length), se->label_length_new_text);
    gtk_idle_remove(se->idle_handler);
    se->idle_handler = 0;
    return TRUE;
}

static void
sample_editor_set_selection_label (int start,
				   int end)
{
    STSample *sts = current_sample;

    if(start > -1) {
	g_sprintf(se->label_selection_new_text, ("%d - %d"), start, end);
	g_sprintf(se->label_length_new_text, ("%d"), end-start);
    } else {
	strcpy(se->label_selection_new_text, _("(no selection)"));
	g_sprintf(se->label_length_new_text, ("%d"), sts->sample.length);
    }

    /* Somewhere on the way to gtk+-1.2.10, gtk_label_set_text() has
       become incredibly slow. So slow that my computer is overwhelmed
       with the frequent calls to this function when one does a
       selection with the mouse -- with the consequence that the idle
       function updating the visual selection in the sample display
       widget is called only very seldom.

       That's why I'm doing the gtk_label_set_text() only if the
       computer is idle. */

    if(!se->idle_handler) {
	se->idle_handler = gtk_idle_add((GtkFunction)sample_editor_idle_draw_function,
						  (gpointer)se);
	g_assert(se->idle_handler != 0);
    }
}

static void
sample_editor_blocked_set_display_loop (int start,
					int end)
{
    g_signal_handlers_block_by_func(G_OBJECT(sampledisplay), G_CALLBACK(sample_editor_display_loop_changed), NULL);
    sample_display_set_loop(sampledisplay, start, end);
    g_signal_handlers_unblock_by_func(G_OBJECT(sampledisplay), G_CALLBACK(sample_editor_display_loop_changed), NULL);
}

void
sample_editor_update (void)
{
    STSample *sts = current_sample;
    st_mixer_sample_info *s;
    char buf[20];
    int m = xm_get_modified();

    sample_display_set_data(sampledisplay, NULL, ST_MIXER_FORMAT_S16_LE, 0, FALSE);

    if(!sts || !sts->sample.data) {
	gtk_widget_set_sensitive(se->vertical_boxes[0], FALSE);
	gtk_widget_set_sensitive(se->vertical_boxes[1], FALSE);
	gtk_widget_set_sensitive(se->vertical_boxes[2], FALSE);
	gtk_widget_set_sensitive(savebutton, 0);
	gtk_widget_set_sensitive(savebutton_rgn, 0);
    }

    if(!sts) {
	return;
    }

	gui_block_smplname_entry(TRUE); /* To prevent the callback */
    gtk_entry_set_text(GTK_ENTRY(gui_cursmpl_name), sts->utf_name);
	gui_block_smplname_entry(FALSE);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_volume), sts->volume);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_panning), sts->panning - 128);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_finetune), sts->finetune);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_relnote), sts->relnote);

    s = &sts->sample;

    g_sprintf(buf, ("%d"), s->length);
    gtk_label_set(GTK_LABEL(se->label_length), buf);

    sample_editor_block_loop_spins(1);
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(loopradio[s->looptype]), TRUE);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(spin_loopstart), 0, s->length - 1);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(spin_loopend), 1, s->length);
    sample_editor_block_loop_spins(0);
    sample_editor_blocked_set_loop_spins(s->loopstart, s->loopend);
    
    sample_editor_set_selection_label(-1, 0);

    if(s->data) {
	sample_display_set_data(sampledisplay, s->data, ST_MIXER_FORMAT_S16_LE, s->length, FALSE);

	if(s->looptype != ST_MIXER_SAMPLE_LOOPTYPE_NONE) {
	    sample_editor_blocked_set_display_loop(s->loopstart, s->loopend);
	}

	gtk_widget_set_sensitive(se->vertical_boxes[0], TRUE);
	gtk_widget_set_sensitive(se->vertical_boxes[1], TRUE);
	gtk_widget_set_sensitive(se->vertical_boxes[2], TRUE);

#if USE_SNDFILE || AUDIOFILE_VERSION
	gtk_widget_set_sensitive(savebutton, 1);
	gtk_widget_set_sensitive(savebutton_rgn, 1);
#endif
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(resolution_radio[sts->treat_as_8bit ? 0 : 1]), TRUE);
    }

    xm_set_modified(m);
}

gboolean
sample_editor_handle_keys (int shift,
			   int ctrl,
			   int alt,
			   guint32 keyval,
			   gboolean pressed)
{
	static gint playing = -1;
    int i;
    int s = sampledisplay->sel_start, e = sampledisplay->sel_end;
    int modifiers = ENCODE_MODIFIERS(shift, ctrl, alt);

    if(s == -1) {
	s = 0;
	e = current_sample->sample.length;
    }

	i = keys_get_key_meaning(keyval, modifiers);
	if(i != -1 && KEYS_MEANING_TYPE(i) == KEYS_MEANING_NOTE) {
		i += 12 * gui_get_current_octave_value() + 1;
		if(!pressed) {
			/* Autorepeat fake keyoff */
			if(keys_is_key_pressed(keyval, modifiers))
				return FALSE;

			if(playing == i) {
				gui_play_stop();
				playing = -1;
				return TRUE;
			}

			return FALSE;
		}

		if(i < 96 && current_sample != NULL && playing != i) {
			playing = i;
			gui_play_note_full(tracker->cursor_ch, i, current_sample, s, e - s);
		}
		return TRUE;
	}

    return FALSE;
}

void
sample_editor_set_sample (STSample *s)
{
    current_sample = s;
    sample_editor_update();
}

static void
sample_editor_update_mixer_position (double songtime)
{
    audio_mixer_position *p;
    int i;

    if(songtime >= 0.0 && current_sample && (p = time_buffer_get(audio_mixer_position_tb, songtime))) {
	for(i = 0; i < sizeof(p->dump) / sizeof(p->dump[0]); i++) {
	    if(p->dump[i].current_sample == &current_sample->sample) {
		sample_display_set_mixer_position(sampledisplay, p->dump[i].current_position);
		return;
	    }
	}
    }

    sample_display_set_mixer_position(sampledisplay, -1);
}

static gint
sample_editor_update_timeout (gpointer data)
{
    double display_songtime = current_driver->get_play_time(current_driver_object);

    sample_editor_update_mixer_position(display_songtime);

    // Not quite the right place for this, but anyway...
    gui_clipping_indicator_update(display_songtime);

    return TRUE;
}

void
sample_editor_start_updating (void)
{
    if(gtktimer != -1)
	return;

    gtktimer = gtk_timeout_add(1000 / update_freq, sample_editor_update_timeout, NULL);
}

void
sample_editor_stop_updating (void)
{
    if(gtktimer == -1)
	return;

    gtk_timeout_remove(gtktimer);
    gtktimer = -1;
    gui_clipping_indicator_update(-1.0);
    sample_editor_update_mixer_position(-1.0);
}

static void
sample_editor_spin_volume_changed (GtkSpinButton *spin)
{
    g_return_if_fail(current_sample != NULL);

    current_sample->volume = gtk_spin_button_get_value_as_int(spin);
    xm_set_modified(1);
}

static void
sample_editor_spin_panning_changed (GtkSpinButton *spin)
{
    g_return_if_fail(current_sample != NULL);

    current_sample->panning = gtk_spin_button_get_value_as_int(spin) + 128;
    xm_set_modified(1);
}

static void
sample_editor_spin_finetune_changed (GtkSpinButton *spin)
{
    g_return_if_fail(current_sample != NULL);

    current_sample->finetune = gtk_spin_button_get_value_as_int(spin);
    xm_set_modified(1);
}

static void
sample_editor_spin_relnote_changed (GtkSpinButton *spin)
{
    g_return_if_fail(current_sample != NULL);

    current_sample->relnote = gtk_spin_button_get_value_as_int(spin);
    xm_set_modified(1);
}

static void
sample_editor_selection_to_loop_clicked (void)
{
    int s = sampledisplay->sel_start, e = sampledisplay->sel_end;

    g_return_if_fail(current_sample != NULL);
    g_return_if_fail(current_sample->sample.data != NULL);

    if(s == -1) {
	return;
    }

    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(loopradio[0]), TRUE);
    
    sample_editor_lock_sample();
    current_sample->sample.loopend = e;
    current_sample->sample.loopstart = s;
    sample_editor_unlock_sample();

    sample_editor_blocked_set_loop_spins(s, e);

    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(loopradio[1]), TRUE);
    xm_set_modified(1);
}


static void
sample_editor_loop_changed ()
{
    int s = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_loopstart)),
	e = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_loopend));

    g_return_if_fail(current_sample != NULL);
    g_return_if_fail(current_sample->sample.data != NULL);
    g_return_if_fail(current_sample->sample.looptype != ST_MIXER_SAMPLE_LOOPTYPE_NONE);

    if(s != current_sample->sample.loopstart || e != current_sample->sample.loopend) {
	if(e <= s) {
	    e = s + 1;
	    sample_editor_blocked_set_loop_spins(s, e);
	}

	sample_editor_lock_sample();
	current_sample->sample.loopend = e;
	current_sample->sample.loopstart = s;
	sample_editor_unlock_sample();

	sample_editor_blocked_set_display_loop(s, e);
    }

    xm_set_modified(1);
}

static void
sample_editor_display_loop_changed (SampleDisplay *sample_display,
				    int start,
				    int end)
{
    g_return_if_fail(current_sample != NULL);
    g_return_if_fail(current_sample->sample.data != NULL);
    g_return_if_fail(current_sample->sample.looptype != ST_MIXER_SAMPLE_LOOPTYPE_NONE);
    g_return_if_fail(start < end);

    if(start != current_sample->sample.loopstart || end != current_sample->sample.loopend) {
	sample_editor_blocked_set_loop_spins(start, end);

	sample_editor_lock_sample();
	current_sample->sample.loopend = end;
	current_sample->sample.loopstart = start;
	sample_editor_unlock_sample();
    }

    xm_set_modified(1);
}

static void
sample_editor_select_none_clicked (void)
{
    sample_display_set_selection(sampledisplay, -1, 1);
}

static void
sample_editor_select_all_clicked (void)
{
    g_return_if_fail(current_sample != NULL);

    sample_display_set_selection(sampledisplay,
				 0, current_sample->sample.length);
}

static void
sample_editor_display_selection_changed (SampleDisplay *sample_display,
					 int start,
					 int end)
{
    g_return_if_fail(current_sample != NULL);
    g_return_if_fail(current_sample->sample.data != NULL);
    g_return_if_fail(start < end);

    sample_editor_set_selection_label(start, end);
}

static void
sample_editor_hscrollbar_changed (GtkAdjustment *adj)
{
    sample_display_set_window(sampledisplay,
			      adj->value,
			      adj->value + sampledisplay->win_length);
}

static void
sample_editor_display_window_changed (SampleDisplay *sample_display,
				      int start,
				      int end)
{
    if(current_sample == NULL)
	return;

    gui_update_range_adjustment(GTK_RANGE(sample_editor_hscrollbar),
				start,
				current_sample->sample.length,
				end - start,
				sample_editor_hscrollbar_changed);
}

static void
sample_editor_loopradio_changed (void)
{
    int n = find_current_toggle(loopradio, 3);

    gtk_widget_set_sensitive(spin_loopstart, n != 0);
    gtk_widget_set_sensitive(spin_loopend, n != 0);
    
    if(current_sample != NULL) {
	if(current_sample->sample.looptype != n) {
	    sample_editor_lock_sample();
	    current_sample->sample.looptype = n;
	    sample_editor_unlock_sample();
	}

	if(n != ST_MIXER_SAMPLE_LOOPTYPE_NONE) {
	    sample_editor_blocked_set_display_loop(current_sample->sample.loopstart, current_sample->sample.loopend);
	} else {
	    sample_editor_blocked_set_display_loop(-1, 1);
	}
    }

    xm_set_modified(1);
}

static void
sample_editor_resolution_changed (void)
{
    STSample *sts = current_sample;
    st_mixer_sample_info *s;
    int n = find_current_toggle(resolution_radio, 2);

    if(!sts)
	return;

    s = &sts->sample;

    sts->treat_as_8bit = (n == 0);

    xm_set_modified(1);
}

static void
sample_editor_clear_clicked (void)
{
    STInstrument *instr;

    sample_editor_lock_sample();

    st_clean_sample(current_sample, NULL, NULL);

    instr = instrument_editor_get_instrument();
    if(st_instrument_num_samples(instr) == 0) {
	instrument_editor_clear_current_instrument();
    } else {
	instrument_editor_update(TRUE);
	sample_editor_update();
    }

    sample_editor_unlock_sample();

    xm_set_modified(1);
}

static void
sample_editor_crop_clicked (void)
{
    sample_editor_crop();
}

static void
sample_editor_show_all_clicked (void)
{
    if(current_sample == NULL || current_sample->sample.data == NULL)
	return;
    sample_display_set_window(sampledisplay, 0, current_sample->sample.length);
}

static void
sample_editor_zoom_in_clicked (void)
{
    int ns = sampledisplay->win_start,
	ne = sampledisplay->win_start + sampledisplay->win_length;
    int l;
    
    if(current_sample == NULL || current_sample->sample.data == NULL)
	return;

    l = sampledisplay->win_length / 4;

    ns += l;
    ne -= l;

    if(ne <= ns)
	ne = ns + 1;

    sample_display_set_window(sampledisplay, ns, ne);
}

static void
sample_editor_zoom_out_clicked (void)
{
    int ns = sampledisplay->win_start,
	ne = sampledisplay->win_start + sampledisplay->win_length;
    int l;
    
    if(current_sample == NULL || current_sample->sample.data == NULL)
	return;

    l = sampledisplay->win_length / 2;

    if(ns > l)
	ns -= l;
    else
	ns = 0;

    if(ne <= current_sample->sample.length - l)
	ne += l;
    else
	ne = current_sample->sample.length;

    sample_display_set_window(sampledisplay, ns, ne);
}

static void
sample_editor_zoom_to_selection_clicked (void)
{
    if(current_sample == NULL || current_sample->sample.data == NULL || sampledisplay->sel_start == -1)
	return;
    sample_display_set_window(sampledisplay, sampledisplay->sel_start, sampledisplay->sel_end);
}

void
sample_editor_copy_cut_common (gboolean copy,
			       gboolean spliceout)
{
    int cutlen, newlen;
    gint16 *newsample;
    STSample *oldsample = current_sample;
    int ss = sampledisplay->sel_start, se;
    
    if(oldsample == NULL || ss == -1)
	return;
    
    se = sampledisplay->sel_end;

    cutlen = se - ss;
    newlen = oldsample->sample.length - cutlen;

    if(copy) {
	if(copybuffer) {
	    free(copybuffer);
	    copybuffer = NULL;
	}
	copybufferlen = cutlen;
	copybuffer = malloc(copybufferlen * 2);
	if(!copybuffer) {
	    static GtkWidget *dialog = NULL;

	    gui_error_dialog(&dialog, N_("Out of memory for copybuffer.\n"), FALSE);
	} else {
	    memcpy(copybuffer,
		   oldsample->sample.data + ss,
		   cutlen * 2);
	}
	memcpy(&copybuffer_sampleinfo, oldsample, sizeof(STSample));
    }

    if(!spliceout)
	return;

    if(newlen == 0) {
	sample_editor_clear_clicked();
	return;
    }

    newsample = malloc(newlen * 2);
    if(!newsample)
	return;

    sample_editor_lock_sample();

    memcpy(newsample,
	   oldsample->sample.data,
	   ss * 2);
    memcpy(newsample + ss,
	   oldsample->sample.data + se,
	   (oldsample->sample.length - se) * 2);

    free(oldsample->sample.data);

    oldsample->sample.data = newsample;
    oldsample->sample.length = newlen;

    /* Move loop start and end along with splice */
    if(oldsample->sample.loopstart > ss &&
       oldsample->sample.loopend < se) {
	/* loop was wholly within selection -- remove it */
	oldsample->sample.looptype = ST_MIXER_SAMPLE_LOOPTYPE_NONE;
	oldsample->sample.loopstart = 0;
	oldsample->sample.loopend = 1;
    } else {
	if(oldsample->sample.loopstart > se) {
	    /* loopstart was after selection */
	    oldsample->sample.loopstart -= (se-ss);
	} else if(oldsample->sample.loopstart > ss) {
	    /* loopstart was within selection */
	    oldsample->sample.loopstart = ss;
	}
	
	if(oldsample->sample.loopend > se) {
	    /* loopend was after selection */
	    oldsample->sample.loopend -= (se-ss);
	} else if(oldsample->sample.loopend > ss) {
	    /* loopend was within selection */
	    oldsample->sample.loopend = ss;
	}
    }

    st_sample_fix_loop(oldsample);
    sample_editor_unlock_sample();
    sample_editor_set_sample(oldsample);
    xm_set_modified(1);
}

static void
sample_editor_cut_clicked (void)
{
    sample_editor_copy_cut_common(TRUE, TRUE);
}

static void
sample_editor_remove_clicked (void)
{
    sample_editor_copy_cut_common(FALSE, TRUE);
}

static void
sample_editor_copy_clicked (void)
{
    sample_editor_copy_cut_common(TRUE, FALSE);
}

static void
sample_editor_init_sample_full (STSample *sample, const char *samplename)
{
    STInstrument *instr;

    st_clean_sample(sample, NULL, NULL);

    instr = instrument_editor_get_instrument();
    if(st_instrument_num_samples(instr) == 0) {
	st_clean_instrument(instr, samplename);
	memset(instr->samplemap, gui_get_current_sample(), sizeof(instr->samplemap));
    }
	
    st_clean_sample(sample, samplename, NULL);

    sample->volume = 64;
    sample->finetune = 0;
    sample->panning = 128;
    sample->relnote = 0;
}

static inline void
sample_editor_init_sample (const char *samplename)
{
	sample_editor_init_sample_full(current_sample, samplename);
}

void
sample_editor_paste_clicked (void)
{
    gint16 *newsample;
    STSample *oldsample = current_sample;
    int ss = sampledisplay->sel_start, newlen;
    int update_ie = 0;

    if(oldsample == NULL || copybuffer == NULL)
	return;

    if(!oldsample->sample.data) {
	/* pasting into empty sample */
	sample_editor_lock_sample();
	sample_editor_init_sample(_("<just pasted>"));/* Use only charachers from FT2 codeset in the translation! */
	oldsample->treat_as_8bit = copybuffer_sampleinfo.treat_as_8bit;
	oldsample->volume = copybuffer_sampleinfo.volume;
	oldsample->finetune = copybuffer_sampleinfo.finetune;
	oldsample->panning = copybuffer_sampleinfo.panning;
	oldsample->relnote = copybuffer_sampleinfo.relnote;
	sample_editor_unlock_sample();
	ss = 0;
	update_ie = 1;
    } else {
	if(ss == -1)
	    return;
    }

    newlen = oldsample->sample.length + copybufferlen;

    newsample = malloc(newlen * 2);
    if(!newsample)
	return;

    sample_editor_lock_sample();

    memcpy(newsample,
	   oldsample->sample.data,
	   ss * 2);
    st_convert_sample(copybuffer,
		      newsample + ss,
		      16,
		      16,
		      copybufferlen);
    memcpy(newsample + (ss + copybufferlen),
	   oldsample->sample.data + ss,
	   (oldsample->sample.length - ss) * 2);

    free(oldsample->sample.data);

    oldsample->sample.data = newsample;
    oldsample->sample.length = newlen;

    sample_editor_unlock_sample();
    sample_editor_update();
    if(update_ie)
	instrument_editor_update(TRUE);
    xm_set_modified(1);
}


static void
sample_editor_reverse_clicked (void)
{
    int ss = sampledisplay->sel_start, se = sampledisplay->sel_end;
    int i;
    gint16 *p, *q;

    if(!current_sample || ss == -1) {
	return;
    }

    sample_editor_lock_sample();

    p = q = current_sample->sample.data;
    p += ss;
    q += se;

    for(i = 0; i < (se - ss)/2; i++) {
	gint16 t = *p;
	*p++ = *--q;
	*q = t;
    }

    xm_set_modified(1);
    sample_editor_unlock_sample();
    sample_editor_update();
    sample_display_set_selection(sampledisplay, ss, se);
}

static void
sample_editor_open_stereo_dialog (GtkWidget **window, GtkWidget **buttons, const gchar *text,
                                  const gchar *title)
{
	static const gchar *labels[] = {N_("Mix"), N_("Left"), N_("Right"), N_("2 samples"), NULL};
	GtkWidget *label, *box1;

	if(!*window) {
		*window = gtk_dialog_new_with_buttons(_(title), GTK_WINDOW(mainwindow), GTK_DIALOG_MODAL,
		                                      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, 
		                                      GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
		gui_dialog_adjust(*window, 2);
		box1 = gtk_dialog_get_content_area(GTK_DIALOG(*window));
		gtk_box_set_spacing(GTK_BOX(box1), 2);

		label = gtk_label_new(_(text));
		gtk_label_set_justify(GTK_LABEL (label),GTK_JUSTIFY_CENTER);
		gtk_box_pack_start(GTK_BOX (box1), label, FALSE, TRUE, 0);
		gtk_widget_show(label);

		make_radio_group(labels, box1, buttons, FALSE, FALSE, NULL);
		gtk_widget_set_tooltip_text(buttons[3], _("Load left and right channels into the current sample and the next one"));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(buttons[0]), TRUE);
	} else
		gtk_window_present(GTK_WINDOW(*window));
}

#if USE_SNDFILE || AUDIOFILE_VERSION

static gboolean
sample_editor_load_wav_main (const int mode, FILE *f, struct wl *wavload)
{
    void *sbuf, *sbuf_loadto, *tmp, *sbuf2 = NULL;
#if USE_SNDFILE
    sf_count_t len;
#else
    int len;
#endif
    int i, count;
    float rate;
    STSample *next = NULL;

	if(mode == MODE_STEREO_2) {
		gint n_cur;

		if((n_cur = modinfo_get_current_sample()) == 127) {
			static GtkWidget *dialog = NULL;

			gui_warning_dialog(&dialog, N_("You have selected the last sample of the instrument, but going "
			                               "to load the second stereo channel to the next sample. Please select "
			                               "a sample slot with lower number or use another loading mode."), FALSE);
			return TRUE;
		}
		next = &(instrument_editor_get_instrument()->samples[n_cur + 1]);
		if(next->sample.length) {
			if(!gui_ok_cancel_modal(mainwindow, N_("The next sample which is about to be overwritten is not empty!\n"
			                                       "Would you like to overwrite it?")))
				return TRUE;
		}
	}

    statusbar_update(STATUS_LOADING_SAMPLE, TRUE);

    len = 2 * wavload->frameCount * wavload->channelCount;
    if(!(tmp = malloc(len))) {
	static GtkWidget *dialog = NULL;

	gui_error_dialog(&dialog, N_("Out of memory for sample data."), FALSE);
	goto errnobuf;
    }
	if(mode == MODE_MONO)
		sbuf = tmp;
	else if(!(sbuf = malloc(2 * wavload->frameCount))) {
		static GtkWidget *dialog = NULL;

		gui_error_dialog(&dialog, N_("Out of memory for sample data."), FALSE);
		free(tmp);
		goto errnobuf;
	}

    if(wavload->sampleWidth == 16) {
	sbuf_loadto = tmp;
    } else {
	sbuf_loadto = tmp + len / 2;
    }

    if(wavload->through_library) {
#if USE_SNDFILE
	if(wavload->frameCount != sf_readf_short(wavload->file, sbuf_loadto, wavload->frameCount)) {
#else
	if(wavload->frameCount != afReadFrames(wavload->file, AF_DEFAULT_TRACK, sbuf_loadto, wavload->frameCount)) {
#endif
	    static GtkWidget *dialog = NULL;

	    gui_error_dialog(&dialog, N_("Read error."), FALSE);
	    goto errnodata;
	}
    } else {
	if(!f)
	    goto errnodata;
	if(wavload->frameCount != fread(sbuf_loadto,
				       wavload->channelCount * wavload->sampleWidth / 8,
				       wavload->frameCount,
				       f)) {
	    static GtkWidget *dialog = NULL;

	    fclose(f);
	    gui_error_dialog(&dialog, N_("Read error."), FALSE);
	    goto errnodata;
	}
    }

    if(wavload->sampleWidth == 8) {
	if(wavload->through_library || wavload->unsignedwords) {
	    st_sample_8bit_signed_unsigned(sbuf_loadto, len / 2);
	}
	st_convert_sample(sbuf_loadto,
			  tmp,
			  8,
			  16,
			  len / 2);
    } else {
	if(wavload->through_library) {
	    // I think that is what the virtualByteOrder stuff is for.
	    // le_16_array_to_host_order(sbuf, len / 2);
	} else {
#ifdef WORDS_BIGENDIAN
	    if(wavload->endianness == 0) {
#else
	    if(wavload->endianness == 1) {
#endif
		byteswap_16_array(tmp, len / 2);
	    }
	    if(wavload->unsignedwords) {
		st_sample_16bit_signed_unsigned(sbuf_loadto, len / 2);
	    }
	}
    }

    sample_editor_lock_sample();
    sample_editor_init_sample(wavload->samplename);
    current_sample->sample.data = sbuf;
    current_sample->treat_as_8bit = (wavload->sampleWidth == 8);
    current_sample->sample.length = wavload->frameCount;

    // Initialize relnote and finetune such that sample is played in original speed
    if(wavload->through_library) {
#if USE_SNDFILE
	current_sample->treat_as_8bit = ((wavload->wavinfo.format & (SF_FORMAT_PCM_S8 | SF_FORMAT_PCM_U8)) != 0);
	rate = wavload->wavinfo.samplerate;
#else
	rate = afGetRate(wavload->file, AF_DEFAULT_TRACK);
#endif
    } else {
	rate = wavload->rate;
    }
    xm_freq_note_to_relnote_finetune(rate,
				     4 * 12 + 1, // at C-4
				     &current_sample->relnote,
				     &current_sample->finetune);

	if(mode == MODE_STEREO_2) {
		if(!(sbuf2 = malloc(2 * wavload->frameCount))) {
			static GtkWidget *dialog = NULL;

			gui_error_dialog(&dialog, N_("Out of memory for sample data."), FALSE);
			goto errnodata;
		}

		g_mutex_lock(next->sample.lock);
		sample_editor_init_sample_full(next, wavload->samplename);
		next->sample.data = sbuf2;
		next->treat_as_8bit = (wavload->sampleWidth == 8);
		next->sample.length = wavload->frameCount;

		xm_freq_note_to_relnote_finetune(rate,
		                                 4 * 12 + 1, // at C-4
		                                 &next->relnote,
		                                 &next->finetune);
	}

    if(mode != MODE_MONO) {
	gint16 *a = tmp, *b = sbuf, *c = sbuf2;

	count = wavload->frameCount;

	/* Code could probably be made shorter. But this is readable. */
	switch(mode) {
	case MODE_STEREO_MIX:
	    for(i = 0; i < count; i++, a+=2, b+=1)
		*b = (a[0] + a[1]) / 2;
	    break;
	case MODE_STEREO_2:
		for(i = 0; i < count; i++, a+=2, b+=1, c+=1) {
			*b = a[0];
			*c = a[1];
		}
		break;
	case MODE_STEREO_LEFT:
	    for(i = 0; i < count; i++, a+=2, b+=1)
		*b = a[0];
	    break;
	case MODE_STEREO_RIGHT:
	    for(i = 0; i < count; i++, a+=2, b+=1)
		*b = a[1];
	    break;
	default:
	    g_assert_not_reached();
	    break;
	}
    }
	if(sbuf != tmp)
		free(tmp);

	if(mode == MODE_STEREO_2) {
		if(gui_playing_mode) {
			mixer->updatesample(&next->sample);
		}
		g_mutex_unlock(next->sample.lock);
	}
    sample_editor_unlock_sample();

    instrument_editor_update(TRUE);
    sample_editor_update();
    xm_set_modified(1);
    statusbar_update(STATUS_SAMPLE_LOADED, FALSE);
    goto errnobuf;

  errnodata:
    statusbar_update(STATUS_IDLE, FALSE);
    free(sbuf);
	if(sbuf != tmp)
		free(tmp);
  errnobuf:
    return FALSE;
}

static void
sample_editor_open_stereowav_dialog (FILE *f, struct wl *wavload)
{
	static GtkWidget *window = NULL;
	gboolean replay;
	gint response;

	sample_editor_open_stereo_dialog(&window, load_radio,
	                                 N_("You have selected a stereo sample!\n(SoundTracker can only handle mono samples!)\n\nPlease choose which channel to load:"),
	                                 N_("Stereo sample loading"));

	do {
		response = gtk_dialog_run(GTK_DIALOG(window));

		gtk_widget_hide(window);
		if(response != GTK_RESPONSE_OK)
			return;

		response = find_current_toggle(load_radio, 4) + 1; /* +1 since 0 means mono mode */
		replay = sample_editor_load_wav_main(MODE_STEREO_2, f, wavload);
	} while(response == MODE_STEREO_2 && replay);
}

static void
sample_editor_open_raw_sample_dialog (FILE *f, struct wl *wavload)
{
    static GtkWidget *window = NULL, *combo;
    static GtkListStore *ls;

    GtkWidget *box1;
    GtkWidget *box2;
    GtkWidget *separator;
    GtkWidget *label;
    GtkWidget *thing;
    GtkTreeIter iter;
    gint response, i, active = 0;
    
    static const char *resolutionlabels[] = { N_("8 bits"), N_("16 bits"), NULL };
    static const char *signedlabels[] = { N_("Signed"), N_("Unsigned"), NULL };
    static const char *endianlabels[] = { N_("Little-Endian"), N_("Big-Endian"), NULL };
    static const char *channelslabels[] = { N_("Mono"), N_("Stereo"), NULL };

    // Standard sampling rates
    static const guint rates[] = {8000, 8363, 11025, 12000, 16000, 22050, 24000, 32000, 33452, 44100, 48000};

	if(!window) {
	    window = gtk_dialog_new_with_buttons(_("Load raw sample"), GTK_WINDOW(mainwindow), GTK_DIALOG_MODAL,
	                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
   
	    wavload_dialog = window;
	    gui_dialog_adjust(window, GTK_RESPONSE_OK);
       
	    box1 = gtk_dialog_get_content_area (GTK_DIALOG(window)); // upper part (vbox)
    
	    label = gtk_label_new (_("You have selected a sample that is not\nin a known format. You can load the raw data now.\n\nPlease choose a format:"));
	    gtk_label_set_justify (GTK_LABEL (label),GTK_JUSTIFY_CENTER);
	    gtk_box_pack_start (GTK_BOX (box1), label, FALSE, TRUE, 0);
	    gtk_widget_show (label);
	
	    separator = gtk_hseparator_new ();
	    gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 5);
	    gtk_widget_show (separator);
	
	    // The toggles
    
	    box2 = gtk_hbox_new(FALSE, 4);
	    gtk_widget_show(box2);
	    gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, TRUE, 0);

	    thing = gtk_label_new(_("Resolution:"));
	    gtk_widget_show(thing);
	    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
	    add_empty_hbox(box2);
	    make_radio_group_full(resolutionlabels, box2, wavload_raw_resolution_w,
				  FALSE, TRUE, NULL, NULL);

	    box2 = gtk_hbox_new(FALSE, 4);
	    gtk_widget_show(box2);
	    gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, TRUE, 0);

	    thing = gtk_label_new(_("Word format:"));
	    gtk_widget_show(thing);
	    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
	    add_empty_hbox(box2);
	    make_radio_group_full(signedlabels, box2, wavload_raw_signed_w,
				  FALSE, TRUE, NULL, NULL);

	    box2 = gtk_hbox_new(FALSE, 4);
	    gtk_widget_show(box2);
	    gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, TRUE, 0);

	    add_empty_hbox(box2);
	    make_radio_group_full(endianlabels, box2, wavload_raw_endian_w,
				  FALSE, TRUE, NULL, NULL);

	    box2 = gtk_hbox_new(FALSE, 4);
	    gtk_widget_show(box2);
	    gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, TRUE, 0);

	    thing = gtk_label_new(_("Channels:"));
	    gtk_widget_show(thing);
	    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
	    add_empty_hbox(box2);
	    make_radio_group_full(channelslabels, box2, wavload_raw_channels_w,
				  FALSE, TRUE, NULL, NULL);

	// Rate selection combo
	    box2 = gtk_hbox_new(FALSE, 4);
	    gtk_widget_show(box2);
	    gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, TRUE, 0);

	    thing = gtk_label_new(_("Sampling Rate:"));
	    gtk_widget_show(thing);
	    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
	    add_empty_hbox(box2);

	    ls = gtk_list_store_new(1, G_TYPE_UINT);
	    for(i = 0; i < sizeof(rates) / sizeof(guint); i++) {
		gtk_list_store_append(ls, &iter);
		gtk_list_store_set(ls, &iter, 0, rates[i], -1);
		if(rates[i] == 8363)
		    active = i;
	    }

	    combo = gui_combo_new(ls);
	    gtk_box_pack_start(GTK_BOX(box2), combo, FALSE, TRUE, 0);
	    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), active); // default is 8363
	    gtk_widget_show (combo);

	    thing = gtk_hseparator_new();
	    gtk_box_pack_end(GTK_BOX(box1), thing, FALSE, FALSE, 4);
	    gtk_widget_show(thing);
		gtk_widget_show(window);
	} else
		gtk_window_present(GTK_WINDOW(window));

	response = gtk_dialog_run(GTK_DIALOG(window));
	gtk_widget_hide(window);

	if(response == GTK_RESPONSE_OK) {
	    wavload->sampleWidth = find_current_toggle(wavload_raw_resolution_w, 2) == 1 ? 16 : 8;
	    wavload->endianness = find_current_toggle(wavload_raw_endian_w, 2);
	    wavload->channelCount = find_current_toggle(wavload_raw_channels_w, 2) == 1 ? 2 : 1;
	    wavload->unsignedwords = find_current_toggle(wavload_raw_signed_w, 2);

	    if(!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(combo), &iter))
		wavload->rate = 8363;
	    else
		gtk_tree_model_get(GTK_TREE_MODEL(ls), &iter, 0, &wavload->rate, -1);

	    if(wavload->sampleWidth == 16) {
		wavload->frameCount /= 2;
	    }

	    if(wavload->channelCount == 2) {
		wavload->frameCount /= 2;
		sample_editor_open_stereowav_dialog(f, wavload);
	    } else {
		sample_editor_load_wav_main(MODE_MONO, f, wavload);
	    }
	}
}

static void
sample_editor_load_wav (const gchar *fn)
{
	struct wl wavload;

#if USE_SNDFILE != 1
    int sampleFormat;
#endif
	gchar *localname = gui_filename_from_utf8(fn);

    g_return_if_fail(current_sample != NULL);
    if(!localname)
		return;

    file_selection_save_path(fn, &gui_settings.loadsmpl_path);

    wavload.samplename = strrchr(fn, '/');
    if(!wavload.samplename)
	wavload.samplename = fn;
    else
	wavload.samplename++;

#if USE_SNDFILE
    wavload.wavinfo.format = 0;
    wavload.file = sf_open(localname, SFM_READ, &wavload.wavinfo);
#else
    wavload.file = afOpenFile(localname, "r", NULL);
#endif
    if(!wavload.file) {
	FILE *f = fopen(localname, "r");
	g_free(localname);
	if(f) {
	    fseek(f, 0, SEEK_END);
	    wavload.frameCount = ftell(f);
	    fseek(f, 0, SEEK_SET);
	    wavload.through_library = FALSE;
	    sample_editor_open_raw_sample_dialog(f, &wavload);
	    fclose(f);
	} else {
	    static GtkWidget *dialog = NULL;

	    gui_error_dialog(&dialog, N_("Can't read sample"), FALSE);
	}
	return;
    }
    g_free(localname);

    wavload.through_library = TRUE;

#if USE_SNDFILE
    wavload.frameCount = wavload.wavinfo.frames;
#else
    wavload.frameCount = afGetFrameCount(wavload.file, AF_DEFAULT_TRACK);
#endif
    if(wavload.frameCount > mixer->max_sample_length) {
	static GtkWidget *dialog = NULL;
	gui_warning_dialog(&dialog, N_("Sample is too long for current mixer module. Loading anyway."), FALSE);
    }

#if USE_SNDFILE

    wavload.channelCount = wavload.wavinfo.channels;
    wavload.sampleWidth = 16;
    
#else

    wavload.channelCount = afGetChannels(wavload.file, AF_DEFAULT_TRACK);
    afGetSampleFormat(wavload.file, AF_DEFAULT_TRACK, &sampleFormat, &wavload.sampleWidth);

    /* I think audiofile-0.1.7 does this automatically, but I'm not sure */
#ifdef WORDS_BIGENDIAN
    afSetVirtualByteOrder(wavload.file, AF_DEFAULT_TRACK, AF_BYTEORDER_BIGENDIAN);
#else
    afSetVirtualByteOrder(wavload.file, AF_DEFAULT_TRACK, AF_BYTEORDER_LITTLEENDIAN);
#endif

#endif


    if((wavload.sampleWidth != 16 && wavload.sampleWidth != 8) || wavload.channelCount > 2) {
	static GtkWidget *dialog = NULL;

	gui_error_dialog(&dialog, N_("Can only handle 8 and 16 bit samples with up to 2 channels"), FALSE);
    } else {
	if(wavload.channelCount == 1) {
		sample_editor_load_wav_main(MODE_MONO, NULL, &wavload);
	} else {
		sample_editor_open_stereowav_dialog(NULL, &wavload);
	}
    }

#if USE_SNDFILE
    sf_close(wavload.file);
#else
    afCloseFile(wavload.file);
#endif
    return;
}


static void
sample_editor_save_wav_main (const gchar *fn,
			     guint32 offset,
			     guint32 length)
{
#if USE_SNDFILE
    SNDFILE *outfile;
    SF_INFO sfinfo;
    int rate = 44100;
#else
    AFfilehandle outfile;
    AFfilesetup outfilesetup;
    double rate = 44100.0;
#endif
	gchar *localname = gui_filename_from_utf8(fn);

	if(!localname)
		return;

    statusbar_update(STATUS_SAVING_SAMPLE, TRUE);

    file_selection_save_path(localname, &gui_settings.savesmpl_path);

#if USE_SNDFILE
    if(current_sample->treat_as_8bit) {
	sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_U8;
    } else {
	sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
    }
    sfinfo.channels = 1;
    sfinfo.samplerate = rate;
    outfile = sf_open (fn, SFM_WRITE, &sfinfo);
#else
    outfilesetup = afNewFileSetup();
    afInitFileFormat(outfilesetup, AF_FILE_WAVE);
    afInitChannels(outfilesetup, AF_DEFAULT_TRACK, 1);
    if(current_sample->treat_as_8bit) {
#if AUDIOFILE_VERSION == 1
    // for audiofile-0.1.x
    afInitSampleFormat(outfilesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 8);
#else
    // for audiofile-0.2.x and 0.3.x
    afInitSampleFormat(outfilesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_UNSIGNED, 8);
#endif
    } else {
	afInitSampleFormat(outfilesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    }
    afInitRate(outfilesetup, AF_DEFAULT_TRACK, rate);
    outfile = afOpenFile(localname, "w", outfilesetup);
    afFreeFileSetup(outfilesetup);
#endif
	g_free(localname);

    if(!outfile) {
	static GtkWidget *dialog = NULL;

	gui_error_dialog(&dialog, N_("Can't open file for writing."), FALSE);
	return;
    }

#if USE_SNDFILE
    sf_writef_short (outfile, 
		     current_sample->sample.data + offset,
		     length);
    sf_close(outfile);
#else
    if(current_sample->treat_as_8bit) {
	void *buf = malloc(1 * length);
	g_assert(buf);
	st_convert_sample(current_sample->sample.data + offset,
			  buf,
			  16,
			  8,
			  length);
	st_sample_8bit_signed_unsigned(buf,
				       length);

	afWriteFrames(outfile, AF_DEFAULT_TRACK,
		      buf,
		      length);
	free(buf);
    } else {
	afWriteFrames(outfile, AF_DEFAULT_TRACK,
		      current_sample->sample.data + offset,
		      length);
    }
    afCloseFile(outfile);
#endif

    statusbar_update(STATUS_SAMPLE_SAVED, FALSE);
}

static void
sample_editor_save_wav (const gchar *fn)
{
    g_return_if_fail(current_sample != NULL);
    if(current_sample->sample.data == NULL) {
	return;
    }
	sample_editor_save_wav_main(fn, 0, current_sample->sample.length);
}

static void
sample_editor_save_region_wav (const gchar *fn)
{
    int rss = sampledisplay->sel_start, rse = sampledisplay->sel_end;

    g_return_if_fail(current_sample != NULL);
    if(current_sample->sample.data == NULL) {
	return;
    }
    
    if(rss == -1) {
        static GtkWidget *dialog = NULL;
        gui_error_dialog(&dialog, N_("Please select region first."), FALSE);
	return;
    }
	sample_editor_save_wav_main(fn, rss, rse - rss);
}

#endif /* USE_SNDFILE || AUDIOFILE_VERSION */

/* ============================ Sampling functions coming up -------- */

static void
clear_buffers (void)
{
	struct recordbuf *r, *r2;

	/* Free allocated sample buffers */
	for(r = recordbufs; r; r = r2) {
		r2 = r->next;
		free(r->data);
		free(r);
	}
	recordbufs = NULL;
}

static void
enable_widgets (gboolean enable)
{
    gtk_widget_set_sensitive(okbutton, enable);
    gtk_widget_set_sensitive(clearbutton, enable);
}

static void
sampling_response (GtkWidget *dialog, gint response, GtkToggleButton *button)
{
	gtk_widget_hide(samplingwindow);
	sampling = FALSE;

	if(button->active) {
		g_signal_handler_block(G_OBJECT(button), toggled_id); /* To prevent data storing on record stop */
		gtk_toggle_button_set_state(button, FALSE);
		g_signal_handler_unblock(G_OBJECT(button), toggled_id);
	}
	if(monitoring) {
		sampling_driver->release(sampling_driver_object);
		monitoring = FALSE;
	}

	if(response == GTK_RESPONSE_OK)
		sample_editor_ok_clicked();

	clock_stop(CLOCK(sclock));
	clock_set_seconds(CLOCK(sclock), 0);
	clear_buffers();
	has_data = FALSE;
}

static void
sampling_widget_hide (GtkToggleButton *button)
{
	sampling_response(NULL, GTK_RESPONSE_CANCEL, button);
}

static void
record_toggled (GtkWidget *button)
{
	if(GTK_TOGGLE_BUTTON(button)->active) {
		recordedlen = 0;
		if(recordbufs)
			clear_buffers();

		if(!monitoring) {
			sampling_driver->open(sampling_driver_object);
			monitoring = TRUE;
		}
		sampling = TRUE;
		clock_set_seconds(CLOCK(sclock), 0);
		clock_start(CLOCK(sclock));
	} else {
		sampling = FALSE;
		sampling_driver->release(sampling_driver_object);
		monitoring = FALSE;

		has_data = TRUE;
		enable_widgets(has_data);
		// _set_chain() instead to display the whole sample
		sample_display_set_data(monitorscope, recordbufs->data, ST_MIXER_FORMAT_S16_LE,
		                        recordbufs->length >> (mixer_get_resolution(format & 0x7) - 1), FALSE);

		clock_stop(CLOCK(sclock));
	}
}

static void
clear_clicked (void)
{
	has_data = FALSE;
	enable_widgets(has_data);

	recordedlen = 0;
	clear_buffers();
	if(!monitoring) {
		sampling_driver->open(sampling_driver_object);
		monitoring = TRUE;
	}
	clock_set_seconds(CLOCK(sclock), 0);
}

static void
sample_editor_monitor_clicked (void)
{
	if(!sampling_driver || !sampling_driver_object) {
		static GtkWidget *dialog = NULL;

		gui_error_dialog(&dialog, N_("No sampling driver available"), FALSE);
		return;
	}

	if(!samplingwindow) {
		GtkWidget *mainbox, *thing, *box, *box2;
		GtkAccelGroup *group = gtk_accel_group_new();
		GClosure *closure;

		samplingwindow = gtk_dialog_new_with_buttons(_("Sampling Window"), GTK_WINDOW(mainwindow), 0,
		                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);
		gtk_window_add_accel_group(GTK_WINDOW(samplingwindow), group);
		g_signal_connect(samplingwindow, "delete-event",
		                 G_CALLBACK(gui_delete_noop), NULL);

		okbutton = gtk_dialog_add_button(GTK_DIALOG(samplingwindow), GTK_STOCK_OK, GTK_RESPONSE_OK);

		mainbox = gtk_dialog_get_content_area(GTK_DIALOG(samplingwindow));
		gtk_container_border_width(GTK_CONTAINER(mainbox), 4);

		box = gtk_vbox_new(FALSE, 2);

		thing = sample_display_new(FALSE);
		gtk_box_pack_start(GTK_BOX(box), thing, TRUE, TRUE, 0);
		monitorscope = SAMPLE_DISPLAY(thing);

		box2 = gtk_hbox_new(FALSE, 4);
		gtk_box_pack_start(GTK_BOX(box), box2, FALSE, TRUE, 0);

		thing = gtk_toggle_button_new_with_label(_("Record"));
		closure = g_cclosure_new_swap(G_CALLBACK(sampling_widget_hide), thing, NULL);
		gtk_accel_group_connect(group, GDK_Escape, 0, 0, closure);
		toggled_id = g_signal_connect(thing, "toggled",
		                              G_CALLBACK(record_toggled), NULL);
		gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, FALSE, 0);
		g_signal_connect(samplingwindow, "response",
		                 G_CALLBACK(sampling_response), thing);

		clearbutton = thing = gtk_button_new_with_label(_("Clear"));
		g_signal_connect(thing, "clicked",
		                 G_CALLBACK(clear_clicked), NULL);
		gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, FALSE, 0);

		sclock = clock_new();
		clock_set_format(CLOCK(sclock), _("%M:%S"));
		clock_set_seconds(CLOCK(sclock), 0);
		gtk_box_pack_start(GTK_BOX(box2), sclock, FALSE, TRUE, 0);

		gtk_box_pack_start(GTK_BOX(mainbox), box, TRUE, TRUE, 0);
		gtk_widget_show_all(samplingwindow);
	} else
		gtk_window_present(GTK_WINDOW(samplingwindow));

	enable_widgets(FALSE);

	recordbufs = NULL;
	sampling = FALSE;
	has_data = FALSE;
	recordedlen = 0;
	current = NULL;
	rate = 44100;
	format = ST_MIXER_FORMAT_S16_LE;

	if(!sampling_driver->open(sampling_driver_object)) {
		static GtkWidget *dialog = NULL;

		sample_editor_stop_sampling();
		gui_error_dialog(&dialog, N_("Sampling failed!"), FALSE);
		monitoring = FALSE;
	} else
		monitoring = TRUE;
}

/* Count is in bytes, not samples. The function returns TRUE if the buffer is acquired and the driver shall allocate a new one */
gboolean
sample_editor_sampled (void *src,
		       guint32 count,
		       int mixfreq,
		       int mixformat)
{
	gboolean sampled = FALSE;

	sample_display_set_data(monitorscope, src, mixformat, count >> (mixer_get_resolution(mixformat & 0x7) - 1), FALSE);

	if(sampling) {
		struct recordbuf *newbuf = malloc(sizeof(struct recordbuf));

		if(!newbuf) {
			/* It is called from audio thread AFAIK */
			error_error(N_("Out of memory while sampling!"));
			sampling = 0;
			return FALSE;
		}
		newbuf->next = NULL;
		newbuf->length = count;
		newbuf->data = src;

		if(!recordbufs){ /* Sampling start */
			recordbufs = newbuf;
			rate = mixfreq;
			format = mixformat;
		} else
			current->next = newbuf;

		current = newbuf;
		sampled = TRUE;
		recordedlen += count;
	}

	return sampled;
}

void
sample_editor_stop_sampling (void)
{

	sampling = FALSE;
	has_data = FALSE;

	if(samplingwindow) {
		if(monitoring)
			sampling_driver->release(sampling_driver_object);
		gtk_widget_hide(samplingwindow);

		clear_buffers();
	}

	monitoring = FALSE;
}

static void
sample_editor_ok_clicked (void)
{
    STInstrument *instr;
	STSample *next = NULL;
    struct recordbuf *r, *r2;
    gint16 *sbuf, *sbuf2= NULL;
    const char *samplename = _("<just sampled>"); /* The translation can use only charachters from FT2 codeset */
    guint multiply, mode = 0;
    gboolean stereo = format & ST_MIXER_FORMAT_STEREO;

    g_return_if_fail(current_sample != NULL && has_data);

	format &= 0x7;
	multiply = mixer_get_resolution(format) - 1; /* 0 - 8 bit, 1 - 16, used for shifts */
	if(!stereo && !multiply) /* 8bit mono */
		recordedlen = recordedlen << 1;
	else if (stereo && multiply) /* 16 bit stereo */
		recordedlen = recordedlen >> 1;

	if(!(sbuf = malloc(recordedlen))) {
		static GtkWidget *dialog = NULL;

		gui_error_dialog(&dialog, N_("Out of memory for sample data."), FALSE);
		return;
	}

	if(stereo) {
		static GtkWidget *window = NULL;
		gboolean replay;

		sample_editor_open_stereo_dialog(&window, record_radio,
		                                 N_("You have recorded a stereo sample!\n(SoundTracker can only handle mono samples!)\n\nPlease choose which channel to use:"),
		                                 N_("Converting stereo sample"));

		do {
			gint n_cur;

			replay = FALSE;

			mode = gtk_dialog_run(GTK_DIALOG(window));
			gtk_widget_hide(window);

			if(mode != GTK_RESPONSE_OK)
				mode = 0; /* Cancelling or other possible unclear status -- do nothing */
			else
				mode = find_current_toggle(record_radio, 4) + 1; /* +1 since 0 means mono mode */
			switch(mode) {
			case MODE_STEREO_LEFT:
			case MODE_STEREO_MIX:
			case MODE_STEREO_RIGHT:
				break;
			case MODE_STEREO_2:
				if((n_cur = modinfo_get_current_sample()) == 127) {
					static GtkWidget *dialog = NULL;

					gui_warning_dialog(&dialog, N_("You have selected the last sample of the instrument, but going "
					                               "to load the second stereo channel to the next sample. Please select "
					                               "a sample slot with lower number or use another loading mode."), FALSE);
					replay = TRUE;
				}
				next = &(instrument_editor_get_instrument()->samples[n_cur + 1]);
				if(next->sample.length)
					replay = !gui_ok_cancel_modal(mainwindow, N_("The next sample which is about to be overwriten is not empty!\n"
					                                             "Would you like to overwrite it?"));
				break;
			default:
				return;
			}
		} while(replay);
	}

	if(mode == MODE_STEREO_2)
		if(!(sbuf2 = malloc(recordedlen))) {
			static GtkWidget *dialog = NULL;

			gui_error_dialog(&dialog, N_("Out of memory for sample data."), FALSE);
			free(sbuf);
			return;
		}

    sample_editor_lock_sample();
    st_clean_sample(current_sample, NULL, NULL);
    instr = instrument_editor_get_instrument();
    if(st_instrument_num_samples(instr) == 0)
	st_clean_instrument(instr, samplename);
    st_clean_sample(current_sample, samplename, NULL);
    current_sample->sample.data = sbuf;

	if(mode == MODE_STEREO_2) {

		g_mutex_lock(next->sample.lock);
		st_clean_sample(next, samplename, NULL);
		next->sample.data = sbuf2;
		next->treat_as_8bit = !multiply;
		next->sample.length = recordedlen >> 1;/* Sample size is given in 16-bit words */
		next->volume = 64;
		next->panning = 128;

		xm_freq_note_to_relnote_finetune(rate,
		                                 4 * 12 + 1, // at C-4
		                                 &next->relnote,
		                                 &next->finetune);
	}

	for(r = recordbufs; r; r = r2) {
		guint j;

		r2 = r->next;

#ifdef WORDS_BIGENDIAN
		if(format == ST_MIXER_FORMAT_S16_LE || format == ST_MIXER_FORMAT_U16_LE)
#else
		if(format == ST_MIXER_FORMAT_S16_BE || format == ST_MIXER_FORMAT_U16_BE)
#endif
			byteswap_16_array(r->data, r->length);

		if(stereo) { /* Looks awfully, but this is the only way to handle this plenty of format combination */
			switch(mode) {
			case MODE_STEREO_LEFT:
				switch(format) {
				case ST_MIXER_FORMAT_S16_LE:
				case ST_MIXER_FORMAT_S16_BE:
					for(j = 0; j < r->length >> 1; j += 2, sbuf++)
						*sbuf = ((gint16 *)r->data)[j];
					break;
				case ST_MIXER_FORMAT_U16_LE:
				case ST_MIXER_FORMAT_U16_BE:
					for(j = 0; j < r->length >> 1; j += 2, sbuf++)
						*sbuf = ((gint16 *)r->data)[j] - 32768;
					break;
				case ST_MIXER_FORMAT_S8:
					for(j = 0; j < r->length; j += 2, sbuf++)
						*sbuf = ((gint8 *)r->data)[j] << 8;
					break;
				case ST_MIXER_FORMAT_U8:
					for(j = 0; j < r->length; j += 2, sbuf++)
						*sbuf = (((gint8 *)r->data)[j] << 8) - 32768;
					break;
				default:
					memset(sbuf, 0, r->length);
					sbuf += r->length >> 1; /* Unknown format */
				}
				break;
			case MODE_STEREO_MIX:
				switch(format) {
				case ST_MIXER_FORMAT_S16_LE:
				case ST_MIXER_FORMAT_S16_BE:
					for(j = 0; j < r->length >> 1; j += 2, sbuf++)
						*sbuf = (((gint16 *)r->data)[j] + ((gint16 *)r->data)[j + 1]) >> 1;
					break;
				case ST_MIXER_FORMAT_U16_LE:
				case ST_MIXER_FORMAT_U16_BE:
					for(j = 0; j < r->length >> 1; j += 2, sbuf++)
						*sbuf = ((((gint16 *)r->data)[j] + ((gint16 *)r->data)[j + 1]) >> 1) - 32768;
					break;
				case ST_MIXER_FORMAT_S8:
					for(j = 0; j < r->length; j += 2, sbuf++)
						*sbuf = (((gint8 *)r->data)[j] + ((gint8 *)r->data)[j + 1]) << 7;
					break;
				case ST_MIXER_FORMAT_U8:
					for(j = 0; j < r->length; j += 2, sbuf++)
						*sbuf = ((((gint8 *)r->data)[j] + ((gint8 *)r->data)[j + 1]) << 7) - 32768;
					break;
				default:
					memset(sbuf, 0, r->length);
					sbuf += r->length >> 1; /* Unknown format */
				}
				break;
			case MODE_STEREO_RIGHT:
				switch(format) {
				case ST_MIXER_FORMAT_S16_LE:
				case ST_MIXER_FORMAT_S16_BE:
					for(j = 0; j < r->length >> 1; j += 2, sbuf++)
						*sbuf = ((gint16 *)r->data)[j + 1];
					break;
				case ST_MIXER_FORMAT_U16_LE:
				case ST_MIXER_FORMAT_U16_BE:
					for(j = 0; j < r->length >> 1; j += 2, sbuf++)
						*sbuf = ((gint16 *)r->data)[j + 1] - 32768;
					break;
				case ST_MIXER_FORMAT_S8:
					for(j = 0; j < r->length; j += 2, sbuf++)
						*sbuf = ((gint8 *)r->data)[j + 1] << 8;
					break;
				case ST_MIXER_FORMAT_U8:
					for(j = 0; j < r->length; j += 2, sbuf++)
						*sbuf = (((gint8 *)r->data)[j + 1] << 8) - 32768;
					break;
				default:
					memset(sbuf, 0, r->length);
					sbuf += r->length >> 1; /* Unknown format */
				}
				break;
			case MODE_STEREO_2:
				switch(format) {
				case ST_MIXER_FORMAT_S16_LE:
				case ST_MIXER_FORMAT_S16_BE:
					for(j = 0; j < r->length >> 1; j += 2, sbuf++, sbuf2++) {
						*sbuf = ((gint16 *)r->data)[j];
						*sbuf2 = ((gint16 *)r->data)[j + 1];
					}
					break;
				case ST_MIXER_FORMAT_U16_LE:
				case ST_MIXER_FORMAT_U16_BE:
					for(j = 0; j < r->length >> 1; j += 2, sbuf++, sbuf2++) {
						*sbuf = ((gint16 *)r->data)[j] - 32768;
						*sbuf2 = ((gint16 *)r->data)[j + 1] - 32768;
					}
					break;
				case ST_MIXER_FORMAT_S8:
					for(j = 0; j < r->length; j += 2, sbuf++, sbuf2++) {
						*sbuf = ((gint8 *)r->data)[j] << 8;
						*sbuf2 = ((gint8 *)r->data)[j + 1] << 8;
					}
					break;
				case ST_MIXER_FORMAT_U8:
					for(j = 0; j < r->length; j += 2, sbuf++, sbuf2++) {
						*sbuf = (((gint8 *)r->data)[j] << 8) - 32768;
						*sbuf2 = (((gint8 *)r->data)[j + 1] << 8) - 32768;
					}
					break;
				default:
					memset(sbuf, 0, r->length);  /* Unknown format */
					memset(sbuf2, 0, r->length);
					sbuf += r->length >> 1;
					sbuf2 += r->length >> 1;
				}
			}
		} else {
			switch(format) {
			case ST_MIXER_FORMAT_S16_LE:
			case ST_MIXER_FORMAT_S16_BE:
				memcpy(sbuf, r->data, r->length);
				sbuf += r->length >> 1;
				break;
			case ST_MIXER_FORMAT_U16_LE:
			case ST_MIXER_FORMAT_U16_BE:
				for(j = 0; j < r->length >> 1; j++, sbuf++)
					*sbuf = ((gint16 *)r->data)[j] - 32768;
				break;
			case ST_MIXER_FORMAT_S8:
				for(j = 0; j < r->length; j++, sbuf++)
					*sbuf = ((gint8 *)r->data)[j] << 8;
				break;
			case ST_MIXER_FORMAT_U8:
				for(j = 0; j < r->length; j++, sbuf++)
					*sbuf = (((gint8 *)r->data)[j] << 8) - 32768;
				break;
			default:
				memset(sbuf, 0, r->length);  /* Unknown format */
				sbuf += r->length >> 1;
			}
		}
	}

    if(recordedlen > mixer->max_sample_length) {
	static GtkWidget *dialog = NULL;
	gui_warning_dialog(&dialog, N_("Recorded sample is too long for current mixer module. Using it anyway."), FALSE);
    }

    current_sample->sample.length = recordedlen >> 1;/* Sample size is given in 16-bit words */
    current_sample->volume = 64;
    current_sample->panning = 128;
    current_sample->treat_as_8bit = !multiply;

    xm_freq_note_to_relnote_finetune((double)rate,
				     4 * 12 + 1, // at C-4
				     &current_sample->relnote,
				     &current_sample->finetune);

    sample_editor_unlock_sample();
	if(mode == MODE_STEREO_2) {
		if(gui_playing_mode) {
			mixer->updatesample(&next->sample);
		}
		g_mutex_unlock(next->sample.lock);
	}

    instrument_editor_update(TRUE);
    sample_editor_update();
    xm_set_modified(1);
}

/* ==================== VOLUME RAMPING DIALOG =================== */

static void
sample_editor_lrvol (GtkWidget *widget,
                     gpointer data)
{
    int mode = GPOINTER_TO_INT(data);

    switch(mode)
    {
    case 0: case 2:
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(sample_editor_volramp_spin_w[mode/2]), 50);
        break;
    case 4: case 8:
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(sample_editor_volramp_spin_w[mode/8]), 200);
        break;
    }
}


static void
sample_editor_open_volume_ramp_dialog (void)
{
	static GtkWidget *volrampwindow = NULL;

    GtkWidget *mainbox, *box1, *thing;

    if(volrampwindow != NULL) {
	gtk_window_present(GTK_WINDOW(volrampwindow));
	return;
    }
    
    volrampwindow = gtk_dialog_new_with_buttons(_("Volume Ramping"), GTK_WINDOW(mainwindow), 0,
                                                _("Normalize"), 1,
                                                GTK_STOCK_EXECUTE, 2,
                                                GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
    g_signal_connect(volrampwindow, "delete_event",
			G_CALLBACK(gui_delete_noop), NULL);
    g_signal_connect(volrampwindow, "response",
			G_CALLBACK(sample_editor_perform_ramp), NULL);

    gui_dialog_adjust(volrampwindow, 2);
    mainbox = gtk_dialog_get_content_area(GTK_DIALOG(volrampwindow));
    gtk_box_set_spacing(GTK_BOX(mainbox), 2);

    thing = gtk_label_new(_("Perform linear volume fade on Selection"));
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);

    thing = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);

    box1 = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(mainbox), box1, FALSE, TRUE, 0);

    gui_put_labelled_spin_button(box1, _("Left [%]:"), -1000, 1000, &sample_editor_volramp_spin_w[0], NULL, NULL, FALSE);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(sample_editor_volramp_spin_w[0]), sample_editor_volramp_last_values[0]);

    thing = gtk_button_new_with_label(_("H"));
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
    gtk_widget_set_tooltip_text(thing, _("Half"));
    g_signal_connect(thing, "clicked",
                       G_CALLBACK(sample_editor_lrvol), (gpointer)0);

    thing = gtk_button_new_with_label(_("D"));
    gtk_widget_set_tooltip_text(thing, _("Double"));
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "clicked",
                       G_CALLBACK(sample_editor_lrvol), (gpointer)4);

    add_empty_hbox(box1);

    gui_put_labelled_spin_button(box1, _("Right [%]:"), -1000, 1000, &sample_editor_volramp_spin_w[1], NULL, NULL, FALSE);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(sample_editor_volramp_spin_w[1]), sample_editor_volramp_last_values[1]);

    thing = gtk_button_new_with_label(_("H"));
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
    gtk_widget_set_tooltip_text(thing, _("Half"));
    g_signal_connect(thing, "clicked",
                       G_CALLBACK(sample_editor_lrvol), (gpointer)2);

    thing = gtk_button_new_with_label(_("D"));
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
    gtk_widget_set_tooltip_text(thing, _("Double"));
    g_signal_connect(thing, "clicked",
                       G_CALLBACK(sample_editor_lrvol), (gpointer)8);

    thing = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 4);
    
    gtk_widget_show_all(volrampwindow);
}

static void
sample_editor_close_volume_ramp_dialog (GtkWidget *w)
{
    sample_editor_volramp_last_values[0] = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(sample_editor_volramp_spin_w[0]));
    sample_editor_volramp_last_values[1] = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(sample_editor_volramp_spin_w[1]));

    gtk_widget_hide(w);
}

static void
sample_editor_perform_ramp (GtkWidget *w, gint action,
			    gpointer data)
{
    double left, right;
    const int ss = sampledisplay->sel_start, se = sampledisplay->sel_end;
    gint i, m, q;
    gint16 *p;
 
    if(!current_sample || ss == -1) {
	sample_editor_close_volume_ramp_dialog(w);
	return;
    }

	switch(action) {
	case 1:
		// Find maximum amplitude
		p = current_sample->sample.data;
		p += ss;
		for(i = 0, m = 0; i < se - ss; i++) {
			q = *p++;
			q = ABS(q);
			if(q > m)
			m = q;
		}
		left = right = (double)0x7fff / m;
		break;
	case 2:
		left = gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(sample_editor_volramp_spin_w[0])) / 100.0;
		right = gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(sample_editor_volramp_spin_w[1])) / 100.0;
		break;
	default:
		sample_editor_close_volume_ramp_dialog(w);
		return;
	}

    // Now perform the actual operation
    sample_editor_lock_sample();

    p = current_sample->sample.data;
    p += ss;
    for(i = 0; i < se - ss; i++) {
	double q = *p;
	q *= left + i * (right - left) / (se - ss);
	*p++ = CLAMP((int)q, -32768, +32767);
    }

    sample_editor_unlock_sample();
    xm_set_modified(1);
    sample_editor_update();
    sample_display_set_selection(sampledisplay, ss, se);
}

/* =================== TRIM AND CROP FUNCTIONS ================== */
static void
sample_editor_trim_dialog (void)
{
    static GtkAdjustment *adj;
    static GtkWidget *trimdialog = NULL, *trimbeg, *trimend;
    GtkWidget *mainbox, *thing, *box;
    gint response;

    if(!trimdialog) {
	trimdialog = gtk_dialog_new_with_buttons(_("Trim parameters"), GTK_WINDOW(mainwindow), GTK_DIALOG_MODAL,
	                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
	gui_dialog_adjust(trimdialog, GTK_RESPONSE_OK);
	mainbox = gtk_dialog_get_content_area(GTK_DIALOG(trimdialog));
	gtk_box_set_spacing(GTK_BOX(mainbox), 2);

	trimbeg = thing = gtk_check_button_new_with_label(_("Trim at the beginning"));
	gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), TRUE);

	trimend = thing = gtk_check_button_new_with_label(_("Trim at the end"));
	gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), TRUE);

	box = gtk_hbox_new(FALSE, 0);

	thing = gtk_label_new(_("Threshold (dB)"));
	gtk_box_pack_start(GTK_BOX(box), thing, FALSE, FALSE, 0);
	thing = gtk_spin_button_new(adj = GTK_ADJUSTMENT(gtk_adjustment_new(-50.0, -80.0, -20.0, 1.0, 5.0, 0.0)), 0, 0);
	gtk_box_pack_end(GTK_BOX(box), thing, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(mainbox), box, FALSE, FALSE, 0);

	thing = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, FALSE, 4);

	gtk_widget_show_all(trimdialog);
    } else
	gtk_window_present(GTK_WINDOW(trimdialog));

    response = gtk_dialog_run(GTK_DIALOG(trimdialog));
    gtk_widget_hide(trimdialog);

    if(response == GTK_RESPONSE_OK)
	sample_editor_trim(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(trimbeg)),
	                   gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(trimend)), adj->value);
}

static void 
sample_editor_trim(gboolean trbeg, gboolean trend, gfloat thrshld)
{
    int start = sampledisplay->sel_start, end = sampledisplay->sel_end;
    int i, c, ofs;
    int amp = 0, val, bval = 0, maxamp, ground;
    int on, off;
    double avg;
    int reselect = 1; 
    gint16 *data;

    if(current_sample == NULL) return;
    if(!trbeg && !trend) return;
   
    /* if there's no selection, we operate on the entire sample */
    if(start == -1) {
        start = 0;
        end = current_sample->sample.length;
        reselect = 0;
    }
    
    data = current_sample->sample.data;
    /* Finding the maximum amplitude */
    for(i = 0, maxamp = 0; i < end - start; i++) {
	val = *(data + i);
	val = ABS(val);
	if(val > maxamp)
	    maxamp = val;
    }
  
    if (maxamp == 0) return;
   
    ground = rint((gfloat)maxamp * pow(10.0, thrshld/20));
    
    /* Computing the beginning average level until we reach the ground level */
    for(c = 0, ofs = start, amp = 0, avg = 0; ofs < end && amp < ground ; ofs++) {
	val = *(data + ofs);
	if (ofs == start) {
    	    bval = - val;
    	    amp = ABS(val);
	}
	if ((val < 0 && bval >= 0) || (val >= 0 && bval < 0)) {
    	    avg += amp;
    	    c++;
    	    amp = 0;
	} else {   
    	    if (ABS(val) > amp) amp = ABS(val);
	}   
	    bval = val;
    }
    avg = avg / c;
    
    /* Locating when sounds turns on.
       That is : when we *last* got higher than the average level. */
    for(amp = maxamp; ofs > start && amp > avg; --ofs) {
        //fprintf(stderr,"reverse\n");
        amp = 0;
        for(val = 1; ofs > start && val > 0; --ofs) {
            val = *(data + ofs);
            if (val > amp) amp = val;
        }
        for(; ofs > start && val <= 0; --ofs) {
            val = *(data + ofs);
            if (-val > amp) amp = -val;
        }
    }
    on = ofs;
   
    /* Computing the ending average level until we reach the ground level */
    for(ofs = end - 1, avg = 0, amp = 0, c = 0; ofs > on && amp < ground ; ofs--) {
        val = *(data + ofs);
        if (ofs == end -1) {
            bval = -val;
            amp = ABS(val);
        }
        if((val < 0 && bval >= 0) || (val >= 0 && bval < 0)) {
            avg += amp;
            c++;
            amp = 0;
        } else {   
            if (ABS(val) > amp) amp = ABS(val);
        }   
        bval = val;
    }
    avg = avg / c;
    
    /* Locating when sounds turns off.
       That is : when we *last* got higher than the average level. */
    for (amp = maxamp; ofs < end && amp > avg; ++ofs) {
        amp = 0;
        for (val = 1; ofs < end && val > 0; ++ofs) {
            val = *(data + ofs);
            if (val > amp) amp = val;
        }
        for (;ofs < end && val <= 0; ++ofs) {
            val = *(data + ofs);
            if (-val > amp) amp = -val;
        }
    }
    off = ofs;
    
    // Wiping blanks out
    if (on < start) on = start;
    if (off > end) off = end;
    sample_editor_lock_sample();
    if (trbeg) {
	sample_editor_delete(current_sample, start, on);
	off -= on - start;
	end -= on - start;
    }
    if (trend)
	sample_editor_delete(current_sample, off, end);
    st_sample_fix_loop(current_sample);
    sample_editor_unlock_sample();
    
    sample_editor_set_sample(current_sample);
    xm_set_modified(1);
    
    if (reselect == 1 && off > on) 
	sample_display_set_selection(sampledisplay, start, start + off - on);
}

static void 
sample_editor_crop()
{
    int start = sampledisplay->sel_start, end = sampledisplay->sel_end;

    if(current_sample == NULL || start == -1) 
    return;

    int l = current_sample->sample.length;
    
    sample_editor_lock_sample();
    sample_editor_delete(current_sample, 0, start);
    sample_editor_delete(current_sample, end - start, l - start);
    sample_editor_unlock_sample();
    
    sample_editor_set_sample(current_sample);
    xm_set_modified(1);
    
}

/* deletes the portion of *sample data from start to end-1 */
void
sample_editor_delete (STSample *sample,int start, int end)
{
    int newlen;
    gint16 *newdata;
    
    if(sample == NULL || start == -1 || start >= end)
	return;
    
    newlen = sample->sample.length - end + start;

    newdata = malloc(newlen * 2);
    if(!newdata)
	return;

    memcpy(newdata, sample->sample.data, start * 2);
    memcpy(newdata + start, sample->sample.data + end, (sample->sample.length - end) * 2);

    free(sample->sample.data);

    sample->sample.data = newdata;
    sample->sample.length = newlen;

    /* Move loop start and end along with splice */
    if(sample->sample.loopstart > start &&
       sample->sample.loopend < end) {
	/* loop was wholly within selection -- remove it */
	sample->sample.looptype = ST_MIXER_SAMPLE_LOOPTYPE_NONE;
	sample->sample.loopstart = 0;
	sample->sample.loopend = 1;
    } else {
	if(sample->sample.loopstart > end) {
	    /* loopstart was after selection */
	    sample->sample.loopstart -= (end-start);
	} else if(sample->sample.loopstart > start) {
	    /* loopstart was within selection */
	    sample->sample.loopstart = start;
	}
	
	if(sample->sample.loopend > end) {
	    /* loopend was after selection */
	    sample->sample.loopend -= (end-start);
	} else if(sample->sample.loopend > start) {
	    /* loopend was within selection */
	    sample->sample.loopend = start;
	}
    }

    st_sample_fix_loop(sample);

}
