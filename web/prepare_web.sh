#!/bin/sh
#$Id: prepare_web.sh,v 1.14 2004-04-19 17:04:08 mschimek Exp $
#
# Checks our html pages out of cvs, puts the files online
# and cleans up.
#
# ssh shell.sourceforge.net -l username
# cd /home/groups/z/za/zapping
#
# ./prepare_web.sh

# By default no files are world accessible.
chmod ug=rwX,o-rwx ./* -R || exit 1
umask 007 || exit 1

cvs -z3 update -dP

# For tests on my own box.
if test x$HOSTNAME != xlocalhost; then
  cvs -z3 -d:pserver:anonymous@cvs1:/cvsroot/zapping co zapping/ChangeLog
  mv zapping/ChangeLog htdocs/
  chmod a+rX htdocs/ChangeLog
  rm -fR zapping
fi

chmod a+rX cgi-bin
cd cgi-bin
# Careful here, these scripts are executable by anyone.
chmod a+rx printenv testenv view
chmod a+r setlib.cfg
cd -

chmod a+rX htdocs
cd htdocs
chmod a+r *.php *.inc *.html *.jpeg *.gif *.png bookmark.ico rescd.zip
for i in images_* screenshots; do
  find $i -name "CVS" -prune -o -exec chmod a+rX '{}' ';'
done
# Created by prepare_dox.sh, not in cvs.
chmod a+rX doc -R
cd -

# Files used by TWiki cgi-bin

chmod a+rX lib
find lib -name "CVS" -prune -o -exec chmod a+rX '{}' ';'

chmod a+rX templates
chmod a+rX templates/*.tmpl

# Nobody executes cgi scripts and needs write access
# to TWiki data (pages) and htdocs/pub (attachments).
# Only root or a cgi script can do this,
# but for clarity, this is what we want:

if test `whoami` = "root"; then
  chown nobody.nogroup data -R
  chmod u+w,go-w,a+rX data -R
  chown nobody.nogroup htdocs/pub -R
  chmod u+w,go-w,a+rX htdocs/pub -R
fi
