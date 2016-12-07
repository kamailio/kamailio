# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=xmlrpc.so

ifeq ($(CROSS_COMPILE),)
XML2CFG=$(shell which xml2-config)
endif

ifneq ($(XML2CFG),)
	DEFS += $(shell $(XML2CFG) --cflags )
	LIBS += $(shell $(XML2CFG) --libs)
else
	INCLUDES +=-I$(LOCALBASE)/include/libxml2 \
				-I$(LOCALBASE)/include
	LIBS += -L$(LOCALBASE)/lib -lxml2
endif

# use system malloc
DEFS+=-DXMLRPC_SYSTEM_MALLOC
DEFS+=-DSER_MOD_INTERFACE

include ../../Makefile.modules
