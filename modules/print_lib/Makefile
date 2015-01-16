#
# print_lib example module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=print_lib.so
LIBS=
SERLIBPATH=../../lib
# ser libraries, in the format path/shortname , where shortname is
# what will be used for -l
SER_LIBS=$(SERLIBPATH)/print/print


DEFS+=-DSER_MOD_INTERFACE

include ../../Makefile.modules
