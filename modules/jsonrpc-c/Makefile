#
# jsonrpc module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=jsonrpc-c.so
LIBS=-lm

BUILDER = $(shell which pkg-config)
ifeq ($(BUILDER),)
	DEFS+=-I/usr/include/json -I$(LOCALBASE)/include/json \
       -I$(LOCALBASE)/include
	LIBS+=-L$(SYSBASE)/include/lib -L$(LOCALBASE)/lib -levent -ljson
else
	DEFS+= $(shell pkg-config --cflags json)
	LIBS+= $(shell pkg-config --libs json)
	DEFS+= $(shell pkg-config --cflags libevent)
	LIBS+= $(shell pkg-config --libs libevent)
endif
 
DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
include ../../Makefile.modules
