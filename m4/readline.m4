dnl stolen from libgadu
dnl Rewritten from scratch. --wojtekka
dnl 

AC_DEFUN([AC_CHECK_READLINE],[
  AC_SUBST(READLINE_LIBS)
  AC_SUBST(READLINE_INCLUDES)

  AC_ARG_WITH(readline,
    [[  --with-readline[=dir]   Compile with readline/locate base dir]],
    if test "x$withval" = "xno" ; then
      without_readline=yes
    elif test "x$withval" != "xyes" ; then
      with_arg="$withval/include:-L$withval/lib $withval/include/readline:-L$withval/lib"
    fi)

  if test "x$without_readline" != "xyes"; then
    AC_MSG_CHECKING(for readline.h)
    for i in $with_arg \
	     /usr/include: \
	     /usr/local/include:-L/usr/local/lib \
             /usr/freeware/include:-L/usr/freeware/lib32 \
	     /usr/pkg/include:-L/usr/pkg/lib \
	     /sw/include:-L/sw/lib \
	     /cw/include:-L/cw/lib \
	     /net/caladium/usr/people/piotr.nba/temp/pkg/include:-L/net/caladium/usr/people/piotr.nba/temp/pkg/lib \
	     /boot/home/config/include:-L/boot/home/config/lib; do
    
      incl=`echo "$i" | sed 's/:.*//'`
      lib=`echo "$i" | sed 's/.*://'`

      have_readline=no

      if test -f $incl/readline/readline.h ; then
        AC_MSG_RESULT($incl/readline/readline.h)
        READLINE_LIBS="$lib -lreadline"
	if test "$incl" != "/usr/include"; then
	  READLINE_INCLUDES="-I$incl/readline -I$incl"
	else
	  READLINE_INCLUDES="-I$incl/readline"
	fi
        have_readline=yes
        break
      elif test -f $incl/readline.h -a "x$incl" != "x/usr/include"; then
        AC_MSG_RESULT($incl/readline.h)
        READLINE_LIBS="$lib -lreadline"
        READLINE_INCLUDES="-I$incl"
        have_readline=yes
        break
      fi

    done

    if test "$have_readline" = yes; then
      dnl Check to see if ncurses is needed.
      dnl Debian's libreadline is linked against ncurses,
      dnl but others' may not be.
      READLINE_LIBS_TEMP="$LIBS"
      READLINE_LDFLAGS_TEMP="$LDFLAGS"
      LIBS=''
      AC_MSG_NOTICE([checking whether libreadline is linked against libncurses])
      AC_CHECK_LIB([readline], [cbreak])
      if test "$LIBS" = ''; then
	LIBS=''
	LDFLAGS="$READLINE_LDFLAGS_TEMP"
        AC_CHECK_LIB([ncurses], [cbreak])
        if test "$LIBS" = ''; then
	  AC_MSG_NOTICE([libreadline is not linked against libncurses, and I can't find it - disabling])
	  have_readline=no
	else
	  READLINE_LIBS="$READLINE_LIBS $LIBS"
	  AC_MSG_NOTICE([libreadline is not linked against libncurses; adding $LIBS])
	fi
      else
	AC_MSG_NOTICE([libreadline is linked against libncurses])
      fi
      LIBS="$READLINE_LIBS_TEMP"
      LDFLAGS="$READLINE_LDFLAGS_TEMP"
    else
      AC_MSG_RESULT(not found)
    fi

    if test "$have_readline" = yes; then
      AC_DEFINE(HAVE_READLINE, 1, [define if You want readline])
    fi

  fi

])

