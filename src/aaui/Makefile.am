AM_CFLAGS = @BUILD_LIB_STATIC@ @XINE_CFLAGS@
DEBUG_CFLAGS = @BUILD_LIB_STATIC@ @DEBUG_CFLAGS@ @XINE_CFLAGS@

if HAVE_AA
aa_programs = aaxine
endif

if HAVE_CACA
caca_programs = cacaxine
endif

bin_PROGRAMS = $(aa_programs) $(caca_programs)

aaxine_SOURCES = $(GETOPT_C) main.c
aaxine_CFLAGS = $(AALIB_CFLAGS) -DAA
aaxine_LDADD = @XINE_LIBS@ $(AALIB_LIBS) $(DYNAMIC_LD_LIBS) -lm \
	       $(top_builddir)/src/common/libcommon.la
aaxine_DEPENDENCIES = $(top_builddir)/src/common/libcommon.la

cacaxine_SOURCES = $(GETOPT_C) main.c
cacaxine_CFLAGS = $(CACA_CFLAGS) -DCACA
cacaxine_LDADD = @XINE_LIBS@ $(CACA_LIBS) $(DYNAMIC_LD_LIBS) -lm \
	       $(top_builddir)/src/common/libcommon.la
cacaxine_DEPENDENCIES = $(top_builddir)/src/common/libcommon.la

$(top_builddir)/src/common/libcommon.la:
	@cd $(top_builddir)/src/common && $(MAKE) libcommon.la

debug:
	@$(MAKE) CFLAGS="$(AALIB_CFLAGS) $(DEBUG_CFLAGS)"

install-debug: debug
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#*

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
