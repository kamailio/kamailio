# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=rabbitmq.so
LIBS=

ifeq ($(CROSS_COMPILE),)
RMQ_BUILDER=$(shell \
	if pkg-config --exists librabbitmq && pkg-config --exists uuid; then \
		echo 'pkg-config librabbitmq uuid'; \
	fi)

	ifeq ($(shell pkg-config --atleast-version=0.13.0 librabbitmq; echo $$?),0)
		DEFS += -DRABBITMQ_DEPRECATION
	endif
endif

ifneq ($(RMQ_BUILDER),)
	DEFS += $(shell $(RMQ_BUILDER) --cflags)
	LIBS += $(shell $(RMQ_BUILDER) --libs)
else
	DEFS+=-I$(LOCALBASE)/include
	LIBS+=-L$(SYSBASE)/include/lib -L$(LOCALBASE)/lib -lrabbitmq -luuid
endif
${eval LIBS=$(LIBS)}

include ../../Makefile.modules
