#
# jansson module makefile
#
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=jansson.so

ifeq ($(CROSS_COMPILE),)
JNS_BUILDER=$(shell \
	if pkg-config --exists jansson; then \
		echo 'pkg-config jansson'; \
	fi)
endif

ifneq ($(JNS_BUILDER),)
	DEFS += $(shell $(JNS_BUILDER) --cflags)
	LIBS += $(shell $(JNS_BUILDER) --libs)
else
	DEFS+=-I$(LOCALBASE)/include
	LIBS+=-L$(SYSBASE)/include/lib -L$(LOCALBASE)/lib -ljansson
endif

DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
include ../../Makefile.modules
