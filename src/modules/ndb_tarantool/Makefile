#
# WARNING: do not run this directly, it should be run by the main Makefile
#

include ../../Makefile.defs
auto_gen=
NAME=ndb_tarantool.so

MSGPACK_EXISTS = $(shell pkg-config --exists msgpack-c 2>/dev/null && echo "yes")

ifeq ($(MSGPACK_EXISTS),yes)
	MSGPACKDEFS = $(shell pkg-config --cflags msgpack-c 2>/dev/null)
	MSGPACKLIBS = $(shell pkg-config --libs msgpack-c 2>/dev/null)
else
	MSGPACKDEFS = -I$(LOCALBASE)/include -I/usr/include
	MSGPACKLIBS = -L$(LOCALBASE)/lib -lmsgpack-c
endif

DEFS += $(MSGPACKDEFS)
LIBS = $(MSGPACKLIBS)

include ../../Makefile.modules
