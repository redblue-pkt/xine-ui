EXTRA_DIST = $(pixmap_DATA) $(desktop_DATA) xine.wmconfig

pixmapdir = $(datadir)/pixmaps

pixmap_DATA = xine_16x16.png \
	xine_22x22.png \
	xine_32x32.png \
	xine_48x48.png \
	xine.xpm

desktopdir = $(datadir)/xine/desktop
desktop_DATA = xine.desktop

debug:
install-debug: debug
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#*

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
