#!/bin/sh
##############################################################################
# This script is used to automate the process of generating new
# releases. The program and release number are taken from configure.in,
# and the files to release are placed under the ver-release dir.
# (C) Iñaki García Etxebarria 2000-2001, under the GPL and stuff
#
# $Id: prepare_dist.sh,v 1.15 2005-01-08 14:54:11 mschimek Exp $
#
# Usage is ./prepare_dist.sh gnome-prefix
#
# Modified 2001-06-01 Michael H. Schimek <mschimek@users.sf.net>
# - bzip2 (0.9.0c) -c didn't, changed to -f, added --repetitive-best
# Modified 2002-09-01 Michael H. Schimek <mschimek@users.sf.net>
# - added RPM_OPTIONS
# - added cvs tag hint
# Modified 2003-05-25 Michael H. Schimek <mschimek@users.sf.net>
# - added check for new rpmbuild
# Modified 2003-11-20 Michael H. Schimek <mschimek@users.sf.net>
# - configure s/--with-gnome-prefix/--prefix/
# - require GNOME_PREFIX argument
# - rewrote PACKAGE, VER grep to match AC_INIT
# Modified 2003-11-21 Michael H. Schimek <mschimek@users.sf.net>
# - correction to work with older configure.in of zvbi, rte
##############################################################################

GNOME_PREFIX=$1
if [ "$GNOME_PREFIX" = "" ]; then
  echo "Please give an install prefix, e.g. /opt/gnome"
  exit 1
fi

IFS="${IFS=         }"; save_IFS="$IFS"; IFS="(), "
set `grep 'AM_INIT_AUTOMAKE(' configure.in`
if ! (echo "$3" | grep "[0-9]" >/dev/null); then
  set `grep 'AC_INIT(' configure.in`
fi
# $1 is AM_INIT_AUTOMAKE or AC_INIT
PACKAGE=$2
VER=$3
IFS="$save_IFS"

while [ "$PACKAGE" = "" ]; do
  echo "Cannot get package name from configure.in, please enter manually:"
  read PACKAGE
done

while ! (echo "$VER" | grep "[0-9]" >/dev/null); do
  echo "Cannot get version from configure.in, please enter manually:"
  read VER
done

CVS_TAG=$PACKAGE-`echo $VER | sed s/\\\\./-/g`

clear
echo "Generating new $PACKAGE release (version $VER, $CVS_TAG)"
echo
echo "Generating the Makefiles"
echo "------------------------"
echo

if test -e ./autogen.sh; then
  NOCONFIGURE="yes" ./autogen.sh || exit 1
fi

if test ! x$GNOME_PREFIX = x; then
    ./configure --prefix=$GNOME_PREFIX || exit 1
else
    ./configure || exit 1
fi

clear
echo "Rebuilding the project to check whether it compiles"
echo "---------------------------------------------------"
echo
make clean || exit 1
make || exit 1

clear
echo "Creating the .tar.gz and .tar.bz2 distros"
echo "-----------------------------------------"
echo
make dist || exit 1

gunzip -c $PACKAGE-$VER.tar.gz >$PACKAGE-$VER.tar
bzip2 -f --repetitive-best $PACKAGE-$VER.tar

RPM_DIR=""
## RedHat
if [ -d /usr/src/redhat ]; then
    RPM_DIR="/usr/src/redhat"
## SuSE, normal user
elif [ -d ~/LnxZip/RPM ]; then
    RPM_DIR=~/LnxZip/RPM
## SuSE, root
elif [ -d /usr/src/packages ]; then
    RPM_DIR=/usr/src/packages
## Mandrake
elif [ -d /usr/src/RPM ]; then
    RPM_DIR=/usr/src/RPM
fi
## RedHat 9 builder
bob=`which rpmbuild` || bob="rpm"

if ! test "x$RPM_DIR" = "x"; then
    clear
    echo "Building the RPM"
    echo "----------------"
    echo
    cp $PACKAGE-$VER.tar.bz2 $RPM_DIR/SOURCES
    $bob $RPM_OPTIONS -ba --clean $PACKAGE.spec || exit 1
    rm $RPM_DIR/SOURCES/$PACKAGE-$VER.tar.bz2
fi

clear
echo "Putting everything under $VER-release"
echo "-------------------------------------"
echo
if ! [ -d $VER-release ]; then
    echo "$VER-release doesn't exist, creating it" && echo
    mkdir $VER-release
fi

if ! test "x$RPM_DIR" = "x"; then
    for i in $RPM_DIR/RPMS/*/$PACKAGE-$VER*; do
	if [ -f $i ]; then
	    mv $i $VER-release
	fi
    done
    mv $RPM_DIR/SRPMS/$PACKAGE-$VER-* $VER-release
else
    echo "The RPM dir couldn't be found, packages not built"
fi

mv $PACKAGE-$VER.tar.gz $VER-release
mv $PACKAGE-$VER.tar.bz2 $VER-release

echo "Done. Remember to commit the project to CVS if necessary."
echo "Release tag: cvs tag -c $CVS_TAG"

if test -e people_to_contact; then
    echo "Remember to notify the following people:"
    cat people_to_contact
fi
