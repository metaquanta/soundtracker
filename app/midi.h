/*
 * Copyright (C) 2000 Luc Tanguay <luc.tanguay@bell.ca>
 * Copyright (C) 1998-2001 Michael Krause
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

#ifndef _MIDI_H
#define _MIDI_H

#include <config.h>

#if defined(DRIVER_ALSA_MIDI)

#include <alsa/version.h>
#if SND_LIB_MAJOR == 1
#define ALSA_PCM_OLD_HW_PARAMS_API
#endif
#include <alsa/asoundlib.h>

/*** Structures ***/

/* Function prototypes */

void midi_init(void);
void midi_fini(void);

#endif

#endif /* _MIDI_H */
