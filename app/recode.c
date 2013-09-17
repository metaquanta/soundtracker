
/*
 * The Real SoundTracker - DOS Charset recoder
 *
 * Copyright (C) 1998-2001 Michael Krause
 *
 * The tables have been taken from recode-3.4.1/ibmpc.c:
 * Copyright (C) 1990, 1993, 1994 Free Software Foundation, Inc.
 * Francois Pinard <pinard@iro.umontreal.ca>, 1988.
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

#include <string.h>
#include <stdio.h>

#include "recode.h"

struct pair {
	guchar length;
	guchar symbol;
	gchar *utf;
};

/* Data for FT2 custom encoding to utf-8 code conversions (non-ASCII characters).  */

static struct pair known_pairs[] =
  { /* FT2 encoding table is a bit rearranged for speedup the recoding
       First utf8 charachters are placed, than aliases of ASCII characters */
    { 2, 0x01, "\xc3\xb7"},			/* division sign */
    { 2, 0x02, "\xc3\x97"},			/* multiply sign */
    { 3, 0x03, "\xe2\x86\x93"},		/* arrow down */
    { 2, 0x04, "\xc3\xa4"},			/* small letter a with diaeresis */
    { 3, 0x05, "\xe2\x86\x91"},		/* arrow up */
    { 2, 0x06, "\xc3\xa5"},			/* small letter a with a ring above */
    { 3, 0x07, "\xe2\x81\xb0"},		/* digit zero in upper register */
    { 2, 0x08, "\xc2\xb9"},			/* digit one in upper register */
    { 2, 0x09, "\xc2\xb2"},			/* digit two in upper register */
    { 2, 0x0a, "\xc2\xb3"},			/* digit three in upper register */
    { 3, 0x0b, "\xe2\x81\xb4"},		/* digit four in upper register */
    { 3, 0x0c, "\xe2\x81\xb5"},		/* digit five in upper register */
    { 3, 0x0d, "\xe2\x81\xb6"},		/* digit six in upper register */
    { 2, 0x0e, "\xc3\x84"},			/* capital letter A with diaeresis */
    { 2, 0x0f, "\xc3\x85"},			/* capital letter A with a ring above */

    { 3, 0x10, "\xe2\x81\xb7"},		/* digit seven in upper register */
    { 3, 0x11, "\xe2\x81\xb8"},		/* digit eight in upper register */
    { 3, 0x12, "\xe2\x81\xb9"},		/* digit nine in upper register */
    { 2, 0x14, "\xc3\xb6"},			/* small letter o with diaeresis */
    { 2, 0x19, "\xc3\x96"},			/* capital letter O with diaeresis */
    { 2, 0x1b, "\xc2\xbd"},			/* 1/2 fraction */
    { 3, 0x1c, "\xe2\xac\x86"},		/* bold arrow up */
    { 3, 0x1d, "\xe2\xac\x87"},		/* bold arrow down */
    { 3, 0x1e, "\xe2\xac\x85"},		/* bold arrow left */
    { 3, 0x1f, "\xe2\x9e\xa1"},		/* bold arrow right */
    { 2, 0xff, "\xc2\xa9"},			/* copyright sign */

    { 1, 0x00, " "},				/* FT2 character table contains TOO MANY spaces */
    { 1, 0x7f, " "},				/* yet another space */
    { 1, 0xfe, " "},				/* yet another space */
    { 1, 0x13, "A"},
    { 1, 0x15, "B"},
    { 1, 0x16, "C"},
    { 1, 0x17, "D"},
    { 1, 0x18, "E"},
    { 1, 0x1a, "F"},
    { 1, 0xa0, "A"},				/* These letters were in slightly different font */
    { 1, 0xa1, "B"},
    { 1, 0xa2, "C"},
    { 1, 0xa3, "D"},
    { 1, 0xa4, "E"},
    { 1, 0xa5, "F"},
    { 0, 0xa6, ""},					/* Something strange, just loose it */
    { 0, 0xa7, ""},					/* Something strange, just loose it */
  };
#define NUMBER_OF_PAIRS (sizeof(known_pairs) / sizeof(struct pair))
/* Symbols encoded with utf-8 multibyte sequences */
#define UTF_PAIRS 26

void
recode_to_utf (const gchar *src, gchar *dest, guint len)
{
	guint i, j, destptr = 0;

	for(i = 0; i < len; i++) {
		guchar c = src[i];

		if(c >= 0x20 && c <= 0x7e)
			dest[destptr++] = c;
		else if(c >= 0xa8 && c <= 0xfd)
			dest[destptr++] = c - 128;
		else { /* The rest of the charset shall be processed via encoding table */
			if(c >= 0x80 && c <= 0x9f)
				c -= 128;
			for(j = 0; j < NUMBER_OF_PAIRS; j++)
				if(c == known_pairs[j].symbol) {
					strncpy(&dest[destptr], known_pairs[j].utf, known_pairs[j].length);
					destptr += known_pairs[j].length;
					break;
				}
			if(j == NUMBER_OF_PAIRS)
				fprintf(stderr, "muh, code: 0x%x\n", c); /* Something's wrond in the code logick */
		}
	}
	dest[destptr] = 0; /* Null-terminating the resulting utf string */
}

gboolean
recode_from_utf (const gchar *src, gchar *dest, guint len)
{
	guint i, j, index = 0;
	guchar c = src[0];
	gchar *ptr;
	gboolean illegal = FALSE;

	memset(dest, 0, len);
	ptr = (gchar *)src;
	for(i = 0; i < len && c != 0; i++) {
		if(c >= 0x20 && c <= 0x7e) { /* ASCII valid for FT2 */
			dest[index++] = c;
		} else {
			for(j = 0; j < UTF_PAIRS; j++) {
				if(!memcmp(known_pairs[j].utf, ptr, known_pairs[j].length)) {
					dest[index++] = known_pairs[j].symbol;
					break;
				}
			}
			if(j == UTF_PAIRS)
				illegal = TRUE;
		}
		ptr = g_utf8_next_char(ptr);
		c = ptr[0];
	}
	return illegal;
}
