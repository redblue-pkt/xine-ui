noinst_LTLIBRARIES = libcommon.la

AM_CFLAGS = @XINE_CFLAGS@ @BUILD_LIB_STATIC@

EXTRA_DIST = getopt.c getopt1.c getopt.h

if HAVE_GETOPT_LONG
else
GETOPT_SRC = getopt.c \
	     getopt1.c
GETOPT_HDR = getopt.h
endif

libcommon_la_SOURCES = \
	$(GETOPT_SRC) \
	utils.c \
	xscreensaver-remote.c

noinst_HEADERS = \
	$(GETOPT_HDR) \
	utils.h \
	xscreensaver-remote.h

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
