#!/bin/sh

srcdir=`cd $(dirname $0) >/dev/null; pwd; cd - >/dev/null`
builddir=`pwd`

. $srcdir/ditest-all.sh

trap "rm -rf $builddir/results-$$-*" EXIT

for feature in scalar mmx 3dnow sse sse2 altivec; do
  ditest_all $feature
  dir="$builddir/results-$$-$feature"
  if test -d $dir; then
    cd $dir
    echo "Generating md5sums-$feature for $implemented."
    md5sum -b $implemented >$builddir/md5sums-$feature || exit 1
    cd -
  fi
done

exit 0
