SUBDIRS = xine-toolkit skins

AM_CFLAGS = @BUILD_LIB_STATIC@ @X_CFLAGS@ @XINE_CFLAGS@ -I$(top_srcdir)/src/xitk/xine-toolkit

EXTRA_DIST = file_browser.c file_browser.h

if HAVE_TERMCAP
remote = xine-remote
endif

bin_PROGRAMS = xine $(remote)

if HAVE_GETOPT_LONG
else
GETOPT_C = getopt.c getopt1.c
GETOPT_H = getopt.h
endif

## xscreensaver-remote.c
xine_SOURCES = \
	$(GETOPT_C) \
	actions.c \
	config_wrapper.c \
	control.c \
	errors.c \
	event.c \
	kbindings.c \
	lang.c \
	lirc.c \
	main.c \
	mrl_browser.c \
	network.c \
	setup.c \
	skins.c \
	snapshot.c \
	utils.c \
	panel.c \
	playlist.c \
	videowin.c \
	viewlog.c 

xine_DEPENDENTIES = utils.h monitor.h 
# double @XINE_LIBS@ to ensure correct libpath (libtool may add -L...)
xine_LDADD = @XINE_LIBS@ $(top_builddir)/src/xitk/xine-toolkit/libxitk.la \
	$(X_PRE_LIBS) @XINE_LIBS@ $(LIRC_LIBS) $(DYNAMIC_LD_LIBS) \
	$(X_LIBS) $(XTEST_LIBS) -lX11 -lm

xine_remote_SOURCES = $(GETOPT_C) xine-remote.c
xine_remote_DEPENDENTIES = xine-remote.c
xine_remote_LDADD = $(NET_LIBS) $(THREAD_LIBS) $(top_builddir)/readline/libreadline.la $(TERMCAP_LIB)

noinst_HEADERS = \
	$(GETOPT_H) \
	actions.h \
	config_wrapper.h \
	control.h \
	errors.h \
	event.h \
	i18n.h \
	kbindings.h \
	lang.h \
	lirc.h \
	mrl_browser.h \
	network.h \
	panel.h \
	playlist.h \
	setup.h \
	skins.h \
	snapshot.h \
	videowin.h \
	viewlog.h 


$(top_builddir)/readline/libreadline.la:
	@cd $(top_builddir)/readline && $(MAKE) libreadline.la

xine-remote.c:	$(srcdir)/network.c
	$(LN_S) $(srcdir)/network.c ./xine-remote.c

xine-remote.o: xine-remote.c
	$(COMPILE) -DNETWORK_CLIENT -c `test -f $< || echo '$(srcdir)/'`$<
##	@$(MAKE) CFLAGS="$(CFLAGS)  

utils.c:
	$(LN_S) $(top_srcdir)/src/common/utils.c ./utils.c

monitor.c:
	$(LN_S) $(top_srcdir)/src/common/monitor.c ./monitor.c

##xscreensaver-remote.c:
##	$(LN_S) $(top_srcdir)/src/common/xscreensaver-remote.c \
##	./xscreensaver-remote.c

getopt.c:
	$(LN_S) $(top_srcdir)/src/common/getopt.c ./getopt.c

getopt1.c:
	$(LN_S) $(top_srcdir)/src/common/getopt1.c ./getopt1.c

getopt.h:
	$(LN_S) $(top_srcdir)/src/common/getopt.h ./getopt.h

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
	-rm -f utils.c monitor.c xine-remote.c
## xscreensaver-remote.c 
