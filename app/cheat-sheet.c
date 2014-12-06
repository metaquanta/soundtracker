
/*
 * The Real SoundTracker - XM effects cheat sheet
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

#ifdef USE_GTKHTML
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gtkhtml/gtkhtml.h>

#include "gui.h"
#include "gui-subs.h"

static gboolean
cheat_sheet_close_requested (GtkWidget *w)
{
    gtk_widget_hide(w);
    return TRUE;
}
#endif

void
cheat_sheet_dialog (void)
{
#ifdef USE_GTKHTML
	GtkWidget *mainbox, *scrolled_window, *thing, *hbox, *text;
	GFile *file;
	GError *error = NULL;
	gchar *contents, *path;
	GtkHTMLStream *stream;
	gsize size;
	GtkHTMLStreamStatus status = GTK_HTML_STREAM_OK;
	guint i = 0;
	const gchar* const *linguas;

	static GtkWidget *cheat_sheet_window = NULL;

	if(cheat_sheet_window != NULL) {
		gtk_window_present(GTK_WINDOW(cheat_sheet_window));
		return;
	}

	linguas = g_get_language_names();
	while(linguas[i]) {
		path = g_strdup_printf(PREFIX"/share/"PACKAGE"/cheat-sheet.%s.html", linguas[i]);
		if(g_file_test(path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
			file = g_file_new_for_path(path);
			break;
		}

		i++;
		g_free(path);
	}

	if(!linguas[i]) {
		static GtkWidget *dialog = NULL;

		gui_error_dialog(&dialog, N_("No Cheat Sheet pages are found!"), FALSE);
		return;
	}

	if(!g_file_load_contents(file, NULL, &contents, &size, NULL, &error)) {
		gchar *mess = g_strdup_printf(_("Cheat sheet file %s cannot be loaded.\n%s"), path, error->message);
		static GtkWidget *dialog = NULL;

		gui_error_dialog(&dialog, mess, TRUE);

		g_free(mess);
		g_error_free(error);
		g_object_unref(file);
		g_free(path);
		return;
	}
	g_object_unref(file);
	g_free(path);

    cheat_sheet_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(cheat_sheet_window), _("XM Effects Cheat Sheet"));
    gtk_window_set_transient_for(GTK_WINDOW(cheat_sheet_window), GTK_WINDOW(mainwindow));
    g_signal_connect(cheat_sheet_window, "delete_event",
			G_CALLBACK(cheat_sheet_close_requested), NULL);
	gui_set_escape_close(cheat_sheet_window);

    mainbox = gtk_vbox_new(FALSE, 2);
    gtk_container_border_width(GTK_CONTAINER(mainbox), 4);
    gtk_container_add(GTK_CONTAINER(cheat_sheet_window), mainbox);

    scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_box_pack_start (GTK_BOX (mainbox), scrolled_window, TRUE, TRUE, 0);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				    GTK_POLICY_NEVER,
				    GTK_POLICY_ALWAYS);

    /* Close button */
    thing = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);

    hbox = gtk_hbutton_box_new ();
    gtk_button_box_set_spacing (GTK_BUTTON_BOX (hbox), 4);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);
    gtk_box_pack_start (GTK_BOX (mainbox), hbox,
			FALSE, FALSE, 0);

    thing = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
    GTK_WIDGET_SET_FLAGS(thing, GTK_CAN_DEFAULT);
    gtk_window_set_default(GTK_WINDOW(cheat_sheet_window), thing);
    g_signal_connect_swapped(GTK_OBJECT (thing), "clicked",
			GTK_SIGNAL_FUNC (gtk_widget_hide), cheat_sheet_window);
    gtk_box_pack_start (GTK_BOX (hbox), thing, FALSE, FALSE, 0);

	text = gtk_html_new();
	stream = gtk_html_begin(GTK_HTML(text));
	gtk_html_write(GTK_HTML(text), stream, contents, size);
	gtk_html_end(GTK_HTML(text), stream, status);
	g_free(contents);

    gtk_container_add (GTK_CONTAINER (scrolled_window), text);
    gtk_widget_grab_focus (text);
    gtk_widget_set_usize(text, 46 * 12, 46 * 12);
    gtk_widget_show_all (cheat_sheet_window);
#endif
}
