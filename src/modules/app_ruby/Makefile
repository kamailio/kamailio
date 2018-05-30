#
# app_ruby module makefile
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=app_ruby.so

RUBYVER=ruby-2.5

ifeq ($(CROSS_COMPILE),)
	BUILDER = $(shell which pkg-config)
ifneq ($(BUILDER),)
	PKGLIBRUBY = $(shell $(BUILDER) --exists $(RUBYVER) > /dev/null 2>&1 ; echo $$? )
ifneq ($(PKGLIBRUBY),0)
	BUILDER =
endif
endif
endif

ifneq ($(BUILDER),)
	DEFS += $(shell $(BUILDER) --cflags $(RUBYVER))
	LIBS += $(shell $(BUILDER) --libs $(RUBYVER))
else
ifneq (,$(findstring darwin,$(OS)))
	DEFS += -I/opt/local/include -I$(LOCALBASE)/include
	LIBS += -L/opt/local/lib -L$(LOCALBASE)/lib -lev
else
	DEFS += -I$(LOCALBASE)/include -I$(SYSBASE)/include
	LIBS += -L$(LOCALBASE)/lib -L$(SYSBASE)/lib -lruby -lpthread -ldl -lobjc
endif
endif

DEFS+=-DKAMAILIO_MOD_INTERFACE

include ../../Makefile.modules
