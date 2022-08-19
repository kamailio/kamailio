#
# utils module makefile
#
#
# WARNING: do not run this directly, it should be run by the main Makefile

include ../../Makefile.defs
auto_gen=
NAME=utils.so

ifeq ($(CROSS_COMPILE),)
XML2CFG=$(shell which xml2-config)
ifeq ($(XML2CFG),)
XML2CFG=$(shell \
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

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1
include ../../Makefile.modules
