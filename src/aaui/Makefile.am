AM_CFLAGS = $(AALIB_CFLAGS) @BUILD_LIB_STATIC@ @XINE_CFLAGS@
DEBUG_CFLAGS = @BUILD_LIB_STATIC@ @DEBUG_CFLAGS@ @XINE_CFLAGS@

if HAVE_AA
aa_programs = aaxine
endif

if HAVE_GETOPT_LONG
else
GETOPT_C = getopt.c getopt1.c
GETOPT_H = getopt.h
endif

bin_PROGRAMS = ${aa_programs} 

aaxine_SOURCES = $(GETOPT_C) main.c
aaxine_LDADD = @XINE_LIBS@ $(AALIB_LIBS) $(DYNAMIC_LD_LIBS) -lm

$(GETOPT_C):
	$(LN_S) $(top_srcdir)/src/common/getopt.c ./getopt.c
	$(LN_S) $(top_srcdir)/src/common/getopt1.c ./getopt1.c
	$(LN_S) $(top_srcdir)/src/common/getopt.h ./getopt.h

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
