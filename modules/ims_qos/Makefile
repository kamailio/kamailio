#
# ims_qos make file
#
# 

include ../../Makefile.defs
auto_gen=
NAME=ims_qos.so
LIBS=

DEFS+=-DOPENSER_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/kcore/kcore
SER_LIBS+=$(SERLIBPATH)/ims/kamailio_ims

ifneq ($(OS),darwin)
	LIBS += -lrt
	LIBS += -lpthread
endif

include ../../Makefile.modules
