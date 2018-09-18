
/*
 * The Real SoundTracker - Preferences handling
 *
 * Copyright (C) 2013 Yury Alyaev
 * Copyright (C) 1998-2001 Michael Krause
 * Copyright (C) 2000 Fabian Giesen
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

#if !defined(_WIN32)

#include <sys/stat.h>

#include <glib/gi18n.h>

#include "gui-subs.h"
#include "preferences.h"

static GKeyFile *kf = NULL;
static gchar *config;

gchar *
prefs_get_prefsdir (void)
{
	static gchar xdir[PATH_MAX]; /* Assume that $HOME is sane */
	const gchar *homedir = g_getenv("HOME");

	if(!homedir)
		homedir = g_get_home_dir();
	sprintf(xdir, "%s/.soundtracker", homedir);
	return(xdir);
}

static void
prefs_check_prefs_dir (void)
{
    struct stat st;
    static GtkWidget *dialog = NULL;
    gchar *dir = prefs_get_prefsdir();

    if(stat(dir, &st) < 0) {
	mkdir(dir, S_IRUSR | S_IWUSR | S_IXUSR);
    strcat(dir, "/tmp");
	mkdir(dir, S_IRUSR | S_IWUSR | S_IXUSR);
	gui_info_dialog(&dialog, _("A directory called '.soundtracker' has been created in your\nhome directory to store configuration files.\n"), FALSE);
    }
}

gchar *
prefs_get_filename (const gchar *name)
{
	gchar *buf;

	prefs_check_prefs_dir();
	buf = g_strdup_printf("%s/%s", prefs_get_prefsdir(), name);
	return buf;
}

void
prefs_init (void)
{
	GError *error;

	config = prefs_get_filename("config");
	kf = g_key_file_new();

	/* Don't panic, this loop will be executed no more than 2 times */
	while(1) {
		error = NULL;
		g_key_file_load_from_file(kf, config, G_KEY_FILE_NONE, &error);
		if(error) {
			if(error->code == G_FILE_ERROR_NOENT) {
				gchar *buf = prefs_get_filename("settings"); /* maybe ST-gtk1 configs exist */

				if(g_file_test(buf, G_FILE_TEST_EXISTS)) {
					if(gui_ok_cancel_modal(NULL, _("Would you like to import settings from old version of Soundtracker?"))) {
						GError *err = NULL;

						if(!g_spawn_command_line_sync("soundtracker_convert_config -f", NULL, NULL, NULL, &err)) {
							gchar *buff = g_strdup_printf(_("An error is occured during converting config:\n%s"), error->message);
							static GtkWidget *dialog = NULL;

							gui_error_dialog(&dialog, buff, TRUE);
							g_free(buff);
							g_error_free(err);
						} else {
							g_free(buf);
							g_error_free(error);
							continue; /* Try to load config after the conversion */
						}
					}
				}
				g_free(buf);
			} else {
				gchar *buf = g_strdup_printf(_("An error is occured during reading or parsing config:\n%s"), error->message);
				static GtkWidget *dialog = NULL;

				gui_error_dialog(&dialog, buf, TRUE);
				g_free(buf);
			}
			g_error_free(error);
		}
		/* The second attempt is not required */
		break;
	}
}

void
prefs_save (void)
{
	gchar *contents;
	gsize length;
	GError *error = NULL;

	g_assert(kf != NULL);

	contents = g_key_file_to_data(kf, &length, NULL);
	g_file_set_contents(config, contents, length, &error);
	if(error) {
		gchar *buf = g_strdup_printf(_("An error is occured during saving config:\n%s"), error->message);
		static GtkWidget *dialog = NULL;

		gui_error_dialog(&dialog, buf, TRUE);
		g_free(buf);
		g_error_free(error);
	}

	g_free(contents);
}

void
prefs_close (void)
{
	g_assert(kf != NULL);

	g_key_file_free(kf);
	kf = NULL;
	g_free(config);
}

gboolean
prefs_remove_key (const gchar *section, const gchar *key)
{
	g_assert(kf != NULL);

	return g_key_file_remove_key(kf, section, key, NULL);
}


gchar**
prefs_get_str_array (const gchar *section,
		     const gchar *key,
		     gsize *length)
{
	gchar **ret;
	GError *error = NULL;

	g_assert(kf != NULL);

	ret = g_key_file_get_string_list(kf, section, key, length, &error);
	if(error) {
		ret = NULL;
		g_error_free(error);
	}

	return ret;
}

gint*
prefs_get_int_array (const gchar *section,
		     const gchar *key,
		     gsize *length)
{
	gint *ret;
	GError *error = NULL;

	g_assert(kf != NULL);

	ret = g_key_file_get_integer_list(kf, section, key, length, &error);
	if(error) {
		ret = NULL;
		g_error_free(error);
	}

	return ret;
}

gboolean*
prefs_get_bool_array (const gchar *section,
		     const gchar *key,
		     gsize *length)
{
	gboolean *ret;
	GError *error = NULL;

	g_assert(kf != NULL);

	ret = g_key_file_get_boolean_list(kf, section, key, length, &error);
	if(error) {
		g_error_free(error);
		error = NULL;
		/* Try to read values as integer list; maybe the config is from elder version */
		ret = (gboolean *)g_key_file_get_integer_list(kf, section, key, length, &error);
		if(error) {
			ret = NULL;
			g_error_free(error);
		}
	}

	return ret;
}

gint
prefs_get_int (const gchar *section,
		     const gchar *key,
		     const gint dflt)
{
	gint retval;

	GError *error = NULL;

	g_assert(kf != NULL);

	retval = g_key_file_get_integer(kf, section, key, &error);
	if(error) {
		retval = dflt;
		g_error_free(error);
	}

	return retval;
}

gboolean
prefs_get_bool (const gchar *section,
		     const gchar *key,
		     const gboolean dflt)
{
	gboolean retval;

	GError *error = NULL;

	g_assert(kf != NULL);

	retval = g_key_file_get_boolean(kf, section, key, &error);
	if(error) {
		g_error_free(error);
		error = NULL;
		/* Try to read values as integer; maybe the config is from elder version */
		retval = g_key_file_get_integer(kf, section, key, &error);
		if(error) {
			retval = dflt;
			g_error_free(error);
		}
	}

	return retval;
}

GdkColor
prefs_get_color (const gchar *section,
                 const gchar *key,
                 const GdkColor dflt)
{
	gint *retval;
	GdkColor ret;
	gsize size;

	GError *error = NULL;

	g_assert(kf != NULL);

	retval = g_key_file_get_integer_list(kf, section, key, &size, &error);
	if(error || size < 3) {
		ret.red = dflt.red;
		ret.green = dflt.green;
		ret.blue =  dflt.blue;
		g_error_free(error);
	} else {
		ret.red = retval[0];
		ret.green = retval[1];
		ret.blue = retval[2];
		g_free(retval);
	}

	ret.pixel = 0;
	return ret;
}

gchar*
prefs_get_string (const gchar *section,
		     const gchar *key,
		     const gchar *dflt)
{
	gchar *retval;

	GError *error = NULL;

	g_assert(kf != NULL);

	retval = g_key_file_get_string(kf, section, key, &error);
	if(error) {
		if(dflt)
			retval = g_strdup(dflt);
		else
			retval = NULL;

		g_error_free(error);
	}

	return retval;
}

gsize
prefs_get_pairs (const gchar *section,
                 gchar ***keys,
                 gchar ***values)
{
	gsize lgth;
	guint i;
	GError *error = NULL;
	gchar **ks, **vs;

	g_assert(kf != NULL);

	ks = g_key_file_get_keys(kf, section, &lgth, &error);
	if(error) {
		g_error_free(error);
		return 0;
	}

	vs = g_new0(gchar *, lgth + 1); /* +1 for terminating NULL */

	for(i = 0; i < lgth; i++) {
		vs[i] = g_key_file_get_string(kf, section, ks[i], &error);
		if(error) {
			g_strfreev(ks);
			g_strfreev(vs);
			g_error_free(error);
			return 0;
		}
	}

	*keys = ks;
	*values = vs;
	return lgth;
}

void
prefs_put_int (const gchar *section,
		     const gchar *key,
		     const gint value)
{
	g_assert(kf != NULL);

	g_key_file_set_integer(kf, section, key, value);
}

void
prefs_put_bool (const gchar *section,
		     const gchar *key,
		     const gboolean value)
{
	g_assert(kf != NULL);

	g_key_file_set_boolean(kf, section, key, value);
}

void
prefs_put_color (const gchar *section,
		     const gchar *key,
	        const GdkColor value)
{
	gint val[3];

	g_assert(kf != NULL);

	val[0] = value.red;
	val[1] = value.green;
	val[2] = value.blue;

	g_key_file_set_integer_list(kf, section, key, val, 3);
}

void
prefs_put_string (const gchar *section,
		  const gchar *key,
		  const gchar *value)
{
	g_assert(kf != NULL);

	g_key_file_set_string(kf, section, key, value);
}
/*
void
prefs_put_str_array (const gchar *section,
                     const gchar *key,
                     const gchar * const *value,
                     gsize length)
{
	g_assert(kf != NULL);

	g_key_file_set_string_list(kf, section, key, value, length);
}
*/
void
prefs_put_int_array (const gchar *section,
                     const gchar *key,
                     gint *value,
                     gsize length)
{
	g_assert(kf != NULL);

	g_key_file_set_integer_list(kf, section, key, value, length);
}

void
prefs_put_bool_array (const gchar *section,
                      const gchar *key,
                      gboolean *value,
                      gsize length)
{
	g_assert(kf != NULL);

	g_key_file_set_boolean_list(kf, section, key, value, length);
}

#else /* defined(_WIN32) */

/*

  Attention!

  These don't work in their current form; what needs to be done is:

  a, change void* into prefs_node*
  b, add prefs_get_file_pointer() - if this is used by the caller
     after opening, use a real file instead of the registry.

 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <glib/gi18n.h>

#include <sys/stat.h>
#include <sys/types.h>

#define  WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "gui-subs.h"
#include "preferences.h"
#include "scope-group.h"
#include "track-editor.h"
#include "errors.h"

void *
prefs_open_read (const char *name)
{
    char buf[256];
    HKEY hk;

    sprintf(buf, "Software/Soundtracker/Soundtracker/%s", name);

    while(strchr(buf, '/'))
        *strchr(buf, '/')='\\';

    if(!RegOpenKeyEx(HKEY_LOCAL_MACHINE, buf, 0, KEY_ALL_ACCESS, &hk))
        hk=0;

    return (void *) hk;
}

void *
prefs_open_write (const char *name)
{
    char buf[256];
    HKEY hk;
    DWORD bla;

    sprintf(buf, "Software/Soundtracker/Soundtracker/%s", name);

    while(strchr(buf, '/'))
        *strchr(buf, '/')='\\';

    if (!RegCreateKeyEx(HKEY_LOCAL_MACHINE, buf, 0, 0, 0, KEY_ALL_ACCESS, 0, &hk, &bla))
      hk=0;

    return (void *) hk;
}

void
prefs_close (void *node)
{
    HKEY hk;

    hk=(HKEY) node;

    RegFlushKey(hk);
    RegCloseKey(hk);
}

static int
prefs_query_reg (HKEY hk,
           const char *key,
           char **buf,
           DWORD *size,
           DWORD *type)
{
    if(RegQueryValueEx(hk, key, 0, type, 0, size)==ERROR_SUCCESS)
    {
        *buf=(char *) malloc(*size+1);

        if(RegQueryValueEx(hk, key, 0, type, *buf, size)==ERROR_SUCCESS)
            return 1;
        else
        {
            free(*buf);
            return 0;
        }
    }
    else
        return 0;
}

static void
prefs_set_reg (HKEY hk,
           const char *key,
           char *buf,
           DWORD size,
           DWORD type)
{
    RegSetValueEx(hk, key, 0, type, buf, size);
}

gboolean
prefs_get_int (void *f,
	       const char *key,
	       int *dest)
{
    char *buf;
    DWORD sz, type;
    HKEY hk;

    hk=(HKEY) f;

    if(prefs_query_reg(hk, key, &buf, &sz, &type))
    {
        if (type==REG_DWORD)
        {
            *dest=*((DWORD *) buf);
            free(buf);
            return 1;
        }
    }

    return 0;
}

gboolean
prefs_get_string (void *f,
		  const char *key,
		  char *dest)
{
    char *buf;
    DWORD sz, type;
    HKEY hk;

    hk=(HKEY) f;

    if(prefs_query_reg(hk, key, &buf, &sz, &type))
    {
        if (type==REG_SZ)
        {
            buf[127]=0;
            strcpy(dest, buf);
            free(buf);
            return 1;
        }
    }

    return 0;
}

void
prefs_put_int (void *f,
	       const char *key,
	       int value)
{
    prefs_set_reg((HKEY) f, key, &value, 4, REG_DWORD);
}

void
prefs_put_string (void *f,
		  const char *key,
		  const char *value)
{
    prefs_set_reg((HKEY) f, key, value, strlen(value+1), REG_SZ);
}

#endif /* defined(_WIN32) */
