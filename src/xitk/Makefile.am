SUBDIRS = skins


CFLAGS = @BUILD_LIB_STATIC@ @GLOBAL_CFLAGS@ @X_CFLAGS@ @XINE_CFLAGS@


bin_PROGRAMS = xine

if HAVE_GETOPT_LONG
else
GETOPT_C = getopt.c getopt1.c
GETOPT_H = getopt.h
endif

##gui_control.c 
xine_SOURCES = $(GETOPT_C) main.c gui_main.c gui_playlist.c \
	gui_parseskin.c gui_videowin.c utils.c monitor.c \
	gui_panel.c gui_lirc.c gui_actions.c
xine_DEPENDENTIES = utils.h monitor.h 
xine_LDADD = @XINE_LIBS@  $(LIRC_LIBS) -lm $(THREAD_LIBS) \
	$(DYNAMIC_LD_LIBS) libxitk.la

#####
## Our "almost" toolkit
##
noinst_LTLIBRARIES = libxitk.la
libxitk_la_SOURCES = gui_button.c gui_labelbutton.c gui_label.c \
	gui_widget.c gui_dnd.c gui_list.c gui_slider.c gui_checkbox.c \
	gui_image.c gui_browser.c 
libxitk_la_LIBADD = $(X_PRE_LIBS) $(X_LIBS) -lXext \
	$(top_builddir)/src/Imlib-light/libImlib-light.la

noinst_HEADERS = gui_button.h gui_labelbutton.h gui_label.h gui_main.h \
	gui_widget.h gui_dnd.h gui_list.h gui_slider.h gui_parseskin.h \
	gui_checkbox.c gui_image.h gui_widget_types.h gui_checkbox.h \
	gui_playlist.h gui_browser.h gui_control.h gui_panel.h gui_lirc.h \
	gui_actions.h

utils.c:
	$(LN_S) $(top_srcdir)/src/common/utils.c ./utils.c

monitor.c:
	$(LN_S) $(top_srcdir)/src/common/monitor.c ./monitor.c

$(GETOPT_C):
	$(LN_S) $(top_srcdir)/src/common/getopt.c ./getopt.c
	$(LN_S) $(top_srcdir)/src/common/getopt1.c ./getopt1.c
	$(LN_S) $(top_srcdir)/src/common/getopt.h ./getopt.h

$(top_builddir)/src/Imlib-light/libImlib-light.la:
	@cd $(top_builddir)/src/Imlib-light && $(MAKE) libImlib-light.la


debug:
	$(MAKE) CFLAGS="$(DEBUG_CFLAGS)"

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#*

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
	-rm -f utils.c
