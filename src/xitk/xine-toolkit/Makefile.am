
CFLAGS = @BUILD_LIB_STATIC@ @GLOBAL_CFLAGS@ @X_CFLAGS@ @XINE_CFLAGS@

noinst_LTLIBRARIES = libxitk.la
libxitk_la_SOURCES = button.c labelbutton.c label.c \
	widget.c dnd.c list.c slider.c checkbox.c \
	image.c browser.c xitk.c filebrowser.c mrlbrowser.c
libxitk_la_LIBADD = $(X_PRE_LIBS) $(X_LIBS) -lXext $(THREAD_LIBS) \
	$(top_builddir)/src/Imlib-light/libImlib-light.la

noinst_HEADERS = button.h labelbutton.h label.h \
	widget.h dnd.h list.h slider.h \
	checkbox.c image.h widget_types.h checkbox.h \
	browser.h filebrowser.h mrlbrowser.h xitk.h _xitk.h


$(top_builddir)/src/Imlib-light/libImlib-light.la:
	@cd $(top_builddir)/src/Imlib-light && $(MAKE) libImlib-light.la


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
