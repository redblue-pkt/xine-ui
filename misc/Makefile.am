SUBDIRS = desktops visuals

skindir = $(XINE_SKINDIR)
skin_DATA = xine-ui_logo.mpv xine_splash.png

mimedir = $(datadir)/mime/packages
mime_DATA = xine-ui.xml

EXTRA_DIST = \
	$(bin_SCRIPTS) \
	SlackBuild.in \
	SlackBuild \
	build_rpms.sh \
	build-xine-check.pl \
	xine-ui.spec.in \
	xine-ui.spec \
	xine-check.sh.in \
	xine-check.en \
	xine_bash_completion \
	xine-ui_logo.mpv \
	xine_splash.png

bin_SCRIPTS = xine-check xine-bugreport

xine-check: xine-check.sh.in xine-check.en
	test `cd $(srcdir); pwd` = `pwd` || cp $(srcdir)/xine-check.sh.in .
	perl $(srcdir)/build-xine-check.pl $(srcdir)/xine-check.en
	chmod a+x xine-check

xine-bugreport: xine-check
	test -f xine-bugreport || $(LN_S) xine-check xine-bugreport

install-data-hook:
	test "x$(DESTDIR)" != "x" || update-mime-database $(mimedir)/..
