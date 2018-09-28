#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=log_systemd.so

ifeq ($(CROSS_COMPILE),)
SYSTEMD_BUILDER=$(shell \
	if pkg-config --exists libsystemd; then \
		echo 'pkg-config libsystemd'; \
	fi)
endif

ifneq ($(SYSTEMD_BUILDER),)
	DEFS += $(shell $(SYSTEMD_BUILDER) --cflags)
	LIBS += $(shell $(SYSTEMD_BUILDER) --libs)
else
	DEFS += -I$(LOCALBASE)/include
	LIBS= -L$(LOCALBASE)/lib -lsystemd
endif

include ../../Makefile.modules
