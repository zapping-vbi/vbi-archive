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
dnl We have found the libs et al but we still need them to be usable.
	AC_MSG_CHECKING(whether we can build a shared library depending on libpython)
	rm -rf testpython
	mkdir testpython
	cd testpython
	cat > testpython.c <<EOF
#include <Python.h>
int testpython (void)
{
Py_Exit (0);
}
EOF
	if /bin/sh ../libtool --mode=compile ${CC} $PY_CFLAGS -c testpython.c >/dev/null 2>&1 && \
	   /bin/sh ../libtool --mode=link ${CC} -o testpython.la -rpath `pwd` -module -avoid-version $PY_LIB_LOC testpython.lo -l$PY_LIBS $PY_EXTRA_LIBS >/dev/null 2>&1 && \
	   grep 'dlname.*testpython' testpython.la >/dev/null 2>&1; then
		AC_MSG_RESULT(yes)
		rm -rf testpython
	else
		AC_MSG_RESULT(no)
		rm -rf testpython
		AC_MSG_ERROR(Cannot build shared libraries using libptyhon)
	fi
	cd ..
])