#!/bin/sh
# $Id: nodo.sh,v 1.1 2004-04-19 17:04:08 mschimek Exp $
#
# Execute a command as user nobody.
# Be careful what you do, this creates a temporary CGI script.
#
# ssh shell.sourceforge.net -l username
# cd /home/groups/z/za/zapping
# ./nodo.sh "whoami; pwd"

script=cgi-bin/nobody-temp-$$

umask 666 || exit 1

cat <<EOF >$script
#!/bin/sh
echo Content-type: text/plain
echo
($*) 2>&1
EOF

chmod a+rx $script

wget -q --proxy=off -O - http://$HOSTNAME/$script

rm cgi-bin/nobody-temp*
