#
# Peering module Makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=peering.so
DEFS+= -DWITH_RADIUS_SUPPORT -I$(LOCALBASE)/include
LIBS=-L$(LOCALBASE)/lib $(RADIUS_LIB)

DEFS+=-DOPENSER_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/kcore/kcore
include ../../Makefile.modules
