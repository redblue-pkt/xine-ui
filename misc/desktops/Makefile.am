EXTRA_DIST = $(data_DATA)

datadir = $(XINE_DESKTOPDIR)

data_DATA = xine_16x16.png xine_22x22.png xine_32x32.png xine_48x48.png \
	xine.desktop xine.wmconfig xine.xpm

debug:
install-debug: install

install-data-local:
## Icons
	$(mkinstalldirs) /usr/share/icons; \
	$(INSTALL_DATA) -o 0 -g 0 @XINE_DESKTOPDIR@/xine.xpm /usr/share/icons/xine.xpm; \
	$(mkinstalldirs) /usr/share/pixmaps; \
	$(INSTALL_DATA) -o 0 -g 0 @XINE_DESKTOPDIR@/xine.xpm /usr/share/pixmaps/xine.xpm; \
	if test -d /etc/X11/wmconfig; then \
	  $(INSTALL_DATA) -o 0 -g 0 @XINE_DESKTOPDIR@/xine.wmconfig /etc/X11/wmconfig/xine; \
	fi; \
	if test -d /etc/X11/applnk/; then \
	  $(mkinstalldirs) /etc/X11/applnk/Multimedia; \
	  $(INSTALL_DATA) -o 0 -g 0 @XINE_DESKTOPDIR@/xine.desktop /etc/X11/applnk/Multimedia/xine.desktop; \
	fi; \
## End Icons
## Gnome
	if test -d /opt/gnome; then \
	  $(mkinstalldirs) /opt/gnome/share/pixmaps; \
	  $(INSTALL_DATA) -o 0 -g 0 @XINE_DESKTOPDIR@/xine.xpm /opt/gnome/share/pixmaps/xine.xpm; \
	  $(mkinstalldirs) /opt/gnome/share/gnome/apps/Multimedia; \
	  $(INSTALL_DATA) -o 0 -g 0 @XINE_DESKTOPDIR@/xine.desktop /opt/gnome/share/gnome/apps/Multimedia/xine.desktop; \
	else \
	  if test -d /usr/share/gnome/apps; then \
	    $(mkinstalldirs) /usr/share/gnome/apps/Multimedia; \
	    $(INSTALL_DATA) -o 0 -g 0 @XINE_DESKTOPDIR@/xine.desktop /usr/share/gnome/apps/Multimedia/xine.desktop; \
	  fi; \
	fi; \
## End Gnome
## Kde2
	if test -d /opt/kde2; then \
	  DIR_HI=/opt/kde2/share/icons/hicolor; \
	  DIR_LO=/opt/kde2/share/icons/locolor; \
	  HIRES='48x48 32x32 22x22'; \
	  LORES='32x32 22x22 16x16'; \
	  for hires in $$HIRES; do \
	    $(mkinstalldirs) $$DIR_HI/$$hires/apps; \
	    $(INSTALL_DATA) -o 0 -g 0 @XINE_DESKTOPDIR@/xine_$$hires.png $$DIR_HI/$$hires/apps/xine.png; \
	  done; \
	  for lores in $$LORES; do \
	    $(mkinstalldirs) $$DIR_LO/$$lores/apps; \
	    $(INSTALL_DATA) -o 0 -g 0 @XINE_DESKTOPDIR@/xine_$$lores.png $$DIR_LO/$$lores/apps/xine.png; \
	  done; \
	  $(mkinstalldirs) /opt/kde2/share/applnk/Multimedia; \
	  $(INSTALL_DATA) -o 0 -g 0 @XINE_DESKTOPDIR@/xine.desktop /opt/kde2/share/applnk/Multimedia/xine.desktop; \
	else \
	  if test -d /usr/share/applnk/Multimedia; then \
	    $(mkinstalldirs) /usr/share/applnk/Multimedia; \
	    $(INSTALL_DATA) -o 0 -g 0 @XINE_DESKTOPDIR@/xine.desktop /usr/share/applnk/Multimedia/xine.desktop; \
	  fi; \
	fi; \
## End Kde2
## Kde
	if test -d /opt/kde/; then \
	  $(mkinstalldirs) /opt/kde/share/icons; \
	  $(mkinstalldirs) /opt/kde/share/applnk/Multimedia/; \
	  $(INSTALL_DATA) -o 0 -g 0 @XINE_DESKTOPDIR@/xine.xpm /opt/kde/share/icons/xine.xpm; \
	  $(INSTALL_DATA) -o 0 -g 0 @XINE_DESKTOPDIR@/xine.desktop /opt/kde/share/applnk/Multimedia/xine.desktop; \
	else \
	  if test -d /usr/share/applnk/Multimedia/; then \
	    $(INSTALL_DATA) -o 0 -g 0 @XINE_DESKTOPDIR@/xine.desktop /usr/share/applnk/Multimedia/xine.desktop; \
	  fi; \
	fi
## End Kde

uninstall-local:
	REMOVALS='/usr/share/icons/xine.xpm /usr/share/pixmaps/xine.xpm /etc/X11/wmconfig/xine /etc/X11/applnk/Multimedia/xine.desktop /opt/gnome/share/pixmaps/xine.xpm /opt/gnome/share/gnome/apps/Multimedia/xine.desktop /usr/share/gnome/apps/Multimedia/xine.desktop /opt/kde2/share/applnk/Multimedia/xine.desktop /usr/share/applnk/Multimedia/xine.desktop /opt/kde/share/icons/xine.xpm /opt/kde/share/applnk/Multimedia/xine.desktop /usr/share/applnk/Multimedia/xine.desktop'; \
	for remove in $$REMOVALS; do \
	  rm -f $$remove; \
	done; \
	HIRES='48x48 32x32 22x22'; \
	LORES='32x32 22x22 16x16'; \
	for hires in $$HIRES; do \
	  rm -f $$DIR_HI/$$hires/apps/xine.png; \
	done; \
	for lores in $$LORES; do \
	  rm -f $$DIR_LO/$$lores/apps/xine.png; \
	done

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#*

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
