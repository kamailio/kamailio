#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=kazoo.so
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
	DEFS+=-I$(LOCALBASE)/include -I$(SYSBASE)/include \
		-I$(SYSBASE)/include/$(JLIB)
	LIBS=-L$(LOCALBASE)/lib -I$(SYSBASE)/lib -lrabbitmq -l$(JLIB) -luuid
else
	DEFS += $(shell $(BUILDER) --cflags $(JLIB))
	LIBS += $(shell $(BUILDER) --libs $(JLIB))
	DEFS += $(shell $(BUILDER) --cflags uuid)
	LIBS += $(shell $(BUILDER) --libs uuid)
	DEFS += $(shell $(BUILDER) --cflags librabbitmq)
	LIBS += $(shell $(BUILDER) --libs librabbitmq)
endif

DEFS += -DKAMAILIO_MOD_INTERFACE


SERLIBPATH=../../lib
SER_LIBS=$(SERLIBPATH)/srdb2/srdb2 $(SERLIBPATH)/srdb1/srdb1
SER_LIBS+=$(SERLIBPATH)/kmi/kmi
SER_LIBS+=$(SERLIBPATH)/kcore/kcore

include ../../Makefile.modules
