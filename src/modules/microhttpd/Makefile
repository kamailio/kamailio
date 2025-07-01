#
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=microhttpd.so

ifeq ($(CROSS_COMPILE),)
BUILDER = $(shell \
	if pkg-config --exists microhttpd; then \
		echo 'pkg-config microhttpd'; \
	fi)
endif

ifneq ($(BUILDER),)
	DEFS += $(shell $(BUILDER) --cflags)
	LIBS += $(shell $(BUILDER) --libs)
else
	DEFS +=-I$(LOCALBASE)/include -I$(SYSBASE)/include
	LIBS +=-L$(LOCALBASE)/lib -L$(SYSBASE)/lib -lmicrohttpd
endif

include ../../Makefile.modules
