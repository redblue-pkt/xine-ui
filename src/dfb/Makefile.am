CFLAGS = $(DIRECTFB_CFLAGS) @BUILD_LIB_STATIC@ @GLOBAL_CFLAGS@ @XINE_CFLAGS@ -DFONTDIR=\"$(XINE_SKINDIR)/../fonts/\"
DEBUG_CFLAGS = @BUILD_LIB_STATIC@ @DEBUG_CFLAGS@ @XINE_CFLAGS@

if HAVE_DIRECTFB
dfb_programs = dfbxine
endif

if HAVE_GETOPT_LONG
else
GETOPT_C = getopt.c getopt1.c
GETOPT_H = getopt.h
endif

bin_PROGRAMS = ${dfb_programs} 

if HAVE_DIRECTFB
datadir = $(XINE_SKINDIR)/../fonts/
data_DATA = cetus.ttf pointer1.png pointer2.png
endif

dfbxine_SOURCES = $(GETOPT_C) utils.c options.c dfb.c main.c
dfbxine_DEPENDENTIES = utils.h dfb.h
dfbxine_LDADD = @XINE_LIBS@ $(DIRECTFB_LIBS) $(DYNAMIC_LD_LIBS) -lm

$(GETOPT_C):
	$(LN_S) $(top_srcdir)/src/common/getopt.c ./getopt.c
	$(LN_S) $(top_srcdir)/src/common/getopt1.c ./getopt1.c
	$(LN_S) $(top_srcdir)/src/common/getopt.h ./getopt.h

utils.c:
	$(LN_S) $(top_srcdir)/src/common/utils.c ./utils.c

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
