SUBDIRS = data

AM_CPPFLAGS = -I$(top_srcdir)/src/xitk
AM_CFLAGS = @XINE_CFLAGS@

if HAVE_X11
oxine_lib = liboxine.a
endif

noinst_LIBRARIES = $(oxine_lib)

liboxine_a_SOURCES = \
	list.c \
	mediamarks.c \
	odk.c \
	otk.c \
	oxine.c \
	playlist.c \
	utils.c

noinst_HEADERS = \
	list.h \
	mediamarks.h \
	odk.h \
	otk.h \
	oxine.h \
	oxine_event.h \
	playlist.h \
	utils.h 

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
