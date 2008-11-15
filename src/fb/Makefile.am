AM_CFLAGS = @XINE_CFLAGS@ $(X_CFLAGS) -DFONTDIR=\"$(XINE_SKINDIR)/../fonts/\"

bin_PROGRAMS = fbxine 

fbxine_DEPENDENCIES = $(top_builddir)/src/common/libcommon.a
fbxine_LDADD = @XINE_LIBS@ $(LIRC_LIBS) -lm $(THREAD_LIBS) $(top_builddir)/src/common/libcommon.a

fbxine_SOURCES = main.c actions.c keys.c options.c callback.c stdctl.c post.c osd.c
noinst_HEADERS = main.h actions.h keys.h options.h callback.h stdctl.h post.h osd.h

if HAVE_LIRC
fbxine_SOURCES += lirc.c lirc.h
endif

$(top_builddir)/src/common/libcommon.a:
	@cd $(top_builddir)/src/common && $(MAKE) libcommon.a
