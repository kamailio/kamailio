#
# json module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=json.so
LIBS=

DEFS+=-I/usr/include/json -I$(LOCALBASE)/include/json \
       -I$(LOCALBASE)/include
LIBS+=-L$(SYSBASE)/include/lib -L$(LOCALBASE)/lib -ljson
 
DEFS+=-DOPENSER_MOD_INTERFACE

SERLIBPATH=../../lib
# SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1
# SER_LIBS+=$(SERLIBPATH)/kmi/kmi
include ../../Makefile.modules
