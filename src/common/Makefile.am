if HAVE_X11
commonx = libcommonx.a
endif

noinst_LIBRARIES = libcommon.a $(commonx)

AM_CFLAGS = @XINE_CFLAGS@ $(X_CFLAGS)

EXTRA_DIST = getopt.c getopt1.c getopt.h xscreensaver-remote.c xscreensaver-remote.h 

if HAVE_GETOPT_LONG
else
GETOPT_SRC = getopt.c getopt1.c
GETOPT_HDR = getopt.h
endif

libcommon_a_SOURCES = $(GETOPT_SRC) utils.c \
	config_wrapper.c config_wrapper.h \
	dump.c           dump.h           \
	globals.c globals.h \
	xine_open_cloexec.c xine_socket_cloexec.c xine_compat.h
libcommon_a_LIBADD = @LIBOBJS@

libcommonx_a_SOURCES = x11utils.c

noinst_HEADERS = $(GETOPT_HDR) utils.h xine_compat.h libcommon.h
