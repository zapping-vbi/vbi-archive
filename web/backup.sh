#!/bin/sh
# $Id: backup.sh,v 1.3 2004-05-02 04:18:51 mschimek Exp $
#
# Back up TWiki runtime data, supposed to be executed daily.

send_backups_to=mschimek@users.sourceforge.net

# Trace execution.
set -x

# Backups are not world accessible.
umask 007 || exit 1

here=`pwd`
if test x$HOSTNAME != xlocalhost; then
  cd /home/groups/z/za/zapping
else
  # Testing.
  cd /usr/local/apache
  send_backups_to=root@localhost
fi

today=`date "+%Y%m%d-%H%M"`

if test x`date +%d` = x27; then
  tar -ch twiki | bzip2 -c >$here/twiki-$today.tar.bz2
# Crap! no metasend at sf.
#  metasend -b -s "twiki-tar" -f $here/twiki-$today.tar.bz2 \
#    -m application/x-bzip -S 500000 -t $send_backups_to -z
  cat twiki-$today.tar.bz2 | uuencode --base64 twiki-$today.tar.bz2 | \
    mail -s twiki-tar $send_backups_to
else
  diff -Nadru twiki.bak twiki >$here/twiki-$today.diff
  if test -s $here/twiki-$today.diff ; then
    bzip2 $here/twiki-$today.diff
#    metasend -b -s "twiki-diff" -f $here/twiki-$today.diff.bz2 \
#      -m application/x-bzip -S 500000 -t $send_backups_to -z
    cat twiki-$today.diff.bz2 | uuencode --base64 twiki-$today.diff.bz2 | \
      mail -s twiki-diff $send_backups_to
  fi
fi

rm -rf twiki.bak
cp -r twiki twiki.bak
