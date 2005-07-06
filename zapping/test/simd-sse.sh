#!/bin/sh

. `dirname $0`/simd-emu.sh

if ! find_emulator sse; then
  exit 77
else
  LD_PRELOAD=$pre $emu ./simd sse
  exit $?
fi
