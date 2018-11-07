#
# registrar module makefile
#
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=ims_ipsec_pcscf.so

ifeq ($(CROSS_COMPILE),)
	BUILDER = $(shell which pkg-config)
ifneq ($(BUILDER),)
	PKGLIBMNL = $(shell $(BUILDER) --exists libmnl > /dev/null 2>&1 ; echo $$? )
ifneq ($(PKGLIBMNL),0)
	BUILDER =
endif
endif
endif

ifneq ($(BUILDER),)
	DEFS += $(shell $(BUILDER) --cflags libmnl)
	LIBS += $(shell $(BUILDER) --libs libmnl)
else
ifneq ($(OS),darwin)
	LIBS += -lpthread
	LIBS += -lmnl
endif
endif

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/ims/kamailio_ims
include ../../Makefile.modules
