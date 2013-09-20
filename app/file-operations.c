
/*
 * The Real SoundTracker - file operations page
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

#include <string.h>

#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <glib/gi18n.h>
#include <gdk/gdktypes.h>

#include "file-operations.h"
#include "keys.h"
#include "track-editor.h"
#include "gui-subs.h"
#include "gui.h"
#include "errors.h"

struct file_op_tmp {
	gint order, index;
	const gchar *title, *path;
	void (*clickfunc)();
	gboolean is_single_click, is_save, need_return;
	const gchar ***formats;
	const gchar *tip;
};

struct file_op {
	GtkWidget *dialog, *entry;
	gint index; /* sub-page index for embedded selectors, otherwise -1 */
	void (*callback)();
	gboolean is_single_click, need_return;
};

static gboolean skip_selection = FALSE; /* Some single-click magick to avoid artifacts when directory changes */
static gint current_subpage = 0;
static gint current_page = 0;

static GtkWidget *rightnb = NULL, *leftbox;
static GSList *fileop_list;
static struct file_op_tmp *fileop_pool;
static struct file_op fileops[DIALOG_LAST];


static void
typeradio_changed (gpointer data)
{
	current_subpage = (gint)data;
	gtk_notebook_set_current_page(GTK_NOTEBOOK(rightnb), current_subpage);
	fileops_focus_entry();
}

static gint
cmpfunc(gconstpointer aa, gconstpointer bb)
{
	struct file_op_tmp *a = (struct file_op_tmp *)aa;
	struct file_op_tmp *b = (struct file_op_tmp *)bb;
	if(!aa || !bb)
		return 0;

	if(a->order < b->order) return -1;
	if(a->order == b->order) return 0;
	return 1;
}

static void set_filepath(GtkWidget *fc, const gchar *path) {
	gchar *newname = gui_filename_from_utf8(path);

	if(!newname)
		return;
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(fc), newname);
	g_free(newname);
}

static void
add_filters(GtkFileChooser *fc, const char **formats[])
{
	GtkFileFilter *current, *omni;
	const gchar **format;
	guint i = 0;

	omni = gtk_file_filter_new();
	gtk_file_filter_set_name(omni, _("All supported types"));

	while((format = formats[i])) {
		guint j = 1;

		current = gtk_file_filter_new();
		gtk_file_filter_set_name(current, _(format[0]));
		while(format[j]) {
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

void
file_selection_create (guint index, const gchar * title, const gchar *path, void(*clickfunc)(),
                       gint order, gboolean is_single_click, gboolean is_save, gboolean need_return,
                       const gchar **formats[], const gchar *tip)
{
	static gboolean firsttime = TRUE;
	static guint num_allocated = 0, already_allocated = 0;

	if(firsttime) {
		firsttime = FALSE;
		memset(&fileops, 0, DIALOG_LAST * sizeof(struct file_op));
	}

	fileops[index].is_single_click = is_single_click;
	if(order > -1) { /* Embedded */
		if(!num_allocated) {
			fileop_list = g_slist_alloc();
		}
		num_allocated++;
		if(num_allocated > already_allocated) {
			if(already_allocated) {
				already_allocated = already_allocated << 1;
				fileop_pool = g_renew(struct file_op_tmp, fileop_pool, already_allocated);
			} else {
				already_allocated = MIN_ALLOC;
				fileop_pool = g_new(struct file_op_tmp, already_allocated);
			}
		}
		/* Remember all requests to create file selection embedded sub-tabs */
		fileop_pool[num_allocated - 1].order = order;
		fileop_pool[num_allocated - 1].title = title;
		fileop_pool[num_allocated - 1].clickfunc = clickfunc;
		fileop_pool[num_allocated - 1].is_save = is_save;
		fileop_pool[num_allocated - 1].is_single_click = is_single_click;
		fileop_pool[num_allocated - 1].need_return = need_return;
		fileop_pool[num_allocated - 1].index = index;
		fileop_pool[num_allocated - 1].path = path;
		fileop_pool[num_allocated - 1].formats = formats;
		fileop_pool[num_allocated - 1].tip = tip;

		fileop_list = g_slist_insert_sorted(fileop_list, &fileop_pool[num_allocated - 1], cmpfunc);

		fileops[index].dialog = NULL; /* No widget is returned since it is not used externally */
	} else { /* Standalone */
		GtkWidget *fc;

		fileops[index].index = -1;
		fileops[index].callback = clickfunc;
		fc = fileops[index].dialog = gtk_file_chooser_dialog_new(title, GTK_WINDOW(mainwindow), is_save ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_OPEN,
		                                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		                                                         is_save ? GTK_STOCK_SAVE : GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
		if(is_save)
			gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(fc), TRUE);
		if(formats)
			add_filters(GTK_FILE_CHOOSER(fc), formats);

		set_filepath(fc, path);
	}
}

void
fileops_page_create (GtkNotebook *nb)
{
    GtkWidget *hbox, *vbox, *thing;

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_container_border_width(GTK_CONTAINER(hbox), 10);
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

static gboolean
check_overwrite_save(const gchar *fullpath, struct file_op *fop, gchar *localpath)
{
	gchar *path;
	gboolean need_free = FALSE;

	if(localpath) /* There exists local-encoded path */
		path = localpath;
	else {
		path = gui_filename_from_utf8(fullpath);
		if(!path)
			return FALSE;
		need_free = TRUE;
	}

	if(g_file_test(path, G_FILE_TEST_EXISTS)) {
		gint response;
		GtkWidget *dialog;

		if(need_free)
			g_free(path);

		dialog = gtk_message_dialog_new(GTK_WINDOW(mainwindow), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
		                                _("The file named \"%s\" already exists.\nDo you want to replace it?"),
		                                g_utf8_next_char(g_strrstr(fullpath, "/")));
		response = gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);

		if(response == GTK_RESPONSE_OK) {
			fop->callback(fullpath);
			return TRUE;
		}
		return FALSE;
	}

	if(need_free)
		g_free(path);
	fop->callback(fullpath);
	return TRUE;
}

static void
file_chosen(struct file_op *fops)
{
	gchar *newname, *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fops->dialog));

	if(!filename)/* Silently ignore errors, because filechooser sometimes returns NULL if the single click is enabled */
		return;
	newname = gui_filename_to_utf8(filename);
	if(!newname)
		return;

	if(!fops->is_single_click)
		if(fops->index == -1) { /* Standalone dialog checks overwriting itself */
			fops->callback(newname);
			if(fops->need_return)
				gui_go_to_page(current_page);
		} else {
			if(check_overwrite_save(newname, fops, filename) && fops->need_return)
				gui_go_to_page(current_page);
		}
	else {/* Single-click magick: directory changes, phantom file chosing skips etc */
		if(skip_selection)
			skip_selection = FALSE;
		else {
			if(g_file_test(filename, G_FILE_TEST_IS_REGULAR || G_FILE_TEST_IS_SYMLINK)) {
				fops->callback(newname);
				if(fops->need_return)
					gui_go_to_page(current_page);
			} else if(g_file_test(filename, G_FILE_TEST_IS_DIR))
				set_filepath(fops->dialog, newname);
		}
	}

	g_free(newname);
	g_free(filename);
}

static void
trigger_on(void)
{
	skip_selection = TRUE;
}

static void
update_entry (GtkFileChooser *fc, struct file_op *fop)
{
	GFile *fl = gtk_file_chooser_get_file(fc);
	glong len = -1;

	if(!fl)
		return;

	if(g_file_query_file_type(fl, 0, NULL) == G_FILE_TYPE_REGULAR) {
		gchar *str, *ext, *name;

		name = g_file_get_basename(fl);
		str = gui_filename_to_utf8(name);
		if(!str)
			return;
		g_free(name);
		gtk_entry_set_text(GTK_ENTRY(fop->entry), str);

		ext = g_strrstr(str, ".");
		if (ext)
			len = g_utf8_pointer_to_offset(str, ext);
		g_free(str);
		gtk_editable_select_region (GTK_EDITABLE (fop->entry), 0, (gint) len);
	}

	g_object_unref(fl);
}

static void
save_clicked (struct file_op *fop)
{
	gchar *fullpath, *utfpath;

	const gchar *name = gtk_entry_get_text(GTK_ENTRY(fop->entry));
	gchar *path = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(fop->dialog));

	if(!name || !path || strlen(name) == 0)
		return;

	utfpath = gui_filename_to_utf8(path);
	g_free(path);
	if(!utfpath)
		return;

	fullpath = g_strconcat(utfpath, "/", name, NULL);
	g_free(utfpath);

	if(check_overwrite_save(fullpath, fop, NULL) && fop->need_return)
		gui_go_to_page(current_page);

	g_free(fullpath);
}

static void
foreach_fn(gpointer lm, gpointer data)
{
	static GtkWidget *thing = NULL;
	static guint num = 0;
	GtkWidget *widget, *box, *buttonbox, *fc;
	struct file_op_tmp *elem = (struct file_op_tmp*)lm;
	gint index;
#ifdef GTK_HACKS
	GList *list;
#endif

	if(!lm)
		return;

	index = elem->index;

	fileops[index].index = num;
	fileops[index].callback = elem->clickfunc;
	fileops[index].need_return = elem->need_return;

	/* Radio buttons */
	thing = gtk_radio_button_new_with_label (thing ? gtk_radio_button_get_group (GTK_RADIO_BUTTON (thing)) : NULL,
	                                         elem->title);
	if(elem->tip)
		gtk_widget_set_tooltip_text(thing, _(elem->tip));
	gtk_widget_show(thing);
	gtk_box_pack_start(GTK_BOX (leftbox), thing, FALSE, FALSE, 0);
	g_signal_connect_swapped(thing, "clicked", G_CALLBACK(typeradio_changed), (gpointer)(num++));

	/* File selectors */
	box = gtk_vbox_new(FALSE, 2);
	/* Even for Save action we create filechooser in OPEN mode and explicetly enable CREATE_DIR button and add filename entry
	   Native SAVE mode leads to creation too many place taking unneccessary widgets */
	fc = gtk_file_chooser_widget_new(GTK_FILE_CHOOSER_ACTION_OPEN);
	set_filepath(fc, elem->path);

	if(elem->formats)
		add_filters(GTK_FILE_CHOOSER(fc), elem->formats);

	gtk_box_pack_start(GTK_BOX(box), fc, TRUE, TRUE, 0);
	g_signal_connect_swapped(fc, "file-activated", G_CALLBACK(file_chosen), &fileops[index]);
	if(elem->is_single_click) {
		g_signal_connect_swapped(fc, "selection-changed", G_CALLBACK(file_chosen), &fileops[index]);
		g_signal_connect(fc, "current-folder-changed", G_CALLBACK(trigger_on), NULL);
	}

	if(elem->is_save) {
		g_signal_connect(fc, "selection-changed", G_CALLBACK(update_entry), &fileops[index]);
		widget = gtk_entry_new();
		fileops[index].entry = widget;
		gtk_box_pack_start(GTK_BOX(box), widget, FALSE, FALSE, 0);
		g_signal_connect_swapped(widget, "activate", G_CALLBACK(save_clicked), &fileops[index]);
	} else
		fileops[index].entry = NULL;

	buttonbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(buttonbox), GTK_BUTTONBOX_END);
	gtk_box_pack_start(GTK_BOX(box), buttonbox, FALSE, FALSE, 0);
	widget = gtk_button_new_from_stock(elem->is_save ? GTK_STOCK_SAVE : GTK_STOCK_OPEN);
	gtk_box_pack_end(GTK_BOX(buttonbox), widget, FALSE, FALSE, 0);
	g_signal_connect_swapped(widget, "clicked", G_CALLBACK(elem->is_save ? save_clicked : file_chosen), &fileops[index]);
	fileops[index].dialog = fc;

	gtk_notebook_append_page(GTK_NOTEBOOK(rightnb), box, NULL);
	gtk_widget_show_all(box);

#ifdef GTK_HACKS /* Create Directory button */
	if(elem->is_save) {
		list = gtk_container_get_children(GTK_CONTAINER(fc));
		widget = GTK_WIDGET(g_list_nth_data(list, 0)); /* browse_widgets_box */
		g_list_free(list);
		list = gtk_container_get_children(GTK_CONTAINER(widget));
		widget = GTK_WIDGET(g_list_nth_data(list, 0)); /* browse_header_box */
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

	typeradio_changed((gpointer)0);
}

void fileops_page_post_create(void)
{
	g_slist_foreach(fileop_list, foreach_fn, NULL);
	g_slist_free(fileop_list);
	g_free(fileop_pool);
}

gboolean
fileops_page_handle_keys (int shift,
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
fileops_open_dialog (GtkWidget *w,
		     gpointer index)
{
	gint response;
	int n = GPOINTER_TO_INT(index);

	if(!fileops[n].dialog) {
		gui_error_dialog(N_("Operation not supported."));
		return;
	}

	if(fileops[n].index != -1) {
		current_page = notebook_current_page;
		gtk_notebook_set_current_page(GTK_NOTEBOOK(rightnb), fileops[n].index);
		gui_go_to_fileops_page();

		return;
	}

	response = gtk_dialog_run(GTK_DIALOG(fileops[n].dialog));
	gtk_widget_hide(fileops[n].dialog);
	if(response == GTK_RESPONSE_ACCEPT) {
		file_chosen(&fileops[n]);
	}
}

/* Restore previous manually set subpage after moving away the file page */
void fileops_restore_subpage(void)
{
	gtk_notebook_set_current_page(GTK_NOTEBOOK(rightnb), current_subpage);
}

void
fileops_tmpclean (void)
{
    DIR *dire;
    struct dirent *entry;
    static char tname[1024], fname[1024];

    strcpy (tname, prefs_get_prefsdir());
    strcat (tname, "/tmp/");

    if(!(dire = opendir(tname))) {
	return;
    }

    while((entry = readdir(dire))) {
	if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
	    strcpy(fname, tname);
	    strcat(fname, entry->d_name);
	    unlink(fname);
        }
    }

    closedir(dire);
}

// For the unidentified reason this does not work at the main notebook page switching
void
fileops_focus_entry (void)
{
	guint i;
	gint page;

	if(!rightnb) /* Fileops page is not fully created */
		return;
	page = gtk_notebook_get_current_page(GTK_NOTEBOOK(rightnb));

	for(i = 0; i < DIALOG_LAST; i++) {
		if(fileops[i].index == page) {
			GtkWidget *entry = fileops[i].entry;
			if(entry)
				gtk_widget_grab_focus(entry);
			return;
		}
	}
}
