noinst_LTLIBRARIES = libcommon.la

AM_CFLAGS = @XINE_CFLAGS@ @X_CFLAGS@ @BUILD_LIB_STATIC@

EXTRA_DIST = getopt.c getopt1.c getopt.h xscreensaver-remote.c xscreensaver-remote.h 

if HAVE_GETOPT_LONG
else
GETOPT_SRC = getopt.c getopt1.c
GETOPT_HDR = getopt.h
endif

if HAVE_X11
##xscreensaver_remote = xscreensaver-remote.c
X11_COMMON_LIBS = $(X_LIBS) $(X_PRE_LIBS) @XV_LIB@ -lX11 $(X_EXTRA_LIBS) -lXext
endif


libcommon_la_SOURCES = $(GETOPT_SRC) utils.c
libcommon_la_LIBADD = $(X11_COMMON_LIBS) $(GICONV_BSD_LIBS) @LTLIBOBJS@

noinst_HEADERS = $(GETOPT_HDR) utils.h libcommon.h

debug:
	@$(MAKE) CFLAGS="$(DEBUG_CFLAGS)"

install-debug: debug
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am


mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#*


maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
