#
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=evapi.so

ifeq ($(CROSS_COMPILE),)
	BUILDER = $(shell which pkg-config)
ifneq ($(BUILDER),)
	PKGLIBEVENT = $(shell $(BUILDER) --exists libevent > /dev/null 2>&1 ; echo $$? )
ifneq ($(PKGLIBEVENT),0)
	BUILDER =
endif
endif
endif

ifneq ($(BUILDER),)
	DEFS += $(shell $(BUILDER) --cflags libevent)
	LIBS += $(shell $(BUILDER) --libs libevent)
else
ifneq (,$(findstring darwin,$(OS)))
	DEFS += -I/opt/local/include -I$(LOCALBASE)/include
	LIBS += -L/opt/local/lib -L$(LOCALBASE)/lib -levent
else
	DEFS += -I$(LOCALBASE)/include -I$(SYSBASE)/include
	LIBS += -L$(LOCALBASE)/lib -L$(SYSBASE)/lib -levent
endif
endif

include ../../Makefile.modules
