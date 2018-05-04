#
# janssonrpcc module makefile
#
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=janssonrpcc.so
LIBS=-lm

BUILDER = $(shell which pkg-config)
ifeq ($(BUILDER),)
	DEFS+=-I$(LOCALBASE)/include
	LIBS+=-L$(SYSBASE)/include/lib -L$(LOCALBASE)/lib -levent -ljansson
else
	DEFS+= $(shell pkg-config --cflags jansson)
	LIBS+= $(shell pkg-config --libs jansson)
	DEFS+= $(shell pkg-config --cflags libevent)
	LIBS+= $(shell pkg-config --libs libevent)
endif

DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
include ../../Makefile.modules
