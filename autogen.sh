#!/bin/sh
#
# Copyright (C) 2000-2003 the xine project
#
# This file is part of xine, a unix video player.
# 
# xine is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# xine is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
#
# Maintained by Stephen Torri <storri@users.sourceforge.net>
#
# run this to generate all the initial makefiles, etc.

PROG=xine-ui

# Minimum value required to build
AUTOMAKE_MIN=1.11.0
AUTOCONF_MIN=2.53

# Check how echo works in this /bin/sh
case `echo -n` in
-n)     _echo_n=   _echo_c='\c';;
*)      _echo_n=-n _echo_c=;;
esac

detect_configure_ac() {

  srcdir=`dirname $0`
  test -z "$srcdir" && srcdir=.

  (test -f $srcdir/configure.ac) || {
    echo $_echo_n "*** Error ***: Directory "\`$srcdir\`" does not look like the"
    echo " top-level directory"
    exit 1
  }
}

parse_version_no() {
  # version no. is extended/truncated to three parts; only digits are handled
  perl -e 'my $v = <>;
	   chomp $v;
	   my @v = split (" ", $v);
	   $v = $v[$#v];
	   $v =~ s/[^0-9.].*$//;
	   @v = split (/\./, $v);
	   push @v, 0 while $#v < 2;
	   print $v[0] * 10000 + $v[1] * 100 + $v[2], "\n"'
}

#--------------------
# AUTOCONF
#-------------------
detect_autoconf() {
  set -- `type autoconf 2>/dev/null`
  RETVAL=$?
  NUM_RESULT=$#
  RESULT_FILE=$3
  if [ $RETVAL -eq 0 -a $NUM_RESULT -eq 3 -a -f "$RESULT_FILE" ]; then
    AC="`autoconf --version | parse_version_no`"
    if [ `expr $AC` -ge "`echo $AUTOCONF_MIN | parse_version_no`" ]; then
      autoconf_ok=yes
    fi
  else
    echo
    echo "**Error**: You must have \`autoconf' >= $AUTOCONF_MIN installed to" 
    echo "           compile $PROG. Download the appropriate package"
    echo "           for your distribution or source from ftp.gnu.org."
    exit 1
  fi
}

run_autoheader () {
  if test x"$autoconf_ok" != x"yes"; then
    echo
    echo "**Warning**: Version of autoconf is less than $AUTOCONF_MIN."
    echo "             Some warning message might occur from autoconf"
    echo
  fi

  echo $_echo_n " + Running autoheader: $_echo_c";
    autoheader;
  echo "done."
}

run_autoconf () {
  if test x"$autoconf_ok" != x"yes"; then
    echo
    echo "**Warning**: Version of autoconf is less than $AUTOCONF_MIN."
    echo "             Some warning message might occur from autoconf"
    echo
  fi

  echo $_echo_n " + Running autoconf: $_echo_c";
    autoconf;
  echo "done."
}

#--------------------
# AUTOMAKE
#--------------------
detect_automake() {
  #
  # expected output from 'type' is
  #   automake is /usr/local/bin/automake
  #
  set -- `type automake 2>/dev/null`
  RETVAL=$?
  NUM_RESULT=$#
  RESULT_FILE=$3
  if [ $RETVAL -eq 0 -a $NUM_RESULT -eq 3 -a -f "$RESULT_FILE" ]; then
    AM="`automake --version | parse_version_no`"
    if [ `expr $AM` -ge "`echo $AUTOMAKE_MIN | parse_version_no`" ]; then
      automake_ok=yes
    fi
  else
    echo
    echo "**Error**: You must have \`automake' >= $AUTOMAKE_MIN installed to" 
    echo "           compile $PROG. Download the appropriate package"
    echo "           for your distribution or source from ftp.gnu.org."
    exit 1
  fi
}

run_automake () {
  if test x"$automake_ok" != x"yes"; then
    echo
    echo "**Warning**: Version of automake is less than $AUTOMAKE_MIN."
    echo "             Some warning message might occur from automake"
    echo
  fi

  echo $_echo_n " + Running automake: $_echo_c";

  automake --gnu --add-missing --copy;
  echo "done."
}

#--------------------
# ACLOCAL
#-------------------
detect_aclocal() {

  # if no automake, don't bother testing for aclocal
  set -- `type aclocal 2>/dev/null`
  RETVAL=$?
  NUM_RESULT=$#
  RESULT_FILE=$3
  if [ $RETVAL -eq 0 -a $NUM_RESULT -eq 3 -a -f "$RESULT_FILE" ]; then
    AC="`aclocal --version | parse_version_no`"
    if [ `expr $AC` -ge "`echo $AUTOMAKE_MIN | parse_version_no`" ]; then
      aclocal_ok=yes
    fi
  else
    echo
    echo "**Error**: You must have \`automake' >= $AUTOMAKE_MIN installed to" 
    echo "           compile $PROG. Download the appropriate package"
    echo "           for your distribution or source from ftp.gnu.org."
    exit 1
  fi
}

run_aclocal () {

  if test x"$aclocal_ok" != x"yes"; then
    echo
    echo "**Warning**: Version of aclocal is less than $AUTOMAKE_MIN."
    echo "             Some warning message might occur from aclocal"
    echo
  fi
  
  echo $_echo_n " + Running aclocal: $_echo_c"
  if [ ! -z "$XINE_CONFIG" ]; then
    echo
    echo "**Warning**: Use of XINE_CONFIG is obsolete. Set PKG_CONFIG_PATH instead."
    echo
    $XINE_CONFIG --acflags;
  fi
  aclocalinclude=`pkg-config --variable=acflags libxine`

  aclocal -I m4 $aclocalinclude
  echo "done." 
}

#--------------------
# CONFIGURE
#-------------------
run_configure () {
  rm -f config.cache
  echo " + Running 'configure $@':"
  if [ -z "$*" ]; then
    echo "   ** If you wish to pass arguments to ./configure, please"
    echo "   ** specify them on the command line."
  fi
  ./configure "$@" 
}


#---------------
# MAIN
#---------------
detect_configure_ac
detect_autoconf
detect_automake
detect_aclocal


#   help: print out usage message
#   *) run aclocal, autoheader, automake, autoconf, configure
case "$1" in
  aclocal)
    run_aclocal
    ;;
  autoheader)
    run_autoheader
    ;;
  automake)
    run_aclocal
    run_automake
    ;;
  autoconf)
    run_aclocal
    run_autoconf
    ;;
  noconfig)
    run_aclocal
    run_autoheader
    run_automake
    run_autoconf
    ;;
  *)
    run_aclocal
    run_autoheader
    run_automake
    run_autoconf
    run_configure "$@"
    ;;
esac
