DEBUG_CFLAGS = @DEBUG_CFLAGS@

if HAVE_X11
imlib_lib = libImlib-light.a
endif

noinst_LIBRARIES = $(imlib_lib)

libImlib_light_a_CFLAGS = $(LIBPNG_CFLAGS)
libImlib_light_a_SOURCES = \
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
