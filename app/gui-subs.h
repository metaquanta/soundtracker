
/*
 * The Real SoundTracker - GUI support routines (header)
 *
 * Copyright (C) 1998-2001 Michael Krause
 * Copyright (C) 2005 Yury Aliaev (GTK+-2 porting)
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

#ifndef _GUI_SUBS_H
#define _GUI_SUBS_H

#include <gtk/gtk.h>

/* values for status bar messages */
enum {
    STATUS_IDLE = 0,
    STATUS_PLAYING_SONG,
    STATUS_PLAYING_PATTERN,
    STATUS_LOADING_MODULE,
    STATUS_MODULE_LOADED,
    STATUS_SAVING_MODULE,
    STATUS_MODULE_SAVED,
    STATUS_LOADING_SAMPLE,
    STATUS_SAMPLE_LOADED,
    STATUS_SAVING_SAMPLE,
    STATUS_SAMPLE_SAVED,
    STATUS_LOADING_INSTRUMENT,
    STATUS_INSTRUMENT_LOADED,
    STATUS_SAVING_INSTRUMENT,
    STATUS_INSTRUMENT_SAVED,
    STATUS_SAVING_SONG,
    STATUS_SONG_SAVED,
};

struct menu_callback
{
	const gchar *widget_name;
	void (*fn)(GtkWidget*, gpointer);
	gpointer data;
};

extern GtkWidget *status_bar;
extern GtkWidget *st_clock;

void                 statusbar_update                 (int message,
						       gboolean force_gui_update);

void                 file_selection_save_path         (const gchar *fn,
                                                       gchar **store);

int                  find_current_toggle              (GtkWidget **widgets,
						       int count);

void                 add_empty_hbox                   (GtkWidget *tobox);
void                 add_empty_vbox                   (GtkWidget *tobox);

void                 make_radio_group                 (const char **labels,
						       GtkWidget *tobox,
						       GtkWidget **saveptr,
						       gint t1,
						       gint t2,
						       void (*sigfunc) (void));
void                 make_radio_group_full            (const char **labels,
						       GtkWidget *tobox,
						       GtkWidget **saveptr,
						       gint t1,
						       gint t2,
						       void (*sigfunc) (void),
						       gpointer data);
GtkWidget*           make_labelled_radio_group_box    (const char *title,
						       const char **labels,
						       GtkWidget **saveptr,
						       void (*sigfunc) (void));
GtkWidget*           make_labelled_radio_group_box_full (const char *title,
						       const char **labels,
						       GtkWidget **saveptr,
						       void (*sigfunc) (void),
						       gpointer data);

void                 gui_put_labelled_spin_button     (GtkWidget *destbox,
						       const char *title,
						       int min,
						       int max,
						       GtkWidget **spin,
						       void(*callback)(),
						       void *callbackdata,
						       gboolean in_mainwindow);
void                 gui_update_range_adjustment      (GtkRange *range,
						       int pos,
						       int upper,
						       int window,
						       void(*func)());

static inline void
gui_set_radio_active (GtkWidget **radiobutton, guint i)
{
	if(GTK_WIDGET_IS_SENSITIVE(radiobutton[i]))
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton[i]), TRUE);
}

typedef enum {
    GUI_SUBS_SLIDER_WITH_HSCALE = 0,
    GUI_SUBS_SLIDER_SPIN_ONLY
} gui_subs_slider_type;

typedef struct gui_subs_slider {
    const char *title;
    int min, max;
    void (*changedfunc)(int value);
    gui_subs_slider_type type;
    GtkAdjustment *adjustment1, *adjustment2;
    GtkWidget *slider, *spin;
    gboolean update_without_signal;
} gui_subs_slider;

GtkWidget *          gui_subs_create_slider           (gui_subs_slider *s,
                                                       gboolean in_mainwindow);

void                 gui_subs_set_slider_value        (gui_subs_slider *s,
                                                       int v);

int                  gui_subs_get_slider_value        (gui_subs_slider *s);

GtkWidget*           gui_combo_new                    (GtkListStore *ls);

gboolean             gui_set_active_combo_item        (GtkWidget *combobox,
                                                       GtkTreeModel *model,
                                                       guint item);

void                 gui_combo_box_prepend_text_or_set_active (GtkComboBox *combobox,
                                                               const gchar *text,
                                                               gboolean force_active);

GtkWidget *          gui_list_in_scrolled_window      (const int n, gchar * const *tp,  GtkWidget *hbox,
                                                       GType *types,
                                                       gfloat *alignments,
                                                       gboolean *expands,
                                                       GtkSelectionMode mode,
                                                       gboolean expand,
                                                       gboolean fill);

GtkWidget *          gui_stringlist_in_scrolled_window(const int n,
                                                       gchar * const *tp,
                                                       GtkWidget *hbox, gboolean expandfill);

void                 gui_list_clear                   (GtkWidget *list);

void                 gui_list_clear_with_model        (GtkTreeModel *model);

GtkTreeModel *       gui_list_freeze                  (GtkWidget *list);

void                 gui_list_thaw                    (GtkWidget *list,
                                                       GtkTreeModel *model);

#define GUI_GET_LIST_STORE(list) GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(list)))

void                 gui_list_handle_selection        (GtkWidget *list,
                                                       GCallback handler,
                                                       gpointer data);

static inline gboolean
gui_list_get_iter (guint n, GtkListStore *tree_model, GtkTreeIter *iter)
{
    gchar *path;
    gboolean result;

    path = g_strdup_printf("%u", n);
    result = gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(tree_model), iter, path);
    g_free(path);
    return result;
}

inline void          gui_string_list_set_text        (GtkWidget *list,
                                                      guint row,
                                                      guint col,
                                                      const gchar *string);

inline void          gui_list_select                  (GtkWidget *list,
                                                       guint row,
                                                       gboolean use_align,
                                                       gfloat align);

GtkWidget *          gui_button                       (GtkWidget * win,
						       char *stock,
						       void *callback,
						       gpointer userdata,
						       GtkWidget *box);

gboolean             gui_ok_cancel_modal              (GtkWidget *window, const gchar *text);
void                 gui_message_dialog               (GtkWidget **dialog, const gchar *text, GtkMessageType type, const gchar *title, gboolean need_update);
#define              gui_warning_dialog(dialog, text, need_update) gui_message_dialog(dialog, text, GTK_MESSAGE_WARNING, N_("Warning"), need_update)
#define              gui_error_dialog(dialog, text, need_update)   gui_message_dialog(dialog, text, GTK_MESSAGE_ERROR, N_("Error!"), need_update)
void                 gui_dialog_adjust                (GtkWidget *dialog, gint default_id);

gchar *              gui_filename_to_utf8             (const gchar *old_name);
gchar *              gui_filename_from_utf8           (const gchar *old_name);

GtkBuilder           *gui_builder_from_file           (const gchar *name, const struct menu_callback cb[]);

gboolean             gui_delete_noop                  (void);
void                 gui_set_escape_close             (GtkWidget *window);
#endif /* _GUI_SUBS_H */
