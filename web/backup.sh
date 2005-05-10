#!/bin/sh
# $Id: backup.sh,v 1.10 2005-05-10 01:11:26 mschimek Exp $
#
# Back up TWiki runtime data, supposed to be executed daily.

send_backups_to=mschimek@users.sourceforge.net

# Trace execution.
set -x

# Backups are not world accessible.
umask 077 || exit 1

here=`pwd`
if test x$HOSTNAME != xlocalhost; then
  # cd /home/groups/z/za/zapping
  cd /tmp/persistent/zapping
else
  # Testing.
  cd /usr/local/apache
  send_backups_to=root@localhost
fi

today=`date "+%Y%m%d-%H%M"`

if test -d twiki.bak; then
  if test x`date +%d` = x27; then
    tar -ch twiki | bzip2 -c | \
    uuencode --base64 twiki-$today.tar.bz2 | \
    mail -s twiki-tar $send_backups_to
  else
    diff -Nadru twiki.bak twiki | bzip2 -c | \
    uuencode --base64 twiki-$today.diff.bz2 | \
    mail -s twiki-diff $send_backups_to
  fi

  rm -rf twiki.bak
fi

cp -r twiki twiki.bak
