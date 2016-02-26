#
# example module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=dnssec.so
LIBS= -lval-threads -lcrypto -lsres -lpthread -lssl

DEFS+=-DKAMAILIO_MOD_INTERFACE
DEFS +=-I$(LOCALBASE)/include

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/kmi/kmi
include ../../Makefile.modules
