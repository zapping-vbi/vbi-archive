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
