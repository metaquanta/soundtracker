dnl Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([soundtracker], [0.6.8-git], [soundtracker-discuss@soundtracker.org])
AC_CONFIG_SRCDIR([app/endian-conv.h])
AM_INIT_AUTOMAKE([1.15])

AM_CONFIG_HEADER(config.h)

dnl -----------------------------------------------------------------------
dnl Test for GCC
dnl -----------------------------------------------------------------------

AC_PROG_CC
AC_PATH_PROG(sedpath, sed)

if test $GCC != yes; then
	echo "Fatal error: Need gcc"
	exit 1
fi

CFLAGS="$CFLAGS -Wall -DG_DISABLE_DEPRECATED"

dnl -----------------------------------------------------------------------
dnl Test for GAS
dnl -----------------------------------------------------------------------

AC_ARG_ENABLE(asm,
[  --disable-asm           Disable use of assembler optimizations],
asm_support=no)

AM_CONDITIONAL(NO_ASM, test x$asm_support = xno)
AM_PROG_AS

if test x$asm_support = xno; then
  AC_DEFINE([NO_ASM], 1, [Set if no assembler support is wanted])
fi

dnl -----------------------------------------------------------------------
dnl Test for poll()
dnl -----------------------------------------------------------------------

AC_CHECK_HEADER(sys/poll.h,
	[AC_CHECK_FUNCS(poll, [], 
		[AC_CHECK_FUNC(select, [], [AC_MSG_ERROR(no poll and no select?!?)])]
		)
	]
)

dnl -----------------------------------------------------------------------
dnl Initialize NLS
dnl -----------------------------------------------------------------------

dnl Also add new catalogs to soundtracker.spec (%build)!
#ALL_LINGUAS="da de es fr gl hr it ja no pl ru sk sl sv"
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.7])

dnl -----------------------------------------------------------------------
dnl Test for GTK+ / GooCanvas
dnl -----------------------------------------------------------------------
SND_MODULES="gtk+-2.0 >= 2.20 gthread-2.0 gmodule-2.0 x11"

PKG_CHECK_MODULES(GTK, $SND_MODULES)
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

canvas=yes
AC_ARG_ENABLE(canvas,
[  --disable-canvas         disable use of GooCanvas],
        canvas=$enableval, canvas=yes)

if test "$canvas" = yes; then
SND_MODULES="goocanvas"
  PKG_CHECK_MODULES(CANVAS, $SND_MODULES)
  if test "_$CANVAS_CFLAGS" = _ ; then
    canvas=no
    AC_MSG_RESULT([GooCanvas not found, building without it.])
  else
	AC_SUBST(CANVAS_CFLAGS)
	AC_SUBST(CANVAS_LIBS)
    LIBS="$LIBS $CANVAS_LIBS"
    CFLAGS="$CFLAGS $CANVAS_CFLAGS"
    AC_DEFINE([USE_CANVAS], 1, [Set if GooCanvas support is compiled in])
  fi
fi

LIBS="$LIBS $GTK_LIBS"
CFLAGS="$CFLAGS $GTK_CFLAGS"

AM_CONDITIONAL(USE_CANVAS, test "x$canvas" = "xyes")

dnl -----------------------------------------------------------------------
dnl Test for GtkHTML
dnl -----------------------------------------------------------------------

html=yes
AC_ARG_ENABLE(gtkhtml,
[  --disable-gtkhtml       disable use of gnome],
        html=$enableval, html=yes)

if test "$html" = yes; then
SND_MODULES="libgtkhtml-3.14"
  PKG_CHECK_MODULES(GTKHTML, $SND_MODULES)
  if test "_$GTKHTML_CFLAGS" = _ ; then
   html=no
    AC_MSG_RESULT([GtkHTML not found, building without it.])
  else
	AC_SUBST(GTKHTML_CFLAGS)
	AC_SUBST(GTKHTML_LIBS)
    LIBS="$LIBS $GTKHTML_LIBS"
    CFLAGS="$CFLAGS $GTKHTML_CFLAGS"
    AC_DEFINE([USE_GTKHTML], 1, [Set if GtkHTML support is compiled in])
  fi
fi

AM_CONDITIONAL(USE_GTKHTML, test "x$html" = "xyes")

dnl -----------------------------------------------------------------------
dnl Test for gdk-pixbuf
dnl -----------------------------------------------------------------------

AC_ARG_ENABLE(gdk-pixbuf,
[  --disable-gdk-pixbuf    Disable use of gdk-pixbuf (default = try)],
gdkpixbuf_support=no)

no_gdkpixbuf=1;
if test x$gdkpixbuf_support != xno; then
SND_MODULES="gdk-pixbuf-2.0 >= 0.15.0"
  PKG_CHECK_MODULES(GDK_PIXBUF, $SND_MODULES)
  AC_SUBST(GDK_PIXBUF)
  AC_SUBST(GDK_PIXBUF)
  LIBS="$LIBS $GDK_PIXBUF_LIBS"
  CFLAGS="$CFLAGS $GDK_PIXBUF_CFLAGS"
dnl  AM_PATH_GDK_PIXBUF(0.8.0,
dnl		     [CFLAGS="$CFLAGS $GDK_PIXBUF_CFLAGS"
dnl		      LIBS="$LIBS $GDK_PIXBUF_LIBS"
dnl		      no_gdkpixbuf=0],
dnl		     [AC_MSG_RESULT([---> Building without GDK_PIXBUF (ignore error above).])])
  no_gdkpixbuf=0;
fi

if test "x${no_gdkpixbuf}" = "x1"; then
  AC_DEFINE(NO_GDK_PIXBUF, 1, [Set to 1 if libgdkpixbuf is not present.])
fi
AM_CONDITIONAL(NO_GDK_PIXBUF, test "x${no_gdkpixbuf}" = "x1")

dnl -----------------------------------------------------------------------
dnl Test for libsndfile
dnl -----------------------------------------------------------------------

sndfile_support=yes
AC_ARG_ENABLE(sndfile,
[  --disable-sndfile       Disable usage of libsndfile (default = try)],
sndfile_support=no)

ac_cv_sndfile=0

if test x$sndfile_support = xyes ; then
        PKG_CHECK_MODULES(SNDFILE, sndfile > 1.0.1, ac_cv_sndfile=1, ac_cv_sndfile=0)
        if test x$ac_cv_sndfile = x1 ; then
                CFLAGS="$CFLAGS $SNDFILE_CFLAGS"
                LIBS="$LIBS $SNDFILE_LIBS"
        fi
fi

AC_DEFINE_UNQUOTED(USE_SNDFILE,${ac_cv_sndfile},
	[Set to 1 if you wish to use libsndfile.])
		
dnl -----------------------------------------------------------------------
dnl Test for audiofile library (if libsndfile is not found or disabled)
dnl -----------------------------------------------------------------------

if test x$ac_cv_sndfile != x1 ; then
	PKG_CHECK_MODULES(AUDIOFILE, audiofile > 0.1.5, ac_cv_audiofile=1,
		[AC_MSG_RESULT([* No sample I/O library found, disabling sample loader (ignore error above).])]
	)
	if test x$ac_cv_audiofile = x1; then
		CFLAGS="$CFLAGS $AUDIOFILE_CFLAGS"
		LIBS="$LIBS $AUDIOFILE_LIBS"
		AF_VERSION=$(pkg-config --modversion audiofile | sed "s/^[[0-9]]*\.//;s/\..*$//")
		AC_DEFINE_UNQUOTED([AUDIOFILE_VERSION],
		          ${AF_VERSION},
		          [Audiofile minor version for use in headers])
	fi
fi

dnl -----------------------------------------------------------------------
dnl Checks for header files
dnl -----------------------------------------------------------------------

AC_HEADER_STDC
AC_CHECK_HEADERS(dlfcn.h)
AC_CHECK_FUNCS(setresuid)

dnl -----------------------------------------------------------------------
dnl Test for OSS headers
dnl -----------------------------------------------------------------------

AC_ARG_ENABLE(oss,
[  --disable-oss           Disable OSS driver (default = try)],
oss_support=no)

if test x$oss_support != xno; then
  AC_CHECK_HEADERS(sys/soundcard.h machine/soundcard.h soundcard.h)
  if test "${ac_cv_header_sys_soundcard_h}" = "yes" || \
     test "${ac_cv_header_machine_soundcard_h}" = "yes" || \
     test "${ac_cv_header_soundcard_h}" = "yes"; then
     oss_support=yes
     AC_DEFINE([DRIVER_OSS], 1, [Set if OSS driver wanted])
     case `uname` in
        OpenBSD*)
        LIBS="$LIBS -lossaudio"
        ;;
	NetBSD*)
        AC_CHECK_LIB(ossaudio, _oss_ioctl, LIBS="$LIBS -lossaudio")
	;;
     esac
  else
    oss_support=no
  fi
fi

AM_CONDITIONAL(DRIVER_OSS, test x$oss_support = xyes)

dnl -----------------------------------------------------------------------
dnl Test for ALSA
dnl -----------------------------------------------------------------------

AC_ARG_ENABLE(alsa,
[  --disable-alsa          Disable ALSA driver (default = try)],
alsa_support=no)

if test x$alsa_support != xno; then
  AM_PATH_ALSA([1.0.0], have_alsa=yes, have_alsa=no)
  if test x$have_alsa = xyes; then
    AC_DEFINE([DRIVER_ALSA], 1, [Set if ALSA driver wanted])
    CFLAGS="$CFLAGS $ALSA_CFLAGS"
    LIBS="$LIBS $ALSA_LIBS"
  fi
fi

AM_CONDITIONAL(DRIVER_ALSA, test x$have_alsa = xyes)

dnl MIDI code still at 0.9 API level, need to modernize...
AM_CONDITIONAL(DRIVER_ALSA_MIDI, false)

dnl -----------------------------------------------------------------------
dnl Test for ESD
dnl -----------------------------------------------------------------------

AC_ARG_ENABLE(esd,
[  --disable-esd           Disable ESD driver (default = try)],
esd_support=no)

if test x$esd_support != xno; then
  AM_PATH_ESD(0.2.8, have_esd=yes, have_esd=no)
  AC_CHECK_FUNCS(esd_play_stream)
  if test x$have_esd = xyes; then
    AC_DEFINE([DRIVER_ESD], 1, [Set if ESD driver wanted])
    CFLAGS="$CFLAGS $ESD_CFLAGS"
    LIBS="$LIBS $ESD_LIBS"
  fi
fi

AM_CONDITIONAL(DRIVER_ESD, test x$have_esd = xyes)

dnl -----------------------------------------------------------------------
dnl Test for JACK
dnl
dnl defines: ST_S_JACK_LIBS, ST_S_JACK_INCLUDES, DRIVER_JACK
dnl -----------------------------------------------------------------------

AC_ARG_ENABLE(jack,
[  --disable-jack	   Disable JACK support (default = try)],
jack_support=no)

if test x$jack_support != xno; then
    AC_CHECK_HEADER(jack/jack.h,jack_support=yes,jack_support=no)
    AC_ARG_WITH(jack,
	    [  --with-jack=DIR	Compile against JACK installed in DIR],
	    [
	        ST_S_JACK_LIBS="-L${withval}/lib"
		ST_S_JACK_INCLUDES="-I${withval}/include"
		jack_support=yes
	    ])
  
    if test x$jack_support = xyes; then
	ST_S_JACK_LIBS="${ST_S_JACK_LIBS} -ljack -lpthread -ldl"
	AC_DEFINE([DRIVER_JACK], 1, [Set if JACK support wanted])
    fi
  
    AC_SUBST(ST_S_JACK_LIBS)
    AC_SUBST(ST_S_JACK_INCLUDES)
fi

AM_CONDITIONAL(DRIVER_JACK, test x$jack_support = xyes)

dnl -----------------------------------------------------------------------
dnl Test for SGI audio
dnl -----------------------------------------------------------------------

AC_ARG_ENABLE(sgi,
[  --disable-sgi           Disable SGI driver (default = try)],
sgi_support=no)

case `uname` in
  IRIX*)
    if test x$sgi_support != xno; then
    AC_CHECK_HEADER(dmedia/audio.h)
    if test $ac_cv_header_dmedia_audio_h = yes
    then
      AC_CHECK_LIB(audio,ALseterrorhandler,driver_sgi=yes)
      if test $driver_sgi = yes
      then
        AC_DEFINE([DRIVER_SGI], 1, [Set if SGI driver wanted])
        LIBS="$LIBS -laudio"
	irix_support=yes
      fi
    fi
  fi
  ;;
esac

AM_CONDITIONAL(DRIVER_IRIX, test x$irix_support = xyes)

dnl -----------------------------------------------------------------------
dnl Test for Sun audio
dnl -----------------------------------------------------------------------

AC_ARG_ENABLE(sun,
[  --disable-sun           Disable Sun driver (default = try)],
sun_support=no)

if test x$sun_support != xno; then
  AC_CHECK_HEADERS(sys/audioio.h)
  if test "${ac_cv_header_sys_audioio_h}" = "yes"; then
    AC_DEFINE([DRIVER_SUN], 1, [Set if Sun audio driver wanted])
  fi
fi

AM_CONDITIONAL(DRIVER_SUN, test "${ac_cv_header_sys_audioio_h}" = "yes")

dnl -----------------------------------------------------------------------
dnl Test for SDL audio
dnl -----------------------------------------------------------------------

AC_ARG_ENABLE(sdl,
[  --disable-sdl           Disable SDL driver (default = try)],
sdl_support=no)

if test x$sdl_support != xno; then
  AM_PATH_SDL(1.2.0, have_sdl=yes, have_sdl=no)
  if test x$have_sdl = xyes; then
    AC_DEFINE([DRIVER_SDL], 1, [Set if SDL audio driver wanted])
    CFLAGS="$CFLAGS $SDL_CFLAGS"
    LIBS="$LIBS $SDL_LIBS"
  fi
fi

AM_CONDITIONAL(DRIVER_SDL, test x$have_sdl = xyes)

dnl -----------------------------------------------------------------------
dnl Other tests
dnl -----------------------------------------------------------------------

AC_PROG_RANLIB

AC_SEARCH_LIBS([rint], [m], [], [
  AC_MSG_ERROR([unable to find the rint() function])
])

AC_C_BIGENDIAN

dnl Check if gtk+ tweaks are used (may cause problems in runtime).
AC_ARG_ENABLE(hacks,
[  --disable-hacks           Disable Gtk+ hacks (default = yes)],
gtk_hacks=no)

if test x$gtk_hacks != xno; then
  AC_DEFINE([GTK_HACKS], 1, [Set if gtk hacks are used])
fi

dnl Check if soundtracker shall be installed suid root
AC_ARG_ENABLE([suid_root],
AS_HELP_STRING([--enable-suid_root], [Install ST suid root (default = no)]),
suid_root=yes)

AM_CONDITIONAL(SUID_ROOT, test x$suid_root = xyes)

dnl Set PREFIX in config.h.
if test "x${prefix}" = "xNONE"; then
  AC_DEFINE_UNQUOTED(PREFIX, "${ac_default_prefix}", [Kein Mensch weiss, warum])
else
  AC_DEFINE_UNQUOTED(PREFIX, "${prefix}", [Kein Mensch weiss, warum])
fi

AC_DEFUN([AC_DEFINE_DIR], [
  prefix_NONE=
  exec_prefix_NONE=
  test "x$prefix" = xNONE && prefix_NONE=yes && prefix=$ac_default_prefix
  test "x$exec_prefix" = xNONE && exec_prefix_NONE=yes && exec_prefix=$prefix
dnl In Autoconf 2.60, ${datadir} refers to ${datarootdir}, which in turn
dnl refers to ${prefix}.  Thus we have to use `eval' twice.
  eval ac_define_dir="\"[$]$2\""
  eval ac_define_dir="\"$ac_define_dir\""
  AC_SUBST($1, "$ac_define_dir")
  AC_DEFINE_UNQUOTED($1, "$ac_define_dir", [$3])
  test "$prefix_NONE" && prefix=NONE
  test "$exec_prefix_NONE" && exec_prefix=NONE
])
AC_DEFINE_DIR(DATADIR, datadir, [Data directory])

AC_OUTPUT([
Makefile
app/Makefile
app/drivers/Makefile
app/mixers/Makefile
po/Makefile.in
doc/Makefile
])
