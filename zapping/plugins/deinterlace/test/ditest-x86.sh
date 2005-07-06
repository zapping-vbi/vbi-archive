#!/bin/sh

srcdir=`cd $(dirname $0) >/dev/null; pwd; cd - >/dev/null`
builddir=`pwd`

. $srcdir/ditest-all.sh

trap "rm -rf $builddir/results-$$-*" EXIT

compare () {
  local method=$1
  local feature=$2
  local special=$3

  echo "Comparing $method $feature implementation against sse results."
  if ! ./dicmp $size $special results-$$-sse/$method.yuv \
    results-$$-$feature/$method.yuv; then
    echo "*** Mismatch in $method $feature and sse results."
#    exit 1
  fi
}

compare_greedyh () {
  local feature=$1
  local special=$2

  for upd in 0 1; do
    for umf in 0 1; do
      for uhs in 0 1; do
        for uvs in 0 1; do
          for uib in 0 1; do
            local method="GreedyH-$upd-$umf-$uhs-$uvs-$uib"
	  echo "Comparing $method $feature implementation against sse results."
	    if ! ./dicmp $size $special results-$$-sse/$method.yuv \
	      results-$$-$feature/$method.yuv; then
	      echo "*** Mismatch in $method $feature and sse results."
#	      exit 1
	    fi
	  done
	done
      done
    done
  done
}

compare_tomsmocomp () {
  local feature=$1
  local special=$2

  for usb in 0 1; do
    for se in 0 1 2 4 6 10 12 14 16 20 22; do
      local method="TomsMoComp-$usb-$se"
      echo "Comparing $method $feature implementation against sse results."
      if ! ./dicmp $size $special results-$$-sse/$method.yuv \
	results-$$-$feature/$method.yuv; then
	echo "*** Mismatch in $method $feature and sse results."
#	exit 1
      fi
    done
  done

  # TODO each $se should produce different results.
}

for feature in scalar mmx 3dnow sse sse2; do
  echo "Testing $feature implementations"
  ditest_all $feature
done

if test -d results-$$-sse; then
  if test -d results-$$-scalar; then
    for method in Weave Bob ScalerBob EvenOnly OddOnly; do
      compare $method scalar
    done
  fi

  if test -d results-$$-mmx; then
    for method in Weave Bob ScalerBob EvenOnly OddOnly; do
      compare $method mmx
    done

    # Ignore fast_vavgu8() rounding error
    for method in VideoBob VideoWeave TwoFrame; do
      compare $method mmx -d1
    done

    # Argh! The mmx implementation of
    # Greedy2Frame GreedyH TomsMoComp MoComp2
    # differs from 3dnow sse sse2 versions for performance reasons.
  else
    echo "*** Have no mmx results to compare against sse reference."
  fi

  if test -d results-$$-3dnow; then
    for method in VideoBob VideoWeave TwoFrame Bob Greedy Greedy2Frame ; do
      compare $method 3dnow
    done

    compare MoComp2 3dnow
    compare_greedyh 3dnow

    # TODO if possible
    # compare_tomsmocomp 3dnow

    # Weave ScalerBob EvenOnly OddOnly not implemented in 3dnow.
  else
    echo "*** Have no 3dnow results to compare against sse reference."
  fi

  if test -d results-$$-sse2; then
    for method in VideoBob VideoWeave TwoFrame Weave Bob ScalerBob EvenOnly \
      OddOnly Greedy Greedy2Frame; do
      compare $method sse2
    done

    # TODO if possible
    # compare_greedyh sse2

    # Ignore error in first and last column (bytes 8 ... 15 and
    # n-16 ... n-9 of each line) due to vector size.
    compare MoComp2 sse2 -l
    compare_tomsmocomp sse2 -l
  else
    echo "*** Have no sse2 results to compare against sse reference."
  fi
else
  echo "*** Have no sse reference results to compare x86 implementations."
fi

exit 0
