#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PACKAGE=rte
GETTEXTIZE_FLAGS="--copy --no-changelog"

(test -f $srcdir/configure.in) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level directory"
    exit 1
}

cd mp1e
echo libtoolize in mp1e/
libtoolize --force --copy
cd -

. $srcdir/m4/autogen.sh
