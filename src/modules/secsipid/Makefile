#
# secsipid module makefile
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=secsipid.so

DEFS+= -I.
LIBS+= -L. -lsecsipid

ifeq ($(OS), darwin)
LIBS+= -framework CoreFoundation -framework Security -lpthread
endif

include ../../Makefile.modules
