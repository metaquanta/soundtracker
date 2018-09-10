
/*
 * The Real SoundTracker - user tips
 *
 * Copyright (C) 1997-2000 by the GIMP authors
 * Copyright (C) 1999-2002 by Michael Krause
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

#include "config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "tips-dialog.h"
#include "preferences.h"
#include "gui.h"
#include "gui-subs.h"

static int  tips_show_next (GtkWidget *widget, gpointer data);
static void tips_toggle_update (GtkWidget *widget, gpointer data);

static GtkWidget *tips_label;

gint tips_dialog_last_tip;
gboolean tips_dialog_show_tips;

#define TIPS_COUNT (sizeof(tips_array)/sizeof(tips_array[0]))

static char const * const tips_array[] = {
    N_("Welcome to SoundTracker!\n"
       "\n"
       "If you are new to this type of program, you will want to get hold of\n"
       "some XM or MOD files first and play with them."),

    N_("You can make SoundTracker's edit mode more responsive to keyboard\n"
       "input by decreasing the mixing buffer size of the \"Editing\" object in\n"
       "the Audio Configuration."),

    N_("You can adjust the loop points in the sample editor by holding Shift\n"
       "and using the left and right mousebuttons.\n"),

    N_("If you want to know more about tracking, and how the various commands\n"
       "work, have a look at http://www.united-trackers.org/"),

    N_("You can assign samples of an instrument to the individual keys by\n"
       "activating its sample and then clicking on the keyboard in the\n"
       "instrument editor page."),

    N_("Is your cursor trapped in a number entry field?\n"
       "Just press Return or Tab to free yourself!")
};

void
tips_box_populate(GtkWidget *vbox, gboolean has_separator)
{
    GtkWidget *hbox1;
    GtkWidget *hbox2;
    GtkWidget *bbox;
    GtkWidget *vbox_bbox2;
    GtkWidget *bbox2;
    GtkWidget *vbox_check;
    GtkWidget *thing;

    hbox1 = gtk_hbox_new (FALSE, 5);
    gtk_container_set_border_width (GTK_CONTAINER (hbox1), 10);
    gtk_box_pack_start (GTK_BOX (vbox), hbox1, FALSE, TRUE, 0);
    gtk_widget_show(hbox1);

	if(has_separator) {
		thing = gtk_hseparator_new();
		gtk_box_pack_end (GTK_BOX (vbox), thing, FALSE, FALSE, 4);
		gtk_widget_show(thing);
	}

    hbox2 = gtk_hbox_new (FALSE, 5);
    gtk_container_set_border_width (GTK_CONTAINER (hbox2), 4);
    gtk_box_pack_end (GTK_BOX (vbox), hbox2, FALSE, TRUE, 0);
    gtk_widget_show(hbox2);

    bbox = gtk_hbutton_box_new ();
    gtk_box_pack_end (GTK_BOX (hbox2), bbox, FALSE, FALSE, 0);
    gtk_widget_show(bbox);

    vbox_bbox2 = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_end (GTK_BOX (hbox2), vbox_bbox2, FALSE, FALSE, 15);
    gtk_widget_show(vbox_bbox2);

    bbox2 = gtk_hbox_new (TRUE, 5);
    gtk_box_pack_end (GTK_BOX (vbox_bbox2), bbox2, TRUE, FALSE, 0);
    gtk_widget_show(bbox2);

    tips_label = gtk_label_new (_(tips_array[tips_dialog_last_tip]));
    gtk_label_set_justify (GTK_LABEL (tips_label), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start (GTK_BOX (hbox1), tips_label, TRUE, TRUE, 3);
    gtk_widget_show(tips_label);

    thing = gtk_button_new_with_label ((_("Previous Tip")));
    g_signal_connect(thing, "clicked",
			G_CALLBACK(tips_show_next),
			(gpointer) "prev");
    gtk_container_add (GTK_CONTAINER (bbox2), thing);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label ((_("Next Tip")));
    g_signal_connect(thing, "clicked",
			G_CALLBACK(tips_show_next),
			(gpointer) "next");
    gtk_container_add (GTK_CONTAINER (bbox2), thing);
    gtk_widget_show(thing);

    vbox_check = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox2), vbox_check, FALSE, TRUE, 0);
    gtk_widget_show(vbox_check);

    thing = gtk_check_button_new_with_label ((_("Show tip next time")));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (thing),
				  tips_dialog_show_tips);
    g_signal_connect(thing, "toggled",
			G_CALLBACK(tips_toggle_update),
			(gpointer) &tips_dialog_show_tips);
    gtk_box_pack_start (GTK_BOX (vbox_check), thing, TRUE, FALSE, 0);
    gtk_widget_show(thing);
}

void
tips_dialog_open ()
{
	static GtkWidget *tips_dialog = NULL;

	if(!tips_dialog) {
		tips_dialog = gtk_dialog_new_with_buttons(_("SoundTracker Tip of the day"), GTK_WINDOW(mainwindow), 0,
		                                          GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
		gui_dialog_adjust(tips_dialog, GTK_RESPONSE_CLOSE);
		g_signal_connect(tips_dialog, "response",
		                 G_CALLBACK(gtk_widget_hide), NULL);
		g_signal_connect(tips_dialog, "delete_event",
		                 G_CALLBACK(gui_delete_noop), NULL);

		tips_box_populate(gtk_dialog_get_content_area(GTK_DIALOG(tips_dialog)), TRUE);
		gtk_widget_show(tips_dialog);
	} else
		gtk_window_present(GTK_WINDOW(tips_dialog));
}

static int
tips_show_next (GtkWidget *widget,
		gpointer  data)
{
  if (!strcmp ((char *)data, "prev"))
    {
      tips_dialog_last_tip--;
      if (tips_dialog_last_tip < 0)
	tips_dialog_last_tip = TIPS_COUNT - 1;
    }
  else
    {
      tips_dialog_last_tip++;
      if (tips_dialog_last_tip >= TIPS_COUNT)
	tips_dialog_last_tip = 0;
    }
  gtk_label_set_text(GTK_LABEL(tips_label), _(tips_array[tips_dialog_last_tip]));
  return FALSE;
}

static void
tips_toggle_update (GtkWidget *widget,
		    gpointer   data)
{
  int *toggle_val;

  toggle_val = (int *) data;

  if (GTK_TOGGLE_BUTTON (widget)->active)
    *toggle_val = TRUE;
  else
    *toggle_val = FALSE;
}

void
tips_dialog_load_settings (void)
{
	tips_dialog_show_tips = prefs_get_bool("tips", "show-tips", TRUE);
	tips_dialog_last_tip = prefs_get_int("tips", "last-tip", 0);

    if(tips_dialog_last_tip >= TIPS_COUNT || tips_dialog_last_tip < 0) {
	tips_dialog_last_tip = 0;
    }
}

void
tips_dialog_save_settings (void)
{
    tips_dialog_last_tip++;
    prefs_put_bool("tips", "show-tips", tips_dialog_show_tips);
    prefs_put_int("tips", "last-tip", tips_dialog_last_tip);
}
