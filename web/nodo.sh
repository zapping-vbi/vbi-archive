#!/bin/sh
# $Id: nodo.sh,v 1.2 2004-04-30 02:15:46 mschimek Exp $
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

umask 666

cat <<EOF >$script
#!/bin/sh
echo Content-type: text/plain
echo
($*) 2>&1
EOF

chmod a+rx $script

wget -q --proxy=off -O - http://$HOSTNAME/$script

rm cgi-bin/nobody-temp*
