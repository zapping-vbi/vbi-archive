#!/bin/sh
#$Id: prepare_dox.sh,v 1.3 2002-10-04 16:38:54 mschimek Exp $
# prepare_dox.sh vbi libzvbi
# prepare_dox.sh rte librte

umask 002
cvs -z3 -d:pserver:anonymous@cvs.zapping.sourceforge.net:/cvsroot/zapping co $1
cd $1/doc
test -e Doxyfile || exit 1
doxygen
cd -
test -e cgi-bin || mkdir cgi-bin
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
