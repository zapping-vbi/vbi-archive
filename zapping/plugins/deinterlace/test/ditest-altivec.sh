#!/bin/sh

srcdir=`cd $(dirname $0); pwd; cd -`
builddir=`pwd`

source $srcdir/ditest-all.sh

trap "rm -rf $builddir/results-$$-*" EXIT
feature=altivec
ditest_all $feature || exit $?
cd $builddir/results-$$-$feature
md5sum -c $srcdir/md5sums-$feature >/dev/null
