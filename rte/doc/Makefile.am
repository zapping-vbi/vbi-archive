## Process this file with automake to produce Makefile.in

if HAVE_DOXYGEN

all-local: html-build.stamp

html-build.stamp: Doxyfile
	doxygen Doxyfile
	touch html-build.stamp

clean-local:
	rm -rf html html-build.stamp

endif
