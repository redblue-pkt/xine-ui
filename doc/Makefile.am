
SUBDIRS = man

EXTRA_DIST = $(docs_DATA)


docsdir = @XINE_DOCDIR@

docs_DATA = \
	README.config_en \
	README.corba \
	README_en \
	README_cs \
	README_es \
	README_pl \
	README_it \
	README_fi \
	README_uk \
	README_fr \
	README_de

debug:
install-debug: install

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#*

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
