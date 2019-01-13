#
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=presence.so

ifeq ($(CROSS_COMPILE),)
XML2CFG=$(shell which xml2-config)
ifeq ($(OS), darwin)
ICU_BUILDER = $(shell \
	if pkg-config --exists icu-uc; then \
		echo 'pkg-config icu-uc'; \
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

ifneq ($(ICU_BUILDER),)
	DEFS += $(shell $(ICU_BUILDER) --cflags )
	LIBS += $(shell $(ICU_BUILDER) --libs)
endif

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1
SER_LIBS+=$(SERLIBPATH)/srutils/srutils
include ../../Makefile.modules
