manfiles = xine.1 aaxine.1 xine-check.1
man_MANS = $(manfiles) xine-bugreport.1

EXTRA_DIST = $(manfiles) xine.1.in aaxine.1.in

xine-bugreport.1: xine-check.1
	ln -s xine-check.1 xine-bugreport.1


mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#*

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in xine.1
