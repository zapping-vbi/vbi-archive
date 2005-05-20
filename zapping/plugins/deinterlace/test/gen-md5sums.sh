#!/bin/sh

source ditest-all.sh

here=`pwd`
here=`cd $here; pwd; cd -`
trap "rm -rf $here/results-$$-*" EXIT

for feature in mmx 3dnow sse sse2 altivec; do
  ditest_all $feature
  dir="$here/results-$$-$feature"
  if test -d $dir; then
    cd $dir
    echo "Generating md5sums-$feature for $implemented."
    md5sum -b $implemented >$here/md5sums-$feature || exit 1
    cd -
    rm -rf $dir
  fi
done

exit 0
