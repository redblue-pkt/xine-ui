EXTRA_DIST = autogen.sh SlackBuild.in SlackBuild build_rpms.sh \
	xine-ui.spec.in xine-ui.spec

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
