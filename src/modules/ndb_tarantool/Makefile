#
# WARNING: do not run this directly, it should be run by the main Makefile
#

include ../../Makefile.defs
auto_gen=
NAME=ndb_tarantool.so

ifeq ($(MSGPACKDEFS),)
MSGPACK_PKG = $(shell \
	if pkg-config --exists msgpack-c 2>/dev/null; then echo "msgpack-c"; \
	elif pkg-config --exists msgpack 2>/dev/null; then echo "msgpack"; \
	elif pkg-config --exists msgpackc 2>/dev/null; then echo "msgpackc"; \
	fi)

ifneq ($(MSGPACK_PKG),)
	MSGPACKDEFS = $(shell pkg-config --cflags $(MSGPACK_PKG) 2>/dev/null)
	MSGPACKLIBS = $(shell pkg-config --libs $(MSGPACK_PKG) 2>/dev/null)
else
	MSGPACKDEFS = -I$(LOCALBASE)/include -I/usr/include
	MSGPACKLIBS = -L$(LOCALBASE)/lib -lmsgpack-c
endif
endif

DEFS += $(MSGPACKDEFS)
LIBS = $(MSGPACKLIBS)

include ../../Makefile.modules
