EXTRA_DIST = \
	getopt.c \
	getopt1.c \
	getopt.h \
	utils.c \
	xscreensaver-remote.c

noinst_HEADERS = \
	utils.h \
	xscreensaver-remote.h

debug:
install-debug: debug
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am


mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#*


maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
