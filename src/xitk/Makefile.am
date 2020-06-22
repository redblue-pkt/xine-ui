AUTOMAKE_OPTIONS = subdir-objects

include $(top_srcdir)/src/common/Makefile.common

SUBDIRS = xine-toolkit .

AM_CFLAGS = @X_CFLAGS@ @XINE_CFLAGS@ @LIBPNG_CFLAGS@ @CURL_CFLAGS@ @NVTVSIMPLE_CFLAGS@ @READLINE_INCLUDES@ -I$(top_srcdir)/src/xitk/xine-toolkit

if HAVE_READLINE
remote = xine-remote
endif

if HAVE_X11
xine_ui = xine $(remote)
oxine_lib = liboxine.a
imlib_lib = libImlib-light.a
endif

bin_PROGRAMS = $(xine_ui)
EXTRA_PROGRAMS = session_ctrl kbindings_man
noinst_LIBRARIES = \
	$(imlib_lib) \
	$(oxine_lib)

kbindings_man_CFLAGS = $(AM_CFLAGS) -DKBINDINGS_MAN
kbindings_man_LDADD = $(THREAD_LIBS) $(LIBCOMMON) $(LTLIBINTL) $(XINE_LIBS)
kbindings_man_SOURCES = kbindings_man.c kbindings_common.c

EXTRA_DIST = \
	$(skins_DATA) \
	$(oxine_DATA)

libImlib_light_a_CFLAGS = $(X_CFLAGS) $(LIBPNG_CFLAGS) $(JPEG_CFLAGS)
libImlib_light_a_SOURCES = \
	Imlib-light/Imlib.h \
	Imlib-light/Imlib_private.h \
	Imlib-light/Imlib_types.h \
	Imlib-light/cache.c \
	Imlib-light/colors.c \
	Imlib-light/load.c \
	Imlib-light/misc.c \
	Imlib-light/rend.c \
	Imlib-light/utils.c

oxinedir = @XINE_OXINEDIR@
oxine_DATA = \
	oxine/data/mediamarks \
	oxine/data/mainmenu

liboxine_a_SOURCES = \
       oxine/list.c \
       oxine/list.h \
       oxine/mediamarks.c \
       oxine/mediamarks.h \
       oxine/odk.c \
       oxine/odk.h \
       oxine/otk.c \
       oxine/otk.h \
       oxine/oxine.c \
       oxine/oxine.h \
       oxine/oxine_event.h \
       oxine/playlist.c \
       oxine/playlist.h \
       oxine/utils.c \
       oxine/utils.h
liboxine_a_CFLAGS = @XINE_CFLAGS@ $(X_CFLAGS)
liboxine_a_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/src/xitk

xine_SOURCES = \
	actions.c \
	control.c \
	download.c \
	errors.c \
	event.c \
	event_sender.c \
	file_browser.c \
	frequencies.c \
	help.c \
	kbindings.c \
	kbindings_common.c \
	lang.c \
	main.c \
	mediamark.c \
	menus.c \
	mrl_browser.c \
	network.c \
	osd.c \
	panel.c \
	playlist.c \
	post.c \
	session_common.c \
	session_internal.h \
	session.c \
	session.h \
	setup.c \
	skins.c \
	snapshot.c \
	splash.c \
	stdctl.c \
	stream_infos.c \
	tvout.c \
	tvset.c \
	videowin.c \
	viewlog.c

if HAVE_TAR
xine_SOURCES += skins_download.c skins_internal.h
endif

if HAVE_LIRC
xine_SOURCES += lirc.c lirc.h
endif

xine_LDADD = \
	     $(top_builddir)/src/xitk/xine-toolkit/libxitk.a \
	     $(imlib_lib) \
	     $(oxine_lib) \
	     $(LIBCOMMON) $(LIBCOMMONX) \
	     $(XINE_LIBS) $(X_PRE_LIBS) $(LIRC_LIBS) $(XFT_LIBS) \
	     $(X_LIBS) $(XTEST_LIBS) $(XSSAVER_LIBS) $(CURL_LIBS) $(NVTVSIMPLE_LIBS) \
	     -lX11 -lXext -lm $(DBG_LIBS) $(XV_LIB) $(LIBPNG_LIBS) $(JPEG_LIBS) \
	     $(LTLIBINTL) $(THREAD_LIBS) $(RT_LIBS)

xine_remote_CFLAGS = $(AM_CFLAGS) -DNETWORK_CLIENT
xine_remote_SOURCES = network.c
xine_remote_DEPENDENCIES = $(LIBCOMMON)
xine_remote_LDADD = $(LTLIBINTL) $(NET_LIBS) $(THREAD_LIBS) $(READLINE_LIBS) \
	$(LIBCOMMON) $(XINE_LIBS)

session_ctrl_CFLAGS = $(AM_CFLAGS)
session_ctrl_LDADD = $(LIBCOMMON) $(DBG_LIBS) $(XINE_LIBS)
session_ctrl_SOURCES = session_common.c session_internal.h session.h ctrl_test.c

noinst_HEADERS = \
	actions.h \
	common.h \
	control.h \
	download.h \
	errors.h \
	event.h \
	event_sender.h \
	file_browser.h \
	frequencies.h \
	i18n.h \
	help.h \
	kbindings.h \
	kbindings_common.h \
	lang.h \
	mediamark.h \
	menus.h \
	mrl_browser.h \
	network.h \
	osd.h \
	panel.h \
	playlist.h \
	post.h \
	setup.h \
	skins.h \
	snapshot.h \
	splash.h \
	stdctl.h \
	stream_infos.h \
	tvout.h \
	tvset.h \
	videowin.h \
	viewlog.h

skinsdir = $(datadir)/xine/skins/xinetic

xinetic_dir = skins/xinetic
skins_DATA = \
	$(xinetic_dir)/CtlBg.png \
	$(xinetic_dir)/CtlDismiss.png \
	$(xinetic_dir)/CtlDummy.png \
	$(xinetic_dir)/CtlReset.png \
	$(xinetic_dir)/CtlSave.png \
	$(xinetic_dir)/Letters.png \
	$(xinetic_dir)/Letters2.png \
	$(xinetic_dir)/MainAutoButton.png \
	$(xinetic_dir)/MainBg.png \
	$(xinetic_dir)/MainBits1.png \
	$(xinetic_dir)/MainBits2.png \
	$(xinetic_dir)/MainVolumeBg.png \
	$(xinetic_dir)/MainVolumePad.png \
	$(xinetic_dir)/MrlBg.png \
	$(xinetic_dir)/MrlDismiss.png \
	$(xinetic_dir)/MrlFlit.png \
	$(xinetic_dir)/MrlSelect.png \
	$(xinetic_dir)/Onglet.png \
	$(xinetic_dir)/PlBg.png \
	$(xinetic_dir)/PlDelete.png \
	$(xinetic_dir)/PlDeleteAll.png \
	$(xinetic_dir)/PlDismiss.png \
	$(xinetic_dir)/PlInputTxt.png \
	$(xinetic_dir)/PlItemBtn.png \
	$(xinetic_dir)/PlMoveDn.png \
	$(xinetic_dir)/PlMoveUp.png \
	$(xinetic_dir)/PlPlay.png \
	$(xinetic_dir)/PlSlidHBg.png \
	$(xinetic_dir)/SlidHPad.png \
	$(xinetic_dir)/SliderHBg.png \
	$(xinetic_dir)/SliderHPad.png \
	$(xinetic_dir)/SliderVBg.png \
	$(xinetic_dir)/SliderVPad.png \
	$(xinetic_dir)/SkItemBtn.png \
	$(xinetic_dir)/SkSlidHBg.png \
	$(xinetic_dir)/SkSlidBG.png \
	$(xinetic_dir)/SkSlidHPad.png \
	$(xinetic_dir)/skinconfig
