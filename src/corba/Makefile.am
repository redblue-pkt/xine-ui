CFLAGS = @BUILD_LIB_STATIC@ @GLOBAL_CFLAGS@ @ORBIT_CFLAGS@

ORBIT_IDL = @ORBIT_IDL@
idldir = $(datadir)/idl
idl_DATA = xine.idl

EXTRA_DIST = $(idl_DATA) xine-impl.c example.py

#BUILT_SOURCES = xine-common.c xine-skels.c xine-stubs.c xine.h

if HAVE_ORBIT
xine.h:
	$(ORBIT_IDL) $(top_srcdir)/corba/xine.idl
xine-common.c: xine.h
	$(ORBIT_IDL) $(top_srcdir)/corba/xine.idl
xine-skels.c: xine.h
	$(ORBIT_IDL) $(top_srcdir)/corba/xine.idl
xine-stubs.c: xine.h
	$(ORBIT_IDL) $(top_srcdir)/corba/xine.idl
corbalib_libs = libcorba.la
corbalib_srcs = xine-common.c xine-skels.c xine-stubs.c \
	corba-utils.c
corbabin_bins = xine-remote
corbabin_srcs = xine-common.c xine-skels.c xine-stubs.c \
	corba-utils.c
endif

noinst_LTLIBRARIES = $(corbalib_libs)
libcorba_la_SOURCES = $(corbalib_srcs) xine-server.c

bin_PROGRAMS = $(corbabin_bins)
xine_remote_SOURCES = $(corbabin_srcs) xine-remote.c
xine_remote_LDADD = @ORBIT_LIBS@


noinst_HEADERS = xine-server.h corba-utils.h


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
