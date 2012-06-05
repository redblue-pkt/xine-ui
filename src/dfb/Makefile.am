include $(top_srcdir)/misc/Makefile.quiet
include $(top_srcdir)/src/common/Makefile.common

fontandpointers = cetus.ttf pointer1.png pointer2.png

EXTRA_DIST = $(fontandpointers)

AM_CFLAGS = $(DIRECTFB_CFLAGS) @XINE_CFLAGS@ -DFONTDIR=\"$(XINE_SKINDIR)/../fonts/\"

if HAVE_DIRECTFB
dfb_programs = dfbxine
endif

bin_PROGRAMS = ${dfb_programs} 

if HAVE_DIRECTFB
fontdir = $(datadir)/xine/fonts/
font_DATA = $(fontandpointers)
endif

dfbxine_DEPENDENCIES = $(LIBCOMMON)
dfbxine_LDADD = $(DIRECTFB_LIBS) -lm $(LIBCOMMON) $(XINE_LIBS)
dfbxine_SOURCES = options.c \
	dfb.c \
	main.c

noinst_HEADERS = dfb.h
