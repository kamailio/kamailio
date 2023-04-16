#
# app_ruby module makefile
#
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=app_ruby_proc.so

PKGLIBRUBY=1

ifeq ($(CROSS_COMPILE),)
	BUILDER = $(shell which pkg-config)
ifneq ($(BUILDER),)
	RUBYVER = $(shell $(BUILDER) --list-all | grep ruby | tail -1 | cut -f 1 -d " ")
ifneq ($(RUBYVER),)
	PKGLIBRUBY = $(shell $(BUILDER) --exists $(RUBYVER) > /dev/null 2>&1 ; echo $$? )
endif
endif
endif

ifneq ($(PKGLIBRUBY),0)
	BUILDER =
endif

ifneq ($(BUILDER),)
	DEFS += $(shell $(BUILDER) --cflags $(RUBYVER))
	LIBS = $(shell $(BUILDER) --libs $(RUBYVER))
else
ifneq (,$(findstring darwin,$(OS)))
	DEFS += -I/opt/local/include -I$(LOCALBASE)/include
	LIBS = -L/opt/local/lib -L$(LOCALBASE)/lib  -lruby -lpthread -ldl
else
	DEFS += -I$(LOCALBASE)/include -I$(SYSBASE)/include
	LIBS = -L$(LOCALBASE)/lib -L$(SYSBASE)/lib -lruby -lpthread -ldl
endif
endif

include ../../Makefile.modules
