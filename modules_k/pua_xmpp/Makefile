# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME= pua_xmpp.so
LIBS=

DEFS+=-I/usr/include/libxml2 -I$(LOCALBASE)/include/libxml2 -I$(LOCALBASE)/include
LIBS+=-L$(LOCALBASE)/lib -lxml2

DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/kcore/kcore
include ../../Makefile.modules
