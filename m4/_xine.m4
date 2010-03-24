dnl
dnl Check for minimum version of libtool
dnl AC_PREREQ_LIBTOOL([MINIMUM VERSION],[ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND ]])
AC_DEFUN([AC_PREREQ_LIBTOOL],
  [
    lt_min_full=ifelse([$1], ,1.3.5,$1)
    lt_min=`echo $lt_min_full | sed -e 's/\.//g'`
    AC_MSG_CHECKING(for libtool >= $lt_min_full)
    lpwd="`pwd`"
    lt_pathname="`echo $lpwd/ltmain.sh | sed -e 's/\=build\///g'`"
    lt_version="`grep '^VERSION' $lt_pathname | sed -e 's/\.//g;s/VERSION\=//g;s/[a-zA-Z]//g;s/-//g'`"

    if test $lt_version -lt 100; then
      lt_version=`expr $lt_version \* 10`
    fi

    if test $lt_version -lt $lt_min; then
      AC_MSG_RESULT(no)
      ifelse([$3], , :, [$3])
    fi
    AC_MSG_RESULT(yes)
    ifelse([$2], , :, [$2])
  ])

dnl
AC_DEFUN([AC_CHECK_LIRC],
  [AC_ARG_ENABLE(lirc,
     [AS_HELP_STRING([--disable-lirc], [turn off LIRC support])],
     [given=Y], [given=N; enable_lirc=yes])

  found_lirc=no
  if test x"$enable_lirc" = xyes; then
    have_lirc=yes
    PKG_CHECK_MODULES(LIRC, liblircclient0, [found_lirc=yes], [:])
    if test "$found_lirc" = yes; then
      LIRC_INCLUDE="$LIRC_CFLAGS"
    else
     AC_REQUIRE_CPP
     AC_CHECK_LIB(lirc_client,lirc_init,
           [AC_CHECK_HEADER(lirc/lirc_client.h, true, have_lirc=no)], have_lirc=no)
     if test "$have_lirc" = "yes"; then

        if test x"$LIRC_PREFIX" != "x"; then
           lirc_libprefix="$LIRC_PREFIX/lib"
	   LIRC_INCLUDE="-I$LIRC_PREFIX/include"
        fi
        for llirc in $lirc_libprefix /lib /usr/lib /usr/local/lib; do
          AC_CHECK_FILE(["$llirc/liblirc_client.so"],
             [LIRC_LIBS="$llirc/liblirc_client.so"
              found_lirc=yes]
             AC_DEFINE([HAVE_LIRC],,[Define this if you have LIRC (liblirc_client) installed]),
             AC_CHECK_FILE(["$llirc/liblirc_client.a"],
                [LIRC_LIBS="$llirc/liblirc_client.a"
                 found_lirc=yes],,)
          )
        done
     else
	test $given = Y && AC_MSG_ERROR([LIRC client support requested but not available])
	AC_MSG_RESULT([*** LIRC client support not available, LIRC support will be disabled ***])
     fi
    fi
  fi
     if test "$found_lirc" = yes; then
	AC_DEFINE([HAVE_LIRC],,[Define this if you have LIRC (liblirc_client) installed])
     fi
     AM_CONDITIONAL([HAVE_LIRC], [test "$found_lirc" = yes])
     AC_SUBST(LIRC_LIBS)
     AC_SUBST(LIRC_INCLUDE)
])

dnl AC_C_ATTRIBUTE_ALIGNED
dnl define ATTRIBUTE_ALIGNED_MAX to the maximum alignment if this is supported
AC_DEFUN([AC_C_ATTRIBUTE_ALIGNED],
    [AC_CACHE_CHECK([__attribute__ ((aligned ())) support],
        [ac_cv_c_attribute_aligned],
        [ac_cv_c_attribute_aligned=0
        for ac_cv_c_attr_align_try in 2 4 8 16 32 64; do
            AC_TRY_COMPILE([],
                [static char c __attribute__ ((aligned($ac_cv_c_attr_align_try))) = 0
; return c;],
                [ac_cv_c_attribute_aligned=$ac_cv_c_attr_align_try])
        done])
    if test x"$ac_cv_c_attribute_aligned" != x"0"; then
        AC_DEFINE_UNQUOTED([ATTRIBUTE_ALIGNED_MAX],
            [$ac_cv_c_attribute_aligned],[maximum supported data alignment])
    fi])

dnl AC_TRY_CFLAGS (CFLAGS, [ACTION-IF-WORKS], [ACTION-IF-FAILS])
dnl check if $CC supports a given set of cflags
AC_DEFUN([AC_TRY_CFLAGS],
    [AC_MSG_CHECKING([if $CC supports $1 flags])
    SAVE_CFLAGS="$CFLAGS"
    CFLAGS="$1"
    AC_TRY_COMPILE([],[],[ac_cv_try_cflags_ok=yes],[ac_cv_try_cflags_ok=no])
    CFLAGS="$SAVE_CFLAGS"
    AC_MSG_RESULT([$ac_cv_try_cflags_ok])
    if test x"$ac_cv_try_cflags_ok" = x"yes"; then
        ifelse([$2],[],[:],[$2])
    else
        ifelse([$3],[],[:],[$3])
    fi])


dnl AC_CHECK_GENERATE_INTTYPES_H (INCLUDE-DIRECTORY)
dnl generate a default inttypes.h if the header file does not exist already
AC_DEFUN([AC_CHECK_GENERATE_INTTYPES],
    [AC_CHECK_HEADER([inttypes.h],[],
        [AC_COMPILE_CHECK_SIZEOF([char],[1])
        AC_COMPILE_CHECK_SIZEOF([short],[2])
        AC_COMPILE_CHECK_SIZEOF([int],[4])
        AC_COMPILE_CHECK_SIZEOF([long long],[8])
        cat >$1/inttypes.h << EOF
#ifndef _INTTYPES_H
#define _INTTYPES_H
/* default inttypes.h for people who do not have it on their system */
#if (!defined __int8_t_defined) && (!defined __BIT_TYPES_DEFINED__)
#define __int8_t_defined
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
#ifdef ARCH_X86
typedef signed long long int64_t;
#endif
#endif
#if (!defined _LINUX_TYPES_H)
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
#ifdef ARCH_X86
typedef unsigned long long uint64_t;
#endif
#endif
#endif
EOF
        ])])


dnl AC_COMPILE_CHECK_SIZEOF (TYPE SUPPOSED-SIZE)
dnl abort if the given type does not have the supposed size
AC_DEFUN([AC_COMPILE_CHECK_SIZEOF],
    [AC_MSG_CHECKING(that size of $1 is $2)
    AC_TRY_COMPILE([],[switch (0) case 0: case (sizeof ($1) == $2):;],[],
        [AC_MSG_ERROR([can not build a default inttypes.h])])
    AC_MSG_RESULT([yes])])


dnl AC_PROG_GMSGFMT_PLURAL
dnl ----------------------
dnl Validate the GMSGFMT program found by gettext.m4; reject old versions
dnl of GNU msgfmt that do not support the "msgid_plural" extension.
AC_DEFUN([AC_PROG_GMSGFMT_PLURAL],
 [dnl AC_REQUIRE(AM_GNU_GETTEXT)
  
  if test "$GMSGFMT" != ":"; then
    AC_MSG_CHECKING([for plural forms in GNU msgfmt])

    changequote(,)dnl We use [ and ] in in .po test input

    dnl If the GNU msgfmt does not accept msgid_plural we define it
    dnl as : so that the Makefiles still can work.
    cat >conftest.po <<_ACEOF
msgid "channel"
msgid_plural "channels"
msgstr[0] "canal"
msgstr[1] "canal"

_ACEOF
    changequote([,])dnl

    if $GMSGFMT -o /dev/null conftest.po >/dev/null 2>&1; then
      AC_MSG_RESULT(yes)
    else
      AC_MSG_RESULT(no)
      AC_MSG_RESULT(
	[found GNU msgfmt program is too old, it does not support plural forms; ignore it])
      GMSGFMT=":"
    fi
    rm -f conftest.po
  fi
])dnl AC_PROG_GMSGFMT_PLURAL

dnl Shims for various functions which are present in newer xine-lib
AC_DEFUN([XINE_LIB_SHIMS],
   [AC_MSG_CHECKING([for re-entrant XML parser in xine-lib])
    tmp_CFLAGS="$CFLAGS"
    tmp_LIBS="$LIBS"
    CFLAGS="$CFLAGS $XINE_CFLAGS"
    LIBS="$LIBS $XINE_LIBS"
    AC_LINK_IFELSE(
	[AC_LANG_PROGRAM([[#include <xine/xmlparser.h>]],
			 [[xml_parser_init_r ((void *)0, 0, XML_PARSER_CASE_INSENSITIVE);]])],
	[AC_DEFINE([[HAVE_XML_PARSER_REENTRANT]], [[1]], [[Define if xml_parser_init_r etc. are available]])
	 AC_MSG_RESULT([[yes]])],
	[AC_MSG_RESULT([[no]])])
    CFLAGS="$tmp_CFLAGS"
    LIBS="$tmp_LIBS"
    ])
