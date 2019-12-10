##
## Process this file with automake to produce Makefile.in
##

ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = dist-bzip2

SUBDIRS = doc po misc src

DEBFILES = debian/README.Debian debian/changelog debian/control \
	debian/copyright debian/links debian/menu \
	debian/rules debian/shlibdeps.sh debian/xine-ui.mime 

EXTRA_DIST = \
	config.rpath \
	autogen.sh \
	\
	m4/aa.m4 \
	m4/gettext.m4 \
	m4/iconv.m4 \
	m4/lib-ld.m4 \
	m4/lib-link.m4 \
	m4/lib-prefix.m4 \
	m4/nls.m4 \
	m4/po.m4 \
	m4/progtest.m4 \
	m4/dl.m4 \
	m4/readline.m4 \
	m4/xft.m4 \
	m4/_xine.m4


distuninstallcheck_listfiles = find . -type f -print | grep -v share/mime/

slackbuild:
	@(cd misc && $(MAKE) SlackBuild) && \
	 export PREFIX="/usr"; ./configure --prefix=$$PREFIX && misc/SlackBuild

translator:
	$(MAKE) -C $(top_builddir)/po update-po
	$(MAKE) -C $(top_builddir)/src/xitk/xine-toolkit/po update-po
