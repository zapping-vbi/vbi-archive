#!/bin/sh
#$Id: prepare_web.sh,v 1.20 2004-07-03 01:39:59 mschimek Exp $
#
# Checks our html pages out of cvs, puts the files online
# and cleans up.
#
# ssh shell.sourceforge.net -l username
# cd /home/groups/z/za/zapping
# ./prepare_web.sh

(
# By default no files are world accessible.
chmod u=rwX,go-rwx . -R

# Trace execution, abort on error.
set -e -x

umask 007

cvs -z3 update -ko -dP

# For tests on my own box.
if test x$HOSTNAME != xlocalhost; then
  cvs -z3 -d:pserver:anonymous@cvs1:/cvsroot/zapping co -ko zapping/ChangeLog
  mv zapping/ChangeLog htdocs/
  chmod a+rX htdocs/ChangeLog
  rm -fR zapping
fi

chmod a+rX cgi-bin
cd cgi-bin
# Careful here, these scripts are executable by anyone.
chmod a+rx attach changes edit manage oops \
  passwd preview rdiff register rename save search statistics \
  upload view viewfile
cat <<EOF >.htaccess
SetHandler cgi-script

EOF
echo "AuthUserFile " `pwd`/../twiki/data/.htpasswd >>.htaccess
cat <<EOF >>.htaccess
AuthName 'Enter your WikiName: (First name and last name, no space, no dots, capitalized, e.g. JohnSmith). Cancel to register if you do not have one.'
AuthType Basic

ErrorDocument 401 /cgi-bin/oops/TWiki/TWikiRegistration?template=oopsauth

<Files ~ "[^/]*\.html$">
       SetHandler blabla
       allow from all
</Files>
<Files "viewauth">
       require valid-user
</Files>
<Files "edit">
       require valid-user
</Files>
<Files "preview">
       require valid-user
</Files>
<Files "save">
       require valid-user
</Files>
<Files "attach">
       require valid-user
</Files>
<Files "upload">
       require valid-user
</Files>
<Files "rename">
       require valid-user
</Files>
<Files "rdiffauth">
       require valid-user
</Files>
<Files "manage">
       require valid-user
</Files>
<Files "installpasswd">
       require valid-user
</Files>
<Files "*">
       allow from all
</Files>
EOF
chmod a+r .htaccess setlib.cfg
cd -

chmod a+rX htdocs
cd htdocs
chmod a+r *.php *.inc *.html *.jpeg *.gif *.png bookmark.ico rescd.zip .htaccess
for i in images_* screenshots style; do
  find $i -name "CVS" -prune -o -exec chmod a+rX '{}' ';'
done
# Created by prepare_dox.sh, not in cvs.
test -e doc && chmod a+rX doc -R

if test -e zvbi-0.3; then
  chmod a+rX zvbi-0.3
  chmod a+r zvbi-0.3/networks.xml
  chmod a+r zvbi-0.3/networks.dtd
fi

cd -

# Files used by TWiki cgi-bin.

chmod a+rX lib
find lib -name "CVS" -prune -o -exec chmod a+rX '{}' ';'

chmod a+rX templates
chmod a+rX templates/*.tmpl

# 'nobody' executes cgi scripts and needs write access
# to TWiki data (pages) and htdocs/pub (attachments).
# Only root can do this, but for clarity this is what we want:

if test `whoami` = "root"; then
  chown nobody.nogroup twiki -R
  chmod u+w,go-w,a+rX twiki -R
else
  chmod a+rwX twiki -R
fi

) 2>&1 | tee prepare_web.log
