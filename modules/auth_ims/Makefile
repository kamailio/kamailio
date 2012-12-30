# $Id$
#
# Digest Authentication - IMS support
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=auth_ims.so
LIBS=

DEFS+=-DOPENSER_MOD_INTERFACE

DEFS += -I/usr/include/libxml2
LIBS += -L$(LOCALBASE)/lib -lxml2 -lrt

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/kcore/kcore
SER_LIBS+=$(SERLIBPATH)/ims/kamailio_ims

include ../../Makefile.modules
