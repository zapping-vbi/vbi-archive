#!/bin/sh

. `dirname $0`/simd-emu.sh

if ! find_emulator sse2; then
  exit 77
else
  LD_PRELOAD=$pre $emu ./simd sse2
  exit $?
fi
