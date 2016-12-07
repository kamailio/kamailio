#
# XCAP client for Kamailio
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=xcap_client.so

ifeq ($(CROSS_COMPILE),)
CURL_BUILDER=$(shell \
	if pkg-config --exists libcurl; then \
		echo 'pkg-config libcurl'; \
	else \
		which curl-config; \
	fi)
endif

ifneq ($(CURLCFG),)
	DEFS += $(shell $(CURLCFG) --cflags)
	LIBS += $(shell $(CURLCFG) --libs)
else
	DEFS+=-I$(LOCALBASE)/include
	LIBS+=-L$(LOCALBASE)/lib -lcurl
endif

DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1
SER_LIBS+=$(SERLIBPATH)/kmi/kmi

include ../../Makefile.modules
