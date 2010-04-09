dnl
dnl Configure.in for xine
dnl
dnl
dnl Don't forget to update the misc/xine_splash.{xcf,png}
dnl

dnl Change these three m4_define lines to set the version
dnl Presence of .cvsversion causes an "hg" suffix
m4_define([xine_MAJOR], [0])
m4_define([xine_MINOR], [99])
m4_define([xine_SUB],   [7])

m4_define([xine_PRE], [m4_esyscmd([test -f .cvsversion && echo -n hg || :])])
m4_define([xine_VERSION], [xine_MAJOR.xine_MINOR.m4_expand([xine_SUB])xine_PRE])

XINE_MAJOR=xine_MAJOR
XINE_MINOR=xine_MINOR
XINE_SUBxine_SUB
XINE_PRE=xine_PRE

AC_PREREQ(2.59)
AC_INIT([xine-ui], [xine_VERSION])

dnl evilness

AC_CONFIG_SRCDIR([src/xitk/main.c])
AM_CONFIG_HEADER(config.h)
AC_CONFIG_LIBOBJ_DIR([src/common])

AM_INIT_AUTOMAKE([dist-bzip2 no-dist-gzip])
AM_MAINTAINER_MODE

TAR_NAME=$PACKAGE_NAME-xine_VERSION
SPEC_VERSION=xine_VERSION

AC_SUBST(XINE_MAJOR)
AC_SUBST(XINE_MINOR)
AC_SUBST(XINE_SUB)
AC_SUBST(TAR_NAME)
AC_SUBST(SPEC_VERSION)

dnl
dnl Check for programs.
dnl
dnl Save CFLAGS, AC_ISC_POSIX set some unwanted default CFLAGS
saved_CFLAGS="$CFLAGS"
AC_ISC_POSIX
CFLAGS="$saved_CFLAGS"
AC_PROG_CC
AM_PROG_CC_C_O
AC_MINIX dnl readline
AC_HEADER_STDC([])
AC_PROG_MAKE_SET
AC_PROG_INSTALL
AC_PROG_RANLIB
AC_PROG_LN_S
PKG_PROG_PKG_CONFIG

dnl NLS Support
AM_GNU_GETTEXT(external)
AM_GNU_GETTEXT_VERSION([0.17])
AC_PROG_GMSGFMT_PLURAL

AC_ARG_WITH([iconv],
   AS_HELP_STRING([--without-iconv], [Disable iconv() use for recoding. You can do this if your system is set to use UTF-8.]))

if test "x$with_iconv" != "xno"; then
  AM_ICONV
fi

AM_CONDITIONAL([HAVE_ICONV], [test "$am_cv_func_iconv" = yes])

dnl
dnl Checks for typedefs, structures, and compiler characteristics.
dnl
AC_C_CONST
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
dnl AC_CHECK_TYPES([ptrdiff_t])
dnl AC_C_BIGENDIAN

dnl
dnl Check for xine-lib
dnl
PKG_CHECK_MODULES([XINE], [libxine >= 1.1.0])
XINE_LIB_SHIMS

CFLAGS="${CFLAGS} ${XINE_CFLAGS}"

dnl Check for xine-list-* (MIME types)
XINE_LIST="`pkg-config --variable=xine_list libxine 2>/dev/null || xine-config --xine-list 2>/dev/null`"
AM_CONDITIONAL([XINE_LIST], [test "$XINE_LIST" != ''])
AC_SUBST(XINE_LIST)

dnl
dnl threads: pkgconfig tell us what should be used, but xitk needs to be
dnl          linked to thread lib, so the following AC_SUBST() are only used
dnl          in src/xitk/xine-toolkit/Makefile.am
dnl
case "$host" in
  *-*-freebsd*)
    THREAD_LIBS="-L/usr/local/lib -pthread"
    THREAD_CFLAGS="-I/usr/local/include -D_THREAD_SAFE"
    CFLAGS="-L/usr/local/lib $THREAD_CFLAGS $CFLAGS"
    CPPFLAGS="-I/usr/local/include -L/usr/local/lib $CPPFLAGS"
    ;;

  *-*-openbsd*)
    THREAD_LIBS="-pthread"
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
dnl Checks for X11
dnl
if test "x$with_x" != "xno"; then
   PKG_CHECK_MODULES([X11], [x11], , [AC_PATH_XTRA])
else
   no_x="yes"
fi

if test x"$no_x" != x"yes"; then
    AC_DEFINE(HAVE_X11,,[Define this if you have X11R6 installed])
fi
AM_CONDITIONAL(HAVE_X11, [test x"$no_x" != "xyes"])

PKG_CHECK_MODULES([XV], [xv], [
  XV_LIB=$XV_LIBS
  ac_have_xv="yes"
  AC_DEFINE(HAVE_XV, 1, [Define this if you have libXv installed])
], [  
  AC_CHECK_LIB(Xv, XvQueryExtension, 
            [ XV_LIB="-lXv"
	      AC_DEFINE(HAVE_XV, 1, [Define this if you have libXv installed])
            ],, [$X_LIBS $X_PRE_LIBS -lXext $X_EXTRA_LIBS])
])

AC_SUBST(XV_LIB)
AM_CONDITIONAL(HAVE_XV, test x$ac_have_xv = "xyes")


dnl
dnl Shm
dnl
AC_ARG_ENABLE([shm],
   AS_HELP_STRING([--disable-shm], [Disable shared memory support in build.]))
AC_ARG_ENABLE([shm-default],
   AS_HELP_STRING([--disable-shm-default], [Disable shared memory support by default.]))

if test "x$enable_shm_default" = "xno"; then
  AC_DEFINE(DISABLE_SHM_DEFAULT,,[Disable shared memory by default])
fi

dnl If we are compiling without shared memory support, then don\'t do
dnl the checks for XShm
if test "x$enable_shm" != "xno"; then
  dnl Check for the Xext library (needed for XShm extension)
  AC_CHECK_LIB(Xext, XShmAttach, 
      X_LIBS="-lXext $X_LIBS", 
      dnl On AIX, it is in XextSam instead, but we still need -lXext
      AC_CHECK_LIB(XextSam, XShmAttach, 
          [X_LIBS="-lXextSam -lXext $X_LIBS"], 
          no_xshm_ext=yes; have_shm=no, $X_LIBS),
      $X_LIBS)

  dnl Test for shared memory headers and semantics, unless we are
  dnl configured to build without shared memory extensions.
  AC_CHECK_HEADERS([sys/ipc.h sys/shm.h], [have_shm=no])

  if test "x$have_shm" != "xno" ; then
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
  if test "x$no_xext_lib" != "xyes"; then
    saved_CPPFLAGS=$CPPFLAGS
    CPPFLAGS="$CPPFLAGS $X_CFLAGS"
    AC_CHECK_HEADERS([X11/extensions/XShm.h], , , [#include <X11/Xlib.h>])
    CPPFLAGS=$saved_CPPFLAGS
  fi
fi


dnl
dnl Checks for Xinerama extension
dnl
AC_ARG_ENABLE([xinerama],
   AS_HELP_STRING([--disable-xinerama], [Don't build support for xinerama (default: check)]))
if test "x$enable_xinerama" != "xno"; then
  PKG_CHECK_MODULES([XINERAMA], [xinerama], [ac_have_xinerama="yes"], [
    AC_CHECK_LIB(Xinerama, XineramaQueryExtension, 
               XINERAMA_LIBS="-lXinerama"
               ac_have_xinerama="yes",,
               $X_LIBS $X_PRE_LIBS -lXext $X_EXTRA_LIBS)
  ])
  
  if test "x$ac_have_xinerama" = "xyes"; then
     X_PRE_LIBS="$X_PRE_LIBS $XINERAMA_LIBS"
     AC_DEFINE(HAVE_XINERAMA,,[Define this if you have libXinerama installed])
  fi
fi
AM_CONDITIONAL(HAVE_XINERAMA, test x$ac_have_xinerama = "xyes")


dnl
dnl Checks for XF86VidMode extension
dnl
PKG_CHECK_MODULES([XXF86VM], [xxf86vm], [ac_have_xf86vidmode="yes"], [
  AC_CHECK_LIB(Xxf86vm, XF86VidModeQueryExtension, 
             XXF86VM_LIBS="-lXxf86vm"
             ac_have_xf86vidmode="yes",,
             $X_LIBS $X_PRE_LIBS -lXext $X_EXTRA_LIBS)
])

if test "x$ac_have_xf86vidmode" = "xyes"; then
   X_PRE_LIBS="$X_PRE_LIBS ${XXF86VM_LIBS}"
   AC_DEFINE(HAVE_XF86VIDMODE,,[Define this if you have libXxf86vm installed])
fi

AM_CONDITIONAL(HAVE_XF86VIDMODE, test x$ac_have_xf86vidmode = "xyes")


dnl
dnl XTest Extension
dnl
PKG_CHECK_MODULES([XTEST], [xtst], [ac_have_xtest="yes"], [
  AC_CHECK_LIB(Xtst, XTestFakeKeyEvent, 
             XTEST_LIBS="-lXtst"
	     ac_have_xtest="yes",, $X_LIBS $X_PRE_LIBS -lXext $X_EXTRA_LIBS)
])

if test "x$ac_have_xtest" = "xyes"; then
   AC_DEFINE(HAVE_XTESTEXTENSION,,[Define this if you have libXtst installed])
fi

AC_SUBST(XTEST_LIBS)
AM_CONDITIONAL(HAVE_XTESTEXTENSION, test x$ac_have_xtest = "xyes")

dnl
dnl XScreenSaver Extension
dnl
PKG_CHECK_MODULES([XSSAVER], [xscrnsaver], [ac_have_xscreensaver="yes"], [
  AC_CHECK_LIB(Xss, XScreenSaverQueryExtension,
		XSSAVER_LIBS="-lXss"
		ac_have_xscreensaver="yes",,  $X_LIBS $X_PRE_LIBS -lXext $X_EXTRA_LIBS)
])

if test "x$ac_have_xscreensaver" = "xyes"; then
  AC_DEFINE(HAVE_XSSAVEREXTENSION,,[Define this if you have libXss installed])
fi

AC_SUBST(XSSAVER_LIBS)
AM_CONDITIONAL(HAVE_XSSAVEREXTENSION, test x$ac_have_xscreensaver = "xyes")


dnl
dnl readline
dnl
if test x"$with_readline" != xno; then
  AC_CHECK_READLINE
fi

AM_CONDITIONAL(HAVE_READLINE, test x"$have_readline" = "xyes")
if test x"$have_readline" != "xyes"; then
  AC_MSG_RESULT([*** no readline found, xine network interface will be disabled ***])
fi

dnl
dnl Libcurl
dnl
AC_ARG_WITH([curl],
   AS_HELP_STRING([--without-curl], [Turn off CURL support.]))

if test x"$with_curl" != xno; then
   PKG_CHECK_MODULES([CURL], [libcurl >= 7.10.2], have_curl="yes", have_curl="no")
   
   if test "x$have_curl" = "xyes"; then
      AC_DEFINE([HAVE_CURL], [1], [Define this if you have libcurl installed])
   else
      AC_MSG_RESULT([*** All of curl dependent parts will be disabled ***])
   fi
fi
AM_CONDITIONAL(HAVE_CURL, test x$have_curl = "xyes")


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
AC_ARG_WITH([aalib],
   AS_HELP_STRING([--without-aalib], [Turn off AALIB support.]))

if test x"$with_aalib" != "xno"; then
  AM_PATH_AALIB(1.2.0,,)
  AM_DL()
else
  no_aalib=yes
fi
AM_CONDITIONAL(HAVE_AA, test x$no_aalib != "xyes")

dnl 
dnl checks for Color AsCii Art library
dnl 
AC_ARG_WITH([caca],
  AS_HELP_STRING([--without-caca], [Do not build CACA support]))

if test "x$with_caca" != "xno"; then
   PKG_CHECK_MODULES([CACA], [caca cucul], [have_caca="yes"], [have_caca="no"])
   AM_DL()
   if test "x$with_caca" = "xyes" && test "x$have_caca" = "xno"; then
      AC_MSG_ERROR([CACA support requested, but libcaca 0.99 not found])
   fi
   AC_COMPILE_IFELSE( [AC_LANG_PROGRAM([[#include <caca.h> 
                                        caca_event_t ev;]], 
                                        [[ev.type = 0;]] 
                       )],
                       [],
                       [AC_DEFINE(CACA_EVENT_OPAQUE, [1], ["Define if struct caca_event is opaque and must not be accessed directly"])]
   )
fi

AM_CONDITIONAL([HAVE_CACA], [test "x$have_caca" = "xyes"])

dnl option for font sets and multibyte string support
AC_ARG_ENABLE([mbs],
   AS_HELP_STRING([--disable-mbs], [Disable multibyte string support]))
if test "x$enable_mbs" != "xno"; then
  AC_DEFINE(WITH_XMB,,[Define this if you want support multibyte strings in gui])
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

AC_ARG_WITH([fb], AS_HELP_STRING([--without-fb], [Turn off framebuffer support]))
AM_CONDITIONAL([HAVE_FB], [test "x$with_fb" != xno])

dnl
dnl Check for DirectFB
dnl

dnl NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE
dnl  DFB:
dnl  Temporary disabled, port to new API needed
dnl NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE
  if test "x" != "x"; then

    DIRECTFB_REQUIRED_VERSION=0.9.10
    PKG_CHECK_MODULES([DIRECTFB], [directfb >= $DIRECTFB_REQUIRED_VERSION],
       [have_directfb=yes], [have_directfb=no])

    if test "x$have_directfb" != "xyes"; then
       AC_MSG_RESULT([*** All of DirectFB dependent parts will be disabled ***])
    fi
  fi

AC_SUBST(DIRECTFB_CFLAGS)
AC_SUBST(DIRECTFB_LIBS)
AM_CONDITIONAL(HAVE_DIRECTFB, test x$have_directfb = "xyes" )

dnl
dnl check for libpng
dnl
PKG_CHECK_MODULES([LIBPNG], [libpng >= 1.2.8],,
  [PKG_CHECK_MODULES([LIBPNG], [libpng12 >= 1.2.8])])

dnl
dnl check for libjpeg
dnl
AC_ARG_VAR([JPEG_CFLAGS], [C compiler flags for libjpeg])
AC_ARG_VAR([JPEG_LIBS], [Linker flags for libjpeg])
JPEG_LIBS="${JPEG_LIBS:--ljpeg}"
saved_CFLAGS="$CFLAGS"
saved_LIBS="$LIBS"
CFLAGS="$CFLAGS $JPEG_CFLAGS"
LIBS="$LIBS $JPEG_LIBS"
AC_CHECK_LIB([jpeg], [jpeg_read_header],
  [AC_CHECK_HEADER([jpeglib.h], [], [AC_MSG_ERROR([JPEG library is required])])
  ], [$JPEG_LIBS])
CFLAGS="$saved_CFLAGS"
LIBS="$saved_LIBS"

dnl
dnl Check for LIRC client support
dnl
AC_CHECK_LIRC


dnl
dnl user selectable support for special VDR interaction keys
dnl
AC_ARG_ENABLE([vdr-keys],
   AS_HELP_STRING([--enable-vdr-keys], [Support special keys for VDR interaction.]))
if test x"$enable_vdr_keys" = xyes ; then
  AC_DEFINE(ENABLE_VDR_KEYS,, [Define this if you want support for VDR keys.])
fi


dnl
dnl Check for nvtvsimple library
dnl
AC_ARG_ENABLE([nvtvsimple],
   AS_HELP_STRING([--disable-nvtvsimple], [Disable support for nvtvsimple]))

if test "x$enable_nvtvsimple" != "xno"; then
   PKG_CHECK_MODULES([NVTVSIMPLE], [nvtvsimple >= 0.4.6],
      [nvtvsimple="yes"], [nvtvsimple="no"])
fi

if test "x$nvtvsimple" = "xyes"; then
   AC_DEFINE([HAVE_NVTVSIMPLE], [1], [Define this if you have nvtvsimple installed])
else
   AC_MSG_RESULT([*** nvtvsimple support will be disabled ***])
fi

dnl
dnl xine-ui use tar, check it
dnl
AC_ARG_WITH([tar],
   AS_HELP_STRING([--without-tar], [Don't use the tar command from within xine-ui (disable skin download)]))

if test "x$with_tar" != "xno"; then
   AC_CHECK_PROGS(TAR, [tar bsdtar star])
fi

if test x"$TAR" = "x"; then
    AC_MSG_RESULT([*** tar-dependent part will be disabled (skin download) ***])
else
    AC_DEFINE(HAVE_TAR,,[Define this if you have tar installed])
fi
AM_CONDITIONAL([HAVE_TAR], [test x"$TAR" != "x"])


dnl
dnl
dnl
BUILD_CC="`$CC -v 2>&1 | tail -n 1`"
BUILD_OS="`uname -s -r -m`"
BUILD_DATE="`date "+%a %d %b %Y %T"`"
AC_SUBST(BUILD_CC)
AC_SUBST(BUILD_OS)
AC_SUBST(BUILD_DATE)

if test "$GCC" = yes; then
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
AC_TRY_CFLAGS("-fno-strict-aliasing", NO_STRICT_ALIASING="-fno-strict-aliasing")
AC_SUBST(NO_STRICT_ALIASING)

CFLAGS="$wall -D_FILE_OFFSET_BITS=64 $CFLAGS $ALSA_CFLAGS $ESD_CFLAGS"
AC_SUBST(CFLAGS)

AC_ARG_ENABLE([debug],
   AS_HELP_STRING([--enable-debug], [Compile with debug information]))

if test "x$enable_debug" = "xyes"; then
   CFLAGS="$CFLAGS -DDEBUG -g"
else
   CFLAGS="$CFLAGS -DNDEBUG"
fi

dnl gcc __attribute__ ((aligned ()))
AC_C_ATTRIBUTE_ALIGNED

dnl
dnl system function checks (defined _GNU_SOURCE)
dnl
AC_DEFINE([_GNU_SOURCE], [], [enable GNU libc extension])
AC_CHECK_LIB(posix4, nanosleep)
AC_CHECK_FUNCS([nanosleep])
AC_REPLACE_FUNCS([getline getsubopt strlcat strlcpy strndup strsep])
AC_CHECK_HEADERS(string.h strings.h linux/kd.hb execinfo.h)


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
dnl desktop dir location, pkgconfig told us about this.
dnl
makeexpand () {
  local i
  local j
  i="$1"
  while test "$i" != "$j"; do j="$i"; eval i="$j"; done
  echo "$i"
}

if test "x$prefix" = xNONE; then
   prefix="${ac_default_prefix}"
fi
if test "x$exec_prefix" = xNONE; then
   exec_prefix='${prefix}'
fi

dnl
dnl NLS, part II
dnl
XITK_LOCALEPATH="`makeexpand ${datadir}/locale`"
AC_DEFINE_UNQUOTED(XITK_LOCALE, "$XITK_LOCALEPATH", [Location of Xine catalog files])

if test "x$XINE_DOCPATH" = "x"; then
   XINE_DOCPATH="${datadir}/doc/xine-ui"
fi
XINE_DOCDIR="`makeexpand $XINE_DOCPATH`"
AC_SUBST(XINE_DOCDIR)
AC_DEFINE_UNQUOTED(XINE_DOCDIR, "$XINE_DOCDIR",[Location of documentation files])

dnl
dnl skins dir location, pkgconfig told us about this.
dnl
XINE_SKINDIR="${datadir}/xine/skins"
XINE_SKINPATH="`makeexpand ${datadir}/xine/skins`"
AC_SUBST(XINE_SKINDIR)
AC_SUBST(XINE_SKINPATH)
AC_DEFINE_UNQUOTED(XINE_SKINDIR, "$XINE_SKINPATH",[Location of xine skin directory])

dnl
dnl Use whichever splash & logo happen to be present.
dnl JPEG is preferred, then PNG then, for the logo, MPV.
dnl
SOURCE_SPLASH="${SOURCE_SPLASH:-`cat misc/splash`}"
AC_SUBST([SOURCE_SPLASH])

AC_DEFUN([XINE_LOCATE_MEDIA],
 [AC_MSG_CHECKING([for a $5 image])
  $1=''
  for i in $2; do
    ac_cv_xine_media_file="misc/splash-${SOURCE_SPLASH}/$4.$i"
    if test -f "$ac_cv_xine_media_file"; then
      $1="$XINE_SKINDIR/$4.$i"
      break
    fi
  done
  if test "$[]$1" = ''; then
    $1="$XINE_SKINDIR/$4.$3"
    ac_cv_xine_media_file="misc/splash-default/$4.$3"
    if ! test -f "$ac_cv_xine_media_file"; then
      AC_MSG_RESULT([no])
      AC_MSG_ERROR([Missing $5 image - need $4.* (any of $2)])
    fi
  fi
  AC_MSG_RESULT([$ac_cv_xine_media_file])
  AC_SUBST([$1], [$ac_cv_xine_media_file])
  AC_DEFINE_UNQUOTED([$1], "`makeexpand $[]$1`", [$6])
])

XINE_LOCATE_MEDIA([XINE_LOGO_MRL], [jpg png mpv], [mpv], [xine-ui_logo], [logo], [official logo MRL])
XINE_LOCATE_MEDIA([XINE_SPLASH], [jpg png], [png], [xine_splash], [splash], [splash image filename])

XINE_VISDIR="${datadir}/xine/visuals"
AC_SUBST(XINE_VISDIR)
AC_DEFINE_UNQUOTED(XINE_VISDIR, "`makeexpand $XINE_VISDIR`",[Location of xine visual animation streams])

XINE_OXINEDIR="${datadir}/xine/oxine"
AC_SUBST(XINE_OXINEDIR)
AC_DEFINE_UNQUOTED(XINE_OXINEDIR, "`makeexpand $XINE_OXINEDIR`",[Location of osd menu (oxine) config files])

DATA_DIR=`makeexpand "$datadir"`
AC_SUBST(DATA_DIR)
SYSCONF_DIR=`makeexpand "$sysconfdir"`
AC_SUBST(SYSCONF_DIR)

dnl
dnl Some include paths ( !!! DO NOT REMOVE !!! )
dnl
INCLUDES='-I$(top_srcdir) -I$(top_builddir) -I$(top_srcdir)/src -I$(top_builddir)/src -I$(top_srcdir)/src/common -I$(top_builddir)/src/common -I$(top_srcdir)/src/xitk/xine-toolkit -I$(top_builddir)/src/xitk/xine-toolkit -I$(prefix)/include $(INTLDIR)'
AC_SUBST(INCLUDES)

AC_DEFINE(NEED_MRLBROWSER,,[Define this to add mrl browser into xitk lib])

dnl Important warnings we _don't_ want to skip
dnl Don't put these under conditional for optimisations, because these
dnl need always to be enabled.
AC_TRY_CFLAGS("-Wformat", wformat="-Wformat")
AC_TRY_CFLAGS("-Wformat=2", wformat="-Wformat=2")
if test "x$wformat" != "x"; then
   AC_TRY_CFLAGS("-Wno-format-zero-length", wformat="$wformat -Wno-format-zero-length")
fi
AC_TRY_CFLAGS("-Wmissing-format-attribute", wformat="$wformat -Wmissing-format-attribute")
CFLAGS="$CFLAGS $wformat"

wbits=''
AC_TRY_CFLAGS("-Wmissing-noreturn", wbits="$wbits -Wmissing-noreturn")
AC_TRY_CFLAGS("-Wdisabled-optimization", wbits="$wbits -Wdisabled-optimization")
AC_TRY_CFLAGS("-Werror-implicit-function-declaration", wbits="$wbits -Werror-implicit-function-declaration")
CFLAGS="$CFLAGS $wbits"

AC_TRY_CFLAGS("-Wstrict-aliasing", wsa="-Wstrict-aliasing")
AC_TRY_CFLAGS("-Wstrict-aliasing=2", wsa="-Wstrict-aliasing=2")
CFLAGS="$CFLAGS $wsa"

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
if test -n "$ac_aux_dir"; then
  case "$ac_aux_dir" in
    /*) MKINSTALLDIRS="$ac_aux_dir/install-sh -d" ;;
    *) MKINSTALLDIRS="\$(top_builddir)/$ac_aux_dir/install-sh -d" ;;
  esac
else
  MKINSTALLDIRS="\$(top_srcdir)/install-sh -d"
fi
AC_SUBST(MKINSTALLDIRS)

dnl
dnl Output configuration files
dnl
AC_CONFIG_FILES([
Makefile
m4/Makefile
src/Makefile
src/common/Makefile
src/xitk/Makefile
src/xitk/Imlib-light/Makefile
src/xitk/xine-toolkit/Makefile
src/xitk/xine-toolkit/po/Makefile.in
src/xitk/skins/Makefile
src/xitk/skins/xinetic/Makefile
src/xitk/oxine/Makefile
src/xitk/oxine/data/Makefile
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
doc/man/es/aaxine.1
doc/man/es/xine-check.1
doc/man/pl/Makefile
doc/man/pl/xine.1
doc/man/pl/aaxine.1
misc/Makefile
misc/build_rpms.sh
misc/SlackBuild
misc/xine-ui.spec
misc/desktops/Makefile
misc/visuals/Makefile
po/Makefile.in
])
AC_CONFIG_COMMANDS([default],[[chmod +x ./misc/SlackBuild ./misc/build_rpms.sh]],[[]])
AC_OUTPUT
