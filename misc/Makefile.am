
SUBDIRS = desktops

EXTRA_DIST = $(bin_SCRIPTS) autogen.sh SlackBuild.in SlackBuild build_rpms.sh \
	xine-ui.spec.in xine-ui.spec xine-check.sh.in xine-check.en \
	build-xine-check.pl xine_bash_completion

# should use real bindir...
#bindir = @XINE_SCRIPTDIR@

# removed xineshot. should no longer be needed... -siggi
bin_SCRIPTS = xine-check xine-bugreport

xine-check: xine-check.sh.in xine-check.en
	test `cd $(srcdir); pwd` = `pwd` || cp $(srcdir)/xine-check.sh.in .
	perl $(srcdir)/build-xine-check.pl $(srcdir)/xine-check.en
	chmod a+x xine-check

xine-bugreport: xine-check
	ln -s xine-check xine-bugreport


debug:
install-debug: debug

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#* 


maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
