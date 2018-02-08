#
# PUA_JSON module
#
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs

auto_gen=
NAME=pua_json.so

LIBS=-ljson-c
DEFS+=-I$(LOCALBASE)/include -I/usr/local/include $(shell pkg-config --cflags json-c)

DEFS+=-DKAMAILIO_MOD_INTERFACE

include ../../Makefile.modules
