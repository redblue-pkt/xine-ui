include $(top_srcdir)/misc/Makefile.quiet
include $(top_srcdir)/src/common/Makefile.common

AM_CFLAGS = @XINE_CFLAGS@

if HAVE_AA
aa_programs = aaxine
endif

if HAVE_CACA
caca_programs = cacaxine
endif

bin_PROGRAMS = $(aa_programs) $(caca_programs)

aaxine_SOURCES = $(GETOPT_C) main.c
aaxine_CFLAGS = $(AALIB_CFLAGS) -DAA $(AM_CFLAGS)
aaxine_LDADD = @XINE_LIBS@ $(AALIB_LIBS) $(DYNAMIC_LD_LIBS) -lm \
	       $(THREAD_LIBS) $(LIBCOMMON)
aaxine_DEPENDENCIES = $(LIBCOMMON)

cacaxine_SOURCES = $(GETOPT_C) main.c
cacaxine_CFLAGS = $(CACA_CFLAGS) -DCACA $(AM_CFLAGS)
cacaxine_LDADD = @XINE_LIBS@ $(CACA_LIBS) $(DYNAMIC_LD_LIBS) -lm \
	       $(THREAD_LIBS) $(LIBCOMMON)
cacaxine_DEPENDENCIES = $(LIBCOMMON)
