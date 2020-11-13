include $(top_srcdir)/src/common/Makefile.common

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

libxitk_a_CFLAGS = $(AM_CFLAGS)
libxitk_a_SOURCES = \
	backend_x11.c \
	browser.c \
	button.c \
	button_list.c \
	checkbox.c \
	combo.c \
	config.c \
	cursors.c \
	dialog.c \
	dnd.c \
	doublebox.c \
	font.c \
	getdeskroot.c \
	image.c \
	inputtext.c \
	intbox.c \
	label.c \
	labelbutton.c \
	menu.c \
	mrlbrowser.c \
	skin.c \
	slider.c \
	tabs.c \
	tips.c \
	widget.c \
	window.c \
	xitk.c \
	xitk_x11.c

# recode.h is added unconditionally to the sources because it contains
# the no-op defines in case iconv is not used.
if HAVE_ICONV
libxitk_a_SOURCES += recode.c
endif

noinst_HEADERS = \
	_backend.h \
	_config.h \
	_xitk.h \
	backend.h \
	browser.h \
	button.h \
	button_list.h \
	checkbox.c \
	checkbox.h \
	combo.h \
	cursors.h \
	default_font.h \
	dlist.h \
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
	recode.h \
	skin.h \
	slider.h \
	tabs.h \
	tips.h \
	window.h \
	widget.h \
	xitk.h \
	xitk_x11.h \
	xitkintl.h

test_SOURCES = test.c
test_DEPENDENCIES = libxitk.a \
	     $(top_builddir)/src/xitk/Imlib-light/libImlib-light.a \
	     $(LIBCOMMON)

test_LDADD = $(test_DEPENDENCIES) \
	     $(X_PRE_LIBS) $(X_LIBS) $(XFT_LIBS) -lX11 -lm \
	     $(THREAD_LIBS) $(LIBPNG_LIBS) $(LTLIBINTL) \
	     $(JPEG_LIBS) $(XINE_LIBS)

$(top_builddir)/src/xitk/Imlib-light/libImlib-light.a:
	@cd $(top_builddir)/src/xitk/Imlib-light && $(MAKE) libImlib-light.a

