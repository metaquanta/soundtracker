
/*
 * The Real SoundTracker - file operations page
 *
 * Copyright (C) 1999-2001 Michael Krause
 * Copyright (C) 2006-2018 Yury Aliaev
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

#include <dirent.h>
#include <errno.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdktypes.h>
#include <glib/gi18n.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "file-operations.h"
#include "gui-subs.h"
#include "gui.h"
#include "keys.h"
#include "track-editor.h"

struct file_op {
    GtkWidget* dialog;
    void (*callback)();
    gboolean firsttime, is_save, skip_selection;
    gint index; /* -1 for standalone dialogs */
};

struct file_op_tmp {
    const gchar *title, *path, *tip, ***formats;
};

//static gboolean skip_selection = FALSE; /* Some single-click magick to avoid artifacts when directory changes */
static intptr_t current_subpage = 0, stored_subpage = 0;
static gint current_page = 0;
static gboolean need_return = FALSE, nostore_subpage = FALSE;

static GtkWidget *rightnb = NULL, *leftbox, *radio[DIALOG_LAST];
static struct file_op fileops[DIALOG_LAST] = { { 0 } };
static struct file_op_tmp* fo_tmp;

static inline gint
find_fileop(guint subpage)
{
    guint i;

    for (i = 0; i < DIALOG_LAST; i++)
        if (fileops[i].index == subpage)
            return i;

    return -1;
}

static void
typeradio_changed(GtkToggleButton* w, gpointer data)
{
    if (gtk_toggle_button_get_active(w)) {
        current_subpage = (intptr_t)data;
        if (!nostore_subpage)
            stored_subpage = current_subpage;
        gtk_notebook_set_current_page(GTK_NOTEBOOK(rightnb), current_subpage);
    }
}

static void set_filepath(GtkWidget* fc, const gchar* path)
{
    gchar* newname = gui_filename_from_utf8(path);

    if (!newname)
        return;
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(fc), newname);
    g_free(newname);
}

static void
add_filters(GtkFileChooser* fc, const char** formats[])
{
    GtkFileFilter *current, *omni;
    const gchar** format;
    guint i = 0;

    omni = gtk_file_filter_new();
    gtk_file_filter_set_name(omni, _("All supported types"));

    while ((format = formats[i])) {
        guint j = 1;

        current = gtk_file_filter_new();
        gtk_file_filter_set_name(current, _(format[0]));
        while (format[j]) {
            gtk_file_filter_add_pattern(current, format[j]);
            gtk_file_filter_add_pattern(omni, format[j]);
            j++;
        }
        gtk_file_chooser_add_filter(fc, current);
        i++;
    }

    gtk_file_chooser_add_filter(fc, omni);
    gtk_file_chooser_set_filter(fc, omni);

    omni = gtk_file_filter_new();
    gtk_file_filter_set_name(omni, _("All files"));
    gtk_file_filter_add_pattern(omni, "*");
    gtk_file_chooser_add_filter(fc, omni);
}

void fileops_dialog_create(const guint index, const gchar* title, const gchar* path, void (*callback)(),
    const gboolean is_embedded, const gboolean is_save,
    const gchar** formats[], const gchar* tip)
{
    if (!fo_tmp) {
        fo_tmp = g_new(struct file_op_tmp, DIALOG_LAST);
        memset(fo_tmp, 0, sizeof(struct file_op_tmp) * DIALOG_LAST);
    }
    g_assert(fo_tmp != NULL);

    fileops[index].callback = callback;

    if (is_embedded) {
        fo_tmp[index].title = title;
        fo_tmp[index].path = path;
        fo_tmp[index].formats = formats;
        fo_tmp[index].tip = tip;
        fileops[index].is_save = is_save;
    } else { /* Standalone */
        GtkWidget* fc;

        fileops[index].index = -1;
        fc = fileops[index].dialog = gtk_file_chooser_dialog_new(title, GTK_WINDOW(mainwindow), is_save ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_OPEN,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            is_save ? GTK_STOCK_SAVE : GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
        if (is_save)
            gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(fc), TRUE);
        if (formats)
            add_filters(GTK_FILE_CHOOSER(fc), formats);

        set_filepath(fc, path);
    }
}

static void
trigger_on(struct file_op* fop)
{
    if (fop->firsttime)
        fop->firsttime = FALSE;
    else
        fop->skip_selection = TRUE;
}

static void
file_chosen(struct file_op* fops)
{
    gchar *utfname, *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fops->dialog));

    /* Silently ignore errors, because filechooser sometimes returns NULL with the "selection-changed" signal */
    if (!filename)
        return;

    if (filename[0] != '\0') {
        if (fops->skip_selection)
            fops->skip_selection = FALSE;
        else {
            utfname = gui_filename_to_utf8(filename);
            if (!utfname) {
                g_free(filename);
                return;
            }

            /* Please don't ask me anything about the further lines. In order to implement single-click
   operation with the Gtk+-2 file chooser one have to do something pervetred unnatural... */

            /* Change directory by single click for both save and load */
            if (g_file_test(filename, G_FILE_TEST_IS_DIR))
                gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(fops->dialog), filename);
            /* File activating by single click only for load */
            else if ((!fops->is_save) && g_file_test(filename, G_FILE_TEST_IS_REGULAR || G_FILE_TEST_IS_SYMLINK)) {
                fops->callback(utfname, filename);
                if (need_return) {
                    need_return = FALSE;
                    gui_go_to_page(current_page);
                }
            }
            g_free(utfname);
        }
    }
    g_free(filename);
}

static void
load_clicked(struct file_op* fops)
{
    fops->skip_selection = FALSE;
    file_chosen(fops);
}

static void
save_clicked(struct file_op* fop)
{
    gchar* utfpath;
    gchar* name = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fop->dialog));

    if (!name)
        return;

    if (name[0] != '\0') {
        utfpath = gui_filename_to_utf8(name);
        if (utfpath) {
            if (g_file_test(name, G_FILE_TEST_EXISTS)) {
                gint response;
                static GtkWidget* dialog = NULL;

                if (!dialog)
                    dialog = gtk_message_dialog_new(GTK_WINDOW(mainwindow), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
                        _("The file named \"%s\" already exists.\nDo you want to replace it?"),
                        g_utf8_next_char(g_strrstr(utfpath, "/")));
                response = gtk_dialog_run(GTK_DIALOG(dialog));
                gtk_widget_hide(dialog);

                if (response == GTK_RESPONSE_OK) {
                    fop->callback(utfpath, name);
                    if (need_return) {
                        gui_go_to_page(current_page);
                        need_return = FALSE;
                    }
                }
            } else {
                fop->callback(utfpath, name);
                if (need_return) {
                    gui_go_to_page(current_page);
                    need_return = FALSE;
                }
            }

            g_free(utfpath);
        }
    }
    g_free(name);
}

/* Two-step procedure since file dialogs can be declared at random order,
   but shoud appear as notebook pages as intended */

void fileops_page_post_create(void)
{
    intptr_t num = 0;
    guint i;

    for (i = 0; i < DIALOG_LAST; i++)
        /* Title cannot be NULL, so we can use it as non-empty element detector */
        if (fileops[i].index != -1 && fo_tmp[i].title) {
            GtkWidget *widget, *box, *buttonbox, *fc;
#ifdef GTK_HACKS
            GList* list;
#endif

            fileops[i].firsttime = TRUE;

            /* Radio buttons */
            radio[num] = gtk_radio_button_new_with_label(num ? gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio[0])) : NULL,
                fo_tmp[i].title);
            if (fo_tmp[i].tip)
                gtk_widget_set_tooltip_text(radio[num], _(fo_tmp[i].tip));
            gtk_widget_show(radio[num]);
            gtk_box_pack_start(GTK_BOX(leftbox), radio[num], FALSE, FALSE, 0);
            g_signal_connect(radio[num], "toggled", G_CALLBACK(typeradio_changed), (gpointer)num);

            /* File selectors */
            box = gtk_vbox_new(FALSE, 2);
#ifdef GTK_HACKS
            /* Even for Save action we create filechooser in OPEN mode and explicetly
			   enable CREATE_DIR button and add filename entry. Native SAVE mode leads
			   to creation of too many place taking unneccessary widgets */
            fc = gtk_file_chooser_widget_new(GTK_FILE_CHOOSER_ACTION_OPEN);
#else
            /* Traditional way for those who don't like hacking */
            fc = gtk_file_chooser_widget_new(fileops[i].is_save ? GTK_FILE_CHOOSER_ACTION_SAVE
                                                                : GTK_FILE_CHOOSER_ACTION_OPEN);
#endif
            set_filepath(fc, fo_tmp[i].path);

            if (fo_tmp[i].formats)
                add_filters(GTK_FILE_CHOOSER(fc), fo_tmp[i].formats);

            gtk_box_pack_start(GTK_BOX(box), fc, TRUE, TRUE, 0);
            g_signal_connect_swapped(fc, "file-activated",
                G_CALLBACK(fileops[i].is_save ? save_clicked : file_chosen), &fileops[i]);
            g_signal_connect_swapped(fc, "selection-changed", G_CALLBACK(file_chosen), &fileops[i]);
            g_signal_connect_swapped(fc, "current-folder-changed", G_CALLBACK(trigger_on), &fileops[i]);

            buttonbox = gtk_hbutton_box_new();
            gtk_button_box_set_layout(GTK_BUTTON_BOX(buttonbox), GTK_BUTTONBOX_END);
            gtk_box_pack_start(GTK_BOX(box), buttonbox, FALSE, FALSE, 0);
            widget = gtk_button_new_from_stock(fileops[i].is_save ? GTK_STOCK_SAVE : GTK_STOCK_OPEN);
            gtk_box_pack_end(GTK_BOX(buttonbox), widget, FALSE, FALSE, 0);
            g_signal_connect_swapped(widget, "clicked",
                G_CALLBACK(fileops[i].is_save ? save_clicked : load_clicked),
                &fileops[i]);
            fileops[i].dialog = fc;

            gtk_notebook_append_page(GTK_NOTEBOOK(rightnb), box, NULL);
            gtk_widget_show_all(box);

#ifdef GTK_HACKS
            if (fileops[i].is_save) { /* Create Directory button */
                list = gtk_container_get_children(GTK_CONTAINER(fc));
                widget = GTK_WIDGET(g_list_nth_data(list, 0)); /* FileChooserDefault */
                g_list_free(list);
                list = gtk_container_get_children(GTK_CONTAINER(widget));
                widget = GTK_WIDGET(g_list_nth_data(list, 0)); /* vbox */
                g_list_free(list);
                list = gtk_container_get_children(GTK_CONTAINER(widget));
                widget = GTK_WIDGET(g_list_nth_data(list, 0)); /* browse_path_bar_box */
                g_list_free(list);
                list = gtk_container_get_children(GTK_CONTAINER(widget)); /* Create Folder button :) */
                widget = GTK_WIDGET(g_list_nth_data(list, g_list_length(list) - 1));
                g_list_free(list);

                gtk_widget_show(widget);
            }
#endif
            fileops[i].index = num++;
        }

    g_free(fo_tmp);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(rightnb), 0);
}

void fileops_page_create(GtkNotebook* nb)
{
    GtkWidget *hbox, *vbox, *thing;

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 10);
    gtk_notebook_append_page(nb, hbox, gtk_label_new(_("File")));
    gtk_widget_show(hbox);

    leftbox = vbox = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);
    gtk_widget_show(vbox);

    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    gtk_widget_show(thing);

    thing = rightnb = gtk_notebook_new();
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(thing), FALSE);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(thing), FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), rightnb, TRUE, TRUE, 0);
    gtk_widget_show(rightnb);
}

gboolean
fileops_page_handle_keys(int shift,
    int ctrl,
    int alt,
    guint32 keyval,
    gboolean pressed)
{
    int i;

    i = keys_get_key_meaning(keyval, ENCODE_MODIFIERS(shift, ctrl, alt));
    if (i != -1 && KEYS_MEANING_TYPE(i) == KEYS_MEANING_NOTE) {
        track_editor_do_the_note_key(i, pressed, keyval, ENCODE_MODIFIERS(shift, ctrl, alt), TRUE);
        return TRUE;
    }

    return FALSE;
}

/* This function is used when the file operations are initiated by menus and buttons
   rather than user's selection of the file tab */
void fileops_open_dialog(gpointer index)
{
    intptr_t n = (intptr_t)index;

    g_assert(fileops[n].dialog != NULL);

    if (fileops[n].index != -1) {
        current_page = notebook_current_page;
        nostore_subpage = TRUE;
        gui_set_radio_active(radio, fileops[n].index);
        nostore_subpage = FALSE;
        need_return = TRUE;
        gui_go_to_fileops_page();
    } else {
        gint response = gtk_dialog_run(GTK_DIALOG(fileops[n].dialog));
        gtk_widget_hide(fileops[n].dialog);
        if (response == GTK_RESPONSE_ACCEPT) {
            gchar* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fileops[n].dialog));

            /* Silently ignore errors */
            if (!filename)
                return;

            if (filename[0] != '\0') {
                gchar* utfname = gui_filename_to_utf8(filename);
                if (!utfname) {
                    g_free(filename);
                    return;
                }
                fileops[n].callback(utfname, filename);
                g_free(utfname);
            }
            g_free(filename);
        }
    }
}

/* Restore previous manually set subpage after moving away from the file page */
void fileops_restore_subpage(void)
{
    nostore_subpage = TRUE;
    gui_set_radio_active(radio, stored_subpage);
    nostore_subpage = FALSE;
}

void fileops_enter_pressed(void)
{
    gint index = find_fileop(current_subpage);

    if (index != -1) {
        fileops[index].skip_selection = FALSE;
        fileops[index].is_save ? save_clicked(&fileops[index]) : file_chosen(&fileops[index]);
    }
}

void fileops_tmpclean(void)
{
    DIR* dire;
    struct dirent* entry;
    static char tname[1024], fname[1024];

    strcpy(tname, prefs_get_prefsdir());
    strcat(tname, "/tmp/");

    if (!(dire = opendir(tname))) {
        return;
    }

    while ((entry = readdir(dire))) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
            strcpy(fname, tname);
            strcat(fname, entry->d_name);
            unlink(fname);
        }
    }

    closedir(dire);
}
