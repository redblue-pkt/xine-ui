SUBDIRS = xine-toolkit skins

AM_CFLAGS = @BUILD_LIB_STATIC@ @X_CFLAGS@ @XINE_CFLAGS@ -I$(top_srcdir)/src/xitk/xine-toolkit

EXTRA_DIST = file_browser.c file_browser.h

#if HAVE_TERMCAP
#remote = xine-remote
#endif
remote=

bin_PROGRAMS = xine $(remote)


#COMMON_DIR = $(top_srcdir)/src/common

#if HAVE_GETOPT_LONG
#else
#GETOPT_C = $(COMMON_DIR)/getopt.c \
#	   $(COMMON_DIR)/getopt1.c
#GETOPT_H = $(COMMON_DIR)/getopt.h
#endif

## xscreensaver-remote.c
xine_SOURCES = \
	actions.c \
	config_wrapper.c \
	control.c \
	errors.c \
	event.c \
	event_sender.c \
	kbindings.c \
	lang.c \
	lirc.c \
	main.c \
	mrl_browser.c \
	network.c \
	setup.c \
	skins.c \
	snapshot.c \
	panel.c \
	playlist.c \
	mediamark.c \
	stream_infos.c \
	videowin.c \
	viewlog.c

xine_DEPENDENCIES = $(top_builddir)/src/common/libcommon.la $(top_builddir)/src/xitk/xine-toolkit/libxitk.la
# double @XINE_LIBS@ to ensure correct libpath (libtool may add -L...)
xine_LDADD = @XINE_LIBS@ $(top_builddir)/src/common/libcommon.la \
	     $(top_builddir)/src/xitk/xine-toolkit/libxitk.la \
	     $(X_PRE_LIBS) @XINE_LIBS@ $(LIRC_LIBS) $(DYNAMIC_LD_LIBS) \
	     $(X_LIBS) $(XTEST_LIBS) -lX11 -lm

# DISABLED CORBA SUPPORT: Must port to new API
#xine_remote_SOURCES = $(GETOPT_C)
#xine_remote_DEPENDENCIES = $(top_builddir)/readline/libreadline.la
#xine_remote_LDADD = $(NET_LIBS) $(THREAD_LIBS) $(top_builddir)/readline/libreadline.la $(TERMCAP_LIB)

noinst_HEADERS = \
	common.h \
	actions.h \
	config_wrapper.h \
	control.h \
	errors.h \
	event.h \
	event_sender.h \
	i18n.h \
	kbindings.h \
	lang.h \
	lirc.h \
	mrl_browser.h \
	network.h \
	panel.h \
	playlist.h \
	mediamark.h \
	setup.h \
	skins.h \
	stream_infos.h \
	snapshot.h \
	videowin.h \
	viewlog.h

# Use this instead of the rule below
#xine_remote_CFLAGS = -DNETWORK_CLIENT

# Old rule
#xine-remote.o: xine-remote.c
#	$(COMPILE) -DNETWORK_CLIENT -c `test -f $< || echo '$(srcdir)/'`$<

##xscreensaver-remote.c:
##	$(LN_S) $(top_srcdir)/src/common/xscreensaver-remote.c \
##	./xscreensaver-remote.c


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
