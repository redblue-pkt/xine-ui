
AM_CFLAGS = @READLINE_LOCAL_CFLAGS@ -DRL_LIBRARY_VERSION=\"@LIBVERSION@\" -static

if HAVE_TERMCAP
readline_lib = libreadline.la
endif

noinst_LTLIBRARIES = $(readline_lib)

EXTRA_DIST = \
	CHANGELOG \
	CHANGES \
	COPYING \
	INSTALL \
	MANIFEST \
	README \
	USAGE \
	emacs_keymap.c \
	histexpand.c \
	savestring.c \
	vi_keymap.c

libreadline_la_SOURCES = \
	bind.c \
	callback.c \
	compat.c \
	complete.c \
	display.c \
	funmap.c \
	history.c \
	histexpand.c \
	histfile.c \
	histsearch.c \
	input.c \
	isearch.c \
	keymaps.c \
	kill.c \
	macro.c \
	nls.c \
	parens.c \
	readline.c \
	rltty.c \
	search.c \
	shell.c \
	signals.c \
	terminal.c \
	tilde.c \
	undo.c \
	util.c \
	vi_mode.c \
	xmalloc.c 

noinst_HEADERS = \
	ansi_stdlib.h \
	chardefs.h \
	histlib.h \
	history.h \
	keymaps.h \
	posixdir.h \
	posixjmp.h \
	posixstat.h \
	readline.h \
	rlconf.h \
	rldefs.h \
	rlprivate.h \
	rlshell.h \
	rlstdc.h \
	rltty.h \
	rltypedefs.h \
	rlwinsize.h \
	tcap.h \
	tilde.h \
	xmalloc.h 

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
