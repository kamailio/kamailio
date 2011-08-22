#
# jsonrpc module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=jsonrpc-c.so
LIBS=-lm -levent

DEFS+=-I/usr/include/json -I$(LOCALBASE)/include/json \
       -I$(LOCALBASE)/include
LIBS+=-L$(SYSBASE)/include/lib -L$(LOCALBASE)/lib -ljson
 
DEFS+=-DOPENSER_MOD_INTERFACE

SERLIBPATH=../../lib
include ../../Makefile.modules
