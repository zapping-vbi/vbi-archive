#!/bin/bash

sliced_bz2="$1"
pgno=${2-100}

die() {
  echo $1
  exit 1
}

if [ -z "$sliced_bz2" ] ; then
  die "Please name bzip2'ed sliced VBI file"
fi

rm -f exp-test-out-*

for module in html png ppm text vtx xpm ; do
  for target in 1 2 3 5 ; do
    bunzip2 < "$sliced_bz2" \
      | ./export -a $target $module $pgno -o exp-test-out-$target.$module \
      || die
    cmp exp-test-out-1*.$module exp-test-out-$target*.$module \
      || die "Mismatch between $module target 1 and $target"
    echo "$module target $target ok"
  done
done

echo "Test complete"
