# $Id$
#
# registrar module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=registrar_pcscf.so
LIBS=

DEFS+=-DOPENSER_MOD_INTERFACE -I/usr/include/libxml2 -I$(LOCALBASE)/include/libxml2

LIBS += -L$(LOCALBASE)/lib -lrt

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/kcore/kcore
SER_LIBS+=$(SERLIBPATH)/ims/kamailio_ims
include ../../Makefile.modules
