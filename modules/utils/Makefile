#
# utils module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=utils.so
LIBS=

DEFS+=-I/usr/include/libxml2 -I$(LOCALBASE)/include/libxml2 \
      -I$(LOCALBASE)/include
LIBS+=-L$(LOCALBASE)/lib -lcurl -lxml2

DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1
SER_LIBS+=$(SERLIBPATH)/kmi/kmi
include ../../Makefile.modules
