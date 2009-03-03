#
# Least Cost Routing Module
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=lcr.so
DEFS+=-I$(LOCALBASE)/include
LIBS=-L$(LOCALBASE)/lib -lpcre

DEFS+=-DOPENSER_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/kmi/kmi
include ../../Makefile.modules

