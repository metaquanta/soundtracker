/*
 * The Real SoundTracker - JACK (output) driver
 *
 * Copyright (C) 2003 Anthony Van Groningen
 * Copyright (C) 2014 Yury Aliaev
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

/*
 * TODO: 
 * need better declicking?
 * endianness?
 * slave transport was removed
 * should master transport always work? even for pattern? Can we determine this info anyway?
 * general thread safety: d->state should be wrapped in state_mx locks as a matter of principle
 *                        In practice this is needed only when we are waiting on state_cv.
 * XRUN counter
 */

#include <config.h>

#if DRIVER_JACK

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <jack/jack.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>

#include "driver-inout.h"
#include "errors.h"
#include "gui-subs.h"
#include "mixer.h"
#include "preferences.h"

/* suggested by Erik de Castro Lopo */
#define INT16_MAX_float 32767.0f
static inline float
sample_convert_s16_to_float(gint16 inval)
{
    return inval * (1.0 / (INT16_MAX_float + 1));
}

typedef jack_default_audio_sample_t audio_t;
typedef jack_nframes_t nframes_t;

typedef enum {
    JackDriverStateIsRolling,
    JackDriverStateIsDeclicking,
    JackDriverStateIsStopping, /* declicking is done, we want to transition to be stopped */
    JackDriverStateIsStopped
} jack_driver_state;

typedef enum {
    JackDriverTransportIsInternal = 0,
    JackDriverTransportIsMaster = 1,
    //	JackDriverTransportIsSlave = 2
} jack_driver_transport;

typedef struct jack_driver {
    /* prefs stuff */
    GtkWidget* configwidget;
    GtkWidget* client_name_label;
    GtkWidget* status_label;
    GtkWidget* transport_check;
    guint transport_check_id, autostart_check_id;
    GtkWidget* declick_check;
    GtkWidget* autostart_check;
    gboolean do_declick;
    gboolean autostart;

    /* jack + audio stuff */
    nframes_t buffer_size;
    unsigned long sample_rate;
    char* client_name;
    jack_client_t* client;
    jack_port_t *left, *right;
    void* mix; // passed to audio_mix, big enough for stereo 16 bit nframes = nframes*4
    STMixerFormat mf;

    /* internal state stuff */
    jack_driver_state state;
    nframes_t position; // frames since ST called open()
    pthread_mutex_t* process_mx; // try to lock this around process_core()
    pthread_cond_t* state_cv; // trigger after declicking if we have the lock
    gboolean locked; // set true if we get it. then we can trigger any CV's during process_core()
    gboolean is_active; // jack seems to be running fine
    jack_driver_transport transport; // who do we serve?

} jack_driver;

static inline float
jack_driver_declick_coeff(nframes_t total, nframes_t current)
{
    /* total = # of frames it takes from 1.0 to 0.0
	   current = total ... 0 */
    return (float)current / (float)total;
}

static void
jack_driver_process_core(nframes_t nframes, jack_driver* d)
{
    audio_t *lbuf, *rbuf;
    gint16* mix = d->mix;
    nframes_t cnt = nframes;
    float gain = 1.0f;
    jack_driver_state state = d->state;

    lbuf = (audio_t*)jack_port_get_buffer(d->left, nframes);
    rbuf = (audio_t*)jack_port_get_buffer(d->right, nframes);

    switch (state) {

    case JackDriverStateIsRolling:
        audio_mix(mix, nframes, d->sample_rate, d->mf);
        d->position += nframes;
        while (cnt--) {
            *(lbuf++) = sample_convert_s16_to_float(*mix++);
            *(rbuf++) = sample_convert_s16_to_float(*mix++);
        }
        break;

    case JackDriverStateIsDeclicking:
        audio_mix(mix, nframes, d->sample_rate, d->mf);
        d->position += nframes;
        while (cnt--) {
            gain = jack_driver_declick_coeff(nframes, cnt);
            *(lbuf++) = gain * sample_convert_s16_to_float(*mix++);
            *(rbuf++) = gain * sample_convert_s16_to_float(*mix++);
        }
        /* safe because ST shouldn't call open() with pending release() */
        d->state = JackDriverStateIsStopping;
        break;

    case JackDriverStateIsStopping:
        /* if locked, then trigger change of state, otherwise keep silent */
        if (d->locked) {
            d->state = JackDriverStateIsStopped;
            pthread_cond_signal(d->state_cv);
        }
        /* fall down */

    case JackDriverStateIsStopped:
    default: //!!! Nullify buffers one time only
        memset(lbuf, 0, nframes * sizeof(audio_t));
        memset(rbuf, 0, nframes * sizeof(audio_t));
    }
}

static int
jack_driver_process_wrapper(nframes_t nframes, void* arg)
{
    jack_driver* d = arg;
    if (pthread_mutex_trylock(d->process_mx) == 0) {
        d->locked = TRUE;
        jack_driver_process_core(nframes, d);
        pthread_mutex_unlock(d->process_mx);
    } else {
        d->locked = FALSE;
        jack_driver_process_core(nframes, d);
    }
    return 0;
}

static void
jack_driver_prefs_transport_callback(void* a, jack_driver* d)
{ //!!! Revise
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(d->transport_check))) {
        if (d->is_active) {
            d->transport = JackDriverTransportIsMaster;
            return;
        } else {
            // reset
            // gtk_signal_handler_block (GTK_OBJECT(d->transport_check), d->transport_check_id);
            // gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(d->transport_check), FALSE);
            // gtk_signal_handler_unblock (GTK_OBJECT(d->transport_check), d->transport_check_id);
            return;
        }
    } else {
        d->transport = JackDriverTransportIsInternal;
    }
}

static void
jack_driver_prefs_declick_callback(GtkToggleButton* widget, jack_driver* d)
{
    d->do_declick = gtk_toggle_button_get_active(widget);
}

static int
jack_driver_sample_rate_callback(nframes_t nframes, void* arg)
{
    jack_driver* d = arg;
    d->sample_rate = nframes;
    return 0;
}

static int
jack_driver_buffer_size_callback(nframes_t nframes, void* arg)
{
    jack_driver* d = arg;
    if (nframes > d->buffer_size) {
        d->buffer_size = nframes;
        d->mix = realloc(d->mix, d->buffer_size * 4);
    }
    return 0;
}

static void
jack_driver_prefs_update(jack_driver* d)
{
    char status_buf[128];
    if (d->is_active) {
        g_sprintf(status_buf, _("Running at %d Hz with %d frames"), (int)d->sample_rate, (int)d->buffer_size);
        gtk_label_set_text(GTK_LABEL(d->client_name_label), d->client_name);
        gtk_label_set_text(GTK_LABEL(d->status_label), status_buf);
    } else {
        g_sprintf(status_buf, _("Jack server not running?"));
        gtk_label_set_text(GTK_LABEL(d->status_label), "");
        gtk_label_set_text(GTK_LABEL(d->client_name_label), status_buf);
    }
}

static void
jack_driver_server_has_shutdown(void* arg)
{
    jack_driver* d = arg;
    d->is_active = FALSE;
    jack_driver_prefs_update(d);
}

static void
jack_driver_activate(void* dp)
{
    jack_driver* d = dp;
    jack_status_t status;
    jack_options_t options = d->autostart ? JackNullOption : JackNoStartServer;
    if (!d->is_active) {
        /* Jack-dependent setup only */
        d->client = jack_client_open("soundtracker", options, &status, NULL);
        if (d->client == NULL) {
            /* we've failed here, but we should have a working dummy driver
			   because ST will segfault on NULL return */
            return;
        }

        d->client_name = jack_get_client_name(d->client);
        d->sample_rate = jack_get_sample_rate(d->client);
        d->buffer_size = jack_get_buffer_size(d->client);
        if (!d->mix)
            d->mix = calloc(1, d->buffer_size * 4);

        d->left = jack_port_register(d->client, "out_1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
        d->right = jack_port_register(d->client, "out_2", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

        jack_set_process_callback(d->client, jack_driver_process_wrapper, d);
        jack_set_sample_rate_callback(d->client, jack_driver_sample_rate_callback, d);
        jack_set_buffer_size_callback(d->client, jack_driver_buffer_size_callback, d);
        jack_on_shutdown(d->client, jack_driver_server_has_shutdown, d);

        if (jack_activate(d->client)) {
            static GtkWidget* dialog = NULL;

            d->is_active = FALSE;
            gui_error_dialog(&dialog, N_("Jack driver activation failed."), FALSE);
        } else {
            d->is_active = TRUE;
        }
    }
    jack_driver_prefs_update(d);
}

static void
jack_driver_prefs_autostart_callback(GtkToggleButton* widget, jack_driver* d)
{
    d->autostart = gtk_toggle_button_get_active(widget);
    if (d->autostart && (!d->is_active))
        jack_driver_activate(d);
}

static void
jack_driver_make_config_widgets(jack_driver* d)
{
    GtkWidget *thing, *mainbox, *hbox;

    d->configwidget = mainbox = gtk_vbox_new(FALSE, 2);

    d->client_name_label = thing = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    d->status_label = thing = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label(_("Update"));
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    g_signal_connect_swapped(thing, "clicked", G_CALLBACK(jack_driver_activate), d);
    gtk_widget_show(thing);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(mainbox), hbox, FALSE, TRUE, 0);
    gtk_widget_show(hbox);

    thing = d->autostart_check = gtk_check_button_new_with_label(_("Jack autostart"));
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    d->autostart_check_id = g_signal_connect(thing, "clicked", G_CALLBACK(jack_driver_prefs_autostart_callback), d);
    gtk_widget_show(thing);

    thing = d->transport_check = gtk_check_button_new_with_label(_("transport master"));
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    d->transport_check_id = g_signal_connect(thing, "clicked", G_CALLBACK(jack_driver_prefs_transport_callback), d);
    gtk_widget_show(thing);

    thing = d->declick_check = gtk_check_button_new_with_label(_("declick"));
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "clicked", G_CALLBACK(jack_driver_prefs_declick_callback), d);
    gtk_widget_show(thing);
}

static void
jack_driver_error(const char* msg)
{
    char buf[256];
    snprintf(buf, 256, _("Jack driver error:\n%s"), msg);
    error_error(buf);
}

static void*
jack_driver_new(void)
{
    jack_driver* d = g_new(jack_driver, 1);

    d->mix = NULL;
    d->mf = ST_MIXER_FORMAT_S16_LE | ST_MIXER_FORMAT_STEREO;
    // d->mf = ST_MIXER_FORMAT_S16_BE | ST_MIXER_FORMAT_STEREO;
    d->state = JackDriverStateIsStopped;
    d->transport = JackDriverTransportIsInternal;
    d->position = 0;
    d->is_active = FALSE;
    d->process_mx = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    d->state_cv = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
    d->do_declick = TRUE;
    pthread_mutex_init(d->process_mx, NULL);
    pthread_cond_init(d->state_cv, NULL);
    jack_driver_make_config_widgets(d);

    jack_set_error_function(jack_driver_error);

    return d;
}

static gboolean
jack_driver_open(void* dp)
{
    jack_driver* d = dp;
    if (!d->is_active) {
        error_warning(_("Jack server is not running or some error occured."));
        return FALSE;
    }
    d->position = 0;
    d->state = JackDriverStateIsRolling;
    return TRUE;
}

static void
jack_driver_release(void* dp)
{
    jack_driver* d = dp;

    pthread_mutex_lock(d->process_mx);
    if (d->do_declick) {
        d->state = JackDriverStateIsDeclicking;
    } else {
        d->state = JackDriverStateIsStopping;
    }
    pthread_cond_wait(d->state_cv, d->process_mx);
    /* at this point process() has set state to stopped */
    pthread_mutex_unlock(d->process_mx);
}

static void
jack_driver_deactivate(void* dp)
{
    jack_driver* d = dp;
    if (d->is_active) {
        d->is_active = FALSE;
        jack_client_close(d->client);
        jack_driver_prefs_update(d);
    }
}

static void
jack_driver_destroy(void* dp)
{
    jack_driver* d = dp;
    gtk_widget_destroy(d->configwidget);
    if (d->mix != NULL) {
        free(d->mix);
        d->mix = NULL;
    }
    pthread_mutex_destroy(d->process_mx);
    pthread_cond_destroy(d->state_cv);
    g_free(d);
}

static GtkWidget*
jack_driver_getwidget(void* dp)
{
    jack_driver* d = dp;
    jack_driver_prefs_update(d);
    return d->configwidget;
}

//!!! Transport master?
static gboolean
jack_driver_loadsettings(void* dp, const gchar* f)
{
    jack_driver* d = dp;
    // prefs_get_string (f, "jack_client_name", d->client_name);
    d->do_declick = prefs_get_bool(f, "jack-declick", TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(d->declick_check), d->do_declick);
    d->autostart = prefs_get_bool(f, "jack-autostart", FALSE);
    /* To prevent Jack server preliminary starting */
    g_signal_handler_block(G_OBJECT(d->autostart_check), d->autostart_check_id);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(d->autostart_check), d->autostart);
    g_signal_handler_unblock(G_OBJECT(d->autostart_check), d->autostart_check_id);

    return TRUE;
}

static gboolean
jack_driver_savesettings(void* dp, const gchar* f)
{
    jack_driver* d = dp;
    //	prefs_put_string (f, "jack-client_name", d->client_name);
    prefs_put_bool(f, "jack-declick", d->do_declick);
    prefs_put_bool(f, "jack-autostart", d->autostart);
    return TRUE;
}

static double
jack_driver_get_play_time(void* dp)
{
    jack_driver* const d = dp;
    return (double)d->position / (double)d->sample_rate;
}

static inline int
jack_driver_get_play_rate(void* d)
{
    jack_driver* const dp = d;
    return (int)dp->sample_rate;
}

st_driver driver_out_jack = {
    "JACK Output",
    jack_driver_new, // create new instance of this driver class
    jack_driver_destroy, // destroy instance of this driver class
    jack_driver_open, // open the driver
    jack_driver_release, // close the driver, release audio
    jack_driver_getwidget, // get pointer to configuration widget
    jack_driver_loadsettings, // load configuration from provided preferences section
    jack_driver_savesettings, // save configuration to specified preferences section
    jack_driver_activate, // run client and optionally the server
    jack_driver_deactivate, // close the client
    jack_driver_get_play_time, // get time offset since first sound output
    jack_driver_get_play_rate
};

#endif /* DRIVER_JACK */
