AM_CFLAGS = @BUILD_LIB_STATIC@ @X_CFLAGS@
DEBUG_CFLAGS = @BUILD_LIB_STATIC@ @DEBUG_CFLAGS@ @X_CFLAGS@

noinst_LTLIBRARIES = libImlib-light.la

libImlib_light_la_SOURCES = \
	cache.c \
	colors.c \
	load.c \
	misc.c \
	rend.c \
	snprintf.c \
	utils.c 

noinst_HEADERS = \
	Imlib.h \
	Imlib_private.h \
	Imlib_types.h

debug:
	@$(MAKE) CFLAGS="$(DEBUG_CFLAGS)"

install-debug: debug
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#*

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
