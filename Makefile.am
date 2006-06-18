##
## Process this file with automake to produce Makefile.in
##

AUTOMAKE_OPTIONS = 1.3

ACLOCAL_AMFLAGS = @XINE_ACFLAGS@ -I m4

SUBDIRS = doc m4 po misc src  

DEBFILES = debian/README.Debian debian/changelog debian/control \
	debian/copyright debian/links debian/menu \
	debian/rules debian/shlibdeps.sh debian/xine-ui.mime 

EXTRA_DIST = config.rpath autogen.sh\
	$(DEBFILES)
	@DEPCOMP@

debug:
	@list='$(SUBDIRS)'; for subdir in $$list; do \
	  (cd $$subdir && $(MAKE) $@) || exit; \
	done;
	$(MAKE) CFLAGS="$(DEBUG_CFLAGS)"
## corba disabled @ORBIT_CFLAGS@"

install-debug: debug
	@list='$(SUBDIRS)'; for subdir in $$list; do \
	  (cd $$subdir && $(MAKE) $@) || exit; \
	done;

prune-cache:
	-rm -f config.cache

release-check:
	@./config.status misc/relchk.sh
	@mv -f .cvsversion .cvsversion.tmp
	@./autogen.sh noconfig && $(SHELL) misc/relchk.sh
	@mv -f .cvsversion.tmp .cvsversion

slackbuild:
	@(cd misc && $(MAKE) SlackBuild) && \
	 export PREFIX="/usr"; ./configure --prefix=$$PREFIX && misc/SlackBuild

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#* 
	-rm -f $(PACKAGE)_$(VERSION).tar.gz
	-rm -f $(distdir).tar.gz $(PACKAGE).tgz package_descriptions
	-rm -rf $(distdir)


maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in configure acinclude.m4 aclocal.m4
	-rm -f config.h.in stamp-h.in ltconfig ltmain.sh
	-rm -f config.guess config.sub install-sh missing mkinstalldirs
	-rm -f depcomp config.log
	-rm -rf autom4te.cache

maintainer-clean-generic-hook: 
	rm -f config.status
