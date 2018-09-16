
noinst_LIBRARIES = libdrivers.a

libdrivers_a_SOURCES = dummy-drivers.c file-output.c

if DRIVER_ALSA_09x
  libdrivers_a_SOURCES += alsa1x.c
endif

if DRIVER_JACK
  libdrivers_a_SOURCES += jack-output.c
endif

if DRIVER_SDL
  libdrivers_a_SOURCES += sdl-output.c
endif

INCLUDES = -I.. ${ST_S_JACK_INCLUDES}

EXTRA_DIST = dsound-output.c
