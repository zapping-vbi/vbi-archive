#!/bin/sh
##############################################################################
# This script is used to automate the process of generating new
# releases. The program and release number are taken from configure.in,
# and the files to release are placed under the ver-release dir.
# (C) Iñaki García Etxebarria 2000-2001, under the GPL and stuff
#
# Usage is ./prepare_dist.sh [gnome_prefix]
#
# Modified 2001-06-01 Michael H. Schimek <mschimek@users.sf.net>
# - bzip2 (0.9.0c) -c didn't, changed to -f, added --repetitive-best
##############################################################################
## Get the package name and version from configure.in
PACKAGE_VER=`grep 'AM_INIT_AUTOMAKE(' configure.in`
PACKAGE_VER=`echo $PACKAGE_VER | sed s/AM_INIT_AUTOMAKE\(\ *//`
PACKAGE_VER=`echo $PACKAGE_VER | sed s/\ *\)//`
PACKAGE=`echo $PACKAGE_VER | sed s/\ *,.*//`
VER=`echo $PACKAGE_VER | sed s/$PACKAGE\ *,\ *//`
if [ $PACKAGE = "" ]; then
echo "Cannot get package name from configure.in, please enter manually:"
read PACKAGE
fi
if [ $VER = "" ]; then
echo "Cannot get version from configure.in, please enter manually:"
read VER
fi
clear
echo "Generating new $PACKAGE release (version $VER)"
echo
echo "Generating the Makefiles"
echo "------------------------" && echo
(NOCONFIGURE="yes" && ./autogen.sh) || exit 1
if test ! x$1 = x; then
    ./configure --with-gnome-prefix=$1 || exit 1
else
    ./configure || exit 1
fi

clear
echo "Rebuilding the project to check whether it compiles"
echo "---------------------------------------------------"
echo
make clean || exit 1
make || exit 1
clear && echo "Creating the .tar.gz and .tar.bz2 distros"
	 echo "-----------------------------------------" && echo
make dist || exit 1
gunzip -c $PACKAGE-$VER.tar.gz >$PACKAGE-$VER.tar
bzip2 -f --repetitive-best $PACKAGE-$VER.tar

RPM_DIR=""
## redhat
if [ -d /usr/src/redhat ]; then
    RPM_DIR="/usr/src/redhat"
## SuSE, normal user
elif [ -d ~/LnxZip/RPM ]; then
    RPM_DIR=~/LnxZip/RPM
## SuSE, root
elif [ -d /usr/src/packages ]; then
    RPM_DIR=/usr/src/packages
## Mandrake
elif [ -d /usr/src/RPM]; then
    RPM_DIR=/usr/src/RPM
fi
if ! test "x$RPM_DIR" = "x"; then
    clear && echo "Building the RPM"
	     echo "----------------" && echo
    cp $PACKAGE-$VER.tar.bz2 $RPM_DIR/SOURCES
    rpm -ba --clean $PACKAGE.spec || exit 1
    rm $RPM_DIR/SOURCES/$PACKAGE-$VER.tar.bz2
fi

clear
echo "Putting everything under $VER-release"
echo "-------------------------------------" && echo
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

echo "Done. Remember to commit the project to CVS if neccesary."
if test -e people_to_contact; then
    echo "Remember to notify the following people:"
    cat people_to_contact
fi
