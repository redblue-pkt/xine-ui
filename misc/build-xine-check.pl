#!/usr/bin/perl
#
# build-xine-check.pl - build xine-check from .templ and message files
# 
# usage: build-xine-check.pl <message file> #FIXME: support several msgfiles

$templ="xine-check.sh.in";
$out="xine-check";

(open TEMPLATE, $templ) || die "unable to open input file";
(open OUT, ">$out") || die "unable to open output file";

$insert='cat <<EOF >$tmpdir/locale.en'."\n";
while (<>) {
  s/\$/\\\$/g;
  $insert .= $_;
}
$insert .= "EOF\n";

while (<TEMPLATE>) {
  if (/^\#\# auto-insert-language-files-here \#\#/) {
    print OUT $insert;
  }
  else {
    print OUT $_;
  }
}
