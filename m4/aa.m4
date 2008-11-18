dnl Configure path and dependencies for aalib.
dnl
dnl Copyright (C) 2001 Daniel Caujolle-Bert <segfault@club-internet.fr>
dnl  
dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 2 of the License, or
dnl (at your option) any later version.
dnl  
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl  
dnl You should have received a copy of the GNU General Public License
dnl along with this program; if not, write to the Free Software
dnl Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
dnl  
dnl  
dnl As a special exception to the GNU General Public License, if you
dnl distribute this file as part of a program that contains a configuration
dnl script generated by Autoconf, you may include it under the same
dnl distribution terms that you use for the rest of that program.
dnl  
dnl AM_PATH_AALIB([MINIMUM-VERSION, [ACTION-IF-FOUND [,ACTION-IF-NOT-FOUND ]]])
dnl Test for AALIB, and define AALIB_CFLAGS and AALIB_LIBS, AALIB_STATIC_LIBS.
dnl
dnl ***********************
dnl 26/09/2001
dnl   * fixed --disable-aalibtest.
dnl 17/09/2001
dnl   * use both aalib-config, and *last chance* aainfo for guessing.
dnl 19/08/2001
dnl   * use aalib-config instead of aainfo now.
dnl 17/06/2001 
dnl   * First shot
dnl

dnl Internal bits, used by AM_PATH_AALIB.
dnl Requires AALIB_CFLAGS and AALIB_FLAGS to be defined
AC_DEFUN([_AALIB_CHECK_VERSION], [
  ac_save_CFLAGS="$CFLAGS"
  ac_save_LIBS="$LIBS"
  CFLAGS="$AALIB_CFLAGS $CFLAGS"
  LIBS="$AALIB_LIBS $LIBS"

dnl Now check if the installed AALIB is sufficiently new. (Also sanity
dnl checks the results of aalib-config to some extent.)

  rm -f conf.aalibtest
  AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <stdio.h>
#include <stdlib.h>
#include <aalib.h>

int main () {
  int major, minor;
  char *tmp_version;

  system ("touch conf.aalibtest");

  /* HP/UX 9 (%@#!) writes to sscanf strings */
  tmp_version = (char *) strdup("$min_aalib_version");
  if (sscanf(tmp_version, "%d.%d", &major, &minor) != 2) {
    printf("%s, bad version string\n", "$min_aalib_version");
    exit(1);
  }

  if ((AA_LIB_VERSION > major) || ((AA_LIB_VERSION == major) && 
#ifdef AA_LIB_MINNOR
      (AA_LIB_MINNOR >= minor)
#else
      (AA_LIB_MINOR >= minor)
#endif
     )) {
    return 0;
  }
  else {
#ifdef AA_LIB_MINNOR
    printf("\n*** An old version of AALIB (%d.%d) was found.\n", AA_LIB_VERSION, AA_LIB_MINNOR);
#else
    printf("\n*** An old version of AALIB (%d.%d) was found.\n", AA_LIB_VERSION, AA_LIB_MINOR);
#endif
    printf("*** You need a version of AALIB newer than %d.%d. The latest version of\n", major, minor);
    printf("*** AALIB is always available from:\n");
    printf("***        http://www.ta.jcu.cz://aa\n");
    printf("***\n");
  }
  return 1;
}
]])],[],[no_aalib=yes],[no_aalib=cc])
  if test "x$no_aalib" = xcc; then 
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <stdio.h>
#include <aalib.h>
]], [[ return ((AA_LIB_VERSION) || 
#ifdef AA_LIB_MINNOR
             (AA_LIB_MINNOR)
#else
             (AA_LIB_MINOR)
#endif
            ); ]])],[no_aalib=''],[no_aalib=yes])
  fi
  CFLAGS="$ac_save_CFLAGS"
  LIBS="$ac_save_LIBS"
])


AC_DEFUN([AM_PATH_AALIB],
[dnl 
dnl
AC_ARG_WITH(aalib-prefix,
    AS_HELP_STRING([--with-aalib-prefix=DIR], [prefix where AALIB is installed (optional)]),
            aalib_config_prefix="$withval", aalib_config_prefix="")
AC_ARG_WITH(aalib-exec-prefix,
    AS_HELP_STRING([--with-aalib-exec-prefix=DIR], [exec prefix where AALIB is installed (optional)]),
            aalib_config_exec_prefix="$withval", aalib_config_exec_prefix="")
AC_ARG_ENABLE(aalibtest, 
    AS_HELP_STRING([--disable-aalibtest], [do not try to compile and run a test AALIB program]),
            enable_aalibtest=$enableval, enable_aalibtest=yes)

  AC_LANG_PUSH([C])

  if test x$aalib_config_exec_prefix != x ; then
     aalib_config_args="$aalib_config_args --exec-prefix=$aalib_config_exec_prefix"
     if test x${AALIB_CONFIG+set} != xset ; then
        AALIB_CONFIG=$aalib_config_exec_prefix/bin/aalib-config
     fi
  fi
  if test x$aalib_config_prefix != x ; then
     aalib_config_args="$aalib_config_args --prefix=$aalib_config_prefix"
     if test x${AALIB_CONFIG+set} != xset ; then
        AALIB_CONFIG=$aalib_config_prefix/bin/aalib-config
     fi
  fi

  min_aalib_version=ifelse([$1], ,1.2,$1)

  if test x"$enable_aalibtest" != "xyes"; then
    AC_MSG_CHECKING([for AALIB version >= $min_aalib_version])
  else
    if test ! -x "$AALIB_CONFIG"; then
      AALIB_CONFIG=""
    fi
    AC_PATH_TOOL(AALIB_CONFIG, aalib-config, no)

    if test "$AALIB_CONFIG" = "no" ; then

dnl aalib-config is missing, check for old aainfo

      AALIB_LIBS="$AALIB_LIBS -laa"
      if test x$aalib_config_exec_prefix != x ; then
        AALIB_CFLAGS="-I$aalib_config_exec_prefix/include"
        AALIB_LIBS="-L$aalib_config_exec_prefix/lib -laa"
        if test x${AAINFO+set} != xset ; then
          AAINFO=$aalib_config_exec_prefix/bin/aainfo
        fi
      fi

      if test x$aalib_config_prefix != x ; then
        AALIB_CFLAGS="-I$aalib_config_prefix/include"
        AALIB_LIBS="-L$aalib_config_prefix/lib -laa"
        if test x${AAINFO+set} != xset ; then
          AAINFO=$aalib_config_prefix/bin/aainfo
        fi
      fi

      if test x"$aalib_config_prefix" = "x"; then
        AC_PATH_TOOL(AAINFO, aainfo, no)
      else
        AC_MSG_CHECKING(for $AAINFO)
        if test -x $AAINFO; then 
          AC_MSG_RESULT(yes)
        else 
          AAINFO="no"
          AC_MSG_RESULT(no)
        fi
      fi

      AC_MSG_CHECKING([for AALIB version >= $min_aalib_version])
      no_aalib=""

      if test x"$AAINFO" = "xno"; then
        no_aalib=yes
      else
        aalib_drivers="`$AAINFO --help | grep drivers | sed -e 's/available//g;s/drivers//g;s/\://g'`"
        for drv in $aalib_drivers; do
          if test $drv = "X11" -a x$x11dep = "x"; then
            AALIB_CFLAGS="$AALIB_CFLAGS `echo $X_CFLAGS|sed -e 's/\-I/\-L/g;s/include/lib/g'`"
            x11dep="yes"
          fi
dnl          if test $drv = "slang" -a x$slangdep = "x"; then 
dnl            slangdep="yes"
dnl          fi
dnl          if test $drv = "gpm" -a x$gmpdep = "x"; then 
dnl            gpmdep="yes"
dnl          fi
        done

        _AALIB_CHECK_VERSION
      fi

    else dnl AALIB_CONFIG
      AC_MSG_CHECKING([for AALIB version >= $min_aalib_version])
      no_aalib=""
      AALIB_CFLAGS="`$AALIB_CONFIG $aalib_config_args --cflags`"
      AALIB_LIBS="`$AALIB_CONFIG $aalib_config_args --libs`"
      aalib_config_major_version=`$AALIB_CONFIG $aalib_config_args --version | \
             sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
      aalib_config_minor_version=`$AALIB_CONFIG $aalib_config_args --version | \
             sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
      aalib_config_sub_version=`$AALIB_CONFIG $aalib_config_args --version | \
             sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`

      _AALIB_CHECK_VERSION
    fi dnl AALIB_CONFIG
  fi dnl enable_aalibtest

  if test "x$no_aalib" = x; then
    AC_MSG_RESULT(yes)
    ifelse([$2], , :, [$2])     
  else
    AC_MSG_RESULT(no)
    if test "$AALIB_CONFIG" = "no"; then
      echo "*** The [aalib-config|aainfo] program installed by AALIB could not be found"
      echo "*** If AALIB was installed in PREFIX, make sure PREFIX/bin is in"
      echo "*** your path, or use --with-aalib-prefix to set the prefix"
      echo "*** where AALIB is installed."
    else
      if test -f conf.aalibtest ; then
        :
      else
        echo "*** Could not run AALIB test program, checking why..."
        CFLAGS="$CFLAGS $AALIB_CFLAGS"
        LIBS="$LIBS $AALIB_LIBS"
        AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <stdio.h>
#include <aalib.h>
]], [[ 
          return ((AA_LIB_VERSION) || 
#ifdef AA_LIB_MINNOR
                  (AA_LIB_MINNOR)
#else
                  (AA_LIB_MINOR)
#endif
                  ); ]])],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding AALIB or finding the wrong"
          echo "*** version of AALIB. If it is not finding AALIB, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"
          echo "***"],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occured. This usually means AALIB was incorrectly installed"
          echo "*** or that you have moved AALIB since it was installed." ])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
      fi
    fi
    AALIB_CFLAGS=""
    AALIB_LIBS=""
    ifelse([$3], , :, [$3])
  fi
  AC_SUBST(AALIB_CFLAGS)
  AC_SUBST(AALIB_LIBS)
  AC_LANG_POP([C])
  rm -f conf.aalibtest
])
