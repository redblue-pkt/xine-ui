EXTRA_DIST = $(pixmap_DATA) xine.wmconfig \
	xine.desktop.in \
	mime.default \
	mime.extra \
	$(icon16_DATA) \
	$(icon22_DATA) \
	$(icon32_DATA) \
	$(icon48_DATA) 

BUILT_SOURCES = xine.desktop
CLEANFILES = $(BUILT_SOURCES)

pixmapdir = $(datadir)/pixmaps
icon16dir = $(datadir)/icons/hicolor/16x16/apps
icon22dir = $(datadir)/icons/hicolor/22x22/apps
icon32dir = $(datadir)/icons/hicolor/32x32/apps
icon48dir = $(datadir)/icons/hicolor/48x48/apps

icon16_DATA = icons/16/xine.png
icon22_DATA = icons/22/xine.png
icon32_DATA = icons/32/xine.png
icon48_DATA = icons/48/xine.png

pixmap_DATA = xine.xpm

desktopdir = $(datadir)/applications
desktop_DATA = xine.desktop

if XINE_LIST
xine.desktop: xine.desktop.in @XINE_LIST@
	exec > $@; cat $<; \
	@XINE_LIST@; \
	awk '{ printf "%s;", $$1 }' < $(srcdir)/mime.extra
else
XINE_MIME = $(if $(shell test "$(XINE_CONFIG)" = '' && echo y),$(shell pkg-config --variable=plugindir libxine),$(shell $(XINE_CONFIG) --plugindir))/mime.types
MIME = $(if $(shell test -f $(XINE_MIME) && echo y),$(XINE_MIME),$(srcdir)/mime.default)
xine.desktop: xine.desktop.in $(MIME) $(srcdir)/mime.extra
	exec > $@; \
	cat $<; \
	cat $(MIME) $(srcdir)/mime.extra | awk '{ printf "%s;", $$1 }'
endif
