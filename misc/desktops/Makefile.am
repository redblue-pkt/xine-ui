EXTRA_DIST = $(pixmap_DATA) xine.wmconfig \
	xine.desktop.in \
	mime.default \
	$(icon16_DATA) \
	$(icon22_DATA) \
	$(icon32_DATA) \
	$(icon48_DATA) \
	$(icon64_DATA) \
	$(icon_s_DATA)

BUILT_SOURCES = xine.desktop
CLEANFILES = $(BUILT_SOURCES)

pixmapdir = $(datadir)/pixmaps
icon16dir = $(datadir)/icons/hicolor/16x16/apps
icon22dir = $(datadir)/icons/hicolor/22x22/apps
icon32dir = $(datadir)/icons/hicolor/32x32/apps
icon48dir = $(datadir)/icons/hicolor/48x48/apps
icon64dir = $(datadir)/icons/hicolor/64x64/apps
icon_sdir = $(datadir)/icons/hicolor/scalable/apps

icon16_DATA = icons/16/xine.png
icon22_DATA = icons/22/xine.png
icon32_DATA = icons/32/xine.png
icon48_DATA = icons/48/xine.png
icon64_DATA = icons/64/xine.png
icon_s_DATA = icons/scalable/xine.svgz

pixmap_DATA = xine.xpm xine_32.xpm

desktopdir = $(datadir)/applications
desktop_DATA = xine.desktop

# xine input plugins dont tell their mime types yet, assume defaults.
# fortunately, these sort well to the end of list.
INPUT_MIME_TYPES = x-content/audio-cdda;x-content/video-dvd;x-content/video-svcd;x-content/video-vcd;

if XINE_LIST
xine.desktop: xine.desktop.in @XINE_LIST@
	$(AM_V_GEN)cat $< > $@; \
	@XINE_LIST@ | perl -ne 'print join(";",sort(split(";")))' >> $@; \
	echo ';$(INPUT_MIME_TYPES)' >> $@
else
xine.desktop: xine.desktop.in $(XINE_MIME) "$(srcdir)/mime.default"
	$(AM_V_GEN)cat $< > $@; \
	(cat "$(XINE_MIME)" 2>/dev/null || cat "$(srcdir)/mime.default" 2>/dev/null) | awk '{ printf "%s;", $$1 }' >> $@; \
	echo '$(INPUT_MIME_TYPES)' >> $@
endif
