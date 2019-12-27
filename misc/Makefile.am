SUBDIRS = desktops visuals

# splash-foo/manifest contains names of extra source files, distributed only.

#MANIFEST := $(shell test ! -f splash-@SOURCE_SPLASH@/manifest || cat splash-@SOURCE_SPLASH@/manifest)
skindir = $(XINE_SKINDIR)
skin_DATA = \
	$(top_srcdir)/@XINE_SPLASH@ $(top_srcdir)/@XINE_LOGO_MRL@ $(top_srcdir)/@XINE_LOGO2_MRL@
#	$(wildcard splash-@SOURCE_SPLASH@/xine-ui_logo.jpg) \
#	$(wildcard splash-@SOURCE_SPLASH@/xine-ui_logo.mpv) \
#	$(wildcard splash-@SOURCE_SPLASH@/xine-ui_logo.png) \
#	$(wildcard splash-@SOURCE_SPLASH@/xine_splash.jpg) \
#	$(wildcard splash-@SOURCE_SPLASH@/xine_splash.png)

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
	xine-ui.xml \
	splash \
	$(skin_DATA)
#	$(wildcard splash-@SOURCE_SPLASH@/manifest) $(addprefix splash-@SOURCE_SPLASH@/,$(MANIFEST))

bin_SCRIPTS = xine-check xine-bugreport

xine-check: xine-check.sh.in xine-check.en
	$(AM_V_GEN)test `cd $(srcdir); pwd` = `pwd` || cp $(srcdir)/xine-check.sh.in .
	$(AM_V_at)perl $(srcdir)/build-xine-check.pl $(srcdir)/xine-check.en
	$(AM_V_at)chmod a+x xine-check

xine-bugreport: xine-check
	$(AM_V_GEN)test -f xine-bugreport || $(LN_S) xine-check xine-bugreport

install-data-hook:
	test "x$(DESTDIR)" != "x" || update-mime-database $(mimedir)/..

uninstall-hook:
	test "x$(DESTDIR)" != "x" || update-mime-database $(mimedir)/..
