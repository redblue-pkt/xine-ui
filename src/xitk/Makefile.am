SUBDIRS = skins


CFLAGS = @BUILD_LIB_STATIC@ @GLOBAL_CFLAGS@ @X_CFLAGS@ @XINE_CFLAGS@


bin_PROGRAMS = xine

if HAVE_GETOPT_LONG
else
GETOPT_C = getopt.c getopt1.c
GETOPT_H = getopt.h
endif

xine_SOURCES =  $(GETOPT_C) main.c gui_button.c gui_labelbutton.c gui_label.c \
	gui_main.c \
	gui_widget.c gui_dnd.c gui_list.c gui_slider.c gui_parseskin.c \
	gui_checkbox.c gui_image.c gui_playlist.c gui_browser.c gui_control.c \
	utils.c monitor.c
xine_DEPENDENTIES = utils.h monitor.h
xine_LDADD = @XINE_LIBS@

noinst_HEADERS = gui_button.h gui_labelbutton.h gui_label.h gui_main.h \
	gui_widget.h gui_dnd.h gui_list.h gui_slider.h gui_parseskin.h \
	gui_checkbox.c gui_image.h gui_widget_types.h gui_checkbox.h \
	gui_playlist.h gui_browser.h gui_control.h

utils.c:
	$(LN_S) $(top_srcdir)/src/common/utils.c ./utils.c

monitor.c:
	$(LN_S) $(top_srcdir)/src/common/monitor.c ./monitor.c

$(GETOPT_C):
	$(LN_S) $(top_srcdir)/src/common/getopt.c ./getopt.c
	$(LN_S) $(top_srcdir)/src/common/getopt1.c ./getopt1.c
	$(LN_S) $(top_srcdir)/src/common/getopt.h ./getopt.h

debug:
	$(MAKE) CFLAGS="$(DEBUG_CFLAGS)"

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#*

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
	-rm -f utils.c
