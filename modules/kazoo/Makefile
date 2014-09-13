# $Id$
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=kazoo.so

DEFS+=-I$(LOCALBASE)/include -I$(SYSBASE)/include -DKAMAILIO_MOD_INTERFACE
LIBS=-L$(LOCALBASE)/lib -I$(SYSBASE)/lib -lrabbitmq -ljson -luuid

DEFS += -DSER_MOD_INTERFACE


SERLIBPATH=../../lib
SER_LIBS=$(SERLIBPATH)/srdb2/srdb2 $(SERLIBPATH)/srdb1/srdb1
SER_LIBS+=$(SERLIBPATH)/kmi/kmi
SER_LIBS+=$(SERLIBPATH)/kcore/kcore

include ../../Makefile.modules
