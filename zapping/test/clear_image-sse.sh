#!/bin/sh

source `dirname $0`/simd-emu.sh

if ! find_emulator sse; then
  exit 77
else
  LD_PRELOAD=$pre $emu ./clear_image sse
  exit $?
fi
