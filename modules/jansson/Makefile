#
# jansson module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=jansson.so

BUILDER = $(shell which pkg-config)
ifeq ($(BUILDER),)
	DEFS+=-I$(LOCALBASE)/include \
	LIBS+=-L$(SYSBASE)/include/lib -L$(LOCALBASE)/lib -ljansson
else
	DEFS+= $(shell pkg-config --cflags jansson)
	LIBS+= $(shell pkg-config --libs jansson)
endif
DEFS+=-DOPENSER_MOD_INTERFACE

SERLIBPATH=../../lib
include ../../Makefile.modules
