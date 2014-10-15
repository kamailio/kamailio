#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=evapi.so

ifeq ($(CROSS_COMPILE),)
	BUILDER = $(shell which pkg-config)
ifneq ($(BUILDER),)
		PKGLIBUV = $(shell $(BUILDER) --exists libev > /dev/null 2>&1 ; echo $$? )
ifneq ($(PKGLIBUV),0)
			BUILDER =
endif
endif
endif

ifneq ($(BUILDER),)
	DEFS += $(shell $(BUILDER) --cflags libev)
	LIBS += $(shell $(BUILDER) --libs libev)
else
ifeq ($(OS), darwin)
	DEFS += -I/opt/local/include
	LIBS += -L/opt/local/lib
endif
	DEFS += -I$(LOCALBASE)/include -I$(SYSBASE)/include
	LIBS += -L$(LOCALBASE)/lib -L$(SYSBASE)/lib -lev
endif


DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srutils/srutils
SER_LIBS+=$(SERLIBPATH)/kcore/kcore

include ../../Makefile.modules
