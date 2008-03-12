AM_CFLAGS = @XINE_CFLAGS@ $(X_CFLAGS) -DFONTDIR=\"$(XINE_SKINDIR)/../fonts/\"

bin_PROGRAMS = fbxine 

fbxine_DEPENDENCIES = $(top_builddir)/src/common/libcommon.a
fbxine_LDADD = @XINE_LIBS@ $(LIRC_LIBS) -lm $(top_builddir)/src/common/libcommon.a

fbxine_SOURCES = main.c lirc.c actions.c keys.c options.c callback.c stdctl.c post.c osd.c config_wrapper.c
noinst_HEADERS = main.h lirc.h actions.h keys.h options.h callback.h stdctl.h post.h osd.h config_wrapper.h

$(top_builddir)/src/common/libcommon.a:
	@cd $(top_builddir)/src/common && $(MAKE) libcommon.a
