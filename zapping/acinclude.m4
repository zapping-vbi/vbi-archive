dnl Checks for availability of Python to C interface lib and setups
dnl PY_LIBS, PY_LIB_LOC, PY_EXTRA_LIBS and PY_CFLAGS adequately.
dnl As a convenience, PYTHON_LIBS (covering _LIBS, _LIB_LOC and
dnl _EXTRA_LIBS) is also set.
dnl This code is adapted with minor changes from gnumeric's
dnl configure.in

AC_DEFUN([AC_PYTHON_CHECK], [
	python_val=""
dnl The name of the python executable
	python_prog="python"
	AC_CHECK_PROG(python_val, "$python_prog", true, false)
	if test ! $python_val; then
		AC_MSG_ERROR(Cannot find the python executable)
	fi
	PY_PREFIX=`$python_prog -c 'import sys ; print sys.prefix'`
	PY_EXEC_PREFIX=`$python_prog -c 'import sys ; print sys.exec_prefix'`
	changequote(<<, >>)dnl
	PY_VERSION=`$python_prog -c 'import sys ; print sys.version[0:3]'`
	changequote([, ])dnl
	if test ! -f $PY_PREFIX/include/python$PY_VERSION/Python.h; then
		AC_MSG_ERROR(Python.h not found in its standard location)
	fi
	PY_LIBS="python$PY_VERSION"
	PY_LIB_LOC="-L$PY_EXEC_PREFIX/lib/python$PY_VERSION/config"
	PY_CFLAGS="-I$PY_PREFIX/include/python$PY_VERSION"
	PY_MAKEFILE="$PY_EXEC_PREFIX/lib/python$PY_VERSION/config/Makefile"
	PY_LOCALMODLIBS=`sed -n -e 's/^LOCALMODLIBS=\(.*\)/\1/p' $PY_MAKEFILE`
	PY_BASEMODLIBS=`sed -n -e 's/^BASEMODLIBS=\(.*\)/\1/p' $PY_MAKEFILE`
	PY_OTHER_LIBS=`sed -n -e 's/^LIBS=\(.*\)/\1/p' $PY_MAKEFILE`
	PY_EXTRA_LIBS="$PY_LOCALMODLIBS $PY_BASEMODLIBS $PY_OTHER_LIBS"
	PYTHON_LIBS="$PY_LIB_LOC -l$PY_LIBS $PY_EXTRA_LIBS"
	AC_SUBST(PY_LIBS)
	AC_SUBST(PY_LIB_LOC)
	AC_SUBST(PY_CFLAGS)
	AC_SUBST(PY_EXTRA_LIBS)
	AC_SUBST(PYTHON_LIBS)
])

dnl Checks the location of the XML Catalog
dnl Usage: JH_PATH_XML_CATALOG([ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
dnl Defines XMLCATALOG and XML_CATALOG_FILE substitutions.
dnl Copied from gtk-doc.

AC_DEFUN([JH_PATH_XML_CATALOG],
[
dnl check for the presence of the XML catalog
  AC_ARG_WITH([xml-catalog],
              AC_HELP_STRING([--with-xml-catalog=CATALOG],
                             [path to xml catalog to use]),,
              [with_xml_catalog=/etc/xml/catalog])
  jh_found_xmlcatalog=true
  XML_CATALOG_FILE="$with_xml_catalog"
  AC_SUBST([XML_CATALOG_FILE])
  AC_MSG_CHECKING([for XML catalog ($XML_CATALOG_FILE)])
  if test -f "$XML_CATALOG_FILE"; then
    AC_MSG_RESULT([found])
  else
    jh_found_xmlcatalog=false
    AC_MSG_RESULT([not found])
  fi

dnl check for the xmlcatalog program
  AC_PATH_PROG(XMLCATALOG, xmlcatalog, no)
  if test "x$XMLCATALOG" = xno; then
    jh_found_xmlcatalog=false
  fi

  if $jh_found_xmlcatalog; then
    ifelse([$1],,[:],[$1])
  else
    ifelse([$2],,[AC_MSG_ERROR([could not find XML catalog])],[$2])
  fi
])

dnl Checks if a particular URI appears in the XML catalog
dnl Usage: JH_CHECK_XML_CATALOG(URI, [FRIENDLY-NAME], [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
dnl Copied from gtk-doc.

AC_DEFUN([JH_CHECK_XML_CATALOG],
[
  AC_REQUIRE([JH_PATH_XML_CATALOG],[JH_PATH_XML_CATALOG(,[:])])dnl
  AC_MSG_CHECKING([for ifelse([$2],,[$1],[$2]) in XML catalog])
  if $jh_found_xmlcatalog && \
     AC_RUN_LOG([$XMLCATALOG --noout "$XML_CATALOG_FILE" "$1" >&2]); then
    AC_MSG_RESULT([found])
    ifelse([$3],,,[$3
])dnl
  else
    AC_MSG_RESULT([not found])
    ifelse([$4],,
       [AC_MSG_ERROR([could not find ifelse([$2],,[$1],[$2]) in XML catalog])],
       [$4])
  fi
])

dnl GNOME_COMPILE_WARNINGS
dnl Turn on many useful compiler warnings
dnl For now, only works on GCC
dnl Copied from gnome-common to eliminate a Gnome CVS module dependency
dnl on distros without gnome-common package.

AC_DEFUN([GNOME_COMPILE_WARNINGS],[
    dnl ******************************
    dnl More compiler warnings
    dnl ******************************

    if test -z "$1" ; then
	default_compile_warnings=yes
    else
	default_compile_warnings="$1"
    fi

    AC_ARG_ENABLE(compile-warnings, 
    [  --enable-compile-warnings=[no/minimum/yes/maximum/error]	Turn on compiler warnings.],, [enable_compile_warnings="$default_compile_warnings"])

    warnCFLAGS=
    if test "x$GCC" != xyes; then
	enable_compile_warnings=no
    fi

    warning_flags=
    realsave_CFLAGS="$CFLAGS"

    case "$enable_compile_warnings" in
    no)
	warning_flags=
	;;
    minimum)
	warning_flags="-Wall"
	;;
    yes)
	warning_flags="-Wall -Wmissing-prototypes"
	;;
    maximum|error)
	warning_flags="-Wall -Wmissing-prototypes -Wnested-externs -Wpointer-arith"
	CFLAGS="$warning_flags $CFLAGS"
	for option in -Wno-sign-compare; do
		SAVE_CFLAGS="$CFLAGS"
		CFLAGS="$CFLAGS $option"
		AC_MSG_CHECKING([whether gcc understands $option])
		AC_TRY_COMPILE([], [],
			has_option=yes,
			has_option=no,)
		CFLAGS="$SAVE_CFLAGS"
		AC_MSG_RESULT($has_option)
		if test $has_option = yes; then
		  warning_flags="$warning_flags $option"
		fi
		unset has_option
		unset SAVE_CFLAGS
	done
	unset option
	if test "$enable_compile_warnings" = "error" ; then
	    warning_flags="$warning_flags -Werror"
	fi
	;;
    *)
	AC_MSG_ERROR(Unknown argument '$enable_compile_warnings' to --enable-compile-warnings)
	;;
    esac
    CFLAGS="$realsave_CFLAGS"
    AC_MSG_CHECKING(what warning flags to pass to the C compiler)
    AC_MSG_RESULT($warning_flags)

    AC_ARG_ENABLE(iso-c,
    [  --enable-iso-c          Try to warn if code is not ISO C ],,
    enable_iso_c=no)

    AC_MSG_CHECKING(what language compliance flags to pass to the C compiler)
    complCFLAGS=
    if test "x$enable_iso_c" != "xno"; then
	if test "x$GCC" = "xyes"; then
	case " $CFLAGS " in
	    *[\ \	]-ansi[\ \	]*) ;;
	    *) complCFLAGS="$complCFLAGS -ansi" ;;
	esac
	case " $CFLAGS " in
	    *[\ \	]-pedantic[\ \	]*) ;;
	    *) complCFLAGS="$complCFLAGS -pedantic" ;;
	esac
	fi
    fi
    AC_MSG_RESULT($complCFLAGS)

    WARN_CFLAGS="$warning_flags $complCFLAGS"
    AC_SUBST(WARN_CFLAGS)
])

dnl For C++, do basically the same thing.

AC_DEFUN([GNOME_CXX_WARNINGS],[
  AC_ARG_ENABLE(cxx-warnings, 
    [  --enable-cxx-warnings=[no/minimum/yes]	Turn on compiler warnings.],,enable_cxx_warnings=minimum)

  AC_MSG_CHECKING(what warning flags to pass to the C++ compiler)
  warnCXXFLAGS=
  if test "x$GCC" != xyes; then
    enable_compile_warnings=no
  fi
  if test "x$enable_cxx_warnings" != "xno"; then
    if test "x$GCC" = "xyes"; then
      case " $CXXFLAGS " in
      *[\ \	]-Wall[\ \	]*) ;;
      *) warnCXXFLAGS="-Wall -Wno-unused" ;;
      esac

      ## -W is not all that useful.  And it cannot be controlled
      ## with individual -Wno-xxx flags, unlike -Wall
      if test "x$enable_cxx_warnings" = "xyes"; then
	warnCXXFLAGS="$warnCXXFLAGS -Wshadow -Woverloaded-virtual"
      fi
    fi
  fi
  AC_MSG_RESULT($warnCXXFLAGS)

   AC_ARG_ENABLE(iso-cxx,
     [  --enable-iso-cxx          Try to warn if code is not ISO C++ ],,
     enable_iso_cxx=no)

   AC_MSG_CHECKING(what language compliance flags to pass to the C++ compiler)
   complCXXFLAGS=
   if test "x$enable_iso_cxx" != "xno"; then
     if test "x$GCC" = "xyes"; then
      case " $CXXFLAGS " in
      *[\ \	]-ansi[\ \	]*) ;;
      *) complCXXFLAGS="$complCXXFLAGS -ansi" ;;
      esac

      case " $CXXFLAGS " in
      *[\ \	]-pedantic[\ \	]*) ;;
      *) complCXXFLAGS="$complCXXFLAGS -pedantic" ;;
      esac
     fi
   fi
  AC_MSG_RESULT($complCXXFLAGS)

  WARN_CXXFLAGS="$CXXFLAGS $warnCXXFLAGS $complCXXFLAGS"
  AC_SUBST(WARN_CXXFLAGS)
])
