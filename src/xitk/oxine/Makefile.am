AM_CFLAGS = -I../

if HAVE_X11
oxine_lib = liboxine.la
endif

noinst_LTLIBRARIES = $(oxine_lib)

liboxine_la_SOURCES = \
	list.c \
	mediamarks.c \
	odk.c \
	otk.c \
	oxine.c \
	utils.c

noinst_HEADERS = \
	list.h \
	mediamarks.h \
	odk.h \
	otk.h \
	oxine.h \
	utils.h 

debug:
	@$(MAKE) CFLAGS="$(DEBUG_CFLAGS) -I../"

install-debug: debug
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#*

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
