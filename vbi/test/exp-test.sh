#!/bin/bash

sliced_bz2="$1"
pgno=${2-100}

die() {
  echo $1
  exit 1
}

target_loop() {
  local option="$1"

  for target in 1 2 3 5 ; do
    bunzip2 < "$sliced_bz2" \
      | ./export -a $target $module$option $pgno -o exp-test-out-$target$option.$module \
      || die
    cmp exp-test-out-1$option*.$module exp-test-out-$target$option*.$module \
      || die "Mismatch between $module$option target 1 and $target"
    echo "$module$option target $target ok"
  done
}

if [ -z "$sliced_bz2" ] ; then
  die "Please name bzip2'ed sliced VBI file"
fi

rm -f exp-test-out-*

for module in html text vtx ; do
  target_loop
done
for module in png ppm xpm ; do
  target_loop ,aspect=0
  target_loop ,aspect=1
done

echo "Test complete"
