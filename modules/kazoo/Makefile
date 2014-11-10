# $Id$
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=kazoo.so

DEFS+=-I$(CURDIR)/deps -I$(LOCALBASE)/include -I$(SYSBASE)/include 
LIBS=-L$(LOCALBASE)/lib -I$(SYSBASE)/lib

DEFS += -DKAMAILIO_MOD_INTERFACE


SERLIBPATH=../../lib
SER_LIBS=$(SERLIBPATH)/srdb2/srdb2 $(SERLIBPATH)/srdb1/srdb1
SER_LIBS+=$(SERLIBPATH)/kmi/kmi
SER_LIBS+=$(SERLIBPATH)/kcore/kcore
SER_LIBS+=$(CURDIR)/deps/kazoo-deps

include ../../Makefile.modules
