
CFLAGS = @CFLAGS@ @READLINE_LOCAL_CFLAGS@ -DRL_LIBRARY_VERSION=\"@LIBVERSION@\" -static

if HAVE_TERMCAP
readline_lib = libreadline.la
endif

noinst_LTLIBRARIES = $(readline_lib)

EXTRA_DIST = emacs_keymap.c vi_keymap.c savestring.c histexpand.c \
	CHANGELOG CHANGES COPYING INSTALL MANIFEST README USAGE

libreadline_la_SOURCES = readline.c vi_mode.c funmap.c keymaps.c parens.c search.c rltty.c \
	complete.c bind.c isearch.c display.c signals.c util.c kill.c undo.c macro.c input.c \
	callback.c terminal.c nls.c compat.c xmalloc.c history.c histexpand.c histfile.c \
	histsearch.c shell.c tilde.c

noinst_HEADERS = ansi_stdlib.h history.h posixjmp.h rlconf.h rlshell.h rltypedefs.h tilde.h \
	chardefs.h keymaps.h posixstat.h rldefs.h rlstdc.h rlwinsize.h xmalloc.h histlib.h \
	posixdir.h readline.h rlprivate.h rltty.h tcap.h

debug:
	@$(MAKE) CFLAGS="$(DEBUG_CFLAGS) @READLINE_LOCAL_CFLAGS@ -DRL_LIBRARY_VERSION=\\\"@LIBVERSION@\\\" -static"

install-debug: debug
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#*

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
