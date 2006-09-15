if HAVE_X11
imlib_lib = libImlib-light.a
endif

noinst_LIBRARIES = $(imlib_lib)

libImlib_light_a_CFLAGS = $(X_CFLAGS) $(LIBPNG_CFLAGS)
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
