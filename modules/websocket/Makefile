# $Id$
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=websocket.so

BUILDER = $(shell which pkg-config)
ifeq ($(BUILDER),)
        DEFS+= -I$(LOCALBASE)/ssl/include
        LIBS=  -L$(LOCALBASE)/lib -L$(LOCALBASE)/ssl/lib \
			-L$(LOCALBASE)/lib64 -L$(LOCALBASE)/ssl/lib64 \
			-lssl
else
	DEFS+= $(shell pkg-config --cflags libssl)
	LIBS=  $(shell pkg-config --libs libssl)
endif

DEFS+=-DOPENSER_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/kcore/kcore

include ../../Makefile.modules

