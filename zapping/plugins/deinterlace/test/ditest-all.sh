#!/bin/sh

source emulator.sh

methods="\
  VideoBob \
  VideoWeave \
  TwoFrame \
  Weave \
  Bob \
  ScalerBob \
  EvenOnly \
  OddOnly \
  Greedy \
  Greedy2Frame \
  GreedyH \
  TomsMoComp \
  MoComp2 \
"

size="-w 256 -h 16"

ditest_wrapper () {
  # -q quiet
  # -r use pseudo random source
  # -m method  deinterlace method
  # -c feature  test the version optimized for feature
  # -w -h source and dest image size (adjusted to be a multiple of the
  #   system page size for a built-in efence test).
  LD_PRELOAD=$pre $emu ./ditest -q -r -m $1 -c $2 $size $4 >$tmpd/$3.yuv

  case "$?" in
    0)
      if test -e $tmpd/$3.yuv; then
        implemented="$implemented $3.yuv"
      fi
      ;;
    55)
      # Have no $feature implementation of $method.
      ;;
    *)
      echo "Test of $feature implementation of $method failed."
      exit 1
      ;;
  esac

  echo -n "."
}

ditest_all () {
  feature=$1

  if ! find_emulator $feature; then
    return 77
  fi

  tmpd="results-$$-$feature"
  rm -rf $tmpd
  mkdir $tmpd || exit $?

  implemented=

  for method in VideoBob VideoWeave TwoFrame Weave Bob ScalerBob EvenOnly \
    OddOnly Greedy Greedy2Frame MoComp2 ; do
    ditest_wrapper $method $feature $method
  done

  # Runtime options - run through all possible combinations.
  for upd in 0 1; do
    for umf in 0 1; do
      for uhs in 0 1; do
        for uvs in 0 1; do
          for uib in 0 1; do
            ditest_wrapper GreedyH $feature \
	      GreedyH-$upd-$umf-$uhs-$uvs-$uib \
	      "-o GreedyUsePulldown=$upd \
               -o GreedyUseMedianFilter=$umf \
               -o GreedyUseHSharpness=$uhs \
               -o GreedyUseVSharpness=$uvs \
               -o GreedyUseInBetween=$uib \
               -o GreedyTestMode=1"
	  done
        done
      done
    done
  done

  # Runtime options - run through all possible combinations.
  for usb in 0 1; do
    for se in 0 1 2 4 6 10 12 14 16 20 22; do
      ditest_wrapper TomsMoComp $feature TomsMoComp-$usb-$se \
	"-o UseStrangeBob=$usb -o SearchEffort=$se"
    done
  done

  echo

  return 0
}
