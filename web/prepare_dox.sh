#!/bin/sh
# $Id: prepare_dox.sh,v 1.4 2003-10-20 23:50:52 mschimek Exp $
#
# This checks out a copy of the module, runs doxygen, puts
# the generated files online and cleans up.
#
# ssh shell.sourceforge.net -l username
# cd /home/groups/z/za/zapping
# ./prepare_dox.sh vbi libzvbi
# ./prepare_dox.sh rte librte

umask 002
cvs -z3 -d:pserver:anonymous@cvs1:/cvsroot/zapping co $1
cd $1/doc
test -e Doxyfile || exit 1
doxygen
cd -
# FIXME doesnt work.
#test -e cgi-bin || mkdir cgi-bin
#test -e cgi-bin/doxysearch || cp /usr/bin/doxysearch cgi-bin/
#cp $1/doc/html/*search.cgi cgi-bin/
test -e htdocs/doc || mkdir htdocs/doc
rm -rf htdocs/doc/$2
cp -r $1/doc/html htdocs/doc/$2
chmod a-x htdocs/doc/$2/*
cd htdocs/doc/$2
#doxytag -s search.idx
cd -
rm -rf $1
