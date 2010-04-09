include $(top_srcdir)/src/common/Makefile.common

AM_CFLAGS = @XINE_CFLAGS@ $(X_CFLAGS) -DFONTDIR=\"$(XINE_SKINDIR)/../fonts/\"

if HAVE_FB
fb_programs = fbxine
endif

bin_PROGRAMS = ${fb_programs}

fbxine_DEPENDENCIES = $(LIBCOMMON)
fbxine_LDADD = @XINE_LIBS@ $(LIRC_LIBS) -lm $(THREAD_LIBS) $(LIBCOMMON)

fbxine_SOURCES = main.c actions.c keys.c options.c callback.c stdctl.c post.c osd.c
noinst_HEADERS = main.h actions.h keys.h options.h callback.h stdctl.h post.h osd.h

if HAVE_LIRC
fbxine_SOURCES += lirc.c lirc.h
endif
