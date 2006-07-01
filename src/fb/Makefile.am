AM_CFLAGS = @XINE_CFLAGS@ -DFONTDIR=\"$(XINE_SKINDIR)/../fonts/\"

DEBUG_CFLAGS = @DEBUG_CFLAGS@ @XINE_CFLAGS@

bin_PROGRAMS = fbxine 

fbxine_DEPENDENCIES = $(top_builddir)/src/common/libcommon.a
fbxine_LDADD = @XINE_LIBS@ $(LIRC_LIBS) $(DYNAMIC_LD_LIBS) -lm $(top_builddir)/src/common/libcommon.a

fbxine_SOURCES = main.c lirc.c actions.c keys.c options.c callback.c stdctl.c post.c osd.c config_wrapper.c
noinst_HEADERS = main.h lirc.h actions.h keys.h options.h callback.h stdctl.h post.h osd.h config_wrapper.h

$(top_builddir)/src/common/libcommon.a:
	@cd $(top_builddir)/src/common && $(MAKE) libcommon.a

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
