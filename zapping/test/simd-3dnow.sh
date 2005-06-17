#!/bin/sh

source `dirname $0`/simd-emu.sh

if ! find_emulator 3dnow; then
  exit 77
else
  LD_PRELOAD=$pre $emu ./simd 3dnow
  exit $?
fi
