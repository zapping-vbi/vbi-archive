#!/bin/sh
##############################################################################
# This script is used to automate the process of generating new
# zapping releases. The release number is taken from configure.in, and
# all the new files to release are placed under the ver-release dir.
# (C) Iñaki García Etxebarria 2000-2001, under the GPL and stuff
# Adapted for mp1e
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
./autogen.sh || exit 1

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

clear && echo "Building the RPM"
	 echo "----------------" && echo
rpm -ba --clean $PACKAGE.spec || exit 1

clear
echo "Putting everything under $VER-release"
echo "-------------------------------------" && echo
if ! [ -d $VER-release ]; then
echo "$VER-release doesn't exist, creating it" && echo
mkdir $VER-release
fi
RPM_DIR=""
## redhat
if [ -d /usr/src/redhat ]; then
    RPM_DIR="/usr/src/redhat"
## SuSE, normal user
elif [ -d ~/LnxZip/RPM]; then
    RPM_DIR=~/LnxZip/RPM
## SuSE, root
elif [ -d /usr/src/packages ]; then
    RPM_DIR=/usr/src/packages
fi
if ! test "x$RPM_DIR" = "x"; then
    echo RPM_DIR is $RPM_DIR, moving packages
    mv $RPM_DIR/RPMS/i386/$PACKAGE-$VER-1.* $VER-release
    mv $RPM_DIR/SRPMS/$PACKAGE-$VER-1.* $VER-release
fi
mv $PACKAGE-$VER.tar.gz $VER-release
mv $PACKAGE-$VER.tar.bz2 $VER-release

echo "Done. Remember to commit the project to CVS if neccesary."