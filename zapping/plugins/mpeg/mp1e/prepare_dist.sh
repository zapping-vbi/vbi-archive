#!/bin/sh
##############################################################################
# This script is used to automate the process of generating new
# zapping releases. The release number is taken from configure.in, and
# all the new files to release are placed under the ver-release dir.
# (C) Iñaki García Etxebarria 2000-2001, under the GPL and stuff
# Adapted for mp1e/rte
#
# Modified 2001-06-01 Michael H. Schimek <mschimek@users.sf.net>
# - bzip2 (0.9.0c) -c didn't, changed to -f, added --repetitive-best
##############################################################################
## Get the version from configure.in
PACKAGE=mp1e
GREP_S="AM_INIT_AUTOMAKE($PACKAGE, "
VER=`grep $GREP_S configure.in | sed -e "s/$GREP_S//;s/)//"`
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
rpm -ta --clean $PACKAGE-$VER.tar.gz || exit 1

clear
echo "Putting everything under $VER-release"
echo "-------------------------------------" && echo
if ! [ -d $VER-release ]; then
echo "$VER-release doesn't exist, creating it" && echo
mkdir $VER-release
fi
mv /usr/src/redhat/RPMS/i386/$PACKAGE-$VER-1.* $VER-release
mv /usr/src/redhat/SRPMS/$PACKAGE-$VER-1.* $VER-release
mv $PACKAGE-$VER.tar.gz $VER-release
mv $PACKAGE-$VER.tar.bz2 $VER-release

echo "Done. Remember to commit the project to CVS if neccesary."