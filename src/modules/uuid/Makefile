#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=uuid.so

ifeq ($(CROSS_COMPILE),)
BUILDER = $(shell \
	if pkg-config --exists uuid; then \
		echo 'pkg-config uuid'; \
	fi)
endif

ifneq ($(BUILDER),)
	DEFS += $(shell $(BUILDER) --cflags)
	LIBS += $(shell $(BUILDER) --libs)
else
	DEFS +=-I$(LOCALBASE)/include -I$(SYSBASE)/include
	LIBS+=-L$(LOCALBASE)/lib -L$(SYSBASE)/lib -luuid
endif

include ../../Makefile.modules
