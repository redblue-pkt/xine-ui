
SUBDIRS = desktops

EXTRA_DIST = $(bin_SCRIPTS) autogen.sh SlackBuild.in SlackBuild build_rpms.sh \
	xine-ui.spec.in xine-ui.spec

bindir = @XINE_SCRIPTDIR@

bin_SCRIPTS = xineshot

debug:
install-debug: debug

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#* 


maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
