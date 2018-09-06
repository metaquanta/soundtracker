
/*
 * The Real SoundTracker - file operations page (header)
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

#ifndef _FILE_OPERATIONS_H
#define _FILE_OPERATIONS_H

#include <gtk/gtk.h>

/* When adding new dialogs, add them in the "not included in the File
   tab" section if they are standalone. But in principle embedded
   dialogs can be shuffled with standalone ones... */

enum {
    DIALOG_LOAD_MOD = 0,    /* Dialogs of the "File" tab */
    DIALOG_SAVE_MOD,
    DIALOG_SAVE_MOD_AS_WAV,
    DIALOG_SAVE_SONG_AS_XM,
    DIALOG_LOAD_SAMPLE,
    DIALOG_SAVE_SAMPLE,
    DIALOG_LOAD_INSTRUMENT,
    DIALOG_SAVE_INSTRUMENT,

    DIALOG_SAVE_RGN_SAMPLE, /* are not included in the "File" tab */
    DIALOG_LOAD_PATTERN,
    DIALOG_SAVE_PATTERN,
    DIALOG_LAST
};

void            fileops_page_create                (GtkNotebook *nb);

void            fileops_page_post_create           (void);

void            fileops_dialog_create              (const guint index, const gchar *title, const gchar *path,
                                                    void(*callback)(), const gboolean is_embedded, const gboolean is_save,
                                                    const gchar **formats[], const gchar *tip);

gboolean        fileops_page_handle_keys           (int shift,
						    int ctrl,
						    int alt,
						    guint32 keyval,
						    gboolean pressed);

void            fileops_open_dialog                (gpointer index);

void		fileops_refresh_list 		   (GtkFileSelection *fs,
						    gboolean grab);

void		fileops_tmpclean                   (void);

void            fileops_restore_subpage            (void);

void            fileops_enter_pressed              (void);
#endif /* _FILE_OPERATIONS_H */
