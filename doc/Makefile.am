
SUBDIRS = man

EXTRA_DIST = $(docs_DATA)


docsdir = @XINE_DOCDIR@

docs_DATA = \
	README.config_en \
	README_en \
	README_cs \
	README_es \
	README_pl \
	README_it \
	README_fi \
	README_uk \
	README_fr \
	README_de \
	README.en.0.Introduction \
	README.en.5.xine-ui \
	README.en.10.Remote_control \
	README.en.15.MRLs \
	README.en.20.DVD \
	README.en.25.VideoCD \
	README.en.30.Advanced \
	README.en.35.Legal_issues

debug:
install-debug: install

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#*

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
