AM_CFLAGS = @BUILD_LIB_STATIC@ @XINE_CFLAGS@ -DFONTDIR=\"$(XINE_SKINDIR)/../fonts/\"

DEBUG_CFLAGS = @BUILD_LIB_STATIC@ @DEBUG_CFLAGS@ @XINE_CFLAGS@

if HAVE_NCURSES
fb_programs = fbxine
endif

bin_PROGRAMS = ${fb_programs} 

fbxine_DEPENDENCIES = $(top_builddir)/src/common/libcommon.la
fbxine_LDADD = @XINE_LIBS@ $(LIRC_LIBS) $(DYNAMIC_LD_LIBS) -lm $(top_builddir)/src/common/libcommon.la $(NCURSES_LIB)

fbxine_SOURCES = main.c lirc.c actions.c keys.c options.c callback.c stdctl.c post.c
noinst_HEADERS = main.h lirc.h actions.h keys.h options.h callback.h stdctl.h post.h

$(top_builddir)/src/common/libcommon.la:
	@cd $(top_builddir)/src/common && $(MAKE) libcommon.la

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
