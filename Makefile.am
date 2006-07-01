##
## Process this file with automake to produce Makefile.in
##

AUTOMAKE_OPTIONS = 1.3

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = doc m4 po misc src  

DEBFILES = debian/README.Debian debian/changelog debian/control \
	debian/copyright debian/links debian/menu \
	debian/rules debian/shlibdeps.sh debian/xine-ui.mime 

EXTRA_DIST = config.rpath autogen.sh\
	$(DEBFILES)
	@DEPCOMP@

release-check:
	@./config.status misc/relchk.sh
	@mv -f .cvsversion .cvsversion.tmp
	@./autogen.sh noconfig && $(SHELL) misc/relchk.sh
	@mv -f .cvsversion.tmp .cvsversion

slackbuild:
	@(cd misc && $(MAKE) SlackBuild) && \
	 export PREFIX="/usr"; ./configure --prefix=$$PREFIX && misc/SlackBuild
