
SUBDIRS = man

EXTRA_DIST = $(docs_DOCS)

docdir = ${prefix}/share/doc/xine-ui
docs_DOCS = FAQ FAQ_es FAQ_fr FAQ_it FAQ_pl \
	README README.corba README.dxr3 README.dxr3_pl README.freebsd README.irix README.mrl \
	README.solaris README.syncfb README.xinerc README_es README_pl README_it \
	bug_report_form README.divx4

check:
	@documentations='$(docs_DOCS)'; \
	ddir=`pwd | sed 's/\\/\=build//'`; \
	echo ddir $$ddir; \
	for doc in $$documentations; do \
	  name=`echo $$doc`; \
	  echo "copying $$ddir/$$doc as ./$$name"; \
	  cp -p $$ddir/$$doc ./$$name; \
	done


install-data-local:
	@documentations='$(docs_DOCS)'; \
	for doc in $$documentations; do \
	  destdir=$(DESTDIR)$(docdir); \
	  name=`echo $$doc` \
	  dir=$$destdir; \
	  $(mkinstalldirs) $$dir; \
	  echo "installing $$doc as $$dir/$$name"; \
	  $(INSTALL_DATA) $$doc $$dir/$$name; \
	done


uninstall-local:
	@documentations='$(docs_DOCS)'; \
	for doc in $$documentations; do \
	  destdir=$(DESTDIR)$(docdir); \
	  name=`echo $$doc` \
	  dir=$$destdir; \
	  echo "removing $$dir/$$name" ; \
	  rm -f $$dir/$$name; \
	done


debug:
install-debug:

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#*

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
