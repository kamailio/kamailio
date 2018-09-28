#
# registrar module makefile
#
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=ims_ipsec_pcscf.so

ifneq ($(OS),darwin)
	LIBS += -lpthread
	LIBS += -lmnl
endif

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/ims/kamailio_ims
include ../../Makefile.modules
