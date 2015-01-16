#
# IMS_ICSCF make file
#
# 

include ../../Makefile.defs
auto_gen=
NAME=ims_icscf.so

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

DEFS += -DOPENSER_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1 $(SERLIBPATH)/kcore/kcore
SER_LIBS+=$(SERLIBPATH)/ims/kamailio_ims
include ../../Makefile.modules
