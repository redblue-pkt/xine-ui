CFLAGS = @BUILD_LIB_STATIC@ @GLOBAL_CFLAGS@ @XINE_CFLAGS@ 

if HAVE_AA
aa_programs = aaxine
endif

bin_PROGRAMS = ${aa_programs} 

aaxine_SOURCES = main.c
aaxine_LDADD = @XINE_LIBS@  -laa -lm $(THREAD_LIBS) $(DYNAMIC_LD_LIBS) 

debug:
	$(MAKE) CFLAGS="$(DEBUG_CFLAGS)"

install-debug: debug
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#*

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
	-rm -f utils.c monitor.c xscreensaver-remote.c


