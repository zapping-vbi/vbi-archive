#!/bin/sh

. `dirname $0`/simd-emu.sh

if ! find_emulator sse; then
  exit 77
else
  LD_PRELOAD=$pre $emu ./copy_image sse
  exit $?
fi
