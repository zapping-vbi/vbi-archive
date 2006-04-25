#!/bin/sh
# $Id: prepare_web.sh,v 1.27 2006-04-25 21:06:30 mschimek Exp $
#
# Checks our html pages out of cvs, puts the files online
# and cleans up.
#
# ssh username@shell.sourceforge.net
# cd /home/groups/z/za/zapping
# ./prepare_web.sh

(
# By default no files are world accessible.
chmod u=rwX,go-rwx . -R

# Trace execution, abort on error.
set -e -x

umask 007

cvs -z3 update -ko -dPA

chmod a+rX cgi-bin
cd cgi-bin
# Redirect from the old Twiki scripts to our current pages.
chmod a+r .htaccess
cd -

chmod a+rX htdocs
cd htdocs

# icon32.png is referenced by our entry on gnomefiles.org.
chmod a+r favicon.ico twiki.css .htaccess icon32.png
for dir in images screenshots Zapping ZVBI; do
  find $dir -name "CVS" -prune -o -exec chmod a+rX '{}' ';'
done

# Created by prepare_dox.sh, not in cvs.
test -e doc && chmod a+rX doc -R

if test -e zvbi-0.3; then
  chmod a+rX zvbi-0.3
  chmod a+r zvbi-0.3/networks.xml
  chmod a+r zvbi-0.3/networks.dtd
fi

cd -

) 2>&1 | tee prepare_web.log
