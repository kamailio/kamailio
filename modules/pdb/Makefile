# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs

auto_gen=
NAME=pdb.so

LIBS=
DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/kmi/kmi $(SERLIBPATH)/kcore/kcore

include ../../Makefile.modules
