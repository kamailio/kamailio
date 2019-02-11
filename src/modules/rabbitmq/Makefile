# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=rabbitmq.so
LIBS=

ifeq ($(CROSS_COMPILE),)
RMQ_BUILDER=$(shell \
	if pkg-config --exists librabbitmq && pkg-config --exists uuid; then \
		echo 'pkg-config librabbitmq uuid'; \
	fi)
endif

ifneq ($(RMQ_BUILDER),)
	DEFS += $(shell $(RMQ_BUILDER) --cflags)
	LIBS += $(shell $(RMQ_BUILDER) --libs)
else
	DEFS+=-I$(LOCALBASE)/include
	LIBS+=-L$(SYSBASE)/include/lib -L$(LOCALBASE)/lib -lrabbitmq -luuid
endif

include ../../Makefile.modules
