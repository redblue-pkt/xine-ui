include $(top_srcdir)/src/common/Makefile.common

SUBDIRS = Imlib-light xine-toolkit skins oxine .

AM_CFLAGS = @X_CFLAGS@ @XINE_CFLAGS@ @CURL_CFLAGS@ @NVTVSIMPLE_CFLAGS@ @READLINE_INCLUDES@ -I$(top_srcdir)/src/xitk/xine-toolkit

if HAVE_READLINE
remote = xine-remote
endif

if HAVE_X11
xine_ui = xine $(remote)
endif

bin_PROGRAMS = $(xine_ui)
EXTRA_PROGRAMS = session_ctrl kbindings_man

kbindings_man_CFLAGS = $(AM_CFLAGS) -DKBINDINGS_MAN
kbindings_man_LDADD = @XINE_LIBS@ $(THREAD_LIBS) $(LIBCOMMON) $(INTLLIBS)
kbindings_man_SOURCES = kbindings_man.c kbindings_common.c

xine_SOURCES = \
	actions.c \
	control.c \
	download.c \
	errors.c \
	event.c \
	event_sender.c \
	file_browser.c \
	frequencies.c \
	help.c \
	kbindings.c \
	kbindings_common.c \
	lang.c \
	main.c \
	mediamark.c \
	menus.c \
	mrl_browser.c \
	network.c \
	osd.c \
	panel.c \
	playlist.c \
	post.c \
	session_common.c \
	session_internal.h \
	session.c \
	session.h \
	setup.c \
	skins.c \
	snapshot.c \
	splash.c \
	stdctl.c \
	stream_infos.c \
	tvout.c \
	tvset.c \
	videowin.c \
	viewlog.c

if HAVE_TAR
xine_SOURCES += skins_download.c skins_internal.h
endif

if HAVE_LIRC
xine_SOURCES += lirc.c lirc.h
endif

xine_LDADD = $(XINE_LIBS) $(LIBCOMMON) $(LIBCOMMONX) \
	     $(top_builddir)/src/xitk/xine-toolkit/libxitk.a \
             $(top_builddir)/src/xitk/Imlib-light/libImlib-light.a \
	     $(top_builddir)/src/xitk/oxine/liboxine.a \
	     $(X_PRE_LIBS) $(LIRC_LIBS) $(XFT_LIBS) \
	     $(X_LIBS) $(XTEST_LIBS) $(XSSAVER_LIBS) $(CURL_LIBS) $(NVTVSIMPLE_LIBS) \
	     -lX11 -lXext -lm $(DBG_LIBS) $(XV_LIB) $(LIBPNG_LIBS) $(JPEG_LIBS) \
	     $(INTLLIBS) $(THREAD_LIBS) $(RT_LIBS)

xine_remote_CFLAGS = $(AM_CFLAGS) -DNETWORK_CLIENT
xine_remote_SOURCES = network.c
xine_remote_DEPENDENCIES = $(LIBCOMMON)
xine_remote_LDADD = $(LTLIBINTL) $(NET_LIBS) $(THREAD_LIBS) $(READLINE_LIBS) \
	$(LIBCOMMON)

session_ctrl_CFLAGS = $(AM_CFLAGS)
session_ctrl_LDADD = @XINE_LIBS@ $(LIBCOMMON) $(DBG_LIBS)
session_ctrl_SOURCES = session_common.c session_internal.h session.h ctrl_test.c

noinst_HEADERS = \
	actions.h \
	common.h \
	control.h \
	download.h \
	errors.h \
	event.h \
	event_sender.h \
	file_browser.h \
	frequencies.h \
	i18n.h \
	help.h \
	kbindings.h \
	kbindings_common.h \
	lang.h \
	mediamark.h \
	menus.h \
	mrl_browser.h \
	network.h \
	osd.h \
	panel.h \
	playlist.h \
	post.h \
	setup.h \
	skins.h \
	snapshot.h \
	splash.h \
	stdctl.h \
	stream_infos.h \
	tvout.h \
	tvset.h \
	videowin.h \
	viewlog.h
