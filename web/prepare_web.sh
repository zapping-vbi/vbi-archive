#!/bin/sh
#$Id: prepare_web.sh,v 1.11 2004-04-17 07:54:53 mschimek Exp $
#
# Checks our html pages out of cvs, puts the files online
# and cleans up.
#
# ssh shell.sourceforge.net -l username
# cd /home/groups/z/za/zapping
# ./prepare_web.sh

for file in *; do chmod ug=rwX,o-rwx $file -R; done
umask 007
cvs -z3 update -dP

cvs -z3 -d:pserver:anonymous@cvs1:/cvsroot/zapping co zapping/ChangeLog
mv zapping/ChangeLog htdocs/
chmod a+rX htdocs/ChangeLog
rm -fR zapping

chmod a+rX cgi-bin
cd cgi-bin
chmod a+rx printenv testenv view
cd -

chmod a+rX htdocs
cd htdocs
chmod a+r *.php *.inc *.html *.jpeg *.gif *.png bookmark.ico rescd.zip
for i in images_* screenshots; do
  find $i -name "CVS" -prune -o -exec chmod a+rX '{}' ';'
done
chmod a+rX doc -R
cd -

chmod a+rX lib
find lib -name "CVS" -prune -o -exec chmod a+rX '{}' ';'
