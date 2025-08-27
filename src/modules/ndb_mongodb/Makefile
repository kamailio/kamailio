#
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=ndb_mongodb.so

ifeq ($(CROSS_COMPILE),)
MONGODBC_BUILDER=$(shell \
	if pkg-config --exists libmongoc-1.0; then \
		echo 'pkg-config libmongoc-1.0'; \
	elif pkg-config --exists mongoc2; then \
		echo 'pkg-config mongoc2'; \
	fi)
endif

ifneq ($(MONGODBC_BUILDER),)
	DEFS += $(shell $(MONGODBC_BUILDER) --cflags)
	LIBS += $(shell $(MONGODBC_BUILDER) --libs)
	ifneq ($(findstring libmongoc-1.0,$(MONGODBC_BUILDER)),)
		DEFS += -DHAVE_LIBMONGOC1
	endif
else
	DEFS += -I$(LOCALBASE)/include
	LIBS= -L$(LOCALBASE)/lib -lmongoc
endif

include ../../Makefile.modules
