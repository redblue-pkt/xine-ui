
SUBDIRS = desktops visuals

datadir = @XINE_SKINDIR@
data_DATA = xine-ui_logo.mpv black.mpv

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
	black.mpv

bin_SCRIPTS = xine-check xine-bugreport

xine-check: xine-check.sh.in xine-check.en
	test `cd $(srcdir); pwd` = `pwd` || cp $(srcdir)/xine-check.sh.in .
	perl $(srcdir)/build-xine-check.pl $(srcdir)/xine-check.en
	chmod a+x xine-check

xine-bugreport: xine-check
	ln -s xine-check xine-bugreport


debug:
	@list='$(SUBDIRS)'; for subdir in $$list; do \
	  (cd $$subdir && $(MAKE) $@) || exit; \
	done;

install-debug: debug
	@list='$(SUBDIRS)'; for subdir in $$list; do \
	  (cd $$subdir && $(MAKE) $@) || exit; \
	done;
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#* 


maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
