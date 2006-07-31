##
## Process this file with automake to produce Makefile.in
##

ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = dist-bzip2

SUBDIRS = doc m4 po misc src  

DEBFILES = debian/README.Debian debian/changelog debian/control \
	debian/copyright debian/links debian/menu \
	debian/rules debian/shlibdeps.sh debian/xine-ui.mime 

EXTRA_DIST = config.rpath autogen.sh

slackbuild:
	@(cd misc && $(MAKE) SlackBuild) && \
	 export PREFIX="/usr"; ./configure --prefix=$$PREFIX && misc/SlackBuild
