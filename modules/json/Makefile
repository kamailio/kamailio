#
# json module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=json.so
LIBS=

BUILDER = $(shell which pkg-config)
ifeq ($(BUILDER),)
	DEFS+=-I/usr/include/json -I$(LOCALBASE)/include/json \
       -I$(LOCALBASE)/include
	LIBS+=-L$(SYSBASE)/include/lib -L$(LOCALBASE)/lib -ljson
else
	DEFS+= $(shell pkg-config --cflags json)
	LIBS+= $(shell pkg-config --libs json)
endif
DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
# SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1
# SER_LIBS+=$(SERLIBPATH)/kmi/kmi
include ../../Makefile.modules
