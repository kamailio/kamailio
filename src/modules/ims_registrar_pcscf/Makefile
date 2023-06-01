#
# ims registrar module makefile
#
#
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=ims_registrar_pcscf.so

ifeq ($(CROSS_COMPILE),)
XML2CFG:=$(shell which xml2-config)
ifeq ($(XML2CFG),)
XML2CFG:=$(shell \
	if pkg-config --exists libxml-2.0; then \
		echo 'pkg-config libxml-2.0'; \
	fi)
endif
endif

ifneq ($(XML2CFG),)
	DEFS += $(shell $(XML2CFG) --cflags )
	LIBS += $(shell $(XML2CFG) --libs)
else
	DEFS+=-I$(LOCALBASE)/include/libxml2 \
		-I$(LOCALBASE)/include
	LIBS+=-L$(LOCALBASE)/lib -lxml2
endif

ifneq ($(OS),darwin)
	LIBS += -lrt
	LIBS += -lpthread
endif

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/ims/kamailio_ims
include ../../Makefile.modules
