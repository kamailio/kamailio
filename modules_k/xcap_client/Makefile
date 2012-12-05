# $Id: Makefile $
#
# XCAP client for Kamailio
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=xcap_client.so

ifeq ($(CROSS_COMPILE),)
CURLCFG=$(shell which curl-config)
endif

ifneq ($(CURLCFG),)
	DEFS += $(shell $(CURLCFG) --cflags)
	LIBS += $(shell $(CURLCFG) --libs)
else
	DEFS+=-I$(LOCALBASE)/include
	LIBS+=-L$(SYSBASE)/lib -L$(LOCALBASE)/lib -lcurl
endif

DEFS+=-DKAMAILIO_MOD_INTERFACE

SERLIBPATH=../../lib
SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1
SER_LIBS+=$(SERLIBPATH)/kmi/kmi

include ../../Makefile.modules
