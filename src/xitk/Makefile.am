SUBDIRS = xine-toolkit skins

CFLAGS = @BUILD_LIB_STATIC@ @GLOBAL_CFLAGS@ @X_CFLAGS@ @XINE_CFLAGS@ -I$(top_srcdir)/src/xitk/xine-toolkit

EXTRA_DIST = file_browser.c file_browser.h

bin_PROGRAMS = xine xine-remote

if HAVE_GETOPT_LONG
else
GETOPT_C = getopt.c getopt1.c
GETOPT_H = getopt.h
endif

xine_SOURCES = $(GETOPT_C) utils.c xscreensaver-remote.c \
	main.c event.c control.c playlist.c \
	videowin.c panel.c kbindings.c lirc.c actions.c mrl_browser.c \
	snapshot.c skins.c setup.c viewlog.c lang.c errors.c network.c
xine_DEPENDENTIES = utils.h monitor.h 
# double @XINE_LIBS@ to ensure correct libpath (libtool may add -L...)
xine_LDADD = @XINE_LIBS@ $(top_builddir)/src/xitk/xine-toolkit/libxitk.la \
	@XINE_LIBS@ $(LIRC_LIBS) $(DYNAMIC_LD_LIBS) \
	$(X_LIBS) $(DPMS_LIBS) -lX11 -lm

xine_remote_SOURCES = $(GETOPT_C) xine-remote.c
xine_remote_DEPENDENTIES = xine-remote.c
xine_remote_LDADD = $(TERMCAP_LIB) $(NET_LIBS) $(THREAD_LIBS) $(top_builddir)/readline/libreadline.la

noinst_HEADERS = $(GETOPT_H) event.h playlist.h control.h skins.h kbindings.h \
	panel.h lirc.h actions.h videowin.h mrl_browser.h snapshot.h setup.h viewlog.h \
	lang.h i18n.h errors.h network.h

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

xscreensaver-remote.c:
	$(LN_S) $(top_srcdir)/src/common/xscreensaver-remote.c \
	./xscreensaver-remote.c

$(GETOPT_C):
	$(LN_S) $(top_srcdir)/src/common/getopt.c ./getopt.c
	$(LN_S) $(top_srcdir)/src/common/getopt1.c ./getopt1.c
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
	-rm -f utils.c monitor.c xscreensaver-remote.c xine-remote.c
