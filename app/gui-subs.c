
/*
 * The Real SoundTracker - GUI support routines
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

#include <config.h>

#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <string.h>

#include "extspinbutton.h"
#include "gui-subs.h"
#include "gui.h"

static const char* status_messages[] = {
    N_("Ready."),
    N_("Playing song..."),
    N_("Playing pattern..."),
    N_("Loading module..."),
    N_("Module loaded."),
    N_("Saving module..."),
    N_("Module saved."),
    N_("Loading sample..."),
    N_("Sample loaded."),
    N_("Saving sample..."),
    N_("Sample saved."),
    N_("Loading instrument..."),
    N_("Instrument loaded."),
    N_("Saving instrument..."),
    N_("Instrument saved."),
    N_("Saving song..."),
    N_("Song saved."),
};

void statusbar_update(int message, gboolean force_update)
{
    gtk_label_set_text(GTK_LABEL(status_bar), _(status_messages[message]));

    /* Take care here... GUI callbacks can be called at this point. */
    if (force_update) {
        while (gtk_events_pending())
            gtk_main_iteration();
    }
}

int find_current_toggle(GtkWidget** widgets, int count)
{
    int i;
    for (i = 0; i < count; i++) {
        if (GTK_TOGGLE_BUTTON(*widgets++)->active) {
            return i;
        }
    }
    return -1;
}

void add_empty_hbox(GtkWidget* tobox)
{
    GtkWidget* thing = gtk_hbox_new(FALSE, 0);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(tobox), thing, TRUE, TRUE, 0);
}

void add_empty_vbox(GtkWidget* tobox)
{
    GtkWidget* thing = gtk_vbox_new(FALSE, 0);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(tobox), thing, TRUE, TRUE, 0);
}

void make_radio_group_full(const char** labels,
    GtkWidget* tobox,
    GtkWidget** saveptr,
    gint t1,
    gint t2,
    void (*sigfunc)(void),
    gpointer data)
{
    GtkWidget* thing = NULL;

    while (*labels) {
        thing = gtk_radio_button_new_with_label((thing
                                                        ? gtk_radio_button_get_group(GTK_RADIO_BUTTON(thing))
                                                        : NULL),
            gettext(*labels++));
        *saveptr++ = thing;
        gtk_widget_show(thing);
        gtk_box_pack_start(GTK_BOX(tobox), thing, t1, t2, 0);
        if (sigfunc) {
            g_signal_connect(thing, "clicked", G_CALLBACK(sigfunc), data);
        }
    }
}

void make_radio_group(const char** labels, GtkWidget* tobox,
    GtkWidget** saveptr, gint t1, gint t2,
    void (*sigfunc)(void))
{
    make_radio_group_full(labels, tobox, saveptr, t1, t2, sigfunc, NULL);
}

GtkWidget*
make_labelled_radio_group_box_full(const char* title,
    const char** labels,
    GtkWidget** saveptr,
    void (*sigfunc)(void),
    gpointer data)
{
    GtkWidget *box, *thing;

    box = gtk_hbox_new(FALSE, 4);

    thing = gtk_label_new(title);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box), thing, FALSE, TRUE, 0);

    make_radio_group_full(labels, box, saveptr, FALSE, TRUE, sigfunc, data);

    return box;
}

GtkWidget*
make_labelled_radio_group_box(const char* title,
    const char** labels,
    GtkWidget** saveptr,
    void (*sigfunc)(void))
{
    return make_labelled_radio_group_box_full(title, labels, saveptr, sigfunc, NULL);
}

void gui_put_labelled_spin_button(GtkWidget* destbox,
    const char* title,
    int min,
    int max,
    GtkWidget** spin,
    void (*callback)(),
    void* callbackdata,
    gboolean in_mainwindow)
{
    GtkWidget *hbox, *thing;

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(destbox), hbox, FALSE, TRUE, 0);
    gtk_widget_show(hbox);

    thing = gtk_label_new(title);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    add_empty_hbox(hbox);

    *spin = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new(min, min, max, 1.0, 5.0, 0.0)), 0, 0, in_mainwindow);
    gtk_box_pack_start(GTK_BOX(hbox), *spin, FALSE, TRUE, 0);
    gtk_widget_show(*spin);
    if (callback)
        g_signal_connect(*spin, "value-changed",
            G_CALLBACK(callback), callbackdata);
}

void file_selection_save_path(const gchar* fn,
    gchar** store)
{
    gchar* dn = g_path_get_dirname(fn);

    if (*store)
        g_free(*store);

    *store = g_strconcat(dn, "/", NULL);
    g_free(dn);
}

static void
gui_subs_slider_update_1(GtkWidget* w,
    gui_subs_slider* s)
{
    int v = s->adjustment1->value;
    gtk_adjustment_set_value(s->adjustment2, v);
    if (!s->update_without_signal) {
        s->changedfunc(v);
    }
}

static void
gui_subs_slider_update_2(GtkSpinButton* spin,
    gui_subs_slider* s)
{
    int v = gtk_spin_button_get_value_as_int(spin);
    if (s->type == GUI_SUBS_SLIDER_WITH_HSCALE) {
        if (v != s->adjustment1->value) {
            /* the 'if' is only needed for gtk+-1.0 */
            gtk_adjustment_set_value(s->adjustment1, v);
        }
    }
    if (!s->update_without_signal) {
        s->changedfunc(v);
    }
}

GtkWidget*
gui_subs_create_slider(gui_subs_slider* s, gboolean in_mainwindow)
{
    GtkWidget *thing, *box;

    box = gtk_hbox_new(FALSE, 4);

    thing = gtk_label_new(gettext(s->title));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box), thing, FALSE, TRUE, 0);

    if (s->type == GUI_SUBS_SLIDER_WITH_HSCALE) {
        s->adjustment1 = GTK_ADJUSTMENT(gtk_adjustment_new(s->min, s->min, s->max, 1, (s->max - s->min) / 10, 0));
        s->slider = gtk_hscale_new(s->adjustment1);
        gtk_scale_set_draw_value(GTK_SCALE(s->slider), FALSE);
        gtk_widget_show(s->slider);
        gtk_box_pack_start(GTK_BOX(box), s->slider, TRUE, TRUE, 0);
        g_signal_connect(s->adjustment1, "value_changed",
            G_CALLBACK(gui_subs_slider_update_1), s);
    } else {
        add_empty_hbox(box);
    }

    s->adjustment2 = GTK_ADJUSTMENT(gtk_adjustment_new(s->min, s->min, s->max, 1, (s->max - s->min) / 10, 0));
    thing = extspinbutton_new(s->adjustment2, 0, 0, in_mainwindow);
    gtk_box_pack_start(GTK_BOX(box), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    g_signal_connect(thing, "value-changed",
        G_CALLBACK(gui_subs_slider_update_2), s);

    s->update_without_signal = FALSE;

    return box;
}

void gui_subs_set_slider_value(gui_subs_slider* s,
    int v)
{
    gtk_adjustment_set_value(s->adjustment2, v);
}

int gui_subs_get_slider_value(gui_subs_slider* s)
{
    return s->adjustment2->value;
}

void //!!! replace with gtk_range_set_range or adjustment-related functions?
gui_update_range_adjustment(GtkRange* range,
    int pos,
    int upper,
    int window,
    void (*func)())
{
    GtkAdjustment* adj;

    adj = gtk_range_get_adjustment(range);
    if (adj->upper != upper || adj->page_size != window) {
        /* pos+1 and the extra set_value is required due to a bug in gtk 1.0.4 */
        adj = GTK_ADJUSTMENT(gtk_adjustment_new(pos + 1, 0, upper, 1, window - 2, window));
        gtk_range_set_adjustment(range, adj); /* old adjustment is freed automagically */
        gtk_adjustment_set_value(adj, pos);
        g_signal_connect(adj, "value_changed", G_CALLBACK(func), NULL);
    } else {
        if ((int)(adj->value) != pos)
            gtk_adjustment_set_value(adj, pos);
    }
}

GtkWidget*
gui_stringlist_in_scrolled_window(const int n, gchar* const* tp, GtkWidget* hbox, gboolean expandfill)
{
    GType* types;
    GtkWidget* list;
    guint i;

    types = g_new(GType, n);
    for (i = 0; i < n; i++)
        types[i] = G_TYPE_STRING;
    list = gui_list_in_scrolled_window(n, tp, hbox, types, NULL, NULL, GTK_SELECTION_BROWSE, expandfill, expandfill);
    g_free(types);
    return list;
}

inline void
gui_list_clear(GtkWidget* list)
{
    gtk_list_store_clear(GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(list))));
}

inline void
gui_list_clear_with_model(GtkTreeModel* model)
{
    gtk_list_store_clear(GTK_LIST_STORE(model));
}

GtkTreeModel*
gui_list_freeze(GtkWidget* list)
{
    GtkTreeModel* model;

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(list));
    g_object_ref(model);
    gtk_tree_view_set_model(GTK_TREE_VIEW(list), NULL);

    return model;
}

void gui_list_thaw(GtkWidget* list, GtkTreeModel* model)
{
    gtk_tree_view_set_model(GTK_TREE_VIEW(list), model);
    g_object_unref(model);
}

static gboolean hover_changed(GtkTreeView* widget, GdkEvent* event, gpointer data)
{
    gboolean is_hover = data != NULL;
    gtk_tree_view_set_hover_selection(widget, is_hover);
    return FALSE;
}

GtkWidget*
gui_list_in_scrolled_window(const int n, gchar* const* tp, GtkWidget* hbox,
    GType* types, gfloat* alignments, gboolean* expands,
    GtkSelectionMode mode, gboolean expand, gboolean fill)
{
    GtkWidget* list;
    GtkWidget* sw;
    guint i;
    GtkListStore* list_store;
    GtkTreeViewColumn* column;
    GtkCellRenderer* renderer;
    GtkTreeSelection* sel;

    list_store = gtk_list_store_newv(n, types);
    list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
    for (i = 0; i < n; i++) {
        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(_(tp[i]), renderer, "text", i, NULL);
        if (alignments) {
            g_object_set(G_OBJECT(renderer), "xalign", alignments[i], NULL);
            gtk_tree_view_column_set_alignment(column, alignments[i]);
        }
        g_object_set(G_OBJECT(renderer), "ypad", 0, NULL);
        if (expands)
            gtk_tree_view_column_set_expand(column, expands[i]);
        gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);
    }

    sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));
    gtk_tree_selection_set_mode(sel, mode);

    sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_ETCHED_IN);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(sw), list);

    /* Making the pointer following the cursor when the button is pressed (like in gtk+-1) */
    /* TODO: enabling autoscrolling when the pointer is moved up/down */
    g_signal_connect(list, "button-press-event", G_CALLBACK(hover_changed), (gpointer)TRUE);
    g_signal_connect(list, "button-release-event", G_CALLBACK(hover_changed), (gpointer)FALSE);
    g_signal_connect(list, "leave-notify-event", G_CALLBACK(hover_changed), (gpointer)FALSE);

    gtk_widget_show(sw);
    gtk_box_pack_start(GTK_BOX(hbox), sw, expand, fill, 0);
    /* According to Gtk+ documentation this is not recommended but the lists are not strippy by default...*/
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(list), TRUE);
    /* This speeds ip a bit */ //!!!but currently not working
    //    gtk_tree_view_set_fixed_height_mode(GTK_TREE_VIEW(list), TRUE);
    gtk_widget_show(list);
    return list;
}

void gui_list_handle_selection(GtkWidget* list, GCallback handler, gpointer data)
{
    GtkTreeSelection* sel;

    sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));
    g_signal_connect_after(sel, "changed", handler, data);
}

void gui_string_list_set_text(GtkWidget* list, guint row, guint col, const gchar* string)
{
    GtkTreeIter iter;
    GtkListStore* list_store;

    if (gui_list_get_iter(row, list_store = GUI_GET_LIST_STORE(list), &iter))
        gtk_list_store_set(list_store, &iter, col, string, -1);
}

void gui_list_select(GtkWidget* list, guint row, gboolean use_align, gfloat align)
{
    gchar* path_string;
    GtkTreePath* path;
    GtkTreeIter iter;
    GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));

    if (!gui_list_get_iter(row, GUI_GET_LIST_STORE(list), &iter))
        return;
    gtk_tree_selection_select_iter(sel, &iter);
    path_string = g_strdup_printf("%u", row);
    path = gtk_tree_path_new_from_string(path_string);
    gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(list), path, NULL,
        use_align, align, 0.0);

    g_free(path_string);
    gtk_tree_path_free(path);
}

GtkWidget*
gui_button(GtkWidget* win, char* stock,
    void* callback, gpointer userdata, GtkWidget* box)
{
    GtkWidget* button;

    button = gtk_button_new_from_stock(stock);
    g_signal_connect(button, "clicked",
        G_CALLBACK(callback), userdata);
    gtk_widget_show(button);

    if (box)
        gtk_container_add(GTK_CONTAINER(box), button);

    return button;
}

void gui_message_dialog(GtkWidget** dialog, const gchar* text, GtkMessageType type, const gchar* title, gboolean need_update)
{
    if (!*dialog) {
        *dialog = gtk_message_dialog_new(GTK_WINDOW(mainwindow), GTK_DIALOG_MODAL, type,
            GTK_BUTTONS_CLOSE, "%s", text);
        gtk_window_set_title(GTK_WINDOW(*dialog), _(title));
    } else if (need_update) {
        gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(*dialog), text);
    }

    gtk_dialog_run(GTK_DIALOG(*dialog));
    gtk_widget_hide(*dialog);
}

gboolean
gui_ok_cancel_modal(GtkWidget* parent, const gchar* text)
{
    gint response;
    static GtkWidget* dialog = NULL;

    if (!dialog) {
        dialog = gtk_message_dialog_new(GTK_WINDOW(parent), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
            NULL);
    }
    gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(dialog), text);
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_hide(dialog);

    return (response == GTK_RESPONSE_OK);
}

gchar*
gui_filename_from_utf8(const gchar* old_name)
{
    GtkWidget* dialog;

    GError* error = NULL;
    gchar* name = g_filename_from_utf8(old_name, -1, NULL, NULL, &error);

    if (!name) {
        dialog = gtk_message_dialog_new(GTK_WINDOW(mainwindow), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
            _("An error occured when filename character set conversion:\n%s\n"
              "The file operation probably failed."),
            error->message);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        g_error_free(error);
    }

    return name;
}

gchar*
gui_filename_to_utf8(const gchar* old_name)
{
    GtkWidget* dialog;

    GError* error = NULL;
    gchar* name = g_filename_to_utf8(old_name, -1, NULL, NULL, &error);

    if (!name) {
        dialog = gtk_message_dialog_new(GTK_WINDOW(mainwindow), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
            _("An error occured when filename character set conversion:\n%s\n"
              "The file operation probably failed."),
            error->message);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        g_error_free(error);
    }

    return name;
}

GtkWidget*
gui_combo_new(GtkListStore* ls)
{
    GtkWidget* thing;
    GtkCellRenderer* cell;

    thing = gtk_combo_box_new_with_model(GTK_TREE_MODEL(ls));
    g_object_unref(ls);

    cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(thing), cell, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(thing), cell, "text", 0, NULL);

    return thing;
}

GtkBuilder* gui_builder_from_file(const gchar* name, const struct menu_callback cb[])
{
    GtkBuilder* builder = gtk_builder_new();
    GError* error = NULL;
    guint i;

    if (!gtk_builder_add_from_file(builder, name, &error)) {
        g_critical(_("%s.\nLoading widgets' description from %s file failed!\n"),
            error->message, name);
        g_error_free(error);
        return NULL;
    }

    if (cb)
        for (i = 0; cb[i].widget_name; i++) {
            GtkWidget* w = GTK_WIDGET(gtk_builder_get_object(builder, cb[i].widget_name));

            if (w)
                g_signal_connect_swapped(w, "activate", G_CALLBACK(cb[i].fn), cb[i].data);
        }

    return builder;
}

gboolean
gui_delete_noop(void)
{
    /* For dialogs, Response callback already hides this window */
    return TRUE;
}

void gui_dialog_adjust(GtkWidget* dialog, gint default_id)
{
    gtk_container_set_border_width(GTK_CONTAINER(dialog), 4);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), default_id);
}

void gui_set_escape_close(GtkWidget* window)
{
    GtkAccelGroup* group = gtk_accel_group_new();
    GClosure* closure = g_cclosure_new_swap(G_CALLBACK(gtk_widget_hide), window, NULL);

    gtk_accel_group_connect(group, GDK_Escape, 0, 0, closure);
    gtk_window_add_accel_group(GTK_WINDOW(window), group);
}

typedef struct _compare_data {
    guint value;
    gint number;
} compare_data;

static gboolean
compare_func(GtkTreeModel* model, GtkTreePath* path, GtkTreeIter* iter, gpointer data)
{
    compare_data* cmp_data = (compare_data*)data;
    guint cur_val;

    gtk_tree_model_get(model, iter, 0, &cur_val, -1);

    if (cur_val == cmp_data->value) {
        gint* indices = gtk_tree_path_get_indices(path);

        cmp_data->number = indices[0];
        return TRUE; /* The desired element is found */
    }

    return FALSE;
}

gboolean
gui_set_active_combo_item(GtkWidget* combobox, GtkTreeModel* model, guint item)
{
    compare_data cmp_data;

    cmp_data.value = item;
    cmp_data.number = -1;
    gtk_tree_model_foreach(model, compare_func, &cmp_data);

    if (cmp_data.number >= 0) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), cmp_data.number);
        return TRUE;
    }

    return FALSE;
}

typedef struct _str_cmp_data {
    const gchar* str;
    gboolean success;
    GtkComboBox* combobox;
} str_cmp_data;

static gboolean
str_cmp_func(GtkTreeModel* model, GtkTreePath* path, GtkTreeIter* iter, gpointer data)
{
    gchar* item_str = NULL;
    str_cmp_data* scd = (str_cmp_data*)data;

    gtk_tree_model_get(model, iter, 0, &item_str, -1);
    if (!item_str)
        return TRUE; /* Aborting due to error */

    if (!g_ascii_strcasecmp(item_str, scd->str)) {
        scd->success = TRUE;
        gtk_combo_box_set_active_iter(scd->combobox, iter);
        g_free(item_str);
        return TRUE;
    }

    g_free(item_str);
    return FALSE;
}

void gui_combo_box_prepend_text_or_set_active(GtkComboBox* combobox, const gchar* text, gboolean force_active)
{
    str_cmp_data scd;

    GtkTreeModel* model = gtk_combo_box_get_model(combobox);

    scd.str = text;
    scd.success = FALSE;
    scd.combobox = combobox;
    gtk_tree_model_foreach(model, str_cmp_func, &scd);

    if (!scd.success) {
        gtk_combo_box_prepend_text(combobox, text);
        if (force_active)
            gtk_combo_box_set_active(combobox, 0);
    }
}

gint gui_get_text_entry(int length,
    void (*changedfunc)(),
    GtkWidget** widget)
{
    GtkWidget* thing;

    thing = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(thing), length);

    *widget = thing;

    return g_signal_connect(thing, "changed",
        G_CALLBACK(changedfunc), NULL);
}
