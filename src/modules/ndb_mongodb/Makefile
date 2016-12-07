#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=ndb_mongodb.so

ifeq ($(CROSS_COMPILE),)
MONGODBC_BUILDER=$(shell \
	if pkg-config --exists libmongoc-1.0; then \
		echo 'pkg-config libmongoc-1.0'; \
	fi)
endif

ifneq ($(MONGODBC_BUILDER),)
	DEFS += $(shell $(MONGODBC_BUILDER) --cflags)
	LIBS += $(shell $(MONGODBC_BUILDER) --libs)
else
	DEFS += -I$(LOCALBASE)/include
	LIBS= -L$(LOCALBASE)/lib -lmongoc
endif

DEFS+=-DKAMAILIO_MOD_INTERFACE

include ../../Makefile.modules
