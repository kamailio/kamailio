#
# registrar module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=ims_registrar_scscf.so

ifeq ($(CROSS_COMPILE),)
XML2CFG=$(shell which xml2-config)
endif

ifneq ($(XML2CFG),)
	DEFS += $(shell $(XML2CFG) --cflags )
	LIBS += $(shell $(XML2CFG) --libs)
else
	DEFS+=-I$(LOCALBASE)/include/libxml2 \
		-I$(LOCALBASE)/include
	LIBS+=-L$(LOCALBASE)/lib -lxml2
endif

DEFS+=-DOPENSER_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/kcore/kcore
SER_LIBS+=$(SERLIBPATH)/ims/kamailio_ims

ifneq ($(OS),darwin)
	LIBS += -lrt
	LIBS += -lpthread
endif

include ../../Makefile.modules
