#!/bin/sh
#$Id: prepare_web.sh,v 1.1 2002-03-16 16:29:28 mschimek Exp $

cd /home/groups/z/za/zapping
chmod ug=rwX,o-rwx . -R
umask 007
cvs -z3 update -dP
cvs -z3 -d:pserver:anonymous@cvs.zapping.sourceforge.net:/cvsroot/zapping co zapping/ChangeLog
mv zapping/ChangeLog htdocs/
rm -dfR zapping
chmod a+rX . htdocs cgi-bin
cd htdocs
chmod a+r *.php *.inc *.html *.jpeg *.gif *.png bookmark.ico rescd.zip
for i in images_* screenshots; do
  find $i -name "CVS" -prune -o -exec chmod a+rX '{}' ';'
done
cd -
