#
# ims_qos_npn make file
#
#

include ../../Makefile.defs
auto_gen=
NAME=ims_qos_npn.so
LIBS=

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/ims/kamailio_ims

ifneq ($(OS),darwin)
	LIBS += -lrt
	LIBS += -lpthread
endif

include ../../Makefile.modules
