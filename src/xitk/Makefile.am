SUBDIRS = Imlib-light xine-toolkit skins

AM_CFLAGS = @BUILD_LIB_STATIC@ @X_CFLAGS@ @XINE_CFLAGS@ @CURL_CFLAGS@ -I$(top_srcdir)/src/xitk/xine-toolkit

if HAVE_TERMCAP
remote = xine-remote
endif

if HAVE_X11
xine_ui = xine $(remote)
endif

bin_PROGRAMS = $(xine_ui)
EXTRA_PROGRAMS = session_ctrl

## xscreensaver-remote.c
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
	stream_infos.c \
	tvset.c \
	videowin.c \
	viewlog.c

xine_DEPENDENCIES = $(top_builddir)/src/common/libcommon.la $(top_builddir)/src/xitk/xine-toolkit/libxitk.la

# double @XINE_LIBS@ to ensure correct libpath (libtool may add -L...)
xine_LDADD = @XINE_LIBS@ $(top_builddir)/src/common/libcommon.la \
	     $(top_builddir)/src/xitk/xine-toolkit/libxitk.la \
	     $(X_PRE_LIBS) @XINE_LIBS@ $(LIRC_LIBS) $(DYNAMIC_LD_LIBS) \
	     $(X_LIBS) $(XTEST_LIBS) $(CURL_LIBS) -lX11 -lm $(DBG_LIBS)

xine_remote_SOURCES = xine-remote.c
xine_remote_DEPENDENCIES = $(top_builddir)/readline/libreadline.la \
			   $(top_builddir)/src/common/libcommon.la \
			   xine-remote.c
xine_remote_LDADD = $(NET_LIBS) $(THREAD_LIBS) \
	$(top_builddir)/readline/libreadline.la $(TERMCAP_LIB) \
	$(top_builddir)/src/common/libcommon.la $(DBG_LIBS)

session_ctrl_LDADD = @XINE_LIBS@ $(top_builddir)/src/common/libcommon.la $(DBG_LIBS)

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
	stream_infos.h \
	tvset.h \
	videowin.h \
	viewlog.h

$(top_builddir)/readline/libreadline.la:
	@cd $(top_builddir)/readline && $(MAKE) libreadline.la

$(top_builddir)/src/common/libcommon.la:
	@cd $(top_builddir)/src/common && $(MAKE) libcommon.la

xine-remote.c:	$(srcdir)/network.c
	$(LN_S) $(srcdir)/network.c ./xine-remote.c

xine-remote.o: xine-remote.c
	$(COMPILE) -DNETWORK_CLIENT -c `test -f $< || echo '$(srcdir)/'`$<

session_ctrl.c: $(srcdir)/session.c
	$(LN_S) $(srcdir)/session.c ./session_ctrl.c

session_ctrl.o: session_ctrl.c
	$(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) \
	$(CPPFLAGS) $(AM_CFLAGS) $(DEBUG_CFLAGS) -DCTRL_TEST -c `test -f $< || echo '$(srcdir)/'`$<

debug:
	@$(MAKE) CFLAGS="$(DEBUG_CFLAGS) -Ixine-toolkit/"

install-debug: debug
	@list='$(SUBDIRS)'; for subdir in $$list; do \
	  (cd $$subdir && $(MAKE) $@) || exit;\
	done;
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#* session_ctrl

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
