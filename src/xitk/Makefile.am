SUBDIRS = xine-toolkit skins

CFLAGS = @BUILD_LIB_STATIC@ @GLOBAL_CFLAGS@ @X_CFLAGS@ @XINE_CFLAGS@ -I$(top_srcdir)/src/xitk/xine-toolkit

EXTRA_DIST = file_browser.c file_browser.h

bin_PROGRAMS = xine

if HAVE_GETOPT_LONG
else
GETOPT_C = getopt.c getopt1.c
GETOPT_H = getopt.h
endif

xine_SOURCES = $(GETOPT_C) utils.c xscreensaver-remote.c \
	main.c event.c control.c playlist.c parseskin.c \
	videowin.c panel.c lirc.c actions.c mrl_browser.c #monitor.c
xine_DEPENDENTIES = utils.h monitor.h 
xine_LDADD = $(top_builddir)/src/xitk/xine-toolkit/libxitk.la \
	@XINE_LIBS@ $(LIRC_LIBS) $(DYNAMIC_LD_LIBS) \
	$(X_LIBS) $(DPMS_LIBS) -lX11 -lm

noinst_HEADERS = $(GETOPT_H) event.h parseskin.h playlist.h control.h \
	panel.h lirc.h actions.h videowin.h mrl_browser.h

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
	-rm -f utils.c monitor.c xscreensaver-remote.c
