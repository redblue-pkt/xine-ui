manfiles = xine.1 aaxine.1 xine-check.1 xine-remote.1
man_MANS = $(manfiles) xine-bugreport.1
mandir=@mandir@/es

EXTRA_DIST = $(manfiles) xine.1.in aaxine.1.in xine-check.1.in
DISTCLEANFILES = xine-bugreport.1

xine-bugreport.1: xine-check.1
	$(LN_S) xine-check.1 xine-bugreport.1
