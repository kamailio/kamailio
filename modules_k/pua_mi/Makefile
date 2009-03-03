# $Id$
#
# PUBLISH
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=pua_mi.so
LIBS=

DEFS+=-I$(LOCALBASE)/include
LIBS+=-L$(SYSBASE)/include/lib -L$(LOCALBASE)/lib

DEFS+=-DOPENSER_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/kmi/kmi
include ../../Makefile.modules
