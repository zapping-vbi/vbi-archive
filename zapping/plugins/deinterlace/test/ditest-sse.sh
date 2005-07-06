#!/bin/sh

srcdir=`cd $(dirname $0) >/dev/null; pwd; cd - >/dev/null`
builddir=`pwd`

. $srcdir/ditest-all.sh

trap "rm -rf $builddir/results-$$-*" EXIT
feature=sse
ditest_all $feature || exit $?
cd $builddir/results-$$-$feature
md5sum -c $srcdir/md5sums-$feature >/dev/null
