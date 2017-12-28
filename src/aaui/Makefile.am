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
aaxine_LDADD = $(AALIB_LIBS) $(DYNAMIC_LD_LIBS) -lm \
	       $(THREAD_LIBS) $(LIBCOMMON) $(XINE_LIBS)
aaxine_DEPENDENCIES = $(LIBCOMMON)

cacaxine_SOURCES = $(GETOPT_C) main.c
cacaxine_CFLAGS = $(CACA_CFLAGS) -DCACA $(AM_CFLAGS)
cacaxine_LDADD = $(CACA_LIBS) $(DYNAMIC_LD_LIBS) -lm \
	       $(THREAD_LIBS) $(LIBCOMMON) $(XINE_LIBS)
cacaxine_DEPENDENCIES = $(LIBCOMMON)
