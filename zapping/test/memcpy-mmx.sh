#!/bin/sh

source `dirname $0`/simd-emu.sh

if ! find_emulator mmx; then
  exit 77
else
  LD_PRELOAD=$pre $emu ./memcpy mmx
  exit $?
fi
