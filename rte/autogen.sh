#!/bin/sh

NORECURSIVE=true
NOCONFIGURE=true

aclf="$ACLOCAL_FLAGS"

topdir=`pwd`
srcdir=`dirname $0`/mp1e
test -z "$srcdir" && srcdir=./mp1e

(test -f $srcdir/configure.in) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level directory"
    exit 1
}

PACKAGE=mp1e
ACLOCAL_FLAGS="$aclf -I $topdir/m4"

. $srcdir/../m4/autogen.sh

# Conditional dist magic. Don't tell the purity police.
# And don't automake in mp1e/, re-run this script instead.
# Dist targets created from cvs source don't need this.
(cd mp1e; for file in `find -iname Makefile.in -print`; do
sed -e :a -e '/\\$/N; s/\\\n//; ta' $file >$file.mp1e; done)

NOCONFIGURE=

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PACKAGE=rte
ACLOCAL_FLAGS="$aclf -I $topdir/m4"

. $srcdir/m4/autogen.sh

(cd mp1e; for file in `find -iname Makefile.in -print`; do
sed -e :a -e '/\\$/N; s/\\\n//; ta' $file >$file.rte;
diff -d --old-line-format='@BACKEND_MP1E_FALSE@%l
' --new-line-format='@BACKEND_MP1E_TRUE@%l
' --unchanged-line-format='%l
' $file.mp1e $file.rte >$file; rm $file.mp1e $file.rte; done)

# Um?
cp ltmain.sh mp1e/
