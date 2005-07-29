#!/bin/sh

find_emulator () {
  local feature=$1

  if ./cpudt $feature; then
    # Excellent!  $feature is supported by our CPU.
    emu=
    pre=
    return 0
  else
    case "$feature" in
      altivec) emu="qemu-ppc" ;;
            *) emu="qemu-i386" ;;
    esac

    if which $emu >/dev/null && $emu ./cpudt $feature; then
      # We have a CPU emulator and it supports $feature, so we can run
      # the test program on older CPUs and when cross-compiling.
      pre=
      return 0
    else
      case "$feature" in
        altivec) pre=nosuchthing ;;
	sse3) pre=nosuchthing ;;
        *) pre=/usr/local/lib/libmmxemu.so ;;
      esac

      if test -e $pre; then
        # We can simulate unsupported mmx instructions on this CPU.
	emu=
        return 0
      else
        echo "*** Cannot run $feature tests on this machine."
        emu=false
	pre=
      fi
    fi
  fi

  return 1
}
