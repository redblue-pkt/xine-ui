
SUBDIRS = po

EXTRA_DIST = $(docs_DATA)

docsdir = $(datadir)/doc/xitk

docs_DATA = README

AM_CFLAGS = @BUILD_LIB_STATIC@ @X_CFLAGS@ @XINE_CFLAGS@ @THREAD_CFLAGS@

EXTRA_PROGRAMS = test
noinst_LTLIBRARIES = libxitk.la

libxitk_la_SOURCES = \
	browser.c \
	button.c \
	checkbox.c \
	combo.c \
	config.c \
	dnd.c \
	filebrowser.c \
	font.c \
	getdeskroot.c \
	image.c \
	inputtext.c \
	intbox.c \
	label.c \
	labelbutton.c \
	list.c \
	menu.c \
	mrlbrowser.c \
	skin.c \
	slider.c \
	tabs.c \
	tips.c \
	widget.c \
	window.c \
	xitk.c

libxitk_la_LIBADD = $(X_LIBS) $(X_PRE_LIBS) -lX11 \
	-lXext $(X_EXTRA_LIBS)  $(THREAD_LIBS) $(INTLLIBS) \
	$(top_builddir)/src/Imlib-light/libImlib-light.la

noinst_HEADERS = \
	_config.h \
	_xitk.h \
	browser.h \
	button.h \
	checkbox.c \
	checkbox.h \
	combo.h \
	dnd.h \
	filebrowser.h \
	font.h \
	image.h \
	inputtext.h \
	intbox.h \
	label.h \
	labelbutton.h \
	menu.h \
	mrlbrowser.h \
	skin.h \
	slider.h \
	tabs.h \
	tips.h \
	window.h \
	widget.h \
	xitk.h \
	xitkintl.h

test_SOURCES = test.c
test_DEPENDENCIES = libxitk.la
test_LDADD = $(top_builddir)/src/xitk/xine-toolkit/libxitk.la

$(top_builddir)/src/Imlib-light/libImlib-light.la:
	@cd $(top_builddir)/src/Imlib-light && $(MAKE) libImlib-light.la


$(top_builddir)/src/xitk/xine-toolkit/libxitk.la:
	@cd $(top_builddir)/src/xitk/xine-toolkit && $(MAKE) libxitk.la

if INCLUDE_INTL
@INTLLIBS@:
	@cd $(top_builddir)/intl && $(MAKE) @INTLLIBS@
endif

debug:
	@$(MAKE) CFLAGS="$(DEBUG_CFLAGS)"

debug-test:
	@$(MAKE) CFLAGS="$(DEBUG_CFLAGS)" test

install-debug: debug
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#* test

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
