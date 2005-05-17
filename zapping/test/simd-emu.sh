#!/bin/sh
program=$1
emulator=$2
feature=$3

if ./cpudt "$feature"; then
  # Excellent!  $feature is supported by our CPU.
  $program "$feature"
elif which $emulator >/dev/null && $emulator ./cpudt "$feature"; then
  # We have a CPU emulator and it supports $feature, so we can run
  # the test program on older CPUs and when cross-compiling.
  $emulator $program "$feature"
else
  # This test won't run (77 is magic).
  exit 77
fi
