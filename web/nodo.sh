#!/bin/sh
# $Id: nodo.sh,v 1.4 2004-05-02 02:32:27 mschimek Exp $
#
# Execute a command as user nobody.
# Be careful what you do, this creates a temporary CGI script.
#
# ssh shell.sourceforge.net -l username
# cd /home/groups/z/za/zapping
# ./nodo.sh "whoami; pwd"

# Trace execution.
set -x

script=cgi-bin/nobody-temp-$$

umask 077 || exit 1

# cat <<here not possible
(
echo '#!/bin/sh'
echo 'echo Content-type: text/plain'
echo 'echo'
echo '(' $* ') 2>&1'
) >$script
chmod a+rx $script

while ! wget -q --proxy=off -O - http://zapping.sourceforge.net/$script; do true; done

rm cgi-bin/nobody-temp*
