# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=erlang.so

LIBS=-L$(LOCALBASE)/lib/erlang/usr/lib -lei -lpthread

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/kcore/kcore

DEFS+=-I/usr/local/lib/erlang/usr/include
DEFS+=-DKAMAILIO_MOD_INTERFACE -D_REENTRANT


include ../../Makefile.modules

