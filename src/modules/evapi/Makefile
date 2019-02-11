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
ifneq (,$(findstring darwin,$(OS)))
	DEFS += -I/opt/local/include -I$(LOCALBASE)/include
	LIBS += -L/opt/local/lib -L$(LOCALBASE)/lib -lev
else
	DEFS += -I$(LOCALBASE)/include -I$(SYSBASE)/include
	LIBS += -L$(LOCALBASE)/lib -L$(SYSBASE)/lib -lev
endif
endif


SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srutils/srutils

include ../../Makefile.modules
