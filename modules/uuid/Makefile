#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=uuid.so

ifeq ($(CROSS_COMPILE),)
	BUILDER = $(shell which pkg-config)
endif

ifneq ($(BUILDER),)
	DEFS += $(shell $(BUILDER) --cflags uuid)
	LIBS += $(shell $(BUILDER) --libs uuid)
else
	DEFS +=-I$(LOCALBASE)/include -I$(SYSBASE)/include
	LIBS+=-L$(LOCALBASE)/lib -L$(SYSBASE)/lib -luuid
endif


DEFS+=-DKAMAILIO_MOD_INTERFACE

include ../../Makefile.modules
