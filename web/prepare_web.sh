#!/bin/sh
# $Id: prepare_web.sh,v 1.29 2013-07-10 04:17:00 mschimek Exp $
#
# Updates our HTML pages from our web CVS module, puts the files
# online and cleans up. See the README file for usage instructions.

(
# By default no files are world accessible.
chmod u=rwX,go-rwx . -R

# Trace execution, abort on error.
set -e -x

umask 007

cd /home/project-web/zapping

cvs update -ko -dPA

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

# The master version of the networks table in libzvbi 0.2 and 0.3,
# published here for a runtime update of the library, although
# presently we do not ship any such code.
if test -e zvbi-0.3; then
  chmod a+rX zvbi-0.3
  chmod a+r zvbi-0.3/networks.xml
  chmod a+r zvbi-0.3/networks.dtd
fi

cd - # htdocs done

) 2>&1 | tee prepare_web.log
