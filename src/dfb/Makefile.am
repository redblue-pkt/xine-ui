
fontandpointers = cetus.ttf pointer1.png pointer2.png

EXTRA_DIST = $(fontandpointers)

CFLAGS = $(DIRECTFB_CFLAGS) @BUILD_LIB_STATIC@ @CFLAGS@ @XINE_CFLAGS@ -DFONTDIR=\"$(XINE_SKINDIR)/../fonts/\"
DEBUG_CFLAGS = @BUILD_LIB_STATIC@ @DEBUG_CFLAGS@ @XINE_CFLAGS@

if HAVE_DIRECTFB
dfb_programs = dfbxine
endif

if HAVE_GETOPT_LONG
else
GETOPT_C = \
	$(top_srcdir)/src/common/getopt.c \
	$(top_srcdir)/src/common/getopt1.c

GETOPT_H = \
 	$(top_srcdir)/src/common/getopt.h
endif

bin_PROGRAMS = ${dfb_programs} 

if HAVE_DIRECTFB
fontdir = $(datadir)/xine/fonts/
font_DATA = $(fontandpointers)
endif

dfbxine_SOURCES = $(GETOPT_C) \
	$(top_srcdir)/src/common/utils.c \
	options.c \
	dfb.c \
	main.c

dfbxine_DEPENDENTIES = utils.h
dfbxine_LDADD = @XINE_LIBS@ $(DIRECTFB_LIBS) $(DYNAMIC_LD_LIBS) -lm

noinst_HEADERS = dfb.h

debug:
	@$(MAKE) CFLAGS="-DFONTDIR=\\\"$(XINE_SKINDIR)/../fonts/\\\" $(DEBUG_CFLAGS)"

install-debug: debug
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#*

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
	-rm -f utils.c
