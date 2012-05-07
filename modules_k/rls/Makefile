# $Id: Makefile 1856 2007-03-15 21:06:00Z  $
#
# Resource List Server
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=rls.so
LIBS=

DEFS+=-I/usr/include/libxml2 -I$(LOCALBASE)/include/libxml2 \
      -I$(LOCALBASE)/include
LIBS+=-L$(LOCALBASE)/lib -lxml2

DEFS+=-DOPENSER_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/kmi/kmi
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1
SER_LIBS+=$(SERLIBPATH)/kcore/kcore
include ../../Makefile.modules
