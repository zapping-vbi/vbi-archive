#!/bin/sh
#$Id: prepare_web.sh,v 1.7 2004-04-17 05:53:24 mschimek Exp $
#
# Checks our html pages out of cvs, puts the files online
# and cleans up.
#
# ssh shell.sourceforge.net -l username
# cd /home/groups/z/za/zapping
# ./prepare_web.sh

chmod ug=rwX,o-rwx . -R
umask 007
cvs -z3 update -dP
cvs -z3 -d:pserver:anonymous@cvs1:/cvsroot/zapping co zapping/ChangeLog
mv zapping/ChangeLog htdocs/
chmod a+rX htdocs/ChangeLog
rm -fR zapping
#chmod a+rX . htdocs
chmod a+rX htdocs
cd htdocs
chmod a+r *.php *.inc *.html *.jpeg *.gif *.png bookmark.ico rescd.zip
for i in images_* screenshots; do
  find $i -name "CVS" -prune -o -exec chmod a+rX '{}' ';'
done
#chmod a+rX doc -R
cd -
