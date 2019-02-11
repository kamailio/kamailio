#
# PUA_JSON module
#
#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs

auto_gen=
NAME=pua_json.so
LIBS=
JLIB=json

BUILDER = $(shell which pkg-config)
ifeq ($(BUILDER),)
	JSONC=$(shell ls $(SYSBASE)/include/lib/libjson*.so $(LOCALBASE)/lib/libjson*.so 2>/dev/null | grep json-c)
else
	JSONC=$(shell pkg-config --libs json-c 2>/dev/null | grep json-c)
endif

ifneq ($(JSONC),)
	JLIB=json-c
endif

ifeq ($(BUILDER),)
	DEFS+=-I/usr/include/$(JLIB) -I$(LOCALBASE)/include/$(JLIB) \
       -I$(LOCALBASE)/include
	LIBS+=-L$(SYSBASE)/include/lib -L$(LOCALBASE)/lib -l$(JLIB)
else
	DEFS+= $(shell pkg-config --cflags $(JLIB))
	LIBS+= $(shell pkg-config --libs $(JLIB))
endif

include ../../Makefile.modules
