SUBDIRS = metal lcd pitt

install-exec-am:
	if test -d @XINE_SKINDIR@/default; then \
	  rm -rf @XINE_SKINDIR@/default; \
	fi; \
	$(LN_S) @XINE_SKINDIR@/metal @XINE_SKINDIR@/default;

debug:
install-debug: install

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#*

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
