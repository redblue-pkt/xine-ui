#!/bin/sh
#
# shlibdeps.sh - script to calculate depends/recommends/suggests for shlibs
#
# usage: debian/shlibdeps.sh <packagename>
#
# (C) 2001 Siggi Langauf <siggi@debian.org>

installdir=debian/$1

OPTIONAL=''

RECOMMENDED="$installdir/usr/bin/aaxine"

#start with all executables and shared objects
REQUIRED=`find $installdir -type f \( -name \*.so -o -perm +111 \)`


#remove all OPTIONAL or RECOMMENDED stuff
for file in `echo $OPTIONAL $RECOMMENDED`; do
    REQUIRED=`echo "$REQUIRED" | grep -v $file`
done

for file in $RECOMMENDED; do
    if test ! -f "$file"; then
	echo "WARNING: non-existing file \"$file\" in RECOMMENDED list"
	RECOMMENDED=`echo "$var" | grep -v $file`
    fi
done

dpkg-shlibdeps -Tdebian/$1.substvars \
               $REQUIRED -dRecommends $RECOMMENDED -dSuggests $OPTIONAL
