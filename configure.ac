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

XINE_MAJOR=0
XINE_MINOR=9
XINE_SUB=15
if test -f .cvsversion; then
  XINE_PRE="cvs"
else
  XINE_PRE=""
fi


#TAR_NAME="xine-ui-"$XINE_MAJOR.$XINE_MINOR.$XINE_SUB$XINE_PRE
#SPEC_VERSION=$XINE_MAJOR.$XINE_MINOR.$XINE_SUB$XINE_PRE
TAR_NAME="xine-ui-"$XINE_MAJOR$XINE_PRE
SPEC_VERSION=$XINE_MAJOR$XINE_PRE
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
dnl Check for AIX weirdos
AC_AIX
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
ALL_LINGUAS="fr es pt_BR de sk pl_PL uk cs"
AM_GNU_GETTEXT(use-libtool)
dnl ac_sources="$nls_cv_header_libgt"
dnl ac_dests="$nls_cv_header_intl"
dnl while test -n "$ac_sources"; do
dnl   set $ac_dests; ac_dest=$1; shift; ac_dests=$*
dnl   set $ac_sources; ac_source=$1; shift; ac_sources=$*
dnl   ac_config_links_1="$ac_config_links_1 $ac_dest:$ac_source"
dnl done
dnl AC_CONFIG_LINKS([$ac_config_links_1])
AC_CONFIG_LINKS($nls_cv_header_libgt, $nls_cv_header_intl)
AM_CONDITIONAL(INCLUDE_INTL, test x$USE_INCLUDED_LIBINTL = "xyes")
if test x"$USE_INCLUDED_LIBINTL" = x"yes"; then
   INTLDIR="-I\$(top_builddir)/intl -I\$(top_srcdir)/intl"
fi


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


dnl
dnl threads: xine-config tell us what should be used, but
dnl          xitk need to be linked to thread lib, so the follow AC_SUBST() 
dnl          are only used in src/xitk/xine-toolkit/Makefile.am
dnl
case "$host" in
  *-*-freebsd*)
    THREAD_LIBS="-L/usr/local/lib -pthread"
    THREAD_CFLAGS="-I/usr/local/include -D_THREAD_SAFE"
    CFLAGS="$CFLAGS -L/usr/local/lib $THREAD_CFLAGS"
    CPPFLAGS="$CPPFLAGS -I/usr/local/include -L/usr/local/lib"
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


dnl
dnl Shm
dnl
AC_ARG_ENABLE(shm, [  --disable-shm           Disable shared memory support if available.], echo $enable_shm, enable_shm="yes")

dnl If we are compiling without shared memory support, then don\'t do
dnl the checks for XShm
if test "x$enable_shm" = "xyes"; then
  # Check for the Xext library (needed for XShm extension)
  AC_CHECK_LIB(Xext, XShmAttach, 
      X_LIBS="-lXext $X_LIBS", 
      # On AIX, it is in XextSam instead, but we still need -lXext
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

  # Check whether shmctl IPC_RMID allowes subsequent attaches
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

  # Check for the X shared memory extension header file
  AC_MSG_CHECKING(X11/extensions/XShm.h)
  if test "x$no_xext_lib" = "xyes"; then
    # If we couldn't find XShmAttach earlier, don't use the header
    AC_MSG_RESULT(no)
    no_xshm=yes
  else
    if test -f "$x_includes/X11/extensions/XShm.h"; then
      AC_MSG_RESULT(yes)
      AC_DEFINE(HAVE_XSHM_H,,[Define if includes XShm.h])
    else
      AC_MSG_RESULT(no)
      no_xshm=yes
    fi
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
dnl configure defaults
LIBVERSION=4.2a
opt_curses=no
opt_shared=no

dnl arguments to configure
AC_ARG_WITH(curses, 
  [  --with-curses           use the curses library instead of the termcap library],
  opt_curses=$withval)

if test "$opt_curses" = "yes"; then
	prefer_curses=yes
fi
AC_TYPE_SIGNAL([])

AC_HEADER_STAT
AC_HEADER_DIRENT
AC_CHECK_FUNCS(lstat memmove putenv select setenv setlocale \
	strcasecmp strpbrk tcgetattr vsnprintf isascii isxdigit getpwuid_r)

AC_FUNC_STRCOLL

AC_CHECK_HEADERS(unistd.h stdlib.h varargs.h stdarg.h string.h strings.h \
		limits.h sys/ptem.h sys/pte.h sys/stream.h sys/select.h \
		termcap.h termios.h termio.h sys/file.h locale.h memory.h)

dnl BASH_SYS_SIGNAL_VINTAGE dnl pure 2.50
dnl BASH_SYS_REINSTALL_SIGHANDLERS dnl pure 2.50
BASH_SIGNAL_CHECK
BASH_REINSTALL_SIGHANDLERS

BASH_FUNC_POSIX_SETJMP
BASH_FUNC_LSTAT
BASH_CHECK_GETPW_FUNCS
BASH_FUNC_STRCOLL

BASH_TYPE_SIGHANDLER
BASH_HAVE_TIOCGWINSZ
BASH_HAVE_TIOCSTAT
BASH_HAVE_FIONREAD
BASH_MISC_SPEED_T
BASH_STRUCT_WINSIZE
BASH_STRUCT_DIRENT_D_INO
BASH_STRUCT_DIRENT_D_FILENO
dnl yuck
case "$host_os" in
aix*)   prefer_curses=yes ;;
esac

BASH_CHECK_LIB_TERMCAP([],
                       AC_MSG_RESULT([*** All of READLINE dependent parts will be disabled ***]))

case "$host_cpu" in
*cray*)	READLINE_LOCAL_CFLAGS=-DCRAY ;;
*s390*) READLINE_LOCAL_CFLAGS=-fsigned-char ;;
esac

case "$host_os" in
isc*)	READLINE_LOCAL_CFLAGS=-Disc386 ;;
esac

AC_SUBST(READLINE_LOCAL_CFLAGS)
AC_SUBST(TERMCAP_LIB)
AC_SUBST(LIBVERSION)
AM_CONDITIONAL(HAVE_TERMCAP, test x"$no_termcap" != "xyes")

dnl
dnl ################################### libcurl ###########################################
dnl
original_LIBS=$LIBS

AM_CONFIG_HEADER(libcurl/ca-bundle.h)

dnl figure out the libcurl version

AC_DEFINE_UNQUOTED(OS, "${host}", [cpu-machine-OS])

dnl check for how to do large files
AC_SYS_LARGEFILE

case $host in
  *-*-cygwin | *-*-mingw* | *-*-pw32*)
    need_no_undefined=yes
    ;;
  *)
    need_no_undefined=no
    ;;
esac

AM_CONDITIONAL(NO_UNDEFINED, test x$need_no_undefined = xyes)

dnl ************************************************************
dnl switch off particular protocols
dnl
AC_MSG_CHECKING([whether to support http])
AC_ARG_ENABLE(http,
[  --enable-http		Enable HTTP support
  --disable-http	Disable HTTP support],
[ case "$enableval" in
  no)
       AC_MSG_RESULT(no)
       AC_DEFINE(CURL_DISABLE_HTTP, 1, [to disable HTTP])
       AC_MSG_WARN([disable HTTP disables FTP over proxy and GOPHER too])
       AC_DEFINE(CURL_DISABLE_GOPHER, 1, [to disable GOPHER])
       AC_SUBST(CURL_DISABLE_HTTP)
       AC_SUBST(CURL_DISABLE_GOPHER)
       ;;
  *)   AC_MSG_RESULT(yes)
       ;;
  esac ],
       AC_MSG_RESULT(yes)
)
AC_MSG_CHECKING([whether to support ftp])
AC_ARG_ENABLE(ftp,
[  --enable-ftp		Enable FTP support
  --disable-ftp	Disable FTP support],
[ case "$enableval" in
  no)
       AC_MSG_RESULT(no)
       AC_DEFINE(CURL_DISABLE_FTP, 1, [to disable FTP])
       AC_SUBST(CURL_DISABLE_FTP)
       ;;
  *)   AC_MSG_RESULT(yes)
       ;;
  esac ],
       AC_MSG_RESULT(yes)
)
AC_MSG_CHECKING([whether to support gopher])
AC_ARG_ENABLE(gopher,
[  --enable-gopher		Enable GOPHER support
  --disable-gopher	Disable GOPHER support],
[ case "$enableval" in
  no)
       AC_MSG_RESULT(no)
       AC_DEFINE(CURL_DISABLE_GOPHER, 1, [to disable GOPHER])
       AC_SUBST(CURL_DISABLE_GOPHER)
       ;;
  *)   AC_MSG_RESULT(yes)
       ;;
  esac ],
       AC_MSG_RESULT(yes)
)
AC_MSG_CHECKING([whether to support file])
AC_ARG_ENABLE(file,
[  --enable-file		Enable FILE support
  --disable-file	Disable FILE support],
[ case "$enableval" in
  no)
       AC_MSG_RESULT(no)
       AC_DEFINE(CURL_DISABLE_FILE, 1, [to disable FILE])
       AC_SUBST(CURL_DISABLE_FILE)
       ;;
  *)   AC_MSG_RESULT(yes)
       ;;
  esac ],
       AC_MSG_RESULT(yes)
)
AC_MSG_CHECKING([whether to support ldap])
AC_ARG_ENABLE(ldap,
[  --enable-ldap		Enable LDAP support
  --disable-ldap	Disable LDAP support],
[ case "$enableval" in
  no)
       AC_MSG_RESULT(no)
       AC_DEFINE(CURL_DISABLE_LDAP, 1, [to disable LDAP])
       AC_SUBST(CURL_DISABLE_LDAP)
       ;;
  *)   AC_MSG_RESULT(yes)
       ;;
  esac ],
       AC_MSG_RESULT(yes)
)
AC_MSG_CHECKING([whether to support dict])
AC_ARG_ENABLE(dict,
[  --enable-dict		Enable DICT support
  --disable-dict	Disable DICT support],
[ case "$enableval" in
  no)
       AC_MSG_RESULT(no)
       AC_DEFINE(CURL_DISABLE_DICT, 1, [to disable DICT])
       AC_SUBST(CURL_DISABLE_DICT)
       ;;
  *)   AC_MSG_RESULT(yes)
       ;;
  esac ],
       AC_MSG_RESULT(yes)
)
AC_MSG_CHECKING([whether to support telnet])
AC_ARG_ENABLE(telnet,
[  --enable-telnet		Enable TELNET support
  --disable-telnet	Disable TELNET support],
[ case "$enableval" in
  no)
       AC_MSG_RESULT(no)
       AC_DEFINE(CURL_DISABLE_TELNET, 1, [to disable TELNET])
       AC_SUBST(CURL_DISABLE_TELNET)
       ;;
  *)   AC_MSG_RESULT(yes)
       ;;
  esac ],
       AC_MSG_RESULT(yes)
)


dnl **********************************************************************
dnl Checks for IPv6
dnl **********************************************************************

AC_MSG_CHECKING([whether to enable ipv6])
AC_ARG_ENABLE(ipv6,
[  --enable-ipv6		Enable ipv6 (with ipv4) support
  --disable-ipv6	Disable ipv6 support],
[ case "$enableval" in
  no)
       AC_MSG_RESULT(no)
       ipv6=no
       ;;
  *)   AC_MSG_RESULT(yes)
       ipv6=yes
       ;;
  esac ],

  AC_TRY_RUN([ /* is AF_INET6 available? */
#include <sys/types.h>
#include <sys/socket.h>
main()
{
 if (socket(AF_INET6, SOCK_STREAM, 0) < 0)
   exit(1);
 else
   exit(0);
}
],
  AC_MSG_RESULT(yes)
  ipv6=yes,
  AC_MSG_RESULT(no)
  ipv6=no,
  AC_MSG_RESULT(no)
  ipv6=no
))

if test "$ipv6" = "yes"; then
  CURL_CHECK_WORKING_GETADDRINFO
fi


dnl **********************************************************************
dnl Checks for libraries.
dnl **********************************************************************

dnl gethostbyname in the nsl lib?
AC_CHECK_FUNC(gethostbyname, , [ AC_CHECK_LIB(nsl, gethostbyname) ])

if test "$ac_cv_lib_nsl_gethostbyname" != "yes" -a "$ac_cv_func_gethostbyname" != "yes"; then
  dnl gethostbyname in the socket lib?
  AC_CHECK_FUNC(gethostbyname, , [ AC_CHECK_LIB(socket, gethostbyname) ])
fi

dnl At least one system has been identified to require BOTH nsl and
dnl socket libs to link properly.
if test "$ac_cv_lib_nsl_gethostbyname" = "$ac_cv_func_gethostbyname"; then
  AC_MSG_CHECKING([trying both nsl and socket libs])
  my_ac_save_LIBS=$LIBS
  LIBS="-lnsl -lsocket $LIBS"
  AC_TRY_LINK( ,
             [gethostbyname();],
             my_ac_link_result=success,
             my_ac_link_result=failure )

  if test "$my_ac_link_result" = "failure"; then
    AC_MSG_RESULT([no])
    AC_MSG_ERROR([couldn't find libraries for gethostbyname()])
    dnl restore LIBS
    LIBS=$my_ac_save_LIBS
  else
    AC_MSG_RESULT([yes])
  fi
fi

dnl resolve lib?
AC_CHECK_FUNC(strcasecmp, , [ AC_CHECK_LIB(resolve, strcasecmp) ])

if test "$ac_cv_lib_resolve_strcasecmp" = "$ac_cv_func_strcasecmp"; then
  AC_CHECK_LIB(resolve, strcasecmp,
              [LIBS="-lresolve $LIBS"],
               ,
               -lnsl)
fi

dnl socket lib?
AC_CHECK_FUNC(connect, , [ AC_CHECK_LIB(socket, connect) ])

dnl dl lib?
AC_CHECK_FUNC(dlclose, , [ AC_CHECK_LIB(dl, dlopen) ])

dnl **********************************************************************
dnl Check how non-blocking sockets are set
dnl **********************************************************************
AC_ARG_ENABLE(nonblocking,
[  --enable-nonblocking  Makes the script detect how to do it
  --disable-nonblocking Makes the script disable non-blocking sockets],
[
  if test "$enableval" = "no" ; then
    AC_MSG_WARN([non-blocking sockets disabled])
    AC_DEFINE(HAVE_DISABLED_NONBLOCKING, 1,
    [to disable NON-BLOCKING connections])
  else
    CURL_CHECK_NONBLOCKING_SOCKET
  fi
],
[
  CURL_CHECK_NONBLOCKING_SOCKET
])

dnl **********************************************************************
dnl Check for the random seed preferences 
dnl **********************************************************************

AC_ARG_WITH(egd-socket,
    [  --with-egd-socket=FILE  Entropy Gathering Daemon socket pathname],
    [ EGD_SOCKET="$withval" ]
)
if test -n "$EGD_SOCKET" ; then
	AC_DEFINE_UNQUOTED(EGD_SOCKET, "$EGD_SOCKET",
        [your Entropy Gathering Daemon socket pathname] )
fi

dnl Check for user-specified random device
AC_ARG_WITH(random,
    [  --with-random=FILE      read randomness from FILE (default=/dev/urandom)],
    [ RANDOM_FILE="$withval" ],
    [
        dnl Check for random device
        AC_CHECK_FILE("/dev/urandom", [ RANDOM_FILE="/dev/urandom"] )
    ]
)
if test -n "$RANDOM_FILE" ; then
	AC_SUBST(RANDOM_FILE)
	AC_DEFINE_UNQUOTED(RANDOM_FILE, "$RANDOM_FILE",
        [a suitable file to read random data from])
fi

dnl **********************************************************************
dnl Check for the presence of Kerberos4 libraries and headers
dnl **********************************************************************

AC_ARG_WITH(krb4-includes,
 [  --with-krb4-includes[=DIR]   Specify location of kerberos4 headers],[
 CPPFLAGS="$CPPFLAGS -I$withval"
 KRB4INC="$withval"
 want_krb4=yes
 ])

AC_ARG_WITH(krb4-libs,
 [  --with-krb4-libs[=DIR]   Specify location of kerberos4 libs],[
 LDFLAGS="$LDFLAGS -L$withval"
 KRB4LIB="$withval"
 want_krb4=yes
 ])


OPT_KRB4=off
AC_ARG_WITH(krb4,dnl
[  --with-krb4[=DIR]       where to look for Kerberos4],[
  OPT_KRB4="$withval"
  if test X"$OPT_KRB4" != Xyes
  then
    LDFLAGS="$LDFLAGS -L$OPT_KRB4/lib"
    KRB4LIB="$OPT_KRB4/lib"
    CPPFLAGS="$CPPFLAGS -I$OPT_KRB4/include"
    KRB4INC="$OPT_KRB4/include"
  fi
  want_krb4="yes"
 ])

AC_MSG_CHECKING([if Kerberos4 support is requested])

if test "$want_krb4" = yes
then
  if test "$ipv6" = "yes"; then
    echo krb4 is not compatible with IPv6
    exit 1
  fi
  AC_MSG_RESULT(yes)

  dnl Check for & handle argument to --with-krb4

  AC_MSG_CHECKING(where to look for Kerberos4)
  if test X"$OPT_KRB4" = Xyes
  then
    AC_MSG_RESULT([defaults])
  else
    AC_MSG_RESULT([libs in $KRB4LIB, headers in $KRB4INC])
  fi

  dnl Check for DES library
  AC_CHECK_LIB(des, des_pcbc_encrypt,
  [
    AC_CHECK_HEADERS(des.h)

    dnl resolv lib?
    AC_CHECK_FUNC(res_search, , [AC_CHECK_LIB(resolv, res_search)])

    dnl Check for the Kerberos4 library
    AC_CHECK_LIB(krb, krb_net_read,
    [
      dnl Check for header files
      AC_CHECK_HEADERS(krb.h)

      dnl we found the required libraries, add to LIBS
      LIBS="-lkrb -ldes $LIBS"

      dnl Check for function krb_get_our_ip_for_realm
      dnl this is needed for NAT networks
      AC_CHECK_FUNCS(krb_get_our_ip_for_realm)

      dnl add define KRB4
      AC_DEFINE(KRB4, 1,
      [if you have the Kerberos4 libraries (including -ldes)])

      dnl substitute it too!
      KRB4_ENABLED=1
      AC_SUBST(KRB4_ENABLED)

      dnl the krb4 stuff needs a strlcpy()
      AC_CHECK_FUNCS(strlcpy)

    ])
  ])
else
  AC_MSG_RESULT(no)
fi


dnl **********************************************************************
dnl Check for the presence of SSL libraries and headers
dnl **********************************************************************

dnl Default to compiler & linker defaults for SSL files & libraries.
OPT_SSL=off
AC_ARG_WITH(ssl,dnl
AC_HELP_STRING([--with-ssl=PATH], [where to look for SSL, PATH points to the SSL installation (default: /usr/local/ssl)])
AC_HELP_STRING([--without-ssl], [disable SSL]),
  OPT_SSL=$withval)

if test X"$OPT_SSL" = Xno
then
  AC_MSG_WARN(SSL/https support disabled)  
else

  dnl Check for and handle argument to --with-ssl.

  dnl save the pre-ssl check flags for a while
  CLEANLDFLAGS="$LDFLAGS"
  CLEANCPPFLAGS="$CPPFLAGS"

  case "$OPT_SSL" in
  yes)
    EXTRA_SSL=/usr/local/ssl ;;
  off)
    EXTRA_SSL= ;;
  *)
    dnl check the given spot right away!
    EXTRA_SSL=$OPT_SSL
    LDFLAGS="$LDFLAGS -L$EXTRA_SSL/lib"
    CPPFLAGS="$CPPFLAGS -I$EXTRA_SSL/include/openssl -I$EXTRA_SSL/include"
    ;;
  esac

  AC_CHECK_LIB(crypto, CRYPTO_lock,[
     HAVECRYPTO="yes"
     ],[
     OLDLDFLAGS="$LDFLAGS"
     OLDCPPFLAGS="$CPPFLAGS"
     LDFLAGS="$CLEANLDFLAGS -L$EXTRA_SSL/lib"
     CPPFLAGS="$CLEANCPPFLAGS -I$EXTRA_SSL/include/openssl -I$EXTRA_SSL/include"
     AC_CHECK_LIB(crypto, CRYPTO_add_lock,[
       HAVECRYPTO="yes" ], [
       LDFLAGS="$OLDLDFLAGS"
       CPPFLAGS="$OLDCPPFLAGS"
       ])
    ])


  if test "$HAVECRYPTO" = "yes"; then
    dnl This is only reasonable to do if crypto actually is there: check for
    dnl SSL libs NOTE: it is important to do this AFTER the crypto lib

    AC_CHECK_LIB(crypto, CRYPTO_add_lock)
    AC_CHECK_LIB(ssl, SSL_connect)

    if test "$ac_cv_lib_ssl_SSL_connect" != yes; then
        dnl we didn't find the SSL lib, try the RSAglue/rsaref stuff
        AC_MSG_CHECKING(for ssl with RSAglue/rsaref libs in use);
        OLIBS=$LIBS
        LIBS="$LIBS -lRSAglue -lrsaref"
        AC_CHECK_LIB(ssl, SSL_connect)
        if test "$ac_cv_lib_ssl_SSL_connect" != yes; then
            dnl still no SSL_connect
            AC_MSG_RESULT(no)
            LIBS=$OLIBS
        else
            AC_MSG_RESULT(yes)
        fi
    fi


    dnl Check for SSLeay headers
    AC_CHECK_HEADERS(openssl/x509.h openssl/rsa.h openssl/crypto.h \
                     openssl/pem.h openssl/ssl.h openssl/err.h,
      OPENSSL_ENABLED=1)

    if test $ac_cv_header_openssl_x509_h = no; then
      AC_CHECK_HEADERS(x509.h rsa.h crypto.h pem.h ssl.h err.h,
        OPENSSL_ENABLED=1)
    fi

    dnl If the ENGINE library seems to be around, check for the OpenSSL engine
    dnl header, it is kind of "separated" from the main SSL check
    AC_CHECK_FUNC(ENGINE_init, [ AC_CHECK_HEADERS(openssl/engine.h) ])

    AC_SUBST(OPENSSL_ENABLED)

  fi

  if test X"$OPT_SSL" != Xoff &&
     test "$OPENSSL_ENABLED" != "1"; then
    AC_MSG_ERROR([OpenSSL libs and/or directories were not found where specified!])
  fi


  dnl these can only exist if openssl exists

  AC_CHECK_FUNCS( RAND_status \
                  RAND_screen \
                  RAND_egd )

fi

dnl **********************************************************************
dnl Check for the presence of ZLIB libraries and headers
dnl **********************************************************************

dnl Check for & handle argument to --with-zlib.

_cppflags=$CPPFLAGS
_ldflags=$LDFLAGS
OPT_ZLIB="/usr/local"
AC_ARG_WITH(zlib,
  AC_HELP_STRING([--with-zlib=PATH], [search for zlib in PATH])
  AC_HELP_STRING([--without-zlib], [disable use of zlib]),
                 [OPT_ZLIB="$withval"])

case "$OPT_ZLIB" in
  no)
    AC_MSG_WARN([zlib disabled]) ;;
  *)
    dnl check for the lib first without setting any new path, since many
    dnl people have it in the default path

    AC_CHECK_LIB(z, inflateEnd, ,
                   [if test -d "$OPT_ZLIB"; then
                      CPPFLAGS="$CPPFLAGS -I$OPT_ZLIB/include"
                      LDFLAGS="$LDFLAGS -L$OPT_ZLIB/lib"
                   fi])

    AC_CHECK_HEADER(zlib.h,[
      AC_CHECK_LIB(z, gzread,
                   [AM_CONDITIONAL(CONTENT_ENCODING, true)
                   HAVE_LIBZ="1"
                   AC_SUBST(HAVE_LIBZ)
                   LIBS="$LIBS -lz"
                   AC_DEFINE(HAVE_ZLIB_H, 1, [if you have the zlib.h header file])
                   AC_DEFINE(HAVE_LIBZ, 1, [If zlib is available])],
                   [ CPPFLAGS=$_cppflags
                   LDFLAGS=$_ldflags])],
      [ CPPFLAGS=$_cppflags
      LDFLAGS=$_ldflags]
    )
    ;;
esac

dnl Default is to try the thread-safe versions of a few functions
OPT_THREAD=on
AC_ARG_ENABLE(thread,dnl
[  --disable-thread       tell configure to not look for thread-safe functions],
  OPT_THREAD=off
)

if test X"$OPT_THREAD" = Xoff
then
  AC_MSG_WARN(libcurl will not get built using thread-safe functions)
  AC_DEFINE(DISABLED_THREADSAFE, 1, \
Set to explicitly specify we don't want to use thread-safe functions)
else

  dnl dig around for gethostbyname_r()
  CURL_CHECK_GETHOSTBYNAME_R()

  dnl dig around for gethostbyaddr_r()
  CURL_CHECK_GETHOSTBYADDR_R()

  dnl poke around for inet_ntoa_r()
  CURL_CHECK_INET_NTOA_R()

  dnl is there a localtime_r()
  CURL_CHECK_LOCALTIME_R()

  AC_CHECK_FUNCS( gmtime_r )

fi

dnl **********************************************************************
dnl Back to "normal" configuring
dnl **********************************************************************

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS( \
        unistd.h \
        malloc.h \
        stdlib.h \
        arpa/inet.h \
        net/if.h \
        netinet/in.h \
	netinet/if_ether.h \
        netdb.h \
        sys/select.h \
        sys/socket.h \
        sys/sockio.h \
        sys/stat.h \
        sys/types.h \
        sys/time.h \
        sys/param.h \
        termios.h \
        termio.h \
        sgtty.h \
        fcntl.h \
        dlfcn.h \
        alloca.h \
        winsock.h \
        time.h \
        io.h \
        pwd.h \
        utime.h \
        sys/utime.h \
        sys/poll.h \
        setjmp.h
)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_HEADER_TIME

# mprintf() checks:

# check for 'long double'
# AC_CHECK_SIZEOF(long double, 8)
# check for 'long long'
# AC_CHECK_SIZEOF(long long, 4)

# check for ssize_t
AC_CHECK_TYPE(ssize_t, int)

TYPE_SOCKLEN_T
TYPE_IN_ADDR_T

dnl Checks for library functions.
dnl AC_PROG_GCC_TRADITIONAL
AC_TYPE_SIGNAL
dnl AC_FUNC_VPRINTF
AC_CHECK_FUNCS( socket \
                select \
                strdup \
                strstr \
                strtok_r \
                strftime \
                uname \
                strcasecmp \
                stricmp \
                strcmpi \
                gethostbyaddr \
                gettimeofday \
                inet_addr \
                inet_ntoa \
                tcsetattr \
                tcgetattr \
                perror \
                closesocket \
                setvbuf \
                sigaction \
                signal \
                getpass_r \
                strlcat \
                getpwuid \
                geteuid \
                dlopen \
                utime \
                sigsetjmp \
                poll
)

dnl sigsetjmp() might be a macro and no function so if it isn't found already
dnl we make an extra check here!
if test "$ac_cv_func_sigsetjmp" != "yes"; then
  AC_MSG_CHECKING([for sigsetjmp defined as macro])
  AC_TRY_LINK( [#include <setjmp.h>],
               [sigjmp_buf jmpenv;
                sigsetjmp(jmpenv, 1);],
               AC_MSG_RESULT(yes)
               AC_DEFINE(HAVE_SIGSETJMP, 1, [If you have sigsetjmp]),
               AC_MSG_RESULT(no)
               )
fi

dnl removed 'getpass' check on October 26, 2000

if test "$ac_cv_func_select" != "yes"; then
  AC_MSG_ERROR(Can't work without an existing select() function)
fi
if test "$ac_cv_func_socket" != "yes"; then
  AC_MSG_ERROR(Can't work without an existing socket() function)
fi

AC_PATH_PROG( PERL, perl, , 
  $PATH:/usr/local/bin/perl:/usr/bin/:/usr/local/bin )
AC_SUBST(PERL)

AC_PATH_PROGS( NROFF, gnroff nroff, , 
  $PATH:/usr/bin/:/usr/local/bin )
AC_SUBST(NROFF)


AC_MSG_CHECKING([CA cert bundle install path])

AC_ARG_WITH(ca-bundle,
AC_HELP_STRING([--with-ca-bundle=FILE], [File name to install the CA bundle as])
AC_HELP_STRING([--without-ca-bundle], [Don't install the CA bundle]),
   [ ca="$withval" ],
   [
  if test "x$prefix" != xNONE; then
    ca="$prefix/share/curl/curl-ca-bundle.crt"
  else
    ca="$ac_default_prefix/share/curl/curl-ca-bundle.crt"
  fi
   ] )

if test "x$ca" = "xno"; then
  dnl let's not keep "no" as path name, blank it instead
  ca=""
else
  AC_DEFINE_UNQUOTED(CURL_CA_BUNDLE, "$ca", [CA bundle full path name])
fi

CURL_CA_BUNDLE="$ca"
AC_SUBST(CURL_CA_BUNDLE)
AC_MSG_RESULT([$ca])

AC_PROG_YACC

CURL_LIBS=$LIBS
LIBS=$original_LIBS
AC_SUBST(CURL_LIBS)
dnl
dnl  ############################## END OF LIBCURL ########################################
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
dnl Find pkg-config
dnl
AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
if test x$PKG_CONFIG = xno ; then
  echo "*** pkg-config not found. See http://pkgconfig.sourceforge.net"
  echo "*** All of DIRECTFB dependent parts will be disabled"
else

dnl
dnl Check for DirectFB
dnl

dnl NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE
dnl  DFB:
dnl  Temporary disabled, port to new API needed
dnl NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE
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
AC_CHECK_HEADERS(string.h strings.h alloca.h)


dnl
dnl Check for LIRC client support
dnl
AC_CHECK_LIRC



dnl NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE
dnl  CORBA:
dnl  Temporary disabled, port to new API needed
dnl NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE
dnl
dnl CORBA support
dnl
dnl AC_ARG_ENABLE(orbit,[  --enable-orbit          build orbit dependant parts of xine [default=no]], , enable_orbit=no)
dnl 
dnl if test "x$enable_orbit" = "xyes"; then
dnl   AM_PATH_ORBIT(0.4.0, [
dnl     AC_DEFINE(HAVE_ORBIT,,[Define this if you have ORBit (libORBit) installed])
dnl     ORBIT_LIBS="$ORBIT_LIBS -lORBitCosNaming"
dnl   ], [
dnl     AC_MSG_RESULT(*** All of ORBit dependent parts will be disabled ***)
dnl     enable_orbit=no])
dnl fi

dnl AM_CONDITIONAL(HAVE_ORBIT, test x"$enable_orbit" = "xyes")
dnl if test x"$enable_orbit" = "xyes"; then
dnl    CORBA_DEP=""
dnl else 
dnl    CORBA_DEP="#"
dnl fi
dnl AC_SUBST(CORBA_DEP)



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
    CFLAGS="$CFLAGS -pipe -fomit-frame-pointer $f_af $f_al $f_aj $m_wm $m_psb -fexpensive-optimizations $f_si $f_nsa -ffast-math -funroll-loops -finline-functions -D_REENTRANT"
    DEBUG_CFLAGS="$X_CFLAGS $DEBUG_CFLAGS -D_REENTRANT -DDEBUG"
    AC_DEFINE(__i386__,1,[Define this if you're running x86 architecture])
    AC_DEFINE([ARCH_X86],,[Define this if you're running x86 architectur])
    ;;

  i?86*-*-linux* | i386-*-solaris* | i?86-* | k?-* | athlon-*)
    if test "$GCC" = yes; then

	dnl Check for gcc cpu optimization support
	AC_TRY_CFLAGS("-mcpu=i386", 
	    sarchopt="-mcpu",
	    AC_TRY_CFLAGS("-march=i386", 
		sarchopt="-march",
	        [ AC_MSG_RESULT(** no cpu optimization supports **)
	          sarchopt=no ]))

	dnl special check for k7 cpu CC support
        AC_TRY_CFLAGS("$sarchopt=athlon", k7cpu="athlon", k7cpu="i686")

	dnl add x86 specific CFLAGS
	CFLAGS="$CFLAGS -O3 -pipe -fomit-frame-pointer $f_af $f_al $f_aj $m_wm $m_psb -fexpensive-optimizations $f_si $f_nsa -ffast-math -funroll-loops -finline-functions"
	DEBUG_CFLAGS="$DEBUG_CFLAGS -O3"
	
	dnl enable x86 specific parts of the code

	if test x"$sarchopt" != "xno"; then
	[
	    archopt_val=

	    case "$host_alias" in
	    i386-*) # *BSD return this even on a P III #-))
		archopt_val=i386 ;;
	    i486-*) # oh dear!
		archopt_val=i486 ;;
	    i586-*)
		archopt_val=pentium ;;
	    i686-*)
		archopt_val=pentiumpro
		if test x"$check_athlon" = "xyes"; then
		    if test -f /proc/cpuinfo; then
			modelname=`cat /proc/cpuinfo | grep "model\ name\	:" | sed -e 's/ //g' | cut -d':' -f2`
			case "$modelname" in
			*Athlon* | *Duron* | *K7*)
			    archopt_val="$k7cpu" ;;
			esac
		    fi 
		fi
		;;
	    k6-*)
		archopt_val=k6 ;;
	    k7-*)
		archopt_val=k7 ;;
	    athlon-*)
		archopt_val=athlon ;;
	    esac

	    if test x"$archopt_val" != x; then
		CFLAGS="$CFLAGS $sarchopt=$archopt_val"
		DEBUG_CFLAGS="$DEBUG_CFLAGS $sarchopt=$archopt_val"
	    fi
	]
	fi

    else
	dnl add x86 specific cc CFLAGS
	CFLAGS="$CFLAGS -O"
	DEBUG_CFLAGS="$DEBUG_CFLAGS -O"
    fi

    AC_DEFINE(__i386__,1,[Define this if you're running x86 architecture])
    AC_DEFINE([ARCH_X86],,[Define this if you're running x86 architectur])
    ;;

  alphaev56-*)
    AC_DEFINE(__alpha__,1,[Define this if you're running Alpha architecture])
    CFLAGS="$CFLAGS -O3 -mcpu=ev56 -mieee"
    DEBUG_CFLAGS="$DEBUG_CFLAGS -O3 -mcpu=ev56 -mieee"
    ;;

  alpha*)
    AC_DEFINE(__alpha__,1,[Define this if you're running Alpha architecture])
    CFLAGS="$CFLAGS -O3 -mieee"
    DEBUG_CFLAGS="$DEBUG_CFLAGS -O3 -mieee"
    ;;

  ppc-*-linux-* | powerpc-*) 
    AC_DEFINE(__ppc__,1,[Define this if you're running PowerPC architecture])
    CFLAGS="$CFLAGS -O3 -pipe -fomit-frame-pointer $m_wm $m_psb -fexpensive-optimizations $f_si $f_nsa -ffast-math -funroll-loops -funroll-all-loops -finline-functions"
    DEBUG_CFLAGS="$DEBUG_CFLAGS -O3"
    ;;

  sparc64-*-linux-* | sparc-*)
    AC_DEFINE(__sparc,1,[Define this if you're running Sparc architecture])

    if test "$GCC" = yes; then
	CFLAGS="$CFLAGS -O3"
	DEBUG_CFLAGS="$DEBUG_CFLAGS -O"
    else
	CFLAGS="$CFLAGS -O"
	DEBUG_CFLAGS="$DEBUG_CFLAGS -O"
    fi
    ;;

  mips-*)
    CFLAGS="$CFLAGS -O3"
    DEBUG_CFLAGS="$DEBUG_CFLAGS -O"
    ;;

  m68k-*)
    CFLAGS="$CFLAGS -O2"
    DEBUG_CFLAGS="$DEBUG_CFLAGS -O"
    ;;

  ia64-*)
    CFLAGS="$CFLAGS -O3"
    DEBUG_CFLAGS="$DEBUG_CFLAGS -O3"
    ;;

  s390-*)
    CFLAGS="$CFLAGS -O3"
    DEBUG_CFLAGS="$DEBUG_CFLAGS -O3"
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

    CFLAGS="$CFLAGS -O3"
    DEBUG_CFLAGS="$DEBUG_CFLAGS -O"
    ;;
esac

CFLAGS="$CFLAGS $COMMON_CFLAGS $ALSA_CFLAGS $ESD_CFLAGS"
DEBUG_CFLAGS="$DEBUG_CFLAGS $COMMON_CFLAGS $ALSA_CFLAGS $ESD_CFLAGS -g -DDEBUG"
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
XINE_DOCPATH="${datadir}/doc/xine-ui"
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

XINE_LOGO_MRL="file:$XINE_SKINDIR/xine_logo.mpv"
AC_SUBST(XINE_LOGO_MRL)
AC_DEFINE_UNQUOTED(XINE_LOGO_MRL, "$XINE_LOGO_MRL",[official logo mrl])

XINE_VISDIR="${datadir}/xine/visuals"
eval XINE_VISDIR=`eval echo "$XINE_VISDIR"`
AC_SUBST(XINE_VISDIR)
AC_DEFINE_UNQUOTED(XINE_VISDIR, "$XINE_VISDIR",[Location of xine visual animation streams])

dnl
dnl Some include paths ( !!! DO NOT REMOVE !!! )
dnl
INCLUDES='-I$(top_srcdir) -I$(top_builddir) -I$(top_srcdir)/readline -I$(top_builddir)/readline -I$(top_srcdir)/libcurl/include -I$(top_builddir)/libcurl/include -I$(top_srcdir)/src -I$(top_builddir)/src -I$(top_srcdir)/src/common -I$(top_builddir)/src/common -I$(top_srcdir)/src/xitk/xine-toolkit -I$(top_builddir)/src/xitk/xine-toolkit -I$(prefix)/include $(INTLDIR)'
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
dnl Output configuration files
dnl
AC_CONFIG_FILES([
Makefile
m4/Makefile
intl/Makefile
readline/Makefile
src/Makefile
src/common/Makefile
src/Imlib-light/Makefile
src/xitk/Makefile
src/xitk/xine-toolkit/Makefile
src/xitk/xine-toolkit/po/Makefile.in
src/xitk/skins/Makefile
src/xitk/skins/xinetic/Makefile
src/xitk/skins/cloudy/Makefile
src/xitk/skins/CelomaChrome/Makefile
src/aaui/Makefile
src/dfb/Makefile
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
libcurl/Makefile
libcurl/include/Makefile
libcurl/include/curl/Makefile
])
AC_CONFIG_COMMANDS([default],[[chmod +x ./misc/SlackBuild ./misc/build_rpms.sh ./misc/relchk.sh]],[[]])
AC_OUTPUT
