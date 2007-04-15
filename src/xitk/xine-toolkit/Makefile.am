
SUBDIRS = po

EXTRA_DIST = $(docs_DATA)

docsdir = $(datadir)/doc/xitk

docs_DATA = README

AM_CFLAGS = @X_CFLAGS@ @XINE_CFLAGS@ @THREAD_CFLAGS@ @XFT_CFLAGS@

if HAVE_X11
xitk_lib = libxitk.a
endif

EXTRA_PROGRAMS = test
noinst_LIBRARIES = $(xitk_lib)

libxitk_a_CFLAGS = $(AM_CFLAGS) $(NO_STRICT_ALIASING)
libxitk_a_SOURCES = \
	browser.c \
	button.c \
	checkbox.c \
	combo.c \
	config.c \
	cursors.c \
	dnd.c \
	doublebox.c \
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

noinst_HEADERS = \
	_config.h \
	_xitk.h \
	browser.h \
	button.h \
	checkbox.c \
	checkbox.h \
	combo.h \
	cursors.h \
	dnd.h \
	doublebox.h \
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
test_DEPENDENCIES = libxitk.a \
             $(top_builddir)/src/xitk/Imlib-light/libImlib-light.a

test_LDADD = libxitk.a \
             $(top_builddir)/src/xitk/Imlib-light/libImlib-light.a \
	     $(X_PRE_LIBS) $(X_LIBS) $(XFT_LIBS) -lX11 \
	     $(THREAD_LIBS) $(LIBPNG_LIBS) $(INTLLIBS)

$(top_builddir)/src/xitk/Imlib-light/libImlib-light.a:
	@cd $(top_builddir)/src/xitk/Imlib-light && $(MAKE) libImlib-light.a
