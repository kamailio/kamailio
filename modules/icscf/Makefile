# $Id$
#
# ICSCF make file
#
# 


include ../../Makefile.defs
auto_gen=
NAME=icscf.so
LIBS=

DEFS += -DOPENSER_MOD_INTERFACE

DEFS += -I/usr/include/libxml2
LIBS += -L$(LOCALBASE)/lib -lxml2 -lrt


SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1 $(SERLIBPATH)/kcore/kcore
SER_LIBS+=$(SERLIBPATH)/ims/kamailio_ims





include ../../Makefile.modules
