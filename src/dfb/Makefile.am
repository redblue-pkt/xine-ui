
fontandpointers = cetus.ttf pointer1.png pointer2.png

EXTRA_DIST = $(fontandpointers)

AM_CFLAGS = $(DIRECTFB_CFLAGS) @BUILD_LIB_STATIC@ @XINE_CFLAGS@ -DFONTDIR=\"$(XINE_SKINDIR)/../fonts/\"
DEBUG_CFLAGS = @BUILD_LIB_STATIC@ @DEBUG_CFLAGS@ @XINE_CFLAGS@

if HAVE_DIRECTFB
dfb_programs = dfbxine
endif

bin_PROGRAMS = ${dfb_programs} 

if HAVE_DIRECTFB
fontdir = $(datadir)/xine/fonts/
font_DATA = $(fontandpointers)
endif

dfbxine_DEPENDENCIES = $(topbuilddir)/src/common/libcommon.la
dfbxine_LDADD = @XINE_LIBS@ $(DIRECTFB_LIBS) $(DYNAMIC_LD_LIBS) -lm $(topbuilddir)/src/common/libcommon.la
dfbxine_SOURCES = options.c \
	dfb.c \
	main.c



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
