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
kbindings_man_LDADD = @XINE_LIBS@ 
	     $(top_builddir)/src/xitk/xine-toolkit/libxitk.a \
	     $(top_builddir)/src/xitk/oxine/liboxine.a \
	     $(top_builddir)/src/common/libcommon.a \
	     $(X_PRE_LIBS) $(LIRC_LIBS) $(DYNAMIC_LD_LIBS) \
	     $(X_LIBS) $(XTEST_LIBS) $(CURL_LIBS) $(NVTVSIMPLE_LIBS) \
	     -lX11 -lm
kbindings_man_SOURCES = kbindings_man.c

xine_SOURCES = \
	actions.c \
	config_wrapper.c \
	control.c \
	download.c \
	errors.c \
	event.c \
	event_sender.c \
	file_browser.c \
	frequencies.c \
	help.c \
	kbindings.c \
	lang.c \
	lirc.c \
	main.c \
	mediamark.c \
	menus.c \
	mrl_browser.c \
	network.c \
	osd.c \
	panel.c \
	playlist.c \
	post.c \
	session.c \
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

xine_LDADD = $(XINE_LIBS) \
	     $(top_builddir)/src/xitk/xine-toolkit/libxitk.a \
             $(top_builddir)/src/xitk/Imlib-light/libImlib-light.a \
	     $(top_builddir)/src/xitk/oxine/liboxine.a \
	     $(top_builddir)/src/common/libcommon.a \
             $(top_builddir)/src/common/libcommonx.a \
	     $(X_PRE_LIBS) $(LIRC_LIBS) $(DYNAMIC_LD_LIBS) $(XFT_LIBS) \
	     $(X_LIBS) $(XTEST_LIBS) $(CURL_LIBS) $(NVTVSIMPLE_LIBS) \
	     -lX11 -lm $(DBG_LIBS) $(XV_LIB)

xine_remote_CFLAGS = $(AM_CFLAGS) -DNETWORK_CLIENT
xine_remote_SOURCES = network.c
xine_remote_DEPENDENCIES = $(top_builddir)/src/common/libcommon.a
xine_remote_LDADD = $(LTLIBINTL) $(NET_LIBS) $(THREAD_LIBS) $(READLINE_LIBS) $(NCURSES_LIB) \
	$(top_builddir)/src/common/libcommon.a $(DBG_LIBS)

session_ctrl_CFLAGS = $(AM_CFLAGS) -DCTRL_TEST
session_ctrl_LDADD = @XINE_LIBS@ $(top_builddir)/src/common/libcommon.a $(DBG_LIBS)
session_ctrl_SOURCES = session.c

noinst_HEADERS = \
	actions.h \
	common.h \
	config_wrapper.h \
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
	lang.h \
	lirc.h \
	mediamark.h \
	menus.h \
	mrl_browser.h \
	network.h \
	osd.h \
	panel.h \
	playlist.h \
	post.h \
	session.h \
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

debug:
	@$(MAKE) CFLAGS="$(DEBUG_CFLAGS) -Ixine-toolkit/"

install-debug: debug
	@list='$(SUBDIRS)'; for subdir in $$list; do \
	  (cd $$subdir && $(MAKE) $@) || exit;\
	done;
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#*

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
