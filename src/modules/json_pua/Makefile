#
# JSON_PUA module
#
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs

auto_gen=
NAME=json_pua.so

LIBS=-ljson-c
DEFS+=-I$(LOCALBASE)/include -I/usr/local/include $(shell pkg-config --cflags json-c)

DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS=$(SERLIBPATH)/srdb1/srdb1

include ../../Makefile.modules
