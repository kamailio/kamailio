#
# gzcompress module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=gzcompress.so

ifeq ($(CROSS_COMPILE),)
	BUILDER = $(shell which pkg-config)
endif

ifneq ($(BUILDER),)
	DEFS += $(shell $(BUILDER) --cflags zlib)
	LIBS += $(shell $(BUILDER) --libs zlib)
else
	DEFS += -I$(LOCALBASE)/include
	LIBS += -L$(LOCALBASE)/lib -lz
endif

DEFS+=-DKAMAILIO_MOD_INTERFACE

include ../../Makefile.modules
