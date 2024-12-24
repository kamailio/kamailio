#
#
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=lwsc.so

ifeq ($(CROSS_COMPILE),)
LWSC_BUILDER=$(shell \
	if pkg-config --exists libwebsockets; then \
		echo 'pkg-config libwebsockets'; \
	fi)
endif

ifneq ($(LWSC_BUILDER),)
	DEFS += $(shell $(LWSC_BUILDER) --cflags)
	LIBS += $(shell $(LWSC_BUILDER) --libs)
else
	DEFS += -I$(LOCALBASE)/include
	LIBS += -L$(LOCALBASE)/lib -lwebsockets
endif
${eval LIBS=$(LIBS)}

include ../../Makefile.modules

