#!/bin/sh
# $Id: prepare_dox.sh,v 1.13 2013-07-10 04:16:56 mschimek Exp $
#
# Arguments: svn_module subdir
#
# This checks out a copy of the module, runs doxygen, puts
# the generated files online under subdir and cleans up.
# See the README file for usage instructions.

(
# Trace execution, abort on error.
set -e -x

# By default all files are world read-only.
umask 006

cd /home/project-web/zapping

svn checkout https://svn.code.sf.net/p/zapping/svn/trunk/$1 $1

# Generate documentation.
cd $1/doc
test -e Doxyfile || exit 1
doxygen
cd -

# FIXME doesn't work.
#test -e cgi-bin || mkdir cgi-bin
#test -e cgi-bin/doxysearch || cp /usr/bin/doxysearch cgi-bin/
#cp $1/doc/html/*search.cgi cgi-bin/

# Put docs under zapping.sf.net/doc/$2
test -e htdocs/doc || mkdir htdocs/doc

rm -rf htdocs/doc/$2
cp -r $1/doc/html htdocs/doc/$2
chmod a-x htdocs/doc/$2/*
chmod a+rX htdocs/doc -R

# Generate search index.
#cd htdocs/doc/$2
#doxytag -s search.idx
#cd -

# Delete the checked out tree.
case "$1" in
zvbi|rte) rm -rf $1 ;;
esac

) 2>&1 | tee prepare_dox.log
