COMPILE = gcc -DXML2 -Wall -g `pkg-config --cflags --libs libxml-2.0`
DEPS = Makefile parser_common.h

all: parser_carsten parser_modern parser_simple
	./parser_carsten && ./parser_modern && ./parser_simple

parser_carsten: parser_carsten.c $(DEPS)
	$(COMPILE) -oparser_carsten parser_carsten.c

parser_modern: parser_modern.c $(DEPS)
	$(COMPILE) -oparser_modern parser_modern.c

parser_simple: parser_simple.c $(DEPS)
	$(COMPILE) -oparser_simple parser_simple.c
