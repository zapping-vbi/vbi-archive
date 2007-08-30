#! /bin/bash
# autogen.sh wrapper for the author/maintainer of this package.
# Everyone else should run autogen.sh instead.

extra_pkg_config_path=""

case `whoami` in
  root)
    echo "Bad boy! Drop out of the root account and try again."
    exit 1
    ;;

  michael)
    # Build system is x86_64 but default host is x86.
    host=${host-"i686-pc-linux-gnu"}
    configure_opts=${configure_opts-"--host=$host"}

    # Regenerate all files. May require Perl, XML tools, Internet
    # access, dead cat, magic spells, ...
    autogen_opts=${autogen_opts-"\
      --enable-maintainer-mode --enable-rebuild-man"}

    if echo "$CFLAGS" | grep -q -e '-O[s1-3]' ; then
      # Libs built with -march=k8 -Os for speed.
      export GNOME2_PATH=${GNOME2_PATH-"/opt/$host/gnome-2.18-k8-os"}
      export X11_PATH=${X11_PATH-"/opt/$host/xorg-7.1-k8-os"}
    else
      # Libs built with -g -O0 for debugging.
      export GNOME2_PATH=${GNOME2_PATH-"/opt/$host/gnome-2.18-g"}
      export X11_PATH=${X11_PATH-"/opt/$host/xorg-7.1-g"}
    fi

    if echo "$host" | grep -E -q -e '^(x86_64|powerpc)-' ; then
      PATH="/usr/bin/$host:/opt/$host/python-2.5-k8-os/bin:$PATH"
      extra_pkg_config_path="/usr/lib/$host/pkgconfig"
    else
      extra_pkg_config_path="/usr/local/lib/pkgconfig"
    fi

    make_opts=${make_opts-"-j3"}

    ;;

  *)
    echo "Who are you? Run autogen.sh instead."
    exit 1
    ;;
esac

PATH=`echo "$PATH:" | sed 's%[^:]*\(gnome\|xorg\|X11\)[^:]*/bin:%%g;s%:*$%%'`
export PATH="$X11_PATH/bin:$GNOME2_PATH/bin:$PATH"

export PKG_CONFIG_PATH=${PKG_CONFIG_PATH-"\
$X11_PATH/lib/pkgconfig:$GNOME2_PATH/lib/pkgconfig:$extra_pkg_config_path"}

export ACLOCAL_FLAGS="\
  -I $X11_PATH/share/aclocal \
  -I $GNOME2_PATH/share/aclocal"

#prefix=$GNOME2_PATH
prefix=`pwd`/inst-`date '+%Y%m%dT%H%M%S'`

# Default is /etc/gconf. $HOME/.gconf is also in the default search
# path but for our tests let's leave those alone.
gconf_source="xml::$prefix/.gconf"

# Default compiler.
export CC=${CC-"gcc -V4.1.2"}

if $CC -v 2>&1 | grep -q -e '^tcc version' ; then
  # If preprocessor output is required use GCC.
  autogen_opts="$autogen_opts CPP=cpp"

  # Optimizations.
  if ! echo "$CFLAGS" | grep -q -e '-O[s1-3]'; then
    CFLAGS="$CFLAGS -g -b -bt 10"
  fi

  # Warnings.
  CFLAGS="$CFLAGS -Wimplicit-function-declaration"
                                        # func use before declaration
  CFLAGS="$CFLAGS -Wunsupported"        # unsupported GCC features
  CFLAGS="$CFLAGS -Wwrite-strings"	# char *foo = "blah";

elif $CC -v 2>&1 | grep -q -e '^gcc version [3-9]\.' ; then
  # Optimizations.
  if echo "$CFLAGS" | grep -q -e '-O[s1-3]'; then
    CFLAGS="$CFLAGS -fomit-frame-pointer -pipe"
  else
    CFLAGS="$CFLAGS -O0 -g -pipe"
  fi

  # Warnings.
  CFLAGS="$CFLAGS -Wchar-subscripts"	# array subscript has char type
  CFLAGS="$CFLAGS -Wcomment"		# nested comments
  CFLAGS="$CFLAGS -Wformat"		# printf format args mismatch
  CFLAGS="$CFLAGS -Wformat-y2k"		# two-digit year strftime format
  CFLAGS="$CFLAGS -Wformat-nonliteral"	# printf format cannot be checked
  CFLAGS="$CFLAGS -Wformat-security"	# printf (var); where user may
                                        # supply var
  CFLAGS="$CFLAGS -Wnonnull"		# function __attribute__ says
                                        # argument must be non-NULL
  CFLAGS="$CFLAGS -Wimplicit-int"	# func decl without a return type
  CFLAGS="$CFLAGS -Wimplicit-function-declaration"
                                        # func use before declaration
  CFLAGS="$CFLAGS -Wmain"		# wrong main() return type or args
  CFLAGS="$CFLAGS -Wmissing-braces"	# int a[2][2] = { 0, 1, 2, 3 };
  CFLAGS="$CFLAGS -Wparentheses"	# if if else, or sth like x <= y <=z
  CFLAGS="$CFLAGS -Wsequence-point"	# a = a++;
  CFLAGS="$CFLAGS -Wreturn-type"	# void return in non-void function
  CFLAGS="$CFLAGS -Wswitch"		# missing case in enum switch
  CFLAGS="$CFLAGS -Wtrigraphs"		# suspicious ??x
  CFLAGS="$CFLAGS -Wunused-function"	# defined static but not used
  CFLAGS="$CFLAGS -Wunused-label"
  CFLAGS="$CFLAGS -Wunused-parameter"
  CFLAGS="$CFLAGS -Wunused-variable"	# declared but not used
  CFLAGS="$CFLAGS -Wunused-value"	# return x, 0;
  CFLAGS="$CFLAGS -Wunknown-pragmas"    # #pragma whatever
  CFLAGS="$CFLAGS -Wfloat-equal"	# x == 1.000
  CFLAGS="$CFLAGS -Wundef"		# #undef X, #if X == 3
  CFLAGS="$CFLAGS -Wendif-labels"	# #endif BLAH
  #CFLAGS="$CFLAGS -Wshadow"		# int foo; bar () { int foo; ... }
  CFLAGS="$CFLAGS -Wpointer-arith"	# void *p = &x; ++p;
  CFLAGS="$CFLAGS -Wbad-function-cast"
  CFLAGS="$CFLAGS -Wcast-qual"		# const int * -> int *
  CFLAGS="$CFLAGS -Wcast-align"		# char * -> int *
  CFLAGS="$CFLAGS -Wwrite-strings"	# char *foo = "blah";
  CFLAGS="$CFLAGS -Wsign-compare"	# int foo; unsigned bar; foo < bar
  #CFLAGS="$CFLAGS -Wconversion"	# proto causes implicit type conversion
  CFLAGS="$CFLAGS -Wmissing-prototypes"	# global func not declared in header
  CFLAGS="$CFLAGS -Wmissing-declarations"
                                        # global func not declared in header
  CFLAGS="$CFLAGS -Wpacked"		# __attribute__((packed)) may hurt
  #CFLAGS="$CFLAGS -Wpadded"		# struct fields were padded, different
                                        # order may save space
  CFLAGS="$CFLAGS -Wnested-externs"	# extern declaration in function;
                                        # use header files, stupid!
  CFLAGS="$CFLAGS -Winline"		# inline function cannot be inlined
  CFLAGS="$CFLAGS -Wall -W"		# other useful warnings
  
  if ! echo "$CFLAGS" | grep -q -e '-O0' ; then
    CFLAGS="$CFLAGS -Wuninitialized"	# int i; return i;
    CFLAGS="$CFLAGS -Wstrict-aliasing"	# code may break C rules used for
                                        # optimization
  fi

  if $CC --version | grep -q -e '(GCC) [4-9]\.' ; then
    CFLAGS="$CFLAGS -Winit-self"	# int i = i;
    CFLAGS="$CFLAGS -Wdeclaration-after-statement"
                                        # int i; i = 1; int j;
    CFLAGS="$CFLAGS -Wmissing-include-dirs"
    CFLAGS="$CFLAGS -Wmissing-field-initializers"
                                        # int a[2][2] = { 0, 1 };
    if ! (echo "$CFLAGS" | grep -q -e '-O0') ; then
      CFLAGS="$CFLAGS -Wstrict-aliasing=2"
    fi
  fi

fi # GCC >= 3.x

export CFLAGS

# In autogen.sh force automake to copy missing files instead of linking
# them, for NFS access.
export am_opt="--copy"

# In autogen.sh use the newest versions, not the oldest useable ones.
# This code will go into tarballs. Build tests may override this.
export REQUIRED_AUTOCONF_VERSION=${REQUIRED_AUTOCONF_VERSION:-2.61}
export REQUIRED_AUTOMAKE_VERSION=${REQUIRED_AUTOMAKE_VERSION:-1.10}

configure_opts="`echo $@ | sed 's/--build-test//'` $configure_opts"

if [ -x ./autogen.sh ]; then
  # Build from CVS sources.
  ./autogen.sh \
    $configure_opts \
    $autogen_opts \
    PATH="$PATH" \
    CC="$CC" \
    CFLAGS="$CFLAGS" \
    --prefix="$prefix" \
    --with-gconf-source="$gconf_source" || exit 1
    # --localstatedir=$GNOME2_PATH/var
elif [ -x ./configure ]; then
  # Build from tarball.
  ./configure \
    $configure_opts \
    PATH="$PATH" \
    CC="$CC" \
    CFLAGS="$CFLAGS" \
    --prefix="$prefix" \
    --with-gconf-source="$gconf_source" || exit 1
else
  # VPATH build.
  /home/src/zapping/zapping/configure \
    $configure_opts \
    PATH="$PATH" \
    CC="$CC" \
    CFLAGS="$CFLAGS" \
    --prefix="$prefix" \
    --with-gconf-source="$gconf_source" || exit 1
fi

if echo "$@" | grep -q -e --build-test ; then
  # Cross build test with our $PATH, $PKG_CONFIG_PATH and $configure_opts.
  make $make_opts || exit 1
  make $make_opts check || exit 1
  gconf_source="xml::`pwd`/.gconf"
  mkdir -p "$gconf_source" || exit 1
  make $make_opts DISTCHECK_CONFIGURE_FLAGS="\
    $configure_opts \
    --with-gconf-source=\"$gconf_source\" \
    " distcheck
fi
