EXTRA_DIST = $(data_DATA)

datadir = $(XINE_DESKTOPDIR)

data_DATA = xine_16x16.png xine_22x22.png xine_32x32.png xine_48x48.png \
	xine.desktop xine.wmconfig xine.xpm

LN_S = @LN_S@ -f
debug:
install-debug: install

# Install only as root
install-data-hook:
	case "`id`" in \
	uid=0\(* ) \
## Icons
	  echo "Installing destops icons/entries"; \
	  $(mkinstalldirs) $(DESTDIR)/usr/share/icons; \
	  $(LN_S) @XINE_DESKTOPDIR@/xine.xpm $(DESTDIR)/usr/share/icons/xine.xpm; \
	  $(mkinstalldirs) $(DESTDIR)/usr/share/pixmaps; \
	  $(LN_S) @XINE_DESKTOPDIR@/xine.xpm $(DESTDIR)/usr/share/pixmaps/xine.xpm; \
	  if test -d /etc/X11/wmconfig; then \
	    $(LN_S) @XINE_DESKTOPDIR@/xine.wmconfig $(DESTDIR)/etc/X11/wmconfig/xine; \
	  fi; \
	  if test -d /etc/X11/applnk/; then \
	    $(mkinstalldirs) $(DESTDIR)/etc/X11/applnk/Multimedia; \
	    $(LN_S) @XINE_DESKTOPDIR@/xine.desktop $(DESTDIR)/etc/X11/applnk/Multimedia/xine.desktop; \
	  fi; \
## End Icons
## Gnome
	  if test -d /opt/gnome; then \
	    $(mkinstalldirs) $(DESTDIR)/opt/gnome/share/pixmaps; \
	    $(LN_S) @XINE_DESKTOPDIR@/xine.xpm $(DESTDIR)/opt/gnome/share/pixmaps/xine.xpm; \
	    $(mkinstalldirs) $(DESTDIR)/opt/gnome/share/gnome/apps/Multimedia; \
	    $(LN_S) @XINE_DESKTOPDIR@/xine.desktop $(DESTDIR)/opt/gnome/share/gnome/apps/Multimedia/xine.desktop; \
	  else \
	    if test -d /usr/share/gnome/apps; then \
	      $(mkinstalldirs) $(DESTDIR)/usr/share/gnome/apps/Multimedia; \
	      $(LN_S) @XINE_DESKTOPDIR@/xine.desktop $(DESTDIR)/usr/share/gnome/apps/Multimedia/xine.desktop; \
	    fi; \
	  fi; \
## End Gnome
## Kde2
	  if test -d /opt/kde2; then \
	    DIR_HI=$(DESTDIR)/opt/kde2/share/icons/hicolor; \
	    DIR_LO=$(DESTDIR)/opt/kde2/share/icons/locolor; \
	    HIRES='48x48 32x32 22x22'; \
	    LORES='32x32 22x22 16x16'; \
	    for hires in $$HIRES; do \
	      $(mkinstalldirs) $$DIR_HI/$$hires/apps; \
	      $(LN_S) @XINE_DESKTOPDIR@/xine_$$hires.png $$DIR_HI/$$hires/apps/xine.png; \
	    done; \
	    for lores in $$LORES; do \
	      $(mkinstalldirs) $$DIR_LO/$$lores/apps; \
	      $(LN_S) @XINE_DESKTOPDIR@/xine_$$lores.png $$DIR_LO/$$lores/apps/xine.png; \
	    done; \
	    $(mkinstalldirs) $(DESTDIR)/opt/kde2/share/applnk/Multimedia; \
	    $(LN_S) @XINE_DESKTOPDIR@/xine.desktop $(DESTDIR)/opt/kde2/share/applnk/Multimedia/xine.desktop; \
	  else \
	    if test -d /usr/share/applnk/Multimedia; then \
	      $(mkinstalldirs) $(DESTDIR)/usr/share/applnk/Multimedia; \
	      $(LN_S) @XINE_DESKTOPDIR@/xine.desktop $(DESTDIR)/usr/share/applnk/Multimedia/xine.desktop; \
	    fi; \
	  fi; \
## End Kde2
## Kde
	  if test -d /opt/kde/; then \
	    $(mkinstalldirs) $(DESTDIR)/opt/kde/share/icons; \
	    $(mkinstalldirs) $(DESTDIR)/opt/kde/share/applnk/Multimedia/; \
	    $(LN_S) @XINE_DESKTOPDIR@/xine.xpm $(DESTDIR)/opt/kde/share/icons/xine.xpm; \
	    $(LN_S) @XINE_DESKTOPDIR@/xine.desktop $(DESTDIR)/opt/kde/share/applnk/Multimedia/xine.desktop; \
	  else \
	    if test -d /usr/share/applnk/Multimedia/; then \
	      rm -f $(DESTDIR)/usr/share/applnk/Multimedia/xine.desktop; \
	      $(LN_S) @XINE_DESKTOPDIR@/xine.desktop $(DESTDIR)/usr/share/applnk/Multimedia/xine.desktop; \
	    fi; \
	  fi; \
## End Kde
	  ;; \
	esac

uninstall-local:
	REMOVALS='/usr/share/icons/xine.xpm /usr/share/pixmaps/xine.xpm /etc/X11/wmconfig/xine /etc/X11/applnk/Multimedia/xine.desktop /opt/gnome/share/pixmaps/xine.xpm /opt/gnome/share/gnome/apps/Multimedia/xine.desktop /usr/share/gnome/apps/Multimedia/xine.desktop /opt/kde2/share/applnk/Multimedia/xine.desktop /usr/share/applnk/Multimedia/xine.desktop /opt/kde/share/icons/xine.xpm /opt/kde/share/applnk/Multimedia/xine.desktop /usr/share/applnk/Multimedia/xine.desktop'; \
	for remove in $$REMOVALS; do \
	  rm -f $$remove; \
	done; \
	DIR_HI=$(DESTDIR)/opt/kde2/share/icons/hicolor; \
	DIR_LO=$(DESTDIR)/opt/kde2/share/icons/locolor; \
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
