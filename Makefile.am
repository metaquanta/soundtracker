## Process this file with automake to produce Makefile.in

SUBDIRS = po doc app

EXTRA_DIST = m4/ChangeLog  config.rpath m4/ChangeLog  TODO FAQ ABOUT-NLS soundtracker.spec \
	soundtracker.desktop \
	soundtracker_splash.png flat.xpm sharp.xpm muted.png \
	downarrow.xpm rightarrow.xpm play.xpm play_cur.xpm stop.xpm lock.xpm \
	ChangeLog-v0.5 ChangeLog-v0.3 ChangeLog-v0.1 soundtracker.xml \
	envelope-box.xml cheat-sheet.*.html soundtracker-icon.png \
	soundtracker.1

man_MANS = soundtracker.1
dist_bin_SCRIPTS = soundtracker_convert_config

#util_DATA = soundtracker.desktop
#utildir = $(datadir)/gnome/apps/Multimedia

st_DATA = soundtracker_splash.png flat.xpm sharp.xpm muted.png downarrow.xpm \
	rightarrow.xpm play.xpm play_cur.xpm stop.xpm lock.xpm soundtracker.xml \
	cheat-sheet.*.html envelope-box.xml soundtracker-icon.png
stdir = $(pkgdatadir)

bindist:
	./configure --disable-gnome --disable-nls --disable-alsa --disable-esd --disable-splash --disable-sndfile
	make
	rm -rf soundtracker-$(VERSION)-bin
	mkdir soundtracker-$(VERSION)-bin
	cp -a README FAQ NEWS AUTHORS INSTALL \
		soundtracker-$(VERSION)-bin
	strip app/soundtracker -o soundtracker-$(VERSION)-bin/soundtracker
	tar zcf soundtracker-$(VERSION)-bin.tar.gz \
		soundtracker-$(VERSION)-bin
	./configure

## disthelp is to be executed by root so that the rpm -tb works well
disthelp:
	make dist
	cp soundtracker-$(VERSION).tar.gz /home/rawstyle/dev/soundtracker-WWW/dl/v0.6/
	make bindist
	cp soundtracker-$(VERSION)-bin.tar.gz /home/rawstyle/dev/soundtracker-WWW/dl/v0.6/
	/home/rawstyle/dev/soundtracker/bin/rpm -tb soundtracker-$(VERSION).tar.gz
	cp /usr/src/packages/RPMS/i386/soundtracker-$(VERSION)-* /home/rawstyle/dev/soundtracker-WWW/dl/v0.6/
	chown -R rawstyle.users /home/rawstyle/dev/soundtracker/*
	chown -R rawstyle.users /home/rawstyle/dev/soundtracker-WWW/*

ACLOCAL_AMFLAGS = -I m4
EXTRA_DIST += m4
