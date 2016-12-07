#
# utils module makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=utils.so

ifeq ($(CROSS_COMPILE),)
XML2CFG=$(shell which xml2-config)
CURL_BUILDER=$(shell \
	if pkg-config --exists libcurl; then \
		echo 'pkg-config libcurl'; \
	else \
		which curl-config; \
	fi)
endif

ifneq ($(XML2CFG),)
	DEFS += $(shell $(XML2CFG) --cflags )
	LIBS += $(shell $(XML2CFG) --libs)
else
	DEFS+=-I$(LOCALBASE)/include/libxml2 \
      -I$(LOCALBASE)/include
	LIBS+=-L$(LOCALBASE)/lib -lxml2
endif

ifneq ($(CURL_BUILDER),)
	DEFS += $(shell $(CURL_BUILDER) --cflags )
	LIBS += $(shell $(CURL_BUILDER) --libs)
else
	DEFS+=-I$(LOCALBASE)/include
	LIBS+=-L$(LOCALBASE)/lib -lcurl
endif

DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1
SER_LIBS+=$(SERLIBPATH)/kmi/kmi
include ../../Makefile.modules
