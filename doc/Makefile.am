
SUBDIRS = man

EXTRA_DIST = $(docs_DATA)

docsdir = @XINE_DOCDIR@

docs_DATA = FAQ_de FAQ_en FAQ_es FAQ_fr FAQ_it FAQ_pl FAQ_uk \
	README_en README.corba README.dxr3 README.dxr3_pl README.freebsd README.irix README.mrl \
	README.solaris README.syncfb README.opengl README.config_en README_es README_pl README_it \
	bug_report_form README.divx4 README_fi README_uk README_fr README_de README.tvmode

debug:
install-debug:

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#*

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
