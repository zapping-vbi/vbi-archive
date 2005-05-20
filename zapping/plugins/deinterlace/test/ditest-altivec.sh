#!/bin/sh

source ditest-all.sh

here=`pwd`
here=`cd $here; pwd; cd -`
trap "rm -rf $here/results-$$-*" EXIT
feature=altivec

ditest_all $feature 2>/dev/null || exit $?
cd $here/results-$$-$feature
md5sum -c $here/md5sums-$feature >/dev/null
