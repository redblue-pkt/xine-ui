##
## Process this file with automake to produce Makefile.in
##

AUTOMAKE_OPTIONS = 1.3

SUBDIRS = src m4 misc 

EXTRA_DIST = cvscompile.sh

debug:
	list='$(SUBDIRS)'; for subdir in $$list; do \
	  (cd $$subdir && $(MAKE) $@) \
	done;
	$(MAKE) CFLAGS="$(DEBUG_CFLAGS) @ORBIT_CFLAGS@"


prune-cache:
	-rm -f config.cache


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
