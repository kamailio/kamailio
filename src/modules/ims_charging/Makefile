#
# ims_charging make file
#
#

include ../../Makefile.defs
auto_gen=
NAME=ims_charging.so
LIBS=

DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/ims/kamailio_ims
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1
LIBS+=-lm

ifneq ($(OS),darwin)
	LIBS += -lrt
endif

include ../../Makefile.modules
