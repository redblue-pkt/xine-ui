
CFLAGS = @BUILD_LIB_STATIC@ @GLOBAL_CFLAGS@ @X_CFLAGS@ @XINE_CFLAGS@

noinst_LTLIBRARIES = libxitk.la
libxitk_la_SOURCES = gui_button.c gui_labelbutton.c gui_label.c \
	gui_widget.c gui_dnd.c gui_list.c gui_slider.c gui_checkbox.c \
	gui_image.c gui_browser.c 
libxitk_la_LIBADD = $(X_PRE_LIBS) $(X_LIBS) -lXext \
	$(top_builddir)/src/Imlib-light/libImlib-light.la

noinst_HEADERS = gui_button.h gui_labelbutton.h gui_label.h \
	gui_widget.h gui_dnd.h gui_list.h gui_slider.h \
	gui_checkbox.c gui_image.h gui_widget_types.h gui_checkbox.h \
	gui_browser.h xitk.h


$(top_builddir)/src/Imlib-light/libImlib-light.la:
	@cd $(top_builddir)/src/Imlib-light && $(MAKE) libImlib-light.la


debug:
	$(MAKE) CFLAGS="$(DEBUG_CFLAGS)"

install-debug: debug
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#*

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
