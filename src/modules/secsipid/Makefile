#
# secsipid module makefile
#
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=secsipid.so

BUILDER = $(shell which pkg-config)
ifeq ($(BUILDER),)
	DEFS+= -I.
	LIBS = -L. -lsecsipid
else
	DEFS+= $(shell pkg-config --cflags secsipid-1)
	LIBS = $(shell pkg-config --libs secsipid-1)
endif

ifeq ($(OS), darwin)
LIBS+= -framework CoreFoundation -framework Security -lpthread
endif

include ../../Makefile.modules
