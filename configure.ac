dnl
dnl Configure.in for xine
dnl

AC_INIT
AC_CONFIG_SRCDIR([src/xitk/main.c])

dnl
dnl Require autoconf version 2.13
dnl
AC_PREREQ(2.53)

dnl
dnl Require libtool minimum version 1.4.0
dnl
dnl AC_PREREQ_LIBTOOL(1.4.0,,AC_MSG_ERROR(*** You should have libtool >= 1.4 installed ***))

dnl
dnl Don't forget to update the misc/xine_splash.{xcf,png}
dnl
XINE_MAJOR=0
XINE_MINOR=99
XINE_SUB=1
if test -f .cvsversion; then
  XINE_PRE="cvs"
else
  XINE_PRE=""
fi


TAR_NAME="xine-ui-"$XINE_MAJOR.$XINE_MINOR.$XINE_SUB$XINE_PRE
SPEC_VERSION=$XINE_MAJOR.$XINE_MINOR.$XINE_SUB$XINE_PRE
#TAR_NAME="xine-ui-"$XINE_MAJOR$XINE_PRE
#SPEC_VERSION=$XINE_MAJOR$XINE_PRE
AC_SUBST(XINE_MAJOR)
AC_SUBST(XINE_MINOR)
AC_SUBST(XINE_SUB)
AC_SUBST(TAR_NAME)
AC_SUBST(SPEC_VERSION)

##
AC_CANONICAL_TARGET([])

##
AM_INIT_AUTOMAKE("xine-ui", $XINE_MAJOR.$XINE_MINOR.$XINE_SUB$XINE_PRE)

dnl
dnl Made possible to build for another arch.
dnl
if test x$XINE_BUILD != "x"; then
  AC_MSG_RESULT(*** build forced to $XINE_BUILD ***)
  build=$XINE_BUILD
  host=$XINE_BUILD
else
  check_athlon=yes
fi

AM_CONFIG_HEADER(config.h)

dnl
dnl Check for programs.
dnl
dnl Save CFLAGS, AC_ISC_POSIX set some unwanted default CFLAGS
saved_CFLAGS="$CFLAGS"
AC_ISC_POSIX
CFLAGS="$saved_CFLAGS"
AC_PROG_CC
AC_MINIX dnl readline
AC_HEADER_STDC([])
AC_PROG_MAKE_SET
AC_PROG_INSTALL
dnl obsolete AC_PROG_RANLIB
AC_PROG_LN_S

dnl
dnl Libtool
dnl
AC_LIBTOOL_DLOPEN
AM_DISABLE_STATIC
AM_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)
if ${CONFIG_SHELL} ./libtool --features | grep "enable static" >/dev/null; then
  STATIC="-static"
else
  STATIC=
fi
AC_SUBST(STATIC)


dnl
dnl Build all libs as static
dnl
BUILD_LIB_STATIC="-static"
BUILD_LIB_STATIC=""
AC_SUBST(BUILD_LIB_STATIC)


dnl
dnl NLS Support
dnl
dnl AM_WITH_NLS(use-libtool,,)
dnl ALL_LINGUAS="cs da de el en@quot en@boldquot es et fr gl id it ja ko nl nn no pl pt pt_BR ru sl sv tr zh"
ALL_LINGUAS="fr es pt_BR de sk pl uk cs it"
AM_GNU_GETTEXT(use-libtool, need-ngettext)
AC_PROG_GMSGFMT_PLURAL
AC_CONFIG_LINKS($nls_cv_header_libgt, $nls_cv_header_intl)
AM_CONDITIONAL(INCLUDED_INTL, test x$USE_INCLUDED_LIBINTL = "xyes")
if test x"$USE_INCLUDED_LIBINTL" = x"yes"; then
   INTLDIR="-I\$(top_builddir)/intl -I\$(top_srcdir)/intl"
fi
dnl AC_SUBST(INTLDIR)


dnl
dnl Checks for typedefs, structures, and compiler characteristics.
dnl
AC_C_CONST
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
dnl AC_CHECK_TYPES([ptrdiff_t])
dnl AC_C_BIGENDIAN

AC_SUBST(DEBUG_CFLAGS)


dnl
dnl Check for xine-lib
dnl
AM_PATH_XINE(1.0.0,, AC_MSG_ERROR(*** You should install xine-lib first ***))
dnl the following is an interim solution for the rc series
dnl checking just the version is not enough, because this is always 1.0.0
SAVE_CFLAGS="$CFLAGS"
CFLAGS="$XINE_CFLAGS $CFLAGS"
AC_CHECK_DECL(XINE_PARAM_AUDIO_AMP_MUTE,,
  AC_MSG_ERROR(*** Your version of xine-lib is too old ***),[#include "xine.h"])
CFLAGS="$SAVE_CFLAGS"


dnl
dnl threads: xine-config tell us what should be used, but
dnl          xitk need to be linked to thread lib, so the follow AC_SUBST() 
dnl          are only used in src/xitk/xine-toolkit/Makefile.am
dnl
case "$host" in
  *-*-freebsd*|*-*-openbsd*)
    THREAD_LIBS="-L/usr/local/lib -pthread"
    THREAD_CFLAGS="-I/usr/local/include -D_THREAD_SAFE"
    CFLAGS="-L/usr/local/lib $THREAD_CFLAGS $CFLAGS"
    CPPFLAGS="-I/usr/local/include -L/usr/local/lib $CPPFLAGS"
    ;;

  *-*-hpux11*)
    THREAD_LIBS=" -pthread"
    THREAD_CFLAGS="-D_REENTRANT"
    CFLAGS="$THREAD_CFLAGS $CFLAGS"
    ;;

  *)
    AC_CHECK_LIB(pthread, pthread_create,
  	     THREAD_LIBS="-lpthread",
 	     AC_MSG_ERROR(pthread needed))
    ;;
esac
AC_SUBST(THREAD_LIBS)
AC_SUBST(THREAD_CFLAGS)

dnl
dnl dynamic linker
dnl
AC_CHECK_LIB(c, dlopen,
	     DYNAMIC_LD_LIBS="",
	     AC_CHECK_LIB(dl, dlopen,
             	          DYNAMIC_LD_LIBS="-ldl",
	                  AC_MSG_ERROR(dynamic linker needed)))
AC_SUBST(DYNAMIC_LD_LIBS)


dnl
dnl Checks for X11
dnl
AC_PATH_XTRA
if test x"$no_x" != x"yes"; then
    AC_DEFINE(HAVE_X11,,[Define this if you have X11R6 installed])
fi
AM_CONDITIONAL(HAVE_X11, [test x"$no_x" != "xyes"])

AC_CHECK_LIB(Xv, XvQueryExtension, 
            [ XV_LIB="-lXv"
	      AC_DEFINE(HAVE_XV, 1, [Define this if you have libXv installed])
            ],, [$X_LIBS $X_PRE_LIBS -lXext $X_EXTRA_LIBS])
AC_SUBST(XV_LIB)
AM_CONDITIONAL(HAVE_XV, test x$ac_have_xv = "xyes")


dnl
dnl Shm
dnl
AC_ARG_ENABLE(shm, [  --disable-shm           Disable shared memory support if available.], echo $enable_shm, enable_shm="yes")

dnl If we are compiling without shared memory support, then don\'t do
dnl the checks for XShm
if test "x$enable_shm" = "xyes"; then
  dnl Check for the Xext library (needed for XShm extension)
  AC_CHECK_LIB(Xext, XShmAttach, 
      X_LIBS="-lXext $X_LIBS", 
      dnl On AIX, it is in XextSam instead, but we still need -lXext
      AC_CHECK_LIB(XextSam, XShmAttach, 
          X_LIBS="-lXextSam -lXext $X_LIBS", 
          no_xshm_ext=yes, $X_LIBS),
      $X_LIBS)

  dnl Test for shared memory headers and semantics, unless we are
  dnl configured to build without shared memory extensions.

  dnl Check for shared memory
  AC_CHECK_HEADER(sys/ipc.h, 
	AC_DEFINE(HAVE_IPC_H,,[Define if ipc.h installed])
	, no_sys_ipc=yes)
  AC_CHECK_HEADER(sys/shm.h,
	AC_DEFINE(HAVE_SHM_H,,[Define if shm.h installed])
	, no_sys_shm=yes)

  if test "x$no_sys_ipc" != "xyes" \
     && test "x$no_sys_shm" != "xyes" \
     && test "x$no_xshm_ext" != "xyes" ; then
    AC_DEFINE(HAVE_SHM,,[Define if have shm])
  fi

  dnl Check whether shmctl IPC_RMID allowes subsequent attaches
  if test "$ac_cv_header_sys_shm_h" = "yes"; then
    AC_MSG_CHECKING(whether shmctl IPC_RMID allowes subsequent attaches)
    AC_TRY_RUN([
          #include <sys/types.h>
          #include <sys/ipc.h>
          #include <sys/shm.h>
          int main()
          {
            int id;
            char *shmaddr;
          id = shmget (IPC_PRIVATE, 4, IPC_CREAT | 0777);
          if (id == -1)
            exit (2);
            shmaddr = shmat (id, 0, 0);
            shmctl (id, IPC_RMID, 0);
            if ((char*) shmat (id, 0, 0) == (char*) -1)
            {
              shmdt (shmaddr);
              exit (1);
            }
            shmdt (shmaddr);
            shmdt (shmaddr);
            exit (0);
          }
      ],
      AC_DEFINE(IPC_RMID_DEFERRED_RELEASE,,[Define if IPC_RMID_DEFERRED_RELEASE is true])
        AC_MSG_RESULT(yes),
      AC_MSG_RESULT(no),
      AC_MSG_RESULT(assuming no))
  fi

  dnl Check for the X shared memory extension header file
  if test "x$no_xext_lib" = "xyes"; then
    dnl If we couldn't find XShmAttach earlier, don't use the header
    no_xshm=yes
  else
    saved_CPPFLAGS=$CPPFLAGS
    CPPFLAGS="$CPPFLAGS $X_CFLAGS"
    AC_CHECK_HEADER(X11/extensions/XShm.h, 
                   [ AC_DEFINE(HAVE_XSHM_H, 1, [Define if includes XShm.h])], no_xshm=yes,
                   [#include <X11/Xlib.h>])
    CPPFLAGS=$saved_CPPFLAGS
  fi
fi


dnl
dnl Checks for Xinerama extension
dnl
AC_CHECK_LIB(Xinerama, XineramaQueryExtension, 
             X_PRE_LIBS="$X_PRE_LIBS -lXinerama"
             AC_DEFINE(HAVE_XINERAMA,,[Define this if you have libXinerama installed])
             ac_have_xinerama="yes",,
             $X_LIBS $X_PRE_LIBS -lXext $X_EXTRA_LIBS)
AM_CONDITIONAL(HAVE_XINERAMA, test x$ac_have_xinerama = "xyes")


dnl
dnl Checks for XF86VidMode extension
dnl
AC_CHECK_LIB(Xxf86vm, XF86VidModeQueryExtension, 
             X_PRE_LIBS="$X_PRE_LIBS -lXxf86vm"
             AC_DEFINE(HAVE_XF86VIDMODE,,[Define this if you have libXxf86vm installed])
             ac_have_xf86vidmode="yes",,
             $X_LIBS $X_PRE_LIBS -lXext $X_EXTRA_LIBS)
AM_CONDITIONAL(HAVE_XF86VIDMODE, test x$ac_have_xf86vidmode = "xyes")


dnl
dnl XTest Extension
dnl
AC_CHECK_LIB(Xtst, XTestFakeKeyEvent, 
             XTEST_LIBS="-lXtst"
             AC_DEFINE(HAVE_XTESTEXTENSION,,[Define this if you have libXtst installed])
	     ac_have_xtest="yes",, $X_LIBS $X_PRE_LIBS -lXext $X_EXTRA_LIBS)
AC_SUBST(XTEST_LIBS)
AM_CONDITIONAL(HAVE_XTESTEXTENSION, test x$ac_have_xtest = "xyes")
 

dnl
dnl readline
dnl
AC_CHECK_READLINE
AM_CONDITIONAL(HAVE_READLINE, test x"$have_readline" = "xyes")
if test x"$have_readline" != "xyes"; then
  AC_MSG_RESULT([*** no readline found, xine network interface will be disabled ***])
fi

dnl
dnl ncurses
dnl
AC_CHECK_HEADERS(curses.h)
AC_CHECK_LIB(ncurses, cbreak,
             AC_DEFINE(HAVE_NCURSES,,[Define this if you have ncurses installed])
             ac_have_ncurses="yes"
             NCURSES_LIB="-lncurses")
AC_SUBST(NCURSES_LIB)
AM_CONDITIONAL(HAVE_NCURSES, test x$ac_have_ncurses = "xyes")

dnl
dnl Libcurl
dnl
AM_PATH_CURL(7.10.2, [
   AC_DEFINE(HAVE_CURL,,[Define this if you have libcurl installed])], [
   AC_MSG_RESULT(*** All of curl dependent parts will be disabled ***)])
AM_CONDITIONAL(HAVE_CURL, test x$no_curl != "xyes")


dnl
dnl
dnl
NET_LIBS=""
AC_CHECK_LIB(socket, socket, NET_LIBS="-lsocket $NET_LIBS",)
AC_CHECK_LIB(nsl, gethostbyname, NET_LIBS="-lnsl $NET_LIBS",)
AC_SUBST(NET_LIBS)

dnl
dnl check for ascii-art library
dnl
AM_PATH_AALIB(1.2.0,,)
AM_CONDITIONAL(HAVE_AA, test x$no_aalib != "xyes")

dnl 
dnl checks for Color AsCii Art library
dnl 
AM_PATH_CACA(0.3,, AC_MSG_RESULT([*** All CACA-dependent parts will be disabled ***]))
AM_CONDITIONAL(HAVE_CACA, test x$no_caca != "xyes")

dnl option for font sets and multibyte string support
AC_ARG_ENABLE(mbs, [  --disable-mbs           Disable multibyte string support], echo $enable_mbs, enable_mbs="yes")
if test "x$enable_mbs" = "xyes"; then
  AC_DEFINE(WITH_XMB,,[Define this if you want support multibyte strings in gui])
fi

dnl
dnl Find pkg-config
dnl
AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
if test x"$PKG_CONFIG" != "xno" ; then

  if test x$pkg_exec_prefix != x ; then
    if test x${PKG_CONFIG+set} != xset ; then
      PKG_CONFIG=$pkg_exec_prefix/bin/pkg-config
    fi
  fi

else
  echo "*** pkg-config not found. See http://pkgconfig.sourceforge.net"
  echo "*** All of DIRECTFB dependent parts will be disabled"
fi

dnl
dnl Check for Xft
dnl
AC_MSG_CHECKING([whether to have Xft support])
AM_PATH_XFT(yes, XFT=true, XFT=false)
if test x"$XFT" = "xtrue" ; then
  saved_CFLAGS=$CFLAGS
  saved_LIBS=$LIBS
  CFLAGS="$CFLAGS $XFT_CFLAGS"
  LIBS="$LIBS $XFT_LIBS"
  AC_TRY_LINK([
#include <X11/Xft/Xft.h>], [ XftFontClose(0, 0); return 1; ], 
    [
      AC_DEFINE(WITH_XFT, 1, [font antialiasing support])
      AC_MSG_CHECKING([Xft UTF-8 support])
      AC_TRY_LINK([
#include <X11/Xft/Xft.h>
        ], [ 
XftDrawStringUtf8(0, 0, 0, 0, 0, 0, 0); return 0; 
        ],
          AC_DEFINE(HAVE_XFT_UTF8_STRING, 1, "Xft UTF8 support")
          AC_MSG_RESULT(yes),
          AC_MSG_RESULT(no)) 
    ],
    [
      AC_MSG_RESULT([***Could not link with Xft. Install Xft if you want support for it***])
      XFT=false
      XFT_CFLAGS=
      XFT_LIBS=
    ])
  CFLAGS=$saved_CFLAGS
  LIBS=$saved_LIBS
fi
AM_CONDITIONAL(WITH_XFT, test x"$XFT" = "xtrue")

dnl
dnl Check for DirectFB
dnl

dnl NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE
dnl  DFB:
dnl  Temporary disabled, port to new API needed
dnl NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE
if test x"$PKG_CONFIG" != "xno" ; then
  if test "x" != "x"; then

    DIRECTFB_REQUIRED_VERSION=0.9.10

    AC_MSG_CHECKING(for DirectFB)
    if $PKG_CONFIG --atleast-version $DIRECTFB_REQUIRED_VERSION directfb ; then
        AC_MSG_RESULT(found)
        DIRECTFB_CFLAGS=`$PKG_CONFIG --cflags directfb`
        DIRECTFB_LIBS=`$PKG_CONFIG --libs directfb`
        have_directfb="yes"
    else
        AC_MSG_RESULT([*** All of DIRECTFB dependent parts will be disabled ***])
    fi

  fi

fi

AC_SUBST(DIRECTFB_CFLAGS)
AC_SUBST(DIRECTFB_LIBS)
AM_CONDITIONAL(HAVE_DIRECTFB, test x$have_directfb = "xyes" )

dnl
dnl check for libpng
dnl
AC_ARG_WITH(png-prefix,
    [  --with-png-prefix=PFX   Prefix where PNG library is installed (optional)], png_prefix="$withval", png_prefix="")

if test x"$png_prefix" != "x"; then
    PNG_LIB="-L$png_prefix/lib"
    PNG_CFLAGS="-I$png_prefix/include"
    old_CPPFLAGS="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS $PNG_CFLAGS"
fi


dnl
dnl First, add -L and -R options for png and X11 to LDFLAGS.
dnl LDFLAGS are passed to the compiler before LIBS; this is important for
dnl some non-gcc compilers, which need a -L{dir} option before a -l{lib}
dnl to see the library we are looking for in directory "dir"
dnl
dnl BTW: Do we really need the X11 library path ($X_LIBS) for a libpng check?
dnl
old_LDFLAGS="$LDFLAGS"
LDFLAGS="$X_LIBS $PNG_LIB $LDFLAGS"


dnl
dnl Look for a -lpng library, that provides a png_read_info routine
dnl If we find such a library,  check that the png.h header is available, too.
dnl
AC_CHECK_LIB(png, png_read_info,
  [AC_CHECK_HEADERS(png.h libpng/png.h,,)],
  [AC_MSG_ERROR([*** libpng is needed (PNG library not found) - try to use --with-png-prefix option ***])], [-lz -lm])

dnl Recent png install header files into $prefix/include/libpng
if test x"$ac_cv_header_libpng_png_h" = "xyes"; then
    PNG_H="libpng/png.h"
fi
if test x"$ac_cv_header_png_h" = "xyes"; then
    PNG_H="png.h"
fi

if test x"$png_prefix" != "x"; then
    CPPFLAGS="$old_CPPFLAGS"
fi
LDFLAGS="$old_LDFLAGS"

if test x"$ac_cv_header_png_h" = "xyes" -o x"$ac_cv_header_libpng_png_h" = "xyes"; then

    dnl OK, -lpng library and <png.h|libpng/png.h> header found.
    dnl Check if this png is an old png version.

    saved_CFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS $PNG_CFLAGS"

    AC_MSG_CHECKING([for png_structp in $PNG_H])
    AC_TRY_COMPILE([#include <$PNG_H>],
	[
	png_structp pp; 
	png_infop info; 
	png_colorp cmap; 

	png_create_read_struct; 
	png_set_IHDR;
	], png_ok=yes, png_ok=no)

    AC_MSG_RESULT($png_ok)
    if test x"$png_ok" = xno; then
    	AC_MSG_ERROR([*** libpng is needed (PNG library is too old) ***])
    fi
    CFLAGS="$saved_CFLAGS"
else
    AC_MSG_ERROR([*** libpng is needed (PNG header file not found) - try to use --with-png-prefix option ***])
fi


if test x"$png_ok" = xyes; then
    X_PRE_LIBS="$X_PRE_LIBS $PNG_LIB -lpng -lz -lm"
    X_CFLAGS="$X_CFLAGS $PNG_CFLAGS"
fi

dnl
dnl
dnl
AC_CHECK_LIB(posix4, nanosleep)
AC_CHECK_FUNCS(vsnprintf snprintf strpbrk strsep nanosleep)
AC_CHECK_HEADER(stdarg.h, AC_DEFINE(HAVE_STDARGS,,[Define this is you have stdarg.h header file installed]))
AC_CHECK_HEADERS(string.h strings.h alloca.h linux/kd.h)


dnl
dnl Check for LIRC client support
dnl
AC_CHECK_LIRC


dnl
dnl Check for nvtvsimple library
dnl
if test x"$PKG_CONFIG" != "xno" ; then
  AC_MSG_CHECKING(for nvtvsimple)
  if $PKG_CONFIG --atleast-version 0.4.6 nvtvsimple; then
        AC_MSG_RESULT(found)
        NVTVSIMPLE_CFLAGS=`$PKG_CONFIG --cflags nvtvsimple`
        NVTVSIMPLE_LIBS=`$PKG_CONFIG --libs nvtvsimple`
        have_nvtvsimple="yes"
        AC_DEFINE([HAVE_NVTVSIMPLE],,
		  [Define this if you have nvtvsimple installed])
  else
    AC_MSG_RESULT([*** nvtvsimple support will be disabled ***])
  fi
fi
AC_SUBST(NVTVSIMPLE_CFLAGS)
AC_SUBST(NVTVSIMPLE_LIBS)


dnl
dnl xine-ui use tar, check it
dnl
AC_CHECK_PROG(TAR, tar, tar, no)
if test x"$TAR" = "xno"; then
    AC_MSG_RESULT([*** tar depend part will be disabled (skin download) ***])
else
    AC_DEFINE(HAVE_TAR,,[Define this if you have tar installed])
fi


dnl
dnl
dnl
BUILD_CC="`$CC -v 2>&1 | tail -1`"
BUILD_OS="`uname -s -r -m`"
BUILD_DATE="`date \"+%a %d %b %Y %T\"`"
AC_SUBST(BUILD_CC)
AC_SUBST(BUILD_OS)
AC_SUBST(BUILD_DATE)

if test "$GCC" = yes; then
    dnl
    dnl check cflags not supported by all gcc versions
    dnl eg: -mpreferred-stack-boundary=2 and 2.91.66,
    dnl and gcc-2.7.2.3 support a bit less options
    dnl
    AC_TRY_CFLAGS("-mpreferred-stack-boundary=2", 
	m_psb="-mpreferred-stack-boundary=2", m_psb="")
    AC_TRY_CFLAGS("-fno-strict-aliasing", f_nsa="-fno-strict-aliasing", f_nsa="")
    AC_TRY_CFLAGS("-fschedule-insns2", f_si="-fschedule-insns2", f_si="")
    AC_TRY_CFLAGS("-mwide-multiply", m_wm="-mwide-multiply", m_wm="")
    dnl
    dnl gcc 3.1 uses the -f version
    dnl
    AC_TRY_CFLAGS("-falign-functions=4", f_af="-falign-functions=4",
        f_af="-malign-functions=4")
    AC_TRY_CFLAGS("-falign-loops=4", f_al="-falign-loops=4",
        f_al="-malign-loops=4")
    AC_TRY_CFLAGS("-falign-jumps=4", f_aj="-falign-jumps=4",
        f_aj="-malign-jumps=4")
    dnl
    dnl Warnings
    dnl
    CFLAGS="-Wchar-subscripts -Wmissing-declarations -Wmissing-prototypes $CFLAGS"
    CFLAGS="-Wnested-externs -Wcast-align $CFLAGS"
    dnl some combinations of gcc+glibc produce useless warnings on memset
    dnl when compiling with -Wpointer-arith, so we check for this first
    AC_MSG_CHECKING(for sane -Wpointer-arith)
    SAVE_CFLAGS="$CFLAGS"
    CFLAGS="-O2 -Wpointer-arith -Werror $CFLAGS"
    AC_TRY_COMPILE([#include <string.h>],[int a; memset(&a, 0, sizeof(int));],
	[AC_MSG_RESULT(yes); CFLAGS="-Wpointer-arith $SAVE_CFLAGS"],
	[AC_MSG_RESULT(no);  CFLAGS="$SAVE_CFLAGS"]);
fi

dnl Flags not supported by all *cc* variants
AC_TRY_CFLAGS("-Wall", wall="-Wall", wall="")

dnl Common cflags for all platforms
COMMON_CFLAGS="$wall -D_FILE_OFFSET_BITS=64"

host_or_hostalias="$host"
if test "$host_or_hostalias" = ""; then
    dnl user has called ./configure with a host parameter unknown to
    dnl config.sub
    dnl
    dnl Try the following switch with user's original host-alias
    dnl input instead.
    dnl
    host_or_hostalias="$host_alias"
fi

case "$host_or_hostalias" in
  i386-*-freebsd*)
    CFLAGS="-pipe -fomit-frame-pointer $f_af $f_al $f_aj $m_wm $m_psb -fexpensive-optimizations $f_si $f_nsa -ffast-math -funroll-loops -finline-functions -D_REENTRANT $CFLAGS"
    DEBUG_CFLAGS="$X_CFLAGS -D_REENTRANT -DDEBUG $DEBUG_CFLAGS"
    AC_DEFINE(__i386__,1,[Define this if you're running x86 architecture])
    AC_DEFINE([ARCH_X86],,[Define this if you're running x86 architectur])

    AC_CHECK_HEADER(/usr/local/include/iconv.h,
			CFLAGS="-I/usr/local/include $CFLAGS"
			LDFLAGS="-L/usr/local/lib $LDFLAGS"
	                GICONV_BSD_LIBS="-liconv"
		        no_BSDiconv="no", 
                        [ no_BSDiconv="yes"
	                  echo
                          echo "****************************************************************"
		          echo "* You need to install a recent version of the port 'libiconv'  *"
			  echo "* (in /usr/ports/converters/libiconv).                         *"
			  echo "* The library in this port is needed to successfully compile   *"
			  echo "* libsputext plugin.                                           *"
                          echo "****************************************************************"
	                  echo 
                        ],)
    ;;

  i?86-*-linux* | i386-*-solaris* | i?86-* | k?-* | athlon-* | pentium*)
    if test "$GCC" = yes -o "${CC##*/}x" = "iccx" ; then

      if test "$GCC" = yes ; then
	dnl Check for gcc cpu optimization support
	AC_TRY_CFLAGS("-mtune=i386", 
	  sarchopt="-mtune",
  	  AC_TRY_CFLAGS("-mcpu=i386", 
	    sarchopt="-mcpu",
            AC_TRY_CFLAGS("-march=i386", 
	      sarchopt="-march",
	      [ AC_MSG_RESULT(** no cpu optimization supports **)
	        sarchopt=no 
	      ]
            )
          )
        )

	dnl special check for k7 cpu CC support
        AC_TRY_CFLAGS("$sarchopt=athlon", k7cpu="athlon", k7cpu="i686")

	dnl add x86 specific gcc CFLAGS
	CFLAGS="-O3 -pipe -fomit-frame-pointer $f_af $f_al $f_aj $m_wm $m_psb -fexpensive-optimizations $f_si $f_nsa -ffast-math -funroll-loops -finline-functions $CFLAGS"

	DEBUG_CFLAGS="-O $DEBUG_CFLAGS"

	if test x"$sarchopt" != "xno"; then
	    [
	    archopt_val=

	    case "$host_or_hostalias" in
	    i386-*) # *BSD return this even on a P III #-))
		archopt_val="i386" ;;
	    i486-*) # oh dear!
		archopt_val="i486" ;;
	    i586-*)
		archopt_val="pentium" 
                ;;
	    pentium-mmx-*)
		archopt_val="pentium-mmx" 
		;;
	    pentiumpro-* | pentium2-* | i686-*)
		archopt_val="pentiumpro"
		if test x"$check_athlon" = "xyes"; then
		    if test -f /proc/cpuinfo; then
			modelname=`cat /proc/cpuinfo | grep "model\ name\	:" | sed -e 's/ //g' | cut -d':' -f2`
			case "$modelname" in
			*Athlon* | *Duron* | *K7*)
			    archopt_val="$k7cpu"
			    ;;
			VIAEzra)
			    archopt_val="c3"
			    ;;
			esac
		    fi 
		fi
		;;
	    k6-2-* | k6-3-*)
		archopt_val="k6-2"
		;;
	    k6-*)
		archopt_val="k6"
		;;
	    pentium3-*)
		archopt_val="pentium3"
		;;
	    pentium4-*)
		archopt_val="pentium4"
		;;
	    athlon-4-* | athlon-xp-* | athlon-mp-*)
		archopt_val="athlon-4"
		;;
	    k7-* | athlon-tbird-* | athlon-*)
		archopt_val="athlon"
		;;
	    
	    esac
	    if test x"$archopt_val" != x; then
		CFLAGS="$sarchopt=$archopt_val $CFLAGS"
		DEBUG_CFLAGS="$sarchopt=$archopt_val $DEBUG_CFLAGS"
	    fi
	    ]
	fi
      else
	dnl we have the Intel compiler
	CFLAGS="-unroll -ipo -ipo_obj -O3 $CFLAGS"
	PASS1_CFLAGS="-prof_genx -prof_dir \$(PWD)/\$(top_builddir)/ $PASS1_CFLAGS"
	PASS2_CFLAGS="-prof_use -prof_dir \$(PWD)/\$(top_builddir)/ $PASS2_CFLAGS"
	AC_SUBST(PASS1_CFLAGS)
	AC_SUBST(PASS2_CFLAGS)
      fi
	
    	dnl enable x86 specific parts of the code
	dnl
	dnl all of this stuff needs gcc/gas; it uses gnu c compiler extensions
	dnl like the extended asm() or __attribute(__cdecl__), or other direct
	dnl mmx/sse/3dnow assembler instructions. 
	dnl
    	AC_DEFINE_UNQUOTED(ARCH_X86,,[Define this if you're running x86 architecture])
	AC_DEFINE(FPM_INTEL,1,[Define to select libmad fixed point arithmetic implementation])
	enable_ffmmx="yes"
	enable_w32dll="yes"

        case "$host_or_hostalias" in
	*-*-cygwin) 
          enable_w32dll="no" 
	  LDFLAGS="-no-undefined @INTLLIBS@ $LDFLAGS"
	  ;;
	esac
    else
	dnl add x86 specific cc CFLAGS
	CFLAGS="-O $CFLAGS"
	DEBUG_CFLAGS="-O $DEBUG_CFLAGS"
    	AC_DEFINE_UNQUOTED(FPM_64BIT,,[Define to select libmad fixed point arithmetic implementation])
    fi

    AC_DEFINE(__i386__,1,[Define this if you're running x86 architecture])
    AC_DEFINE([ARCH_X86],,[Define this if you're running x86 architectur])
    ;;

  alphaev56-*)
    AC_DEFINE(__alpha__,1,[Define this if you're running Alpha architecture])
    CFLAGS="-O3 -mcpu=ev56 -mieee $CFLAGS"
    DEBUG_CFLAGS="-O -mcpu=ev56 -mieee $DEBUG_CFLAGS"
    ;;

  alpha*)
    AC_DEFINE(__alpha__,1,[Define this if you're running Alpha architecture])
    CFLAGS="-O3 -mieee $CFLAGS"
    DEBUG_CFLAGS="-O -mieee $DEBUG_CFLAGS"
    ;;

  ppc-*-linux-* | powerpc-*) 
    AC_DEFINE(__ppc__,1,[Define this if you're running PowerPC architecture])
    CFLAGS="-O3 -pipe -fomit-frame-pointer $m_wm $m_psb -fexpensive-optimizations $f_si $f_nsa -ffast-math -funroll-loops -finline-functions $CFLAGS"
    DEBUG_CFLAGS="-O $DEBUG_CFLAGS"
    ;;

  sparc*-*-linux*)
    AC_DEFINE(__sparc,1,[Define this if you're running Sparc architecture])
    case $host_alias in
    sparc-*)    cpu_cflags="-mcpu=supersparc -mtune=supersparc" ;;
    sparc64-*)
      cpu_cflags="-mcpu=ultrasparc -mtune=ultrasparc"
    ;;
    esac

    CFLAGS="-O3 $cpu_cflags -funroll-loops -finline-functions $CFLAGS"
    DEBUG_CFLAGS="-O $cpu_cflags -funroll-loops -finline-functions $DEBUG_CFLAGS"

    ;;

  sparc-*-solaris*)
    AC_DEFINE(__sparc,1,[Define this if you're running Sparc architecture])
    if test "$GCC" = yes; then
	case `uname -m` in
	sun4c) cpu_cflags="-mcpu=v7 -mtune=supersparc" ;;
	sun4m) cpu_cflags="-mcpu=v8 -mtune=supersparc" ;;
	sun4u)
	  case `$CC --version 2>/dev/null` in
	  1.*|2.*)
	    # -mcpu=ultrasparc triggers a GCC 2.95.x compiler bug when
	    # compiling video_out.c:
	    #   gcc: Internal compiler error: program cc1 got fatal signal 11
	    # avoid -mcpu=ultrasparc with gcc 2.*
            cpu_cflags="-mcpu=v8 -mtune=ultrasparc"
	    ;;
          *)
	    # GCC 3 or newer should have no problem with -mcpu=ultrasparc 
            cpu_cflags="-mcpu=ultrasparc -mtune=ultrasparc"
	    ;;
	  esac
          ;;
	*)     cpu_cflags= ;;
	esac

	cc_optimize_cflags="-O3 $cpu_cflags -funroll-loops -finline-functions"
	cc_debug_cflags="-O $cpu_cflags -funroll-loops -finline-functions"
    else
	case `uname -m` in
	sun4c) cpu_cflags="-xarch=v7 -xchip=ultra2" ;;
	sun4m) cpu_cflags="-xarch=v8 -xchip=ultra2" ;;
	sun4u) cpu_cflags="-xarch=v8plusa -xchip=ultra2" ;;
	esac

	cc_optimize_cflags="-xO5 $cpu_cflags -xCC"
	cc_debug_cflags="-O"
    fi
    CFLAGS="$cc_optimize_cflags $CFLAGS"
    DEBUG_CFLAGS="$cc_debug_cflags $DEBUG_CFLAGS"
    ;;

  mips-*)
    CFLAGS="-O3 $CFLAGS"
    DEBUG_CFLAGS="-O $DEBUG_CFLAGS"
    ;;

  m68k-*)
    dnl used to be -O2, but that makes gcc 2.95.2 segfault
    dnl see http://bugs.debian.org/146006 for more info
    CFLAGS="-O $CFLAGS"
    DEBUG_CFLAGS="-O $DEBUG_CFLAGS"
    ;;

  ia64-*)
    CFLAGS="-O3 $CFLAGS"
    DEBUG_CFLAGS="-O $DEBUG_CFLAGS"
    ;;

  hppa-*)
    dnl -O3 seems to trigger gcc internal compiler errors, at least up to gcc 3.2.1
    CFLAGS="-O $CFLAGS"
    DEBUG_CFLAGS="-O $DEBUG_CFLAGS"
    ;;

  hppa*-hp-*)
    CFLAGS="-O2 $CFLAGS"
    DEBUG_CFLAGS="-O $DEBUG_CFLAGS"
    ;;

  x86_64-*)
    CFLAGS="-O3 -fomit-frame-pointer $m_wm $m_psb -fexpensive-optimizations $f_si $f_nsa -ffast-math -funroll-loops -finline-functions $CFLAGS"
    DEBUG_CFLAGS="-O0 -g $DEBUG_CFLAGS"
    ;;

  s390-*)
    CFLAGS="-O3 $CFLAGS"
    DEBUG_CFLAGS="-O $DEBUG_CFLAGS"
    ;;

  armv4l-*-linux*)
    CFLAGS="-O2 -fsigned-char -ffast-math -mcpu=strongarm1100 -fomit-frame-pointer -fthread-jumps -fregmove $CFLAGS"
    dnl    CFLAGS="-O1 -fforce-mem -fforce-addr -fthread-jumps -fcse-follow-jumps -fcse-skip-blocks -fexpensive-optimizations -fregmove -fschedule-insns2 -finline-functions -fsigned-char -fomit-frame-pointer -march=armv4 -mtune=strongarm $CFLAGS"
    DEBUG_CFLAGS="-O2 $DEBUG_CFLAGS"
    ;;

  *)
    echo
    echo "****************************** WARNING ******************************"
    echo
    echo "Host type '$host' ($host_alias) is currently not supported by xine"
    echo
    echo "Assuming that -O3 makes the compiler produce useful code."
    echo
    echo "You might experience problems with this, so please report your"
    echo "architecture and, if possible, known good optimization flags for"
    echo "your compiler to <xine-devel@lists.sf.net>"\!
    echo
    echo "*********************************************************************"
    echo "(sleeping one minute so you can read this...)"
    sleep 60

    CFLAGS="-O3 $CFLAGS"
    DEBUG_CFLAGS="-O $DEBUG_CFLAGS"
    ;;
esac
AC_SUBST(GICONV_BSD_LIBS)
AM_CONDITIONAL(HAVE_GICONV, test x$no_BSDiconv != "xyes")

CFLAGS="-DNDEBUG $COMMON_CFLAGS $CFLAGS $ALSA_CFLAGS $ESD_CFLAGS"
DEBUG_CFLAGS="$COMMON_CFLAGS $DEBUG_CFLAGS $ALSA_CFLAGS $ESD_CFLAGS -g -DDEBUG"
AC_SUBST(CFLAGS)
AC_SUBST(DEBUG_CFLAGS)

dnl gcc __attribute__ ((aligned ()))
AC_C_ATTRIBUTE_ALIGNED


dnl
dnl Check for GNU getopt_long()
dnl
AC_MSG_CHECKING(for GNU getopt_long)
AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

static struct option long_options[] = {
  {"help"    , no_argument, 0, 1 },
  {"version" , no_argument, 0, 2 },
  {0         , no_argument, 0, 0 }
};

int main(int argc, char **argv) {
  int option_index = 0;
  int c;

  opterr = 0;
  while((c = getopt_long(argc, argv, "?hv", 
			 long_options, &option_index)) != EOF) {
  }
  return 0;
}
], [AC_MSG_RESULT(yes); 
	ac_getopt_long=yes; 
	AC_DEFINE(HAVE_GETOPT_LONG,,[Define this if you have GNU getopt_long() implemented])], 
   [AC_MSG_RESULT(no); ac_getopt_long=no],
   [AC_MSG_RESULT(no); ac_getopt_long=no])
AM_CONDITIONAL(HAVE_GETOPT_LONG, test x"$ac_getopt_long" = "xyes")

dnl
dnl desktop dir location, xine-config told us about this.
dnl
escapedprefix="`echo $prefix | sed -e 's/\\//\\\\\//g'`"
XINE_SCRIPTDIR="`echo $xine_script_dir|sed -e 's/\^'$escapedprefix/'\${prefix}'/`"
AC_SUBST(XINE_SCRIPTDIR)
AC_DEFINE_UNQUOTED(XINE_SCRIPTDIR, "$XINE_SCRIPTDIR", [Location of the scripts])

XINE_LOCALE="`echo $xine_locale_dir|sed -e 's/\^'$escapedprefix/'\${prefix}'/`"
AC_SUBST(XINE_LOCALE)
AC_DEFINE_UNQUOTED(XITK_LOCALE, "$XINE_LOCALE",[Location of Xine catalog files])


dnl
dnl NLS, part II
dnl
if test "x$prefix" = xNONE; then
   prefix="${ac_default_prefix}"
fi
if test "x$exec_prefix" = xNONE; then
   exec_prefix='${prefix}'
fi

if test "x$XINE_DOCPATH" = "x"; then
   XINE_DOCPATH="${datadir}/doc/xine-ui"
fi
eval XINE_DOCDIR=`eval echo "$XINE_DOCPATH"`
AC_SUBST(XINE_DOCDIR)
AC_DEFINE_UNQUOTED(XINE_DOCDIR, "$XINE_DOCDIR",[Location of documentation files])

dnl
dnl skins dir location, xine-config told us about this.
dnl
XINE_SKINDIR="${datadir}/xine/skins"
eval XINE_SKINDIR=`eval echo "$XINE_SKINDIR"`
AC_SUBST(XINE_SKINDIR)
AC_DEFINE_UNQUOTED(XINE_SKINDIR, "$XINE_SKINDIR",[Location of xine skin directory])

XINE_LOGO_MRL="file:$XINE_SKINDIR/xine-ui_logo.mpv"
AC_SUBST(XINE_LOGO_MRL)
AC_DEFINE_UNQUOTED(XINE_LOGO_MRL, "$XINE_LOGO_MRL",[official logo mrl])

XINE_BLACK_STREAM="file:$XINE_SKINDIR/black.mpv"
AC_SUBST(XINE_BLACK_STREAM)
AC_DEFINE_UNQUOTED(XINE_BLACK_STREAM, "$XINE_BLACK_STREAM",[black stream])

XINE_SPLASH="$XINE_SKINDIR/xine_splash.png"
AC_SUBST(XINE_SPLASH)
AC_DEFINE_UNQUOTED(XINE_SPLASH, "$XINE_SPLASH",[splash image])

XINE_VISDIR="${datadir}/xine/visuals"
eval XINE_VISDIR=`eval echo "$XINE_VISDIR"`
AC_SUBST(XINE_VISDIR)
AC_DEFINE_UNQUOTED(XINE_VISDIR, "$XINE_VISDIR",[Location of xine visual animation streams])

DATA_DIR=`eval echo "$datadir"`
AC_SUBST(DATA_DIR)
SYSCONF_DIR=`eval echo "$sysconfdir"`
AC_SUBST(SYSCONF_DIR)

dnl
dnl Some include paths ( !!! DO NOT REMOVE !!! )
dnl
INCLUDES='-I$(top_srcdir) -I$(top_builddir) -I$(top_srcdir)/src -I$(top_builddir)/src -I$(top_srcdir)/src/common -I$(top_builddir)/src/common -I$(top_srcdir)/src/xitk/xine-toolkit -I$(top_builddir)/src/xitk/xine-toolkit -I$(prefix)/include $(INTLDIR)'
AC_SUBST(INCLUDES)

AC_DEFINE(NEED_MRLBROWSER,,[Define this to add mrl browser into xitk lib])

dnl
dnl It seems automake 1.5 don't take care about this script
dnl
if test ! -z "$am_depcomp"; then
	DEPCOMP="depcomp"
fi
AC_SUBST(DEPCOMP)


dnl
dnl Newest automake workaround
dnl
AC_SUBST(mkdir_p)


dnl
dnl Output configuration files
dnl
AC_CONFIG_FILES([
Makefile
m4/Makefile
intl/Makefile
src/Makefile
src/common/Makefile
src/xitk/Makefile
src/xitk/Imlib-light/Makefile
src/xitk/xine-toolkit/Makefile
src/xitk/xine-toolkit/po/Makefile.in
src/xitk/skins/Makefile
src/xitk/skins/xinetic/Makefile
src/aaui/Makefile
src/dfb/Makefile
src/fb/Makefile
doc/Makefile
doc/man/Makefile
doc/man/de/Makefile
doc/man/de/xine.1
doc/man/de/aaxine.1
doc/man/de/xine-check.1
doc/man/en/Makefile
doc/man/en/xine.1
doc/man/en/aaxine.1
doc/man/en/xine-check.1
doc/man/fr/Makefile
doc/man/fr/xine.1
doc/man/es/Makefile
doc/man/es/xine.1
doc/man/pl/Makefile
doc/man/pl/xine.1
doc/man/pl/aaxine.1
misc/Makefile
misc/build_rpms.sh
misc/SlackBuild
misc/xine-ui.spec
misc/relchk.sh
misc/desktops/Makefile
misc/visuals/Makefile
po/Makefile.in
])
AC_CONFIG_COMMANDS([default],[[chmod +x ./misc/SlackBuild ./misc/build_rpms.sh ./misc/relchk.sh]],[[]])
AC_OUTPUT
