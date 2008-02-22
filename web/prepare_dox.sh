#!/bin/sh
# $Id: prepare_dox.sh,v 1.12 2008-02-22 04:18:34 mschimek Exp $
#
# This checks out a copy of the module, runs doxygen, puts
# the generated files online and cleans up.
#
# ssh username@shell.sourceforge.net
# cd /home/groups/z/za/zapping
# ./prepare_dox.sh vbi libzvbi
# ./prepare_dox.sh rte librte

(
# Trace execution, abort on error.
set -e -x

# By default all files are world read-only.
umask 006

#cvs -z3 -d:pserver:anonymous@cvs1:/cvsroot/zapping co $1
#cvs -d:pserver:anonymous@zapping.cvs.sourceforge.net:/cvsroot/zapping login
cvs -z3 -d:pserver:anonymous@zapping.cvs.sourceforge.net:/cvsroot/zapping co $1

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

rm -rf $1

) 2>&1 | tee prepare_dox.log
