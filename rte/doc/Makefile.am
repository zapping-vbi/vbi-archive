## Process this file with automake to produce Makefile.in

all-local: html-build.stamp

html-build.stamp: Doxyfile
if HAVE_DOXYGEN
	doxygen Doxyfile
endif
	touch html-build.stamp

clean-local:
	rm -rf html html-build.stamp
