##
## Process this file with automake to produce Makefile.in
##
EXTRA_DIST = _xine.m4 ORBit.m4 xine.m4

debug:

install-debug: debug
        list='$(SUBDIRS)'; for subdir in $$list; do \
          (cd $$subdir && $(MAKE) $@) \
        done;

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#*


maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
