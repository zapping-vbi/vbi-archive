#!/bin/sh
##############################################################################
# This script is used to automate the process of generating new
# zapping releases. The release number is taken from configure.in, and
# all the new files to release are placed under the ver-release dir.
# (C) Iñaki García Etxebarria 2000-2001, under the GPL and stuff
##############################################################################
## Get the version from configure.in
PACKAGE=zapping
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
gunzip -c $PACKAGE-$VER.tar.gz | bzip2 -c > $PACKAGE-$VER.tar.bz2

clear && echo "Building the RPM"
	 echo "----------------" && echo
rpm -ta --clean $PACKAGE-$VER.tar.gz || exit 1

clear
echo "Putting everything under releases/$VER"
echo "----------------------------------------" && echo
if ! [ -d releases ]; then
echo "releases/ doesn't exist, creating it" && echo
mkdir releases
fi
if ! [ -d releases/$VER ]; then
echo "releases/$VER doesn't exist, creating it" && echo
mkdir releases/$VER
fi
mv /usr/src/redhat/RPMS/i386/$PACKAGE-$VER-1.* releases/$VER
mv /usr/src/redhat/SRPMS/$PACKAGE-$VER-1.* releases/$VER
mv $PACKAGE-$VER.tar.gz releases/$VER
mv $PACKAGE-$VER.tar.bz2 releases/$VER

echo "Done. Remember to commit the project to CVS if neccesary."