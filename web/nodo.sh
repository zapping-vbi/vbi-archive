#!/bin/sh
# $Id: nodo.sh,v 1.6 2004-05-02 02:58:57 mschimek Exp $
#
# Execute a command as user nobody.
# Be careful what you do, this creates a temporary CGI script.
#
# ssh shell.sourceforge.net -l username
# cd /home/groups/z/za/zapping
# ./nodo.sh "whoami; pwd"

# Trace execution, abort on error.
set -e -x

script=cgi-bin/nobody-temp-$$

umask 077

# cat <<here not possible
echo '#!/bin/sh' >$script
echo 'echo Content-type: text/plain' >>$script
echo 'echo' >>$script
echo '(' $* ') 2>&1' >>$script

chmod a+rx $script

wget -q --proxy=off -O - http://zapping.sourceforge.net/$script

rm cgi-bin/nobody-temp*
