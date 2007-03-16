# $Id$
#
# print example module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=print_lib.so
LIBS=
SER_LIBS=-L../../lib/print/  -lprint

$(NAME): ../../lib/print/libprint.so

../../lib/print/libprint.so: 
	cd ../../lib/print; $(MAKE)

include ../../Makefile.modules
