
SUBDIRS = po

CFLAGS = @BUILD_LIB_STATIC@ @GLOBAL_CFLAGS@ @X_CFLAGS@ @XINE_CFLAGS@ @THREAD_CFLAGS@

EXTRA_PROGRAMS = test
noinst_LTLIBRARIES = libxitk.la
libxitk_la_SOURCES = browser.c button.c checkbox.c combo.c dnd.c filebrowser.c font.c \
		image.c inputtext.c label.c labelbutton.c list.c mrlbrowser.c skin.c \
		slider.c widget.c window.c tabs.c intbox.c xitk.c
libxitk_la_LIBADD = $(X_LIBS) $(X_PRE_LIBS) -lX11 -lXext $(X_EXTRA_LIBS) \
	$(THREAD_LIBS) $(INTLLIBS) $(top_builddir)/src/Imlib-light/libImlib-light.la

noinst_HEADERS = _xitk.h browser.h button.h checkbox.c checkbox.h combo.h dnd.h tabs.h \
		filebrowser.h font.h image.h inputtext.h label.h labelbutton.h xitkintl.h \
		list.h mrlbrowser.h skin.h slider.h window.h widget.h widget_types.h \
		intbox.h xitk.h

test_SOURCES = test.c
test_LDADD = $(top_builddir)/src/xitk/xine-toolkit/libxitk.la

$(top_builddir)/src/Imlib-light/libImlib-light.la:
	@cd $(top_builddir)/src/Imlib-light && $(MAKE) libImlib-light.la


$(top_builddir)/src/xitk/xine-toolkit/libxitk.la:
	@cd $(top_builddir)/src/xitk/xine-toolkit && $(MAKE) libxitk.la


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
