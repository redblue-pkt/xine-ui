if HAVE_ORBIT

AM_CFLAGS = @BUILD_LIB_STATIC@ @ORBIT_CFLAGS@

EXTRA_DIST = xine.idl example.py

idldir = $(datadir)/idl
idl_DATA = xine.idl

xine.h:
	$(ORBIT_IDL) $(top_srcdir)/corba/xine.idl

xine-common.c: xine.h
	$(ORBIT_IDL) $(top_srcdir)/corba/xine.idl

xine-skels.c: xine.h
	$(ORBIT_IDL) $(top_srcdir)/corba/xine.idl

xine-stubs.c: xine.h
	$(ORBIT_IDL) $(top_srcdir)/corba/xine.idl
noinst_LTLIBRARIES = libcorba.la
libcorba_la_SOURCES = xine-impl.c corba-utils.c xine-server.c
libcorba_la_CFLAGS = $(AM_FLAGS)

bin_PROGRAMS = xine-remote
xine_remote_SOURCES = xine-impl.c corba-utils.c xine-remote.c

xine_remote_LDADD = @ORBIT_LIBS@

noinst_HEADERS = xine-server.h corba-utils.h

endif

clean-generic:
	-rm -f xine-common.c xine-skels.c xine-stubs.c xine.h

debug:
	@$(MAKE) CFLAGS="$(DEBUG_CFLAGS) @ORBIT_CFLAGS@"

install-debug: debug
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am


mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#*


maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
	-rm -f xine-common.c xine-skels.c xine-stubs.c xine.h

